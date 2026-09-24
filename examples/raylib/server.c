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

static void AcceptConnection(NBN_Server *server, Vector2 spawn, NBN_ConnectionHandle *conn) {
    // Accept the connection with some data
    // this data can be read by the client upon processing the connection event
    NBN_Writer *writer = NBN_Server_WriteConnectionData(server);

    NBN_Writer_WriteUInt32(writer, (uint32_t)spawn.x);
    NBN_Writer_WriteUInt32(writer, (uint32_t)spawn.y);
    NBN_Writer_WriteUInt32(writer, conn->id); // TODO: id is 64bits

    NBN_Server_AcceptIncomingConnection(server);
}

static void HandleNewConnection(NBN_Server *server) {
    TraceLog(LOG_INFO, "New connection");

    // If the server is full
    if (client_count == MAX_CLIENTS) {
        // Reject the connection (send a SERVER_FULL_CODE code to the client)
        TraceLog(LOG_INFO, "Connection rejected");
        NBN_Server_RejectIncomingConnectionWithCode(server, SERVER_FULL_CODE);

        return;
    }

    // Otherwise...

    NBN_ConnectionHandle *conn = NBN_Server_GetIncomingConnection(server);

    // Read the connection request data transmitted by the client
    NBN_Reader reader = NBN_Server_ReadConnectionRequestData(server);
    char name[CLIENT_NAME_MAX_LEN];

    if (NBN_Reader_ReadString(&reader, name, sizeof(name)) < 0) {
        TraceLog(LOG_ERROR, "Failed to read client name");
        abort();
    }

    // Get a spawning position for the client
    Vector2 spawn = spawns[conn->id % MAX_CLIENTS];

    // Build some "initial" data that will be sent to the connected client

    AcceptConnection(server, spawn, conn);

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

static void HandleClientDisconnection(NBN_Server *server) {
    NBN_DisconnectionInfo info = NBN_Server_GetDisconnectionInfo(server);

    TraceLog(LOG_INFO, "Client has disconnected (id: %d, user data: %p)", info.conn_id, info.user_data);

    Client *client = info.user_data;

    assert(client);
    DestroyClient(client);

    client_count--;
}

static int HandleUpdateStateMessage(NBN_Message *msg, Client *sender) {
    // Update the state of the client with the data from the received UPDATE_STATE_MESSAGE message
    NBN_Reader reader = NBN_ReadMessage(msg);

    return UpdateClientStateMessage_Read(&reader, &sender->state);
}

static int HandleChangeColorMessage(NBN_Message *msg, Client *sender) {
    // Update the client color
    NBN_Reader reader = NBN_ReadMessage(msg);

    return ChangeColorMessage_Read(&reader, &sender->state.color);
}

static int HandleReceivedMessage(NBN_Server *server) {
    NBN_Message *msg = NBN_Server_GetMessage(server);
    assert(msg->connection != NULL);
    Client *sender = msg->connection->user_data;
    assert(sender != NULL);

    int ret = -1;

    switch (msg->header.type) {
        case UPDATE_STATE_MESSAGE:
            // The server received a client state update
            ret = HandleUpdateStateMessage(msg, sender);
            break;

        case CHANGE_COLOR_MESSAGE:
            // The server received a client switch color action
            ret = HandleChangeColorMessage(msg, sender);
            break;

        default:
            TraceLog(LOG_ERROR, "Received an unexpected message: %d", msg->header.type);
            ret = -1;
    }

    // notify nbnet that we are done processing this incoming message
    NBN_Server_ReleaseMessage(server, msg);

    return ret;
}

// Broadcasts the latest game state to all connected clients
static int BroadcastGameState(NBN_Server *server) {
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

    NBN_Client_Iterator it = 0;
    NBN_ConnectionHandle *cli;

    // Broadcast GAME_STATE_MESSAGE to all clients
    while ((cli = NBN_Server_GetNextClient(server, &it)) != NULL) {
        uint8_t *buffer = (uint8_t *)malloc(MESSAGE_BUFFER_SIZE);
        NBN_Writer writer = NBN_Writer_Create(buffer, MESSAGE_BUFFER_SIZE);
        GameStateMessage_Write(&writer, &game_state);
        int ret = NBN_Server_CreateUnreliableMessage(server, GAME_STATE_MESSAGE, buffer, writer.position, cli);

        if (ret < 0) {
            return -1;
        }
    }

    return 0;
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
                [--jitter=<value>] [--throttle=<value>] [--throttle_min_time=<value>] [--throttle_max_time=<value>]\n");

        return 1;
    }

    // Even though we do not display anything we still use raylib logging capacibilities
    SetTraceLogLevel(LOG_TRACE);

    // Initialize the server with a protocol name and a port
    // protocol name has to match between the server and the clients
    NBN_Server *server = NBN_Server_Create(RAYLIB_EXAMPLE_PROTOCOL_NAME, RAYLIB_EXAMPLE_PORT);

    // Start the server with the configuration
    if (NBN_Server_Start(server) < 0) {
        TraceLog(LOG_ERROR, "Game server failed to start. Exit");

        return 1;
    }

    // Network conditions simulated variables (read from the command line, default is always 0)
    Options options = GetOptions();

    NBN_Server_SetPing(server, options.ping);
    NBN_Server_SetJitter(server, options.jitter);
    NBN_Server_SetPacketLoss(server, options.packet_loss);
    NBN_Server_SetPacketDuplication(server, options.packet_duplication);
    NBN_Server_SetThrottle(server, options.throttle, options.throttle_min_time, options.throttle_max_time);

    float tick_dt = 1.f / TICK_RATE; // Tick delta time

    while (running) {
        int ev;

        // Poll for server events
        while ((ev = NBN_Server_Poll(server)) != EV_NONE) {
            if (ev < 0) {
                TraceLog(LOG_ERROR, "An occured while polling network events. Exit");

                break;
            }

            switch (ev) {
                case EV_CONNECTED:
                    // A new client has requested a connection
                    HandleNewConnection(server);
                    break;

                case EV_DISCONNECTED:
                    // A client has disconnected
                    HandleClientDisconnection(server);
                    break;

                case EV_MESSAGE_RECEIVED:
                    // A message from a client has been received
                    if (HandleReceivedMessage(server) < 0) {
                        // TODO: kick client
                        break;
                    }
                    break;

                case EV_OUTGOING_MESSAGE_PROCESSED: {
                    NBN_Message *msg = NBN_Server_GetMessage(server);
                    free(msg->data);
                    break;
                }
            }
        }

        if (BroadcastGameState(server) < 0) {
            TraceLog(LOG_ERROR, "An occured while broadcasting game states. Exit");

            break;
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_Server_Flush(server) < 0) {
            TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");

            break;
        }

        NBN_ServerStats stats = NBN_Server_GetStats(server);

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
    NBN_Server_Stop(server);

    return 0;
}
