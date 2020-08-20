#include "../../nbnet.h"
#include "shared.h"

/* A simple structure to represent client states */
typedef struct
{
    uint32_t id;
    int x;
    int y;
    Color color;
} Client;

static unsigned int client_count = 0;

static int SendSpawnMessage(NBN_Connection *connection)
{
    Client *client = connection->user_data;

    /* Create a spawn message on the reliable ordered channel */
    SpawnMessage *msg = NBN_GameServer_CreateMessage(SPAWN_MESSAGE, RELIABLE_CHANNEL, connection);

    /* Make sure we did not fail to create the message */
    if (msg == NULL)
        return -1;

    msg->client_id = client->id;
    msg->x = client->x;
    msg->y = client->y;

    TraceLog(LOG_INFO, "Send spawn message (%d, %d) to client %d", msg->x, msg->y, connection->id);

    /* Make sure we did not fail to send the message */
    if (NBN_GameServer_SendMessageTo(connection) < 0)
        return -1;

    return 0;
}

static void CloseClient(NBN_Connection *client)
{
    NBN_GameServer_CloseClient(client);
}

static void OnClientConnected(NBN_Connection *connection)
{
    TraceLog(LOG_INFO, "Client has connected (id: %d)", connection->id);

    if (client_count == MAX_CLIENTS)
    {
        // TODO
        return;
    }

    Client *client = malloc(sizeof(Client));

    client->id = connection->id;
    client->x = 200;
    client->y = 200;

    /* Attach client data to actual nbnet client connection */
    NBN_GameServer_AttachDataToClientConnection(connection, client);

    if (SendSpawnMessage(connection) < 0)
    {
        TraceLog(LOG_WARNING, "Failed to send spawn message to client %d, closing client", connection->id);
        CloseClient(connection);
    }
}

static void OnClientDisconnected(NBN_Connection *connection)
{
    TraceLog(LOG_INFO, "Client has disconnected (id: %d)", connection->id);

    free(connection->user_data); /* fetch the client data that we attached on this client connection */

    client_count--;
}

static void HandleUpdatePositionMessage(UpdatePositionMessage *msg, NBN_Connection *connection)
{
    Client *client = connection->user_data;

    client->x = msg->x;
    client->y = msg->y;
}

static void HandleReceivedMessage(void)
{
    NBN_MessageInfo msg;

    /*
        Read info about the last received message.

        The "data" field of NBN_MessageInfo structure is a pointer to the user defined message
        so in our case SpawnMessage, ChangeColorMessage or UpdatePositionMessage.
    */
    NBN_GameServer_ReadReceivedMessage(&msg);

    switch (msg.type)
    {
        case UPDATE_POSITION_MESSAGE:
            HandleUpdatePositionMessage(msg.data, msg.sender);
            break;
    }
}

static void HandleGameServerEvent(NBN_GameServerEvent ev)
{
    switch (ev)
    {
    case NBN_CLIENT_CONNECTED:
        OnClientConnected(NBN_GameServer_GetConnectedClient());
        break;

    case NBN_CLIENT_DISCONNECTED:
        OnClientDisconnected(NBN_GameServer_GetDisconnectedClient());
        break;

    case NBN_CLIENT_MESSAGE_RECEIVED:
        HandleReceivedMessage();
        break;

    default:
        break;
    }
}

static int BroadcastGameState(void)
{
    NBN_List *clients = NBN_GameServer_GetClients(); /* get a linked list of all connected clients (list of NBN_Connection) */
    ClientState client_states[MAX_CLIENTS];
    unsigned int client_index = 0;

    /* loop through the list and build an array of client states */
    NBN_ListNode *current_node = clients->head;

    while (current_node)
    {
        NBN_Connection *connection = current_node->data;
        Client *client = connection->user_data; /* get the client user data attached to the client connection */

        /* Build the state of the current client */
        client_states[client_index] = (ClientState){ .client_id = client->id, .x = client->x, .y = client->y };

        client_index++;
        current_node = current_node->next;
    }

    assert(client_index == client_index);

    /* Now loop over the list again and send a GameStateMessage to each of them */
    current_node = clients->head;

    while (current_node)
    {
        NBN_Connection *connection = current_node->data;
        GameStateMessage *msg = NBN_GameServer_CreateMessage(GAME_STATE_MESSAGE, UNRELIABLE_CHANNEL, connection);

        if (msg == NULL)
            return -1;

        msg->client_count = client_index;

        /* Copy the client state array we built before for every GameStateMessage we send */
        memcpy(msg->client_states, client_states, sizeof(ClientState) * MAX_CLIENTS);

        current_node = current_node->next;

        if (NBN_GameServer_SendMessageTo(connection) < 0)
        {
            /*TraceLog(LOG_WARNING, "Failed to send game state message to client %d, closing client", connection->id);
            NBN_GameServer_CloseClient(connection);*/
        }
    }

    return 0;
}

int main(void)
{
    SetTraceLogLevel(LOG_INFO);

    NBN_GameServer_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME);

    RegisterChannels();
    RegisterMessages();

    if (NBN_GameServer_Start(42042) < 0)
    {
        TraceLog(LOG_ERROR, "Game client failed to start. Exit");

        return 1;
    }

    float tick_dt = 1.f / TICK_RATE; /* tick delta time */

    while (true)
    {
        NBN_GameServer_AddTime(tick_dt);

        NBN_GameServerEvent ev = NBN_GameServer_Poll();

        if (ev == NBN_ERROR)
        {
            TraceLog(LOG_WARNING, "An occured while polling network events. Exit");

            break;
        }

        HandleGameServerEvent(ev);

        if (BroadcastGameState() < 0)
        {
            TraceLog(LOG_WARNING, "An occured while broadcasting game states. Exit");

            break;
        }

        if (NBN_GameServer_Flush() < 0)
        {
            TraceLog(LOG_WARNING, "An occured while flushing the send queue. Exit");

            break;
        }

        /* We do not want our server simulation to run too fast so sleep for a duration of "tick dt" */
        TickSleep(tick_dt);
    }

    NBN_GameServer_Stop();

    return 0;
}