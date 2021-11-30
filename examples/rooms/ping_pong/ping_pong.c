#include <emscripten.h>

#define NBN_LogInfo LogInfo
#define NBN_LogTrace LogTrace
#define NBN_LogDebug LogDebug
#define NBN_LogError LogError

#include "logging.h"

#define NBNET_IMPL

#include "../../../nbnet.h"
#include "../../../net_drivers/webrtc.h"

#define ROOM_SERVER_HOST "127.0.0.1"
#define ROOM_SERVER_PORT 42042

#define TICK_DT (1 / 30.0)

static int StartHost(void)
{
    NBN_GameServer_Init(ROOM_SERVER_HOST, ROOM_SERVER_PORT);

    if (NBN_GameServer_Start() < 0)
    {
        LogError("Failed to start game server");

        return -1;
    }

    return 0;
}

static int StartClient(const char *room_id)
{
    NBN_GameClient_Init(room_id, ROOM_SERVER_HOST, ROOM_SERVER_PORT);

    if (NBN_GameClient_Start() < 0)
    {
        LogError("Failed to start game client");

        return -1;
    }

    return 0;
}

static int HostTick(void)
{
    NBN_GameServer_AddTime(TICK_DT);

    int ev;

    while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
    {
        if (ev < 0)
            return -1;

        switch (ev)
        {
        case NBN_NEW_CONNECTION:
            NBN_GameServer_AcceptIncomingConnection();

            LogInfo("New client connected (ID: %d)", NBN_GameServer_GetIncomingConnection()->id);
            break;

        case NBN_CLIENT_DISCONNECTED:
            LogInfo("Client %d has disconnected", NBN_GameServer_GetDisconnectedClient()->id);
            break;

        case NBN_CLIENT_MESSAGE_RECEIVED:
            break;
        }
    }

    if (NBN_GameServer_SendPackets() < 0)
        return -1;

    return 0;
}

static int ClientTick(void)
{
    NBN_GameClient_AddTime(TICK_DT);

    int ev;

    while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
    {
        if (ev < 0)
            return -1;

        switch (ev)
        {
            case NBN_CONNECTED:
                LogInfo("Connected");
                break;

            case NBN_DISCONNECTED:
                LogInfo("Disconnected");
                return -1;
        }
    }

    if (NBN_GameClient_IsConnected())
    {
    }

    if (NBN_GameClient_SendPackets() < 0)
        return -1;

    return 0;
}

int main(int argc, char *argv[])
{
    bool is_host;

    log_set_level(LOG_INFO);

    if (argc > 1)
    {
        const char *room_id = argv[1];

        LogInfo("Starting as client... (room id: %s)", room_id);

        if (StartClient(room_id) < 0)
            return -1;

        is_host = false;
    }
    else
    {
        LogInfo("Starting as host...");

        if (StartHost() < 0)
            return -1;

        is_host = true;
    }

    while (true)
    {
        if (is_host)
        {
            if (HostTick() < 0)
                return -1;
        }
        else
        {
            if (ClientTick() < 0)
                return -1;
        }

        emscripten_sleep(TICK_DT * 1000);
    }

    return 0;
}