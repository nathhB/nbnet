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

#include "shared.h"

static bool connected = false;
static bool disconnected = false;
static bool spawned = false;
static int client_closed_code = 0;
static ClientState local_client_state;

/* Array to hold other client states (`MAX_CLIENTS - 1` because we don't need to store the state of the local client) */
static ClientState *clients[MAX_CLIENTS - 1] = {NULL};

/*
    Array of client ids that were updated in the last received GameStateMessage.
    This is used to detect and destroy disconnected remote clients.
*/
static int updated_ids[MAX_CLIENTS];

/* Number of currently connected clients*/
static unsigned int client_count = 0;

/* Conversion table between client color values and raylib colors */
Color client_colors_to_raylib_colors[] = {
    RED,        // CLI_RED
    LIME,       // CLI_GREEN
    BLUE,       // CLI_BLUE
    YELLOW,     // CLI_YELLOW
    ORANGE,     // CLI_ORANGE
    PURPLE,     // CLI_PURPLE
    PINK        // CLI_PINK
};

static void HandleDisconnection(int code)
{
    TraceLog(LOG_INFO, "Disconnected from server (code: %d)", code);

    disconnected = true;
    client_closed_code = code;
}

static void HandleSpawnMessage(SpawnMessage *msg)
{
    TraceLog(LOG_INFO, "Received spawn message, position: (%d, %d), client id: %d", msg->x, msg->y, msg->client_id);

    local_client_state.client_id = msg->client_id;
    local_client_state.x = msg->x;
    local_client_state.y = msg->y;
    spawned = true;
}

static bool ClientExists(uint32_t client_id)
{
    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        if (clients[i] && clients[i]->client_id == client_id)
            return true;
    }

    return false;
}

static void CreateClient(ClientState state)
{
    assert(client_count < MAX_CLIENTS - 1);

    ClientState *client = NULL;

    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        if (clients[i] == NULL)
        {
            client = malloc(sizeof(ClientState));
            clients[i] = client;

            break;
        }
    }

    assert(client != NULL);

    client->client_id = state.client_id;
    client->color = state.color;
    client->x = state.x;
    client->y = state.y;

    client_count++;

    NBN_LogInfo("New remote client (ID: %d)", client->client_id);
}

static void UpdateClient(ClientState state)
{
    ClientState *client = NULL;

    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        if (clients[i] && clients[i]->client_id == state.client_id)
        {
            client = clients[i];

            break;
        }
    }

    assert(client != NULL);

    client->client_id = state.client_id;
    client->color = state.color;
    client->x = state.x;
    client->y = state.y;
}

static void DestroyClient(uint32_t client_id)
{
    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        ClientState *client = clients[i];

        if (client && client->client_id == client_id)
        {
            NBN_LogInfo("Destroy disconnected client (ID: %d)", client->client_id);

            free(client);
            clients[i] = NULL;
            client_count--;

            return;
        }
    }
}

static void DestroyDisconnectedClients(void)
{
    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        if (clients[i] == NULL)
            continue;

        uint32_t client_id = clients[i]->client_id;
        bool is_disconnected = true;

        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if ((int)client_id == updated_ids[j])
            {
                is_disconnected = false;

                break;
            }
        }

        if (is_disconnected)
            DestroyClient(client_id);
    }
}

static void HandleGameStateMessage(GameStateMessage *msg)
{
    if (!spawned)
        return;

    /* Reset the updated client ids array before */
    for (int i = 0; i < MAX_CLIENTS; i++)
        updated_ids[i] = -1;

    /* Loop over the received client states */
    for (unsigned int i = 0; i < msg->client_count; i++)
    {
        ClientState state = msg->client_states[i];

        /* Ignore the state of the local client */
        if (state.client_id != local_client_state.client_id)
        {
            /*
                If the client already exists we update it with the latest received state.
                If the client does not exist, we create it.
            */
            if (ClientExists(state.client_id))
                UpdateClient(state);
            else
                CreateClient(state);

            updated_ids[i] = state.client_id;
        }
    }

    DestroyDisconnectedClients();
}

static void HandleReceivedMessage(void)
{
    /*
        Read info about the last received message.
    */
    NBN_MessageInfo msg_info = NBN_GameClient_GetReceivedMessageInfo();

    switch (msg_info.type)
    {
    case SPAWN_MESSAGE:
        HandleSpawnMessage(msg_info.data);
        break;

    case GAME_STATE_MESSAGE:
        HandleGameStateMessage(msg_info.data);
        break;
    }
}

static int HandleGameClientEvent(NBN_GameClientEvent ev)
{
    switch (ev)
    {
    case NBN_CONNECTED:
        /* We are connected to the server */
        connected = true;
        break;

    case NBN_DISCONNECTED:
        /* We have been disconnected from the server */
        HandleDisconnection(NBN_GameClient_GetClientClosedCode());
        break;

    case NBN_MESSAGE_RECEIVED:
        /* We received a message from the server */
        HandleReceivedMessage();
        break;

    case NBN_ERROR:
        return -1;
    }

    return 0;
}

static int SendPositionUpdate(void)
{
    /* Create an UpdatePositionMessage */
    UpdatePositionMessage *msg = NBN_GameClient_CreateMessage(UPDATE_POSITION_MESSAGE);

    /* Check for errors */
    if (msg == NULL)
        return -1;

    /* Fill message data */
    msg->x = local_client_state.x;
    msg->y = local_client_state.y;

    /* Send the message on the unreliable channel */
    if (NBN_GameClient_EnqueueUnreliableMessage() < 0)
        return -1;

    return 0;
}

