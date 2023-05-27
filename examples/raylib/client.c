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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "shared.h"

#define TARGET_FPS 100

static bool connected = false;         // Connected to the server
static bool disconnected = false;      // Got disconnected from the server
static bool spawned = false;           // Has spawned
static int server_close_code;          // The server code used when closing the connection
static ClientState local_client_state; // The state of the local client

// Array to hold other client states (`MAX_CLIENTS - 1` because we don't need to store the state of the local client)
static ClientState *clients[MAX_CLIENTS - 1] = {NULL};

/*
 * Array of client ids that were updated in the last received GameStateMessage.
 * This is used to detect and destroy disconnected remote clients.
 */
static int updated_ids[MAX_CLIENTS];

// Number of currently connected clients
static unsigned int client_count = 0;

// Conversion table between client color values and raylib colors
Color client_colors_to_raylib_colors[] = {
    RED,    // CLI_RED
    LIME,   // CLI_GREEN
    BLUE,   // CLI_BLUE
    YELLOW, // CLI_YELLOW
    ORANGE, // CLI_ORANGE
    PURPLE, // CLI_PURPLE
    PINK    // CLI_PINK
};

static void SpawnLocalClient(int x, int y, uint32_t client_id)
{
    TraceLog(LOG_INFO, "Received spawn message, position: (%d, %d), client id: %d", x, y, client_id);

    // Update the local client state based on spawn info sent by the server
    local_client_state.client_id = client_id;
    local_client_state.x = x;
    local_client_state.y = y;

    spawned = true;
}

static void HandleConnection(void)
{
    NBN_Stream *rs = NBN_GameClient_GetAcceptDataReadStream();

    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int client_id = 0;

    NBN_SerializeUInt(rs, x, 0, GAME_WIDTH);
    NBN_SerializeUInt(rs, y, 0, GAME_HEIGHT);
    NBN_SerializeUInt(rs, client_id, 0, UINT_MAX);

    SpawnLocalClient(x, y, client_id);

    connected = true;
}

static void HandleDisconnection(void)
{
    int code = NBN_GameClient_GetServerCloseCode(); // Get the server code used when closing the client connection

    TraceLog(LOG_INFO, "Disconnected from server (code: %d)", code);

    disconnected = true;
    server_close_code = code;
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

    // Create a new remote client state and store it in the remote clients array at the first free slot found
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

    // Fill the newly created client state with client state info received from the server
    memcpy(client, &state, sizeof(ClientState));

    client_count++;

    TraceLog(LOG_INFO, "New remote client (ID: %d)", client->client_id);
}

static void UpdateClient(ClientState state)
{
    ClientState *client = NULL;

    // Find the client matching the client id of the received remote client state
    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        if (clients[i] && clients[i]->client_id == state.client_id)
        {
            client = clients[i];

            break;
        }
    }

    assert(client != NULL);

    // Update the client state with the latest client state info received from the server
    memcpy(client, &state, sizeof(ClientState));
}

static void DestroyClient(uint32_t client_id)
{
    // Find the client matching the client id and destroy it
    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        ClientState *client = clients[i];

        if (client && client->client_id == client_id)
        {
            TraceLog(LOG_INFO, "Destroy disconnected client (ID: %d)", client->client_id);

            free(client);
            clients[i] = NULL;
            client_count--;

            return;
        }
    }
}

static void DestroyDisconnectedClients(void)
{
    /* Loop over all remote client states and remove the one that have not
     * been updated with the last received game state.
     * This is how we detect disconnected clients.
     */
    for (int i = 0; i < MAX_CLIENTS - 1; i++)
    {
        if (clients[i] == NULL)
            continue;

        uint32_t client_id = clients[i]->client_id;
        bool disconnected = true;

        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if ((int)client_id == updated_ids[j])
            {
                disconnected = false;

                break;
            }
        }

        if (disconnected)
            DestroyClient(client_id);
    }
}

static void HandleGameStateMessage(GameStateMessage *msg)
{
    if (!spawned)
        return;

    // Start by resetting the updated client ids array
    for (int i = 0; i < MAX_CLIENTS; i++)
        updated_ids[i] = -1;

    // Loop over the received client states
    for (unsigned int i = 0; i < msg->client_count; i++)
    {
        ClientState state = msg->client_states[i];

        // Ignore the state of the local client
        if (state.client_id != local_client_state.client_id)
        {
            // If the client already exists we update it with the latest received state
            if (ClientExists(state.client_id))
                UpdateClient(state);
            else // If the client does not exist, we create it
                CreateClient(state);

            updated_ids[i] = state.client_id;
        }
    }

    // Destroy disconnected clients
    DestroyDisconnectedClients();

    GameStateMessage_Destroy(msg);
}

static void HandleReceivedMessage(void)
{
    // Fetch info about the last received message
    NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();

    switch (msg_info.type)
    {
    // We received the latest game state from the server
    case GAME_STATE_MESSAGE:
        HandleGameStateMessage(msg_info.data);
        break;
    }
}

