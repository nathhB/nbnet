#include "../../nbnet.h"
#include "shared.h"

static bool connected = false;
static bool disconnected = false;
static bool spawned = false;
static uint32_t local_client_id;
static Vector2 local_client_pos;

static void HandleSpawnMessage(SpawnMessage *msg)
{
    TraceLog(LOG_INFO, "Received spawn message, position: (%d, %d), client id: %d", msg->x, msg->y, msg->client_id);

    local_client_id = msg->client_id;
    local_client_pos.x = msg->x;
    local_client_pos.y = msg->y;
    spawned = true;
}

static void HandleGameStateMessage(GameStateMessage *msg)
{
    if (!spawned)
        return;

    /* Update each client expect the local one based on the received client states */
    for (unsigned int i = 0; i < msg->client_count; i++)
    {
        ClientState state = msg->client_states[i];

        if (state.client_id != local_client_id)
        {
            printf("LOL: %d %d\n", state.x, state.y);
        }
    }
}

static void HandleReceivedMessage(void)
{
    NBN_MessageInfo msg;

    /*
        Read info about the last received message.

        The "data" field of NBN_MessageInfo structure is a pointer to the user defined message
        so in our case SpawnMessage, ChangeColorMessage or UpdatePositionMessage.
    */
    NBN_GameClient_ReadReceivedMessage(&msg);

    switch (msg.type)
    {
        case SPAWN_MESSAGE:
            HandleSpawnMessage(msg.data);
            break;

        case GAME_STATE_MESSAGE:
            HandleGameStateMessage(msg.data);
            break;
    }
}

static void HandleGameClientEvent(NBN_GameClientEvent ev)
{
    switch (ev)
    {
    case NBN_CONNECTED:
        /* We are connected to the server */
        connected = true;
        break;

    case NBN_DISCONNECTED:
        /* We have been disconnected from the server */
        disconnected = true;
        break;

    case NBN_MESSAGE_RECEIVED:
        /* We received a message from the server */
        HandleReceivedMessage();
        break;

    default:
        break;
    }
}

static int SendPositionUpdate(void)
{
    /* Create an update position message on the unreliable ordered channel */
    UpdatePositionMessage *msg = NBN_GameClient_CreateMessage(UPDATE_POSITION_MESSAGE, UNRELIABLE_CHANNEL);

    if (msg == NULL)
        return -1;

    msg->x = local_client_pos.x;
    msg->y = local_client_pos.y;

    if (NBN_GameClient_SendMessage() < 0)
        return -1;

    return 0;
}

static int UpdateClient(void)
{
    if (spawned)
    {
        /* Very basic movement code */
        if (IsKeyDown(KEY_UP))
            local_client_pos.y = MAX(0, local_client_pos.y - 5);
        else if (IsKeyDown(KEY_DOWN))
            local_client_pos.y = MIN(GAME_HEIGHT, local_client_pos.y + 5);

        if (IsKeyDown(KEY_LEFT))
            local_client_pos.x = MAX(0, local_client_pos.x - 5);
        else if (IsKeyDown(KEY_RIGHT))
            local_client_pos.x = MIN(GAME_WIDTH, local_client_pos.x + 5);

        /* Send the updated client position to the server */
        if (SendPositionUpdate() < 0)
        {
            TraceLog(LOG_WARNING, "Failed to send position update");

            return -1;
        }
    }

    return 0;
}

int main(void)
{
    SetTraceLogLevel(LOG_INFO);

    InitWindow(GAME_WIDTH, GAME_HEIGHT, "raylib client");

    /*
        Init the client with a protocol name.

        Protocol name can be anything but has to match the one used on the server.
    */
    NBN_GameClient_Init(RAYLIB_EXAMPLE_PROTOCOL_NAME);

    /*
        Register the channels we are going to use to exhange messages between the client and the server.

        Channels have to be registered on both client and server sides.
    */
    RegisterChannels();

    /*
        Register the actual messages that will be exchanged between the client and the server.

        Messages have to be registered on both client and server sides.
    */
    RegisterMessages();

    /*
        Start the server on localhost and port 42042.
    */
    if (NBN_GameClient_Start("127.0.0.1", 42042) < 0)
    {
        TraceLog(LOG_WARNING, "Game client failed to start. Exit");

        return 1;
    }
    
    float tick_dt = 1.f / TICK_RATE; /* tick delta time */

    /* Loop until the window is closed or we are disconnected from the server */
    while (!WindowShouldClose() && !disconnected)
    {
        /*
            Update the game client time, this should be done first every frame.
        */
        NBN_GameClient_AddTime(GetFrameTime());

        /*
            Poll for network events.
        */
        NBN_GameClientEvent ev = NBN_GameClient_Poll();

        if (ev == NBN_ERROR)
        {
            TraceLog(LOG_WARNING, "An occured while polling network events. Exit");

            break;
        }

        HandleGameClientEvent(ev);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        if (connected)
        {
            if (UpdateClient() < 0)
                break;
        }
        else
        {
            DrawText("Connecting to server...", 265, 280, 20, LIGHTGRAY);
        }

        EndDrawing();

        /*
            Flush the send queue: all pending messages will be packed together into packets and sent to the server.

            This should be done last every frame. 
        */
        if (NBN_GameClient_Flush() < 0)
        {
            TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");

            break;
        }

        /* We do not want our client simulation to run too fast so sleep for a duration of "tick dt" */
        TickSleep(tick_dt);
    }

    /*
        Stop the game client.
    */
    NBN_GameClient_Stop();

    CloseWindow();

    return 0;
}