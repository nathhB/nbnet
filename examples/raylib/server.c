/*

Copyright (C) 2020 BIAGINI Nathan

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/

#if defined(_WIN32) || defined(_WIN64)

#include <synchapi.h> /* For Sleep function */

#endif /* WINDOWS */

#include "shared.h"

/* A simple structure to represent clients */
typedef struct
{
    /*
        Keep track of the underlying nbnet connection that we can use to send messages
        to that particular client.
    */
    NBN_Connection *connection;

    /* The state of client */
    ClientState state;
} Client;

/* Array of connected clients */
static Client *clients[MAX_CLIENTS] = {NULL};

static unsigned int client_count = 0;

static int SendSpawnMessage(Client *client)
{
    /* Create a spawn message on the reliable ordered channel */
    SpawnMessage *msg = NBN_GameServer_CreateReliableMessage(SPAWN_MESSAGE);

    /* Make sure we did not fail to create the message */
    if (msg == NULL)
        return -1;

    msg->client_id = client->state.client_id;
    msg->x = client->state.x;
    msg->y = client->state.y;

    TraceLog(LOG_INFO, "Send spawn message (%d, %d) to client %d", msg->x, msg->y, client->state.client_id);

    NBN_GameServer_SendMessageTo(client->connection);

    return 0;
}

static void HandleNewConnection(void)
{
    TraceLog(LOG_INFO, "New connection");

    if (client_count == MAX_CLIENTS)
    {
        TraceLog(LOG_INFO, "Connection rejected");
        NBN_GameServer_RejectConnection(SERVER_FULL_CODE);

        return;
    }

    NBN_Connection *connection = NBN_GameServer_AcceptConnection();

    TraceLog(LOG_INFO, "Connection accepted (ID: %d)", connection->id);

    Client *client = NULL;

    /* Find a free slot in the clients array to store this new client */
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] == NULL)
        {
            client = malloc(sizeof(Client));
            clients[i] = client;

            break;
        }
    }

    assert(client != NULL);

    client->connection = connection;
    client->state = (ClientState){.client_id = connection->id, .x = 200, .y = 200, .color = CLI_RED, .val = 0};

    /* Send a SpawnMessage to that client */
    if (SendSpawnMessage(client) < 0)
    {
        TraceLog(LOG_WARNING, "Failed to send spawn message to client %d, closing client", connection->id);
        NBN_GameServer_CloseClient(connection, -1);
    }

    client_count++;
}

static void DestroyClient(uint32_t client_id)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] && clients[i]->state.client_id == client_id)
        {
            free(clients[i]);
            clients[i] = NULL;

            return;
        }
    }
}

static void HandleClientDisconnection(uint32_t client_id)
{
    TraceLog(LOG_INFO, "Client has disconnected (id: %d)", client_id);

    DestroyClient(client_id);

    client_count--;
}

static Client *FindClientById(uint32_t client_id)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] && clients[i]->state.client_id == client_id)
            return clients[i];
    }

    return NULL;
}

static void HandleUpdatePositionMessage(UpdatePositionMessage *msg, NBN_Connection *connection)
{
    Client *client = FindClientById(connection->id);

    assert(client != NULL);

    client->state.x = msg->x;
    client->state.y = msg->y;
    client->state.val = msg->val;
}

static void HandleChangeColorMessage(ChangeColorMessage *msg, NBN_Connection *connection)
{
    Client *client = FindClientById(connection->id);

    assert(client != NULL);

    client->state.color = msg->color;
}

static void HandleReceivedMessage(void)
{
    /*
        Read info about the last received message.
    */
    NBN_MessageInfo msg_info = NBN_GameServer_GetReceivedMessageInfo();

    switch (msg_info.type)
    {
    case UPDATE_POSITION_MESSAGE:
        HandleUpdatePositionMessage(msg_info.data, msg_info.sender);
        break;

    case CHANGE_COLOR_MESSAGE:
        HandleChangeColorMessage(msg_info.data, msg_info.sender);
        break;
    }
}

static void HandleGameServerEvent(int ev)
{
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

static int BroadcastGameState(void)
{
    ClientState client_states[MAX_CLIENTS];
    unsigned int client_index = 0;

    /* Loop over clients array to build an array of ClientState */
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        Client *client = clients[i];

        if (client == NULL)
            continue;

        client_states[client_index] = (ClientState){
            .client_id = client->state.client_id,
            .x = client->state.x,
            .y = client->state.y,
            .color = client->state.color};

        client_index++;
    }

    assert(client_index == client_count);

    /* Build a GameStateMessage */
    GameStateMessage *msg = NBN_GameServer_CreateUnreliableMessage(GAME_STATE_MESSAGE);

    /* Check for errors */
    if (msg == NULL)
        return -1;

    /* Fill message data */
    msg->client_count = client_index;
    memcpy(msg->client_states, client_states, sizeof(ClientState) * MAX_CLIENTS);

    /* Broadcast GameStateMessage to all connected clients */
    NBN_GameServer_BroadcastMessage();

    return 0;
}

int main(void)
{
    SetTraceLogLevel(LOG_DEBUG);

    NBN_GameServer_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, RAYLIB_EXAMPLE_PORT);

    RegisterMessages();

    if (NBN_GameServer_Start() < 0)
    {
        TraceLog(LOG_ERROR, "Game client failed to start. Exit");

        return 1;
    }

    float tick_dt = 1.f / TICK_RATE; /* Tick delta time */

    while (true)
    {
        NBN_GameServer_AddTime(tick_dt);

        int ev;

        while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
        {
            if (ev < 0)
            {
                TraceLog(LOG_WARNING, "An occured while polling network events. Exit");

                break;
            }

            HandleGameServerEvent(ev);
        }

        if (BroadcastGameState() < 0)
        {
            TraceLog(LOG_WARNING, "An occured while broadcasting game states. Exit");

            break;
        }

        if (NBN_GameServer_SendPackets() < 0)
        {
            TraceLog(LOG_WARNING, "An occured while flushing the send queue. Exit");

            break;
        }

        NBN_GameServerStats stats = NBN_GameServer_GetStats();

        TraceLog(LOG_INFO, "Upload: %f Bps | Download: %f Bps", stats.upload_bandwidth, stats.download_bandwidth);

        /* Cap the simulation rate to the target tick rate */
#if defined(_WIN32) || defined(_WIN64)
        Sleep(tick_dt * 1000);
#else /* UNIX / OSX */
        long nanos = tick_dt * 1e9;
        struct timespec t = {.tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999};

        nanosleep(&t, &t);
#endif
    }

    NBN_GameServer_Stop();

    return 0;
}
