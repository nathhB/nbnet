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

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)

#include <synchapi.h> // For Sleep function

#endif // Windows

#include "shared.h"

// A simple structure to represent connected clients
typedef struct
{
    // Underlying nbnet connection of that client, used to send messages to that particular client
    NBN_Connection *connection;

    // Client state
    ClientState state;
} Client;

// Array of connected clients, NULL means that the slot is free (i.e no clients)
static Client *clients[MAX_CLIENTS] = {NULL};

// Number of currently connected clients
static unsigned int client_count = 0;

static int SendSpawnMessage(Client *client)
{
    // Create a reliable SpawnMessage message
    SpawnMessage *msg = NBN_GameServer_CreateReliableMessage(SPAWN_MESSAGE);

    if (msg == NULL)
        return -1;

    // Fill message data
    msg->client_id = client->state.client_id;
    msg->x = client->state.x;
    msg->y = client->state.y;

    TraceLog(LOG_INFO, "Send spawn message (%d, %d) to client %d", msg->x, msg->y, client->state.client_id);

    // Send the message to the client
    NBN_GameServer_SendMessageTo(client->connection);

    return 0;
}

static void HandleNewConnection(void)
{
    TraceLog(LOG_INFO, "New connection");

    // If the server is full
    if (client_count == MAX_CLIENTS)
    {
        // Reject the connection (send a SERVER_FULL_CODE code to the client)
        TraceLog(LOG_INFO, "Connection rejected");
        NBN_GameServer_RejectConnectionWithCode(SERVER_FULL_CODE);

        return;
    }

    // Otherwise, accept the client and store the client
    NBN_Connection *connection = NBN_GameServer_AcceptConnection();

    TraceLog(LOG_INFO, "Connection accepted (ID: %d)", connection->id);

    Client *client = NULL;

    // Find a free slot in the clients array and create a new client
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

    client->connection = connection; // Store the nbnet connection

    // Fille the client state with initial spawning data
    client->state = (ClientState){.client_id = connection->id, .x = 200, .y = 200, .color = CLI_RED, .val = 0};

    // Send a SpawnMessage to that client
    if (SendSpawnMessage(client) < 0)
    {
        TraceLog(LOG_WARNING, "Failed to send spawn message to client %d, closing client", connection->id);
        NBN_GameServer_CloseClient(connection);
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

static void HandleClientDisconnection()
{
    uint32_t client_id = NBN_GameServer_GetDisconnectedClientId(); // Get the id of the disconnected client

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

static void HandleUpdateStateMessage(UpdateStateMessage *msg, Client *sender)
{
    // Update the state of the client with the data from the received UpdateStateMessage message
    sender->state.x = msg->x;
    sender->state.y = msg->y;
    sender->state.val = msg->val;
}

static void HandleChangeColorMessage(ChangeColorMessage *msg, Client *sender)
{
    // Update the client color
    sender->state.color = msg->color;
}

static void HandleReceivedMessage(void)
{
    // Fetch info about the last received message
    NBN_MessageInfo msg_info = NBN_GameServer_GetReceivedMessageInfo();

    // Find the client that sent the message
    Client *sender = FindClientById(msg_info.sender->id);

    assert(sender != NULL);

    switch (msg_info.type)
    {
        case UPDATE_STATE_MESSAGE:
            // The server received a client state update
            HandleUpdateStateMessage(msg_info.data, sender);
            break;

        case CHANGE_COLOR_MESSAGE:
            // The server received a client switch color action
            HandleChangeColorMessage(msg_info.data, sender);
            break;
    }
}

static void HandleGameServerEvent(int ev)
{
    switch (ev)
    {
        case NBN_NEW_CONNECTION:
            // A new client has requested a connection
            HandleNewConnection();
            break;

        case NBN_CLIENT_DISCONNECTED:
            // A previsouly connected client has disconnected
            HandleClientDisconnection();
            break;

        case NBN_CLIENT_MESSAGE_RECEIVED:
            // A message from a client has been received
            HandleReceivedMessage();
            break;
    }
}

// Broadcasts the latest game state to all connected clients
static int BroadcastGameState(void)
{
    ClientState client_states[MAX_CLIENTS];
    unsigned int client_index = 0;

    // Loop over the clients array and build an array of ClientState
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        Client *client = clients[i];

        if (client == NULL)
            continue;

        client_states[client_index] = (ClientState){
            .client_id = client->state.client_id,
                .x = client->state.x,
                .y = client->state.y,
                .val = client->state.val,
                .color = client->state.color};

        client_index++;
    }

    assert(client_index == client_count);

    // Create a GameStateMessage unreliable message
    GameStateMessage *msg = NBN_GameServer_CreateUnreliableMessage(GAME_STATE_MESSAGE);

    if (msg == NULL)
        return -1;

    // Fill message data
    msg->client_count = client_index;
    memcpy(msg->client_states, client_states, sizeof(ClientState) * MAX_CLIENTS);

    // Broadcast the message
    NBN_GameServer_BroadcastMessage();

    return 0;
}

int main(int argc, char *argv[])
{
    // Read command line arguments
    if (ReadCommandLine(argc, argv))
    {
        printf("Usage: server [--packet_loss=<value>] [--packet_duplication=<value>] [--ping=<value>] \
                [--jitter=<value>]\n");

        return 1;
    }

    // Even though we do not display anything we still use raylib logging capacibilities
    SetTraceLogLevel(LOG_DEBUG);

    // Init server with a protocol name and a port, must be done first
    NBN_GameServer_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, RAYLIB_EXAMPLE_PORT);

    // Register messages, have to be done after NBN_GameServer_Init and before NBN_GameServer_Start
    RegisterMessages();

    // Network conditions simulated variables (read from the command line, default is always 0)
    NBN_Debug_SetPing(GetOptions().ping);
    NBN_Debug_SetJitter(GetOptions().jitter);
    NBN_Debug_SetPacketLoss(GetOptions().packet_loss);
    NBN_Debug_SetPacketDuplication(GetOptions().packet_duplication);

    // Start the server
    if (NBN_GameServer_Start() < 0)
    {
        TraceLog(LOG_ERROR, "Game client failed to start. Exit");

        return 1;
    }

    float tick_dt = 1.f / TICK_RATE; // Tick delta time

    while (true)
    {
        // Update the server clock
        NBN_GameServer_AddTime(tick_dt);

        int ev;

        // Poll for server events
        while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
        {
            if (ev < 0)
            {
                TraceLog(LOG_WARNING, "An occured while polling network events. Exit");

                break;
            }

            HandleGameServerEvent(ev);
        }

        // Broadcast latest game state
        if (BroadcastGameState() < 0)
        {
            TraceLog(LOG_WARNING, "An occured while broadcasting game states. Exit");

            break;
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameServer_SendPackets() < 0)
        {
            TraceLog(LOG_WARNING, "An occured while flushing the send queue. Exit");

            break;
        }

        NBN_GameServerStats stats = NBN_GameServer_GetStats();

        TraceLog(LOG_INFO, "Upload: %f Bps | Download: %f Bps", stats.upload_bandwidth, stats.download_bandwidth);

        // Cap the simulation rate to TICK_RATE ticks per second (just like for the client)
#if defined(_WIN32) || defined(_WIN64)
        Sleep(tick_dt * 1000);
#else
        long nanos = tick_dt * 1e9;
        struct timespec t = {.tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999};

        nanosleep(&t, &t);
#endif
    }

    // Stop the server
    NBN_GameServer_Stop();

    return 0;
}
