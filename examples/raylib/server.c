/*

   Copyright (C) 2024 BIAGINI Nathan

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

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// For Sleep function
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <synchapi.h>
#include <windows.h>
#include <winsock2.h>
#else
#include <time.h>
#endif

#include "shared.h"

// A simple structure to represent connected clients
typedef struct {
    // Underlying nbnet connection, used to send messages to that particular client
    NBN_ConnectionHandle *conn;

    // Client state
    ClientState state;
} Client;

// Array of connected clients, NULL means that the slot is free (i.e no clients)
static Client *clients[MAX_CLIENTS] = {NULL};

// Number of currently connected clients
static unsigned int client_count = 0;

// Spawn positions
static Vector2 spawns[] = {(Vector2){50, 50}, (Vector2){GAME_WIDTH - 100, 50}, (Vector2){50, GAME_HEIGHT - 100},
                           (Vector2){GAME_WIDTH - 100, GAME_HEIGHT - 100}};

static void AcceptConnection(Vector2 spawn, NBN_ConnectionHandle *conn) {
    // Accept the connection with some data
    // this data can be read by the client upon processing the connection event
    NBN_Writer *writer = NBN_GameServer_WriteConnectionData();

    NBN_Writer_WriteUInt32(writer, (uint32_t)spawn.x);
    NBN_Writer_WriteUInt32(writer, (uint32_t)spawn.y);
    NBN_Writer_WriteUInt32(writer, conn->id); // TODO: id is 64bits

    NBN_GameServer_AcceptIncomingConnection();
}

static int HandleNewConnection(void) {
    TraceLog(LOG_INFO, "New connection");

    // If the server is full
    if (client_count == MAX_CLIENTS) {
        // Reject the connection (send a SERVER_FULL_CODE code to the client)
        TraceLog(LOG_INFO, "Connection rejected");
        NBN_GameServer_RejectIncomingConnectionWithCode(SERVER_FULL_CODE);

        return 0;
    }

    // Otherwise...

    NBN_ConnectionHandle *conn = NBN_GameServer_GetIncomingConnection();

    // Read the connection request data transmitted by the client
    NBN_Reader *reader = NBN_GameServer_ReadConnectionRequestData();
    char name[CLIENT_NAME_MAX_LEN];

    if (NBN_Reader_ReadString(reader, name, sizeof(name)) < 0) {
        TraceLog(LOG_ERROR, "Failed to read client name");
        abort();
    }

    // Get a spawning position for the client
    Vector2 spawn = spawns[conn->id % MAX_CLIENTS];

    // Build some "initial" data that will be sent to the connected client

    AcceptConnection(spawn, conn);

    TraceLog(LOG_INFO, "Connection accepted (ID: %d, name: %s)", conn->id, name);

    Client *client = NULL;

    // Find a free slot in the clients array and create a new client
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            client = malloc(sizeof(Client));
            clients[i] = client;

            break;
        }
    }

    assert(client != NULL);

    client->conn = conn;
    conn->user_data = client;

    // Fill the client state with initial spawning data
    client->state = (ClientState){.client_id = conn->id, .x = 200, .y = 400, .color = CLI_RED, .val = 0};
    memcpy(client->state.name, name, sizeof(client->state.name));

    client_count++;

    return 0;
}

static void DestroyClient(Client *client) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->state.client_id == client->state.client_id) {
            clients[i] = NULL;

            return;
        }
    }

    free(client);
}

static void HandleClientDisconnection(void) {
    NBN_DisconnectionInfo info = NBN_GameServer_GetDisconnectionInfo();

    TraceLog(LOG_INFO, "Client has disconnected (id: %d, user data: %p)", info.conn_id, info.user_data);

    Client *client = info.user_data;

    assert(client);
    DestroyClient(client);

    client_count--;
}

static int HandleUpdateStateMessage(Client *sender) {
    // Update the state of the client with the data from the received UPDATE_STATE_MESSAGE message
    NBN_Reader *reader = NBN_GameServer_ReadMessage();

    return UpdateClientStateMessage_Read(reader, &sender->state);
}

static int HandleChangeColorMessage(Client *sender) {
    // Update the client color
    NBN_Reader *reader = NBN_GameServer_ReadMessage();

    return ChangeColorMessage_Read(reader, &sender->state.color);
}

static int HandleReceivedMessage(void) {
    // Fetch info about the last received message
    NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();
    assert(msg_info.sender != NULL);
    Client *sender = msg_info.sender->user_data;
    assert(sender != NULL);

    switch (msg_info.type) {
    case UPDATE_STATE_MESSAGE:
        // The server received a client state update
        return HandleUpdateStateMessage(sender);

    case CHANGE_COLOR_MESSAGE:
        // The server received a client switch color action
        return HandleChangeColorMessage(sender);
    }

    // Received an unexpected message
    return -1;
}

static int HandleGameServerEvent(int ev) {
    switch (ev) {
    case NBN_SERVER_NEW_CONNECTION:
        // A new client has requested a connection
        if (HandleNewConnection() < 0)
            return -1;
        break;

    case NBN_SERVER_DISCONNECTION:
        // A previously connected client has disconnected
        HandleClientDisconnection();
        break;

    case NBN_SERVER_MESSAGE_RECEIVED:
        // A message from a client has been received
        if (HandleReceivedMessage() < 0) {
            // TODO: kick client
            return -1;
        }
        break;
    }

    return 0;
}

// Broadcasts the latest game state to all connected clients
static int BroadcastGameState(void) {
    static GameState game_state;
    unsigned int client_index = 0;

    // Loop over the clients and build the game state
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Client *client = clients[i];

        if (client == NULL)
            continue;

        ClientState state = (ClientState){.client_id = client->state.client_id,
                                          .x = client->state.x,
                                          .y = client->state.y,
                                          .val = client->state.val,
                                          .color = client->state.color};

        memcpy(state.name, client->state.name, sizeof(client->state.name));
        game_state.client_states[client_index] = state;
        client_index++;
    }

    assert(client_index == client_count);

    game_state.client_count = client_count;

    // Create a unreliable message GAME_STATE_MESSAGE and write to it
    NBN_Writer *writer = NBN_GameServer_CreateUnreliableMessage(GAME_STATE_MESSAGE);
    GameStateMessage_Write(writer, &game_state);

    return NBN_GameServer_EnqueueBroadcastMessage();
}

static bool running = true;

#ifndef __EMSCRIPTEN__
#include <signal.h>

static void SigintHandler(int dummy) { running = false; }

#endif

int main(int argc, char *argv[]) {
#ifndef __EMSCRIPTEN__
    signal(SIGINT, SigintHandler);
#endif

    // Read command line arguments
    if (ReadCommandLine(argc, argv)) {
        printf("Usage: server [--packet_loss=<value>] [--packet_duplication=<value>] [--ping=<value>] \
                [--jitter=<value>]\n");

        return 1;
    }

    // Even though we do not display anything we still use raylib logging capacibilities
    SetTraceLogLevel(LOG_TRACE);

    // Initialize the server with a protocol name and a port
    // protocol name has to match between the server and the clients
    NBN_GameServer_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, RAYLIB_EXAMPLE_PORT);

    // Start the server with the configuration
    if (NBN_GameServer_Start() < 0) {
        TraceLog(LOG_ERROR, "Game server failed to start. Exit");

        return 1;
    }

    // Network conditions simulated variables (read from the command line, default is always 0)
    NBN_GameServer_SetPing(GetOptions().ping);
    NBN_GameServer_SetJitter(GetOptions().jitter);
    NBN_GameServer_SetPacketLoss(GetOptions().packet_loss);
    NBN_GameServer_SetPacketDuplication(GetOptions().packet_duplication);

    float tick_dt = 1.f / TICK_RATE; // Tick delta time

    while (running) {
        int ev;

        // Poll for server events
        while ((ev = NBN_GameServer_Poll()) != NBN_SERVER_NO_EVENT) {
            if (ev < 0) {
                TraceLog(LOG_ERROR, "An occured while polling network events. Exit");

                break;
            }

            if (HandleGameServerEvent(ev) < 0)
                break;
        }

        if (BroadcastGameState() < 0) {
            TraceLog(LOG_ERROR, "An occured while broadcasting game states. Exit");

            break;
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameServer_Flush() < 0) {
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
