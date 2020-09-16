#define NBNET_IMPL

#include "soak.h"

/* nbnet UDP driver implementation */
#define NBN_DRIVER_UDP_IMPL

#include "../net_drivers/udp.h"

typedef struct
{
    unsigned int id;
    unsigned int recved_messages_count;
    unsigned int last_recved_message_id;
    bool error;
    NBN_List *echo_queue;
    NBN_Connection *connection;
} SoakClient;

static SoakClient *clients[SOAK_MAX_CLIENTS] = {NULL};
static unsigned int client_count = 0;

static void HandleNewConnection(void)
{
    if (client_count == SOAK_MAX_CLIENTS)
        NBN_GameServer_RejectConnection(-1);

    assert(clients[client_count] == NULL);

    NBN_Connection *connection = NBN_GameServer_AcceptConnection();
    SoakClient *soak_client = malloc(sizeof(SoakClient));

    soak_client->id = connection->id;
    soak_client->recved_messages_count = 0;
    soak_client->last_recved_message_id = 0;
    soak_client->error = false;
    soak_client->echo_queue = NBN_List_Create();
    soak_client->connection = connection;

    clients[client_count++] = soak_client;

    Soak_LogInfo("Client has connected (ID: %d)", soak_client->id);
}

static void HandleClientDisconnection(uint32_t client_id)
{
    SoakClient *soak_client = clients[client_id];

    assert(soak_client != NULL);

    Soak_LogInfo("Client has disconnected (ID: %d)", client_id);

    NBN_List_Destroy(soak_client->echo_queue, true, (void (*)(void *))SoakMessage_Destroy);
    free(soak_client);
}

static void EchoReceivedSoakMessages(void)
{
    for (int i = 0; i < client_count; i++)
    {
        SoakClient *soak_client = clients[i];

        assert(soak_client != NULL);

        NBN_ListNode *current_echo_node = soak_client->echo_queue->head;

        while (current_echo_node)
        {
            SoakMessage *msg = current_echo_node->data;
            SoakMessage *echo_msg = NBN_GameServer_CreateReliableMessage(SOAK_MESSAGE);

            current_echo_node = current_echo_node->next;

            if (echo_msg == NULL)
            {
                Soak_LogError("Failed to create soak message");
                NBN_GameServer_CloseClient(soak_client->connection, -1);

                return;
            }

            echo_msg->id = msg->id;
            echo_msg->data_length = msg->data_length;

            if (!NBN_GameServer_CanSendMessageTo(soak_client->connection, true))
                break;

            memcpy(echo_msg->data, msg->data, msg->data_length);

            NBN_GameServer_SendMessageTo(soak_client->connection);

            free(NBN_List_Remove(soak_client->echo_queue, msg));
        }
    }
}

static int HandleReceivedSoakMessage(SoakMessage *msg, NBN_Connection *sender)
{
    SoakClient *soak_client = clients[sender->id];

    if (soak_client->error)
      return 0;

    if (msg->id != soak_client->last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d (from client: %d)\n",
          soak_client->last_recved_message_id + 1, msg->id, sender->id);

        soak_client->error = true;

        return -1;
    }

    Soak_LogInfo("Received message %d from client %d\n", msg->id, sender->id);

    soak_client->recved_messages_count++;
    soak_client->last_recved_message_id = msg->id;

    SoakMessage *dup_msg = malloc(sizeof(SoakMessage));

    memcpy(dup_msg, msg, sizeof(SoakMessage));

    NBN_List_PushBack(soak_client->echo_queue, dup_msg);

    return 0;
}

static void HandleReceivedMessage(void)
{
    NBN_MessageInfo msg = NBN_GameServer_GetReceivedMessageInfo();

    switch (msg.type)
    {
    case SOAK_MESSAGE:
        if (HandleReceivedSoakMessage((SoakMessage *)msg.data, msg.sender) < 0)
            NBN_GameServer_CloseClient(msg.sender, -1);
        break;
    
    default:
        Soak_LogError("Received unexpected message (type: %d)", msg.type);

        NBN_GameServer_CloseClient(msg.sender, -1);
        break;
    }
}

static int Tick(void)
{
    NBN_GameServer_AddTime(SOAK_TICK_DT);

    int ev;

    while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
    {
        if (ev < 0)
            return -1;

        switch (ev)
        {
        case NBN_NEW_CONNECTION:
            HandleNewConnection();
            break;

        case NBN_CLIENT_DISCONNECTED:
            HandleClientDisconnection(NBN_GameServer_GetDisconnectedClientId());
            break;

        case NBN_CLIENT_MESSAGE_RECEIVED:
            HandleReceivedMessage();
            break;
        }
    }

    EchoReceivedSoakMessages();

    if (NBN_GameServer_SendPackets() < 0)
    {
        Soak_LogError("Failed to flush game server send queue. Exit");

        return -1;
    }

    return 0;
}

static void SigintHandler(int dummy)
{
    Soak_Stop();
}

int main(int argc, char *argv[])
{
    signal(SIGINT, SigintHandler);

    NBN_GameServer_Init(SOAK_PROTOCOL_NAME, SOAK_PORT);

    if (Soak_Init(argc, argv) < 0)
    {
        NBN_GameServer_Stop();

        return 1;
    }

    NBN_GameServer_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, Soak_Debug_PrintAddedToRecvQueue);

    if (NBN_GameServer_Start())
    {
        Soak_LogError("Failed to start game server");

        return 1;
    }

    int ret = Soak_MainLoop(Tick);

    NBN_GameServer_Stop();
    Soak_Deinit();

    return ret;
}