static void HandleGameClientEvent(int ev)
{
    switch (ev)
    {
    case NBN_CONNECTED:
        // We are connected to the server
        HandleConnection();
        break;

    case NBN_DISCONNECTED:
        // The server has closed our connection
        HandleDisconnection();
        break;

    case NBN_MESSAGE_RECEIVED:
        // We received a message from the server
        HandleReceivedMessage();
        break;
    }
}

static int SendPositionUpdate(void)
{
    UpdateStateMessage *msg = UpdateStateMessage_Create();

    // Fill message data
    msg->x = local_client_state.x;
    msg->y = local_client_state.y;
    msg->val = local_client_state.val;

    // Unreliably send it to the server
    if (NBN_GameClient_SendUnreliableMessage(UPDATE_STATE_MESSAGE, msg) < 0)
        return -1;

    return 0;
}

static int SendColorUpdate(void)
{
    ChangeColorMessage *msg = ChangeColorMessage_Create();

    // Fill message data
    msg->color = local_client_state.color;

    // Reliably send it to the server
    if (NBN_GameClient_SendReliableMessage(CHANGE_COLOR_MESSAGE, msg) < 0)
        return -1;

    return 0;
}

bool color_key_pressed = false;

static int Update(void)
{
    if (!spawned)
        return 0;

    // Movement code
    if (IsKeyDown(KEY_UP))
        local_client_state.y = MAX(0, local_client_state.y - 5);
    else if (IsKeyDown(KEY_DOWN))
        local_client_state.y = MIN(GAME_HEIGHT - 50, local_client_state.y + 5);

    if (IsKeyDown(KEY_LEFT))
        local_client_state.x = MAX(0, local_client_state.x - 5);
    else if (IsKeyDown(KEY_RIGHT))
        local_client_state.x = MIN(GAME_WIDTH - 50, local_client_state.x + 5);

    // Color switching
    if (IsKeyDown(KEY_SPACE) && !color_key_pressed)
    {
        color_key_pressed = true;
        local_client_state.color = (local_client_state.color + 1) % MAX_COLORS;

        TraceLog(LOG_INFO, "Switched color, new color: %d", local_client_state.color);

        if (SendColorUpdate() < 0)
        {
            TraceLog(LOG_WARNING, "Failed to send color update");

            return -1;
        }
    }

    if (IsKeyUp(KEY_SPACE))
        color_key_pressed = false;

    // Increasing/Decreasing floating point value
    if (IsKeyDown(KEY_K))
        local_client_state.val = MIN(MAX_FLOAT_VAL, local_client_state.val + 0.005);

    if (IsKeyDown(KEY_J))
        local_client_state.val = MAX(MIN_FLOAT_VAL, local_client_state.val - 0.005);

    // Send the latest local client state to the server
    if (SendPositionUpdate() < 0)
    {
        TraceLog(LOG_WARNING, "Failed to send client state update");

        return -1;
    }

    return 0;
}

void DrawClient(ClientState *state, bool is_local)
{
    Color color = client_colors_to_raylib_colors[state->color];
    const char *text = TextFormat("%.3f", state->val);
    int font_size = 20;
    int text_width = MeasureText(text, font_size);

    DrawText(text, (state->x + 25) - text_width / 2, state->y - 20, font_size, color);
    DrawRectangle(state->x, state->y, 50, 50, color);

    if (is_local)
        DrawRectangleLinesEx((Rectangle){state->x, state->y, 50, 50}, 3, DARKBROWN);
}

void DrawHUD(void)
{
    NBN_ConnectionStats stats = NBN_GameClient_GetStats();
    unsigned int ping = stats.ping * 1000;
    unsigned int packet_loss = stats.packet_loss * 100;

    DrawText(TextFormat("FPS: %d", GetFPS()), 450, 350, 32, MAROON);
    DrawText(TextFormat("Ping: %d ms", ping), 450, 400, 32, MAROON);
    DrawText(TextFormat("Packet loss: %d %%", packet_loss), 450, 450, 32, MAROON);
    DrawText(TextFormat("Upload: %.1f Bps", stats.upload_bandwidth), 450, 500, 32, MAROON);
    DrawText(TextFormat("Download: %.1f Bps", stats.download_bandwidth), 450, 550, 32, MAROON);
}

void Draw(void)
{
    BeginDrawing();
    ClearBackground(LIGHTGRAY);

    if (disconnected)
    {
        if (server_close_code == -1)
        {
            if (connected)
                DrawText("Connection to the server was lost", 265, 280, 20, RED);
            else
                DrawText("Server cannot be reached", 265, 280, 20, RED);
        }
        else if (server_close_code == SERVER_FULL_CODE)
        {
            DrawText("Cannot connect, server is full", 265, 280, 20, RED);
        }
    }
    else if (connected && spawned)
    {
        // Start by drawing the remote clients
        for (int i = 0; i < MAX_CLIENTS - 1; i++)
        {
            if (clients[i])
                DrawClient(clients[i], false);
        }

        // Then draw the local client
        DrawClient(&local_client_state, true);

        // Finally draw the HUD
        DrawHUD();
    }
    else
    {
        DrawText("Connecting to server...", 265, 280, 20, RED);
    }

    EndDrawing();
}

