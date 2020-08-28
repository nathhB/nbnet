#include <limits.h>
#include <time.h>

#include "shared.h"

#define NBNET_IMPL /* nbnet implementation */

#include "../../nbnet.h"

#if defined(NBN_DRIVER_UDP)

/* nbnet udp driver implementation */

#define NBN_DRIVER_UDP_IMPL

#include "../../net_drivers/udp.h"

#elif defined(NBN_DRIVER_UDP_SDL_NET)

/* nbnet SDL_net udp driver implementation */

#define NBN_DRIVER_UDP_SDL_NET_IMPL 

#include "../../net_drivers/udp_sdl_net.h"

#endif

void RegisterMessages(void)
{
    /*
        The first argument of NBN_RegisterMessage is the type of the message (user defined), the seond one is the message
        builder function, the third one is the message serializer and the final one is the message destructor.
    */
    NBN_RegisterMessage(SPAWN_MESSAGE, SpawnMessage_Create, SpawnMessage_Serialize, SpawnMessage_Destroy);
    NBN_RegisterMessage(CHANGE_COLOR_MESSAGE, ChangeColorMessage_Create, ChangeColorMessage_Serialize, ChangeColorMessage_Destroy);
    NBN_RegisterMessage(UPDATE_POSITION_MESSAGE, UpdatePositionMessage_Create, UpdatePositionMessage_Serialize, UpdatePositionMessage_Destroy);
    NBN_RegisterMessage(GAME_STATE_MESSAGE, GameStateMessage_Create, GameStateMessage_Serialize, GameStateMessage_Destroy);
}

SpawnMessage *SpawnMessage_Create(void)
{
    return malloc(sizeof(SpawnMessage));
}

ChangeColorMessage *ChangeColorMessage_Create(void)
{
    return malloc(sizeof(ChangeColorMessage));
}

UpdatePositionMessage *UpdatePositionMessage_Create(void)
{
    return malloc(sizeof(UpdatePositionMessage));
}

GameStateMessage* GameStateMessage_Create(void)
{
    return malloc(sizeof(GameStateMessage));
}

int SpawnMessage_Serialize(SpawnMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->client_id, 0, UINT_MAX);
    SERIALIZE_UINT(msg->x, 0, GAME_WIDTH);
    SERIALIZE_UINT(msg->y, 0, GAME_HEIGHT);

    return 0;
}

int ChangeColorMessage_Serialize(ChangeColorMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->color, 0, MAX_COLORS - 1);

    return 0;
}

int UpdatePositionMessage_Serialize(UpdatePositionMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->x, 0, GAME_WIDTH);
    SERIALIZE_UINT(msg->y, 0, GAME_HEIGHT);

    return 0;
}

int GameStateMessage_Serialize(GameStateMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->client_count, 0, MAX_CLIENTS);

    for (unsigned int i = 0; i < msg->client_count; i++)
    {
        SERIALIZE_UINT(msg->client_states[i].client_id, 0, UINT_MAX);
        SERIALIZE_UINT(msg->client_states[i].color, 0, MAX_COLORS - 1);
        SERIALIZE_UINT(msg->client_states[i].x, 0, GAME_WIDTH);
        SERIALIZE_UINT(msg->client_states[i].y, 0, GAME_HEIGHT);

        // TODO: color
    }

    return 0;
}

void SpawnMessage_Destroy(SpawnMessage *msg)
{
    free(msg);
}

void ChangeColorMessage_Destroy(ChangeColorMessage *msg)
{
    free(msg);
}

void UpdatePositionMessage_Destroy(UpdatePositionMessage *msg)
{
    free(msg);
}

void GameStateMessage_Destroy(GameStateMessage *msg)
{
    free(msg);
}

void TickSleep(float tick_dt)
{
#if defined(_WIN32) || defined(_WIN64)
    Sleep(tick_dt * 1000);
#else
    long nanos = tick_dt * 1e9;
    struct timespec t = { .tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999 };

    nanosleep(&t, &t);
#endif /* WINDOWS */
}
