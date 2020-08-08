#define NBNET_IMPL

#include "soak.h"

typedef struct
{
    unsigned int id;
    unsigned int recved_messages_count;
    unsigned int last_recved_message_id;
    bool error;
} SoakClient;

static void handle_client_connection(void)
{
    SoakClient *soak_client = malloc(sizeof(SoakClient));

    soak_client->id = NBN_GameServer_GetConnectedClient()->id;
    soak_client->recved_messages_count = 0;
    soak_client->last_recved_message_id = 0;
    soak_client->error = false;

    NBN_GameServer_GetConnectedClient()->user_data = soak_client;

    Soak_LogInfo("Client has connected (id: %d)", soak_client->id);
}

static void handle_client_disconnection(void)
{
    SoakClient *soak_client = NBN_GameServer_GetDisconnectedClient()->user_data;

    Soak_LogInfo("Client has disconnected (id: %d)", soak_client->id);

    free(soak_client);
}

static void close_client(NBN_Connection *client)
{
    NBN_GameServer_CloseClient(client);
}

static int echo_soak_message(SoakMessage *msg, NBN_Connection *sender_cli)
{
    SoakMessage *echo_msg = NBN_GameServer_CreateMessage(SOAK_MESSAGE, SOAK_CHAN_RELIABLE_ORDERED_1, sender_cli);

    if (echo_msg == NULL)
    {
        Soak_LogError("Failed to create soak message");

        return -1;
    }

    echo_msg->id = msg->id;
    echo_msg->data_length = msg->data_length;

    memcpy(echo_msg->data, msg->data, msg->data_length);

    NBN_GameServer_SendMessageTo(sender_cli);

    return 0;
}

static int handle_soak_message(SoakMessage *msg, NBN_Connection *sender_cli)
{
    SoakClient *soak_client = sender_cli->user_data;

    if (soak_client->error)
      return 0;

    if (msg->id != soak_client->last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d (from client: %d)\n",
          soak_client->last_recved_message_id + 1, msg->id, sender_cli->id);

        soak_client->error = true;

        return -1;
    }

    Soak_LogInfo("Received message %d from client %d\n", msg->id, sender_cli->id);

    soak_client->recved_messages_count++;
    soak_client->last_recved_message_id = msg->id;

    if (echo_soak_message(msg, sender_cli) < 0)
        return -1;

    return 0;
}

static void handle_message(void)
{
    NBN_MessageInfo msg;

    NBN_GameServer_ReadReceivedMessage(&msg);

    switch (msg.type)
    {
    case SOAK_MESSAGE:
        if (handle_soak_message((SoakMessage *)msg.data, msg.sender) < 0)
            close_client(msg.sender);
        break;
    
    default:
        Soak_LogError("Received unexpected message (type: %d)", msg.type);

        close_client(msg.sender);
        break;
    }
}

static int tick(void)
{
    NBN_GameServerEvent ev;

    while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
    {
        switch (ev)
        {
        case NBN_CLIENT_CONNECTED:
            handle_client_connection();
            break;

        case NBN_CLIENT_DISCONNECTED:
            handle_client_disconnection();
            break;

        case NBN_CLIENT_CONNECTION_REQUESTED:
            /* unused for now */
            break;

        case NBN_CLIENT_MESSAGE_RECEIVED:
            handle_message();
            break;
        }
    }

    if (NBN_GameServer_Flush() < 0)
    {
        Soak_LogError("Failed to flush game server send queue. Exit");

        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (Soak_ReadCommandLine(argc, argv) < 0)
        return 1;

    NBN_GameServer_Init(SOAK_PROTOCOL_NAME);
    Soak_Init();

    NBN_GameServer_Debug_SetMinPacketLossRatio(Soak_GetOptions().min_packet_loss);
    NBN_GameServer_Debug_SetMaxPacketLossRatio(Soak_GetOptions().max_packet_loss);
    NBN_GameServer_Debug_SetPacketDuplicationRatio(Soak_GetOptions().packet_duplication);
    NBN_GameServer_Debug_SetPing(Soak_GetOptions().ping);
    NBN_GameServer_Debug_SetJitter(Soak_GetOptions().jitter);
    NBN_GameServer_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, Soak_Debug_PrintAddedToRecvQueue);

    if (NBN_GameServer_Start(SOAK_PORT))
    {
        Soak_LogError("Failed to start game server");

        return 1;
    }

    int ret = Soak_MainLoop(tick);

    NBN_GameServer_Stop();

    return ret;
}