static int SendColorUpdate(void)
{
    /* Create an ChangeColorMessage */
    ChangeColorMessage *msg = NBN_GameClient_CreateMessage(CHANGE_COLOR_MESSAGE);

    /* Check for errors */
    if (msg == NULL)
        return -1;

    /* Fill message data */
    msg->color = local_client_state.color;

    /* Send the message on the reliable channel */
    if (NBN_GameClient_EnqueueReliableMessage() < 0)
        return -1;

    return 0;
}

static int Update(void)
{
    if (!spawned)
        return 0;

    /* Very basic movement code */
    if (IsKeyDown(KEY_UP))
        local_client_state.y = MAX(0, local_client_state.y - 5);
    else if (IsKeyDown(KEY_DOWN))
        local_client_state.y = MIN(GAME_HEIGHT - 50, local_client_state.y + 5);

    if (IsKeyDown(KEY_LEFT))
        local_client_state.x = MAX(0, local_client_state.x - 5);
    else if (IsKeyDown(KEY_RIGHT))
        local_client_state.x = MIN(GAME_WIDTH - 50, local_client_state.x + 5);

    /* Color switching */
    if (IsKeyPressed(KEY_SPACE))
    {
        local_client_state.color = (local_client_state.color + 1) % MAX_COLORS;

        TraceLog(LOG_INFO, "Switched color, new color: %d", local_client_state.color);

        if (SendColorUpdate() < 0)
        {
            TraceLog(LOG_WARNING, "Failed to send color update");

            return -1;
        }
    }

    /* Send the latest client position to the server */
    if (SendPositionUpdate() < 0)
    {
        TraceLog(LOG_WARNING, "Failed to send position update");

        return -1;
    }

    return 0;
}

void DrawClient(ClientState *state, bool is_local)
{
    DrawRectangle(state->x, state->y, 50, 50, client_colors_to_raylib_colors[state->color]);

    if (is_local)
        DrawRectangleLinesEx((Rectangle){state->x, state->y, 50, 50}, 3, DARKBROWN);
}

void DrawHUD(void)
{
    NBN_ConnectionStats stats = NBN_GameClient_GetStats();
    unsigned int ping = stats.ping * 1000;
    unsigned int packet_loss = stats.packet_loss * 100;

    DrawText(FormatText("FPS: %d", GetFPS()), 450, 350, 32, MAROON);
    DrawText(FormatText("Ping: %d ms", ping), 450, 400, 32, MAROON);
    DrawText(FormatText("Packet loss: %d %%", packet_loss), 450, 450, 32, MAROON);
    DrawText(FormatText("Upload: %.1f Bps", stats.upload_bandwidth), 450, 500, 32, MAROON);
    DrawText(FormatText("Download: %.1f Bps", stats.download_bandwidth), 450, 550, 32, MAROON);
}

void Draw(void)
{
    BeginDrawing();
    ClearBackground(LIGHTGRAY);

    if (disconnected)
    {
        if (client_closed_code == -1)
        {
            if (connected)
                DrawText("Connection to the server was lost", 265, 280, 20, RED);
            else
                DrawText("Server cannot be reached", 265, 280, 20, RED);
        }
        else if (client_closed_code == SERVER_FULL_CODE)
        {
            DrawText("Cannot connect, server is full", 265, 280, 20, RED);
        }
    }
    else if (connected && spawned)
    {
        /* Start by drawing the remote clients */
        for (int i = 0; i < MAX_CLIENTS - 1; i++)
        {
            if (clients[i])
                DrawClient(clients[i], false);
        }

        /* Then draw the local client */
        DrawClient(&local_client_state, true);

        /* Finally draw the HUD */
        DrawHUD();
    }
    else
    {
        DrawText("Connecting to server...", 265, 280, 20, RED);
    }

    EndDrawing();
}

int main(void)
{
    SetTraceLogLevel(LOG_INFO);

    InitWindow(GAME_WIDTH, GAME_HEIGHT, "raylib client");

    /*
        Init the client with a protocol name.

        Protocol name can be anything but has to match the one used on the server.
    */
    NBN_GameClient_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, "127.0.0.1", RAYLIB_EXAMPLE_PORT);

    /*
        Register the actual messages that will be exchanged between the client and the server.

        Messages have to be registered on both client and server sides.
    */
    RegisterMessages();

    /*
        Start the server on localhost and port 42042.
    */
    if (NBN_GameClient_Start() < 0)
    {
        TraceLog(LOG_WARNING, "Game client failed to start. Exit");

        return 1;
    }

    float tick_dt = 1.f / TICK_RATE; /* Tick delta time */

    /* Loop until the window is closed or we are disconnected from the server */
    while (!WindowShouldClose())
    {
        /*
            Update the game client time, this should be done first every frame.
        */
        NBN_GameClient_AddTime(GetFrameTime());

        NBN_GameClientEvent ev;

        /*
            Poll network events.
        */
        while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
        {
            if (HandleGameClientEvent(ev) < 0)
            {
                TraceLog(LOG_WARNING, "An occured while polling network events. Exit");

                break;
            }
        }

        if (connected && !disconnected)
        {
            if (Update() < 0)
                break;
        }

        Draw();

        /*
            Flush the send queue: all pending messages will be packed together into packets and sent to the server.

            This should be the last thing you do every frame. 
        */
        if (!disconnected)
        {
            if (NBN_GameClient_SendPackets() < 0)
            {
                TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");

                break;
            }
        }

/* Cap the simulation rate to the target tick rate */
#if defined(_WIN32) || defined(_WIN64)
        Sleep(tick_dt * 1000);
#else /* UNIX / OSX */
        long nanos = tick_dt * 1e9;
        struct timespec t = {.tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999};

        nanosleep(&t, &t);
#endif
    }

    /*
        Stop the game client.
    */
    NBN_GameClient_Stop();

    CloseWindow();

    return 0;
}
