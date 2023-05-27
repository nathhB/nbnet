/*

   Copyright (C) 2023 BIAGINI Nathan

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

// For Sleep function
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h> 
#elif defined(_WIN32) || defined(_WIN64)
#include <synchapi.h> 
#else
#include <time.h>
#endif

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

// Spawn positions
static Vector2 spawns[] = {
    (Vector2){50, 50},
    (Vector2){GAME_WIDTH - 100, 50},
    (Vector2){50, GAME_HEIGHT - 100},
    (Vector2){GAME_WIDTH - 100, GAME_HEIGHT - 100}
};

static int HandleNewConnection(void)
{
    TraceLog(LOG_INFO, "New connection");

    // If the server is full
    if (client_count == MAX_CLIENTS)
    {
        // Reject the connection (send a SERVER_FULL_CODE code to the client)
        TraceLog(LOG_INFO, "Connection rejected");
        NBN_GameServer_RejectIncomingConnectionWithCode(SERVER_FULL_CODE);

        return 0;
    }

    // Otherwise...

    NBN_Connection *connection = NBN_GameServer_GetIncomingConnection(); 

    // Get a spawning position for the client
    Vector2 spawn = spawns[connection->id % MAX_CLIENTS];

    // Build some "initial" data that will be sent to the connected client

    NBN_Stream *ws = NBN_GameServer_GetConnectionAcceptDataWriteStream(connection);

    unsigned int x = (unsigned int)spawn.x;
    unsigned int y = (unsigned int)spawn.y;

    NBN_SerializeUInt(ws, x, 0, GAME_WIDTH);
    NBN_SerializeUInt(ws, y, 0, GAME_HEIGHT);
    NBN_SerializeUInt(ws, connection->id, 0, UINT_MAX);
    
    // Accept the connection
    NBN_GameServer_AcceptIncomingConnection();

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

    // Fill the client state with initial spawning data
    client->state = (ClientState){.client_id = connection->id, .x = 200, .y = 400, .color = CLI_RED, .val = 0};

    client_count++;

    return 0;
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

static void DestroyClient(Client *client)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] && clients[i]->state.client_id == client->state.client_id)
        {
            clients[i] = NULL;

            return;
        }
    }

    free(client);
}

static void HandleClientDisconnection()
{
    NBN_Connection *cli_conn = NBN_GameServer_GetDisconnectedClient(); // Get the disconnected client

    TraceLog(LOG_INFO, "Client has disconnected (id: %d)", cli_conn->id);

    Client *client = FindClientById(cli_conn->id);

    assert(client);

    DestroyClient(client);
    NBN_Connection_Destroy(cli_conn);

    client_count--;
}

static void HandleUpdateStateMessage(UpdateStateMessage *msg, Client *sender)
{
    // Update the state of the client with the data from the received UpdateStateMessage message
    sender->state.x = msg->x;
    sender->state.y = msg->y;
    sender->state.val = msg->val;

    UpdateStateMessage_Destroy(msg);
}

static void HandleChangeColorMessage(ChangeColorMessage *msg, Client *sender)
{
    // Update the client color
    sender->state.color = msg->color;

    ChangeColorMessage_Destroy(msg);
}

static void HandleReceivedMessage(void)
{
    // Fetch info about the last received message
    NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

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

static int HandleGameServerEvent(int ev)
{
    switch (ev)
    {
        case NBN_NEW_CONNECTION:
            // A new client has requested a connection
            if (HandleNewConnection() < 0)
                return -1;
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

    return 0;
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

        GameStateMessage *msg = GameStateMessage_Create();

        // Fill message data
        msg->client_count = client_index;
        memcpy(msg->client_states, client_states, sizeof(ClientState) * MAX_CLIENTS);

        // Unreliably send the message to all connected clients
        NBN_GameServer_SendUnreliableMessageTo(client->connection, GAME_STATE_MESSAGE, msg);

        client_index++;
    }

    assert(client_index == client_count);

    return 0;
}

static bool running = true;

#ifndef __EMSCRIPTEN__

static void SigintHandler(int dummy)
{
    running = false;
}

#endif

int main(int argc, char *argv[])
{
#ifndef __EMSCRIPTEN__
    signal(SIGINT, SigintHandler);
#endif

    // Read command line arguments
    if (ReadCommandLine(argc, argv))
    {
        printf("Usage: server [--packet_loss=<value>] [--packet_duplication=<value>] [--ping=<value>] \
                [--jitter=<value>]\n");

        return 1;
    }

    // Even though we do not display anything we still use raylib logging capacibilities
    SetTraceLogLevel(LOG_DEBUG);

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register(); // Register the WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver

#ifdef SOAK_WEBRTC_C_DRIVER
    NBN_WebRTC_C_Register(); // Register the native WebRTC driver
#endif

#endif // __EMSCRIPTEN__

    // Initialize server with a protocol name and a port, must be done first
#ifdef EXAMPLE_ENCRYPTION
    NBN_GameServer_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, RAYLIB_EXAMPLE_PORT, true);
#else
    NBN_GameServer_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, RAYLIB_EXAMPLE_PORT, false);
#endif

    if (NBN_GameServer_Start() < 0)
    {
        TraceLog(LOG_ERROR, "Game server failed to start. Exit");

        return 1;
    }

    // Register messages, have to be done after NBN_GameServer_Init and before NBN_GameServer_Start
    NBN_GameServer_RegisterMessage(
            CHANGE_COLOR_MESSAGE,
            (NBN_MessageBuilder)ChangeColorMessage_Create,
            (NBN_MessageDestructor)ChangeColorMessage_Destroy,
            (NBN_MessageSerializer)ChangeColorMessage_Serialize);
    NBN_GameServer_RegisterMessage(
            UPDATE_STATE_MESSAGE,
            (NBN_MessageBuilder)UpdateStateMessage_Create,
            (NBN_MessageDestructor)UpdateStateMessage_Destroy,
            (NBN_MessageSerializer)UpdateStateMessage_Serialize);
    NBN_GameServer_RegisterMessage(
            GAME_STATE_MESSAGE,
            (NBN_MessageBuilder)GameStateMessage_Create,
            (NBN_MessageDestructor)GameStateMessage_Destroy,
            (NBN_MessageSerializer)GameStateMessage_Serialize);

    // Network conditions simulated variables (read from the command line, default is always 0)
    NBN_GameServer_SetPing(GetOptions().ping);
    NBN_GameServer_SetJitter(GetOptions().jitter);
    NBN_GameServer_SetPacketLoss(GetOptions().packet_loss);
    NBN_GameServer_SetPacketDuplication(GetOptions().packet_duplication); 

    float tick_dt = 1.f / TICK_RATE; // Tick delta time

    while (running)
    {
        // Update the server clock
        NBN_GameServer_AddTime(tick_dt);

        int ev;

        // Poll for server events
        while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
        {
            if (ev < 0)
            {
                TraceLog(LOG_ERROR, "An occured while polling network events. Exit");

                break;
            }

            if (HandleGameServerEvent(ev) < 0)
                break;
        }

        // Broadcast latest game state
        if (BroadcastGameState() < 0)
        {
            TraceLog(LOG_ERROR, "An occured while broadcasting game states. Exit");

            break;
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameServer_SendPackets() < 0)
        {
            TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");

            break;
        }

        NBN_GameServerStats stats = NBN_GameServer_GetStats();

        TraceLog(LOG_TRACE, "Upload: %f Bps | Download: %f Bps", stats.upload_bandwidth, stats.download_bandwidth);

        // Cap the simulation rate to TICK_RATE ticks per second (just like for the client)
#if defined(__EMSCRIPTEN__)
        emscripten_sleep(tick_dt * 1000);
#elif defined(_WIN32) || defined(_WIN64)
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
