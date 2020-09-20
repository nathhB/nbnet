#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <time.h>
#endif

#include "soak.h"

enum
{
    OPT_MESSAGES_COUNT,
    OPT_PACKET_LOSS,
    OPT_PACKET_DUPLICATION,
    OPT_PING,
    OPT_JITTER
};

static bool running = true;
static SoakOptions soak_options = {0};

int Soak_Init(int argc, char *argv[])
{
    srand(SOAK_SEED);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

    NBN_RegisterMessage(SOAK_MESSAGE, SoakMessage_Create, SoakMessage_Serialize, SoakMessage_Destroy);

    /* Packet simulator configuration */
    NBN_Debug_SetPing(soak_options.ping);
    NBN_Debug_SetJitter(soak_options.jitter);
    NBN_Debug_SetPacketLoss(soak_options.packet_loss);
    NBN_Debug_SetPacketDuplication(soak_options.packet_duplication);

    return 0;
}

void Soak_Deinit(void)
{
    Soak_LogInfo("Done.");
    Soak_LogInfo("Memory report:\n");

    NBN_MemoryReport mem_report = NBN_MemoryManager_GetReport();

    Soak_LogInfo("Total alloc count: %d", mem_report.alloc_count);
    Soak_LogInfo("Total dealloc count: %d", mem_report.dealloc_count);
    Soak_LogInfo("Total created message count: %d", mem_report.created_message_count);
    Soak_LogInfo("Total destroyed message count: %d", mem_report.destroyed_message_count);

    for (int i = 0; i < 5; i++)
    {
        Soak_LogInfo("Object %d | Allocs: %d, Deallocs: %d", i, mem_report.object_allocs[i], mem_report.object_deallocs[i]);
    }
}

int Soak_ReadCommandLine(int argc, char *argv[])
{
#ifdef NBN_GAME_CLIENT
    if (argc < 2)
    {
        printf("Usage: client --message_count=<value> [--packet_loss=<value>] \
            [--packet_duplication=<value>] [--ping=<value>] [--jitter=<value>]\n");

        return -1;
    }
#endif

    int opt;
    int option_index;
    struct option long_options[] = {
        { "messages_count", required_argument, NULL, OPT_MESSAGES_COUNT },
        { "packet_loss", required_argument, NULL, OPT_PACKET_LOSS },
        { "packet_duplication", required_argument, NULL, OPT_PACKET_DUPLICATION },
        { "ping", required_argument, NULL, OPT_PING },
        { "jitter", required_argument, NULL, OPT_JITTER }
    };

    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
#ifdef NBN_GAME_CLIENT
        case OPT_MESSAGES_COUNT:
            soak_options.messages_count = atoi(optarg);
            break;
#endif

        case OPT_PACKET_LOSS:
            soak_options.packet_loss = atof(optarg);
            break;

        case OPT_PACKET_DUPLICATION:
            soak_options.packet_duplication = atof(optarg);
            break;

        case OPT_PING:
            soak_options.ping = atof(optarg);
            break;

        case OPT_JITTER:
            soak_options.jitter = atof(optarg);
            break;

        case '?':
            return -1;
        
        default:
            return -1;
        }
    }

    return 0;
}

int Soak_MainLoop(int (*Tick)(void))
{
    while (running)
    {
        if (Tick() < 0)
            return 1;

#ifdef __EMSCRIPTEN__
    emscripten_sleep(1.f / SOAK_TICK_RATE);
#else
    long nanos = SOAK_TICK_DT * 1e9;
    struct timespec t = { .tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999 };

    nanosleep(&t, &t);
#endif
    }

    return 0;
}

void Soak_Stop(void)
{
    running = false;

    Soak_LogInfo("Soak test stopped");
}

SoakOptions Soak_GetOptions(void)
{
    return soak_options;
}

SoakMessage *SoakMessage_Create(void)
{
    SoakMessage *msg = malloc(sizeof(SoakMessage));

    msg->id = 0;

    return msg; 
}

void SoakMessage_Destroy(SoakMessage *msg)
{
    free(msg);
}

int SoakMessage_Serialize(SoakMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->id, 0, UINT32_MAX);
    SERIALIZE_UINT(msg->data_length, 1, SOAK_MESSAGE_MAX_DATA_LENGTH);
    SERIALIZE_BYTES(msg->data, msg->data_length);

    return 0;
}

void Soak_Debug_PrintAddedToRecvQueue(NBN_Connection *conn, NBN_Message *msg)
{
    if (msg->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        Soak_LogDebug("Soak message chunk added to recv queue");
    }
    else
    {
        SoakMessage *soak_message = (SoakMessage *)msg->data;

        Soak_LogDebug("Soak message added to recv queue (conn id: %d, msg id: %d, soak msg id: %d)",
                conn->id, msg->header.id, soak_message->id);
    }
}