static double tick_dt = 1.0 / TICK_RATE; // Tick delta time (in seconds)
static double acc = 0;

void UpdateAndDraw(void)
{
    // Very basic fixed timestep implementation.
    // Target FPS is either 100 (in desktop) or whatever the browser frame rate is (in web) but the simulation runs at
    // TICK_RATE ticks per second.
    //
    // We keep track of accumulated times and simulates as many tick as we can using that time

    acc += GetFrameTime(); // Accumulates time

    // Simulates as many ticks as we can
    while (acc >= tick_dt)
    {
        NBN_GameClient_AddTime(tick_dt);

        int ev;

        while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
        {
            if (ev < 0)
            {
                TraceLog(LOG_WARNING, "An occured while polling network events. Exit");

                break;
            }

            HandleGameClientEvent(ev);
        }

        if (connected && !disconnected)
        {
            if (Update() < 0)
                break;
        }

        if (!disconnected)
        {
            if (NBN_GameClient_SendPackets() < 0)
            {
                TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");

                break;
            }
        }

        acc -= tick_dt; // Consumes time
    }

    Draw();
}

int main(int argc, char *argv[])
{
    // Read command line arguments expect when we are running in a web browser.
    // When running in web browser we need another way to provide arguments (TODO)
#ifdef __EMSCRIPTEN__
    if (ReadCommandLine(argc, argv) < 0)
    {
        printf("Usage: client [--packet_loss=<value>] [--packet_duplication=<value>] [--ping=<value>] \
[--jitter=<value>]\n");

        return 1;
    }
#else
    (void)argc;
    (void)argv;
#endif

    SetTraceLogLevel(LOG_DEBUG);
    InitWindow(GAME_WIDTH, GAME_HEIGHT, "raylib client");

    // Set target FPS to 100 when we are not running in a web browser
#ifndef __EMSCRIPTEN__
    SetTargetFPS(TARGET_FPS);
#endif

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register(); // Register the WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__

    // Initialize the client with a protocol name (must be the same than the one used by the server), the server ip address and port
#ifdef EXAMPLE_ENCRYPTION
    NBN_GameClient_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, "127.0.0.1", RAYLIB_EXAMPLE_PORT, true, NULL);
#else
    NBN_GameClient_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME, "127.0.0.1", RAYLIB_EXAMPLE_PORT, false, NULL);
#endif

    if (NBN_GameClient_Start() < 0)
    {
        TraceLog(LOG_WARNING, "Game client failed to start. Exit");

        return 1;
    }

    // Register messages, have to be done after NBN_GameClient_Start
    // Messages need to be registered on both client and server side
    NBN_GameClient_RegisterMessage(
        CHANGE_COLOR_MESSAGE,
        (NBN_MessageBuilder)ChangeColorMessage_Create,
        (NBN_MessageDestructor)ChangeColorMessage_Destroy,
        (NBN_MessageSerializer)ChangeColorMessage_Serialize);
    NBN_GameClient_RegisterMessage(
        UPDATE_STATE_MESSAGE,
        (NBN_MessageBuilder)UpdateStateMessage_Create,
        (NBN_MessageDestructor)UpdateStateMessage_Destroy,
        (NBN_MessageSerializer)UpdateStateMessage_Serialize);
    NBN_GameClient_RegisterMessage(
        GAME_STATE_MESSAGE,
        (NBN_MessageBuilder)GameStateMessage_Create,
        (NBN_MessageDestructor)GameStateMessage_Destroy,
        (NBN_MessageSerializer)GameStateMessage_Serialize);

    // Network conditions simulated variables (read from the command line, default is always 0)
    NBN_GameClient_SetPing(GetOptions().ping);
    NBN_GameClient_SetJitter(GetOptions().jitter);
    NBN_GameClient_SetPacketLoss(GetOptions().packet_loss);
    NBN_GameClient_SetPacketDuplication(GetOptions().packet_duplication); 

    // Main loop
#ifdef __EMSCRIPTEN__
    while (true)
    {
        UpdateAndDraw();

        // Since we don't set any target FPS when running in a web browser we need to sleep for the correct amount
        // of time to achieve the targetted FPS
        emscripten_sleep(1000 / TARGET_FPS);
    }
#else
    while (!WindowShouldClose())
    {
        UpdateAndDraw();
    }
#endif

    if (!disconnected)
    {
        NBN_GameClient_Disconnect();
    }

    // Stop the client
    NBN_GameClient_Stop();

    CloseWindow();

    return 0;
}
