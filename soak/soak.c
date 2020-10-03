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

    NBN_RegisterMessage(SOAK_MESSAGE, SoakMessage);

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
        int ret = Tick();

        if (ret < 0) // Error
            return 1;

        if (ret == SOAK_DONE) // All soak messages have been received
            return 0;

#ifdef __EMSCRIPTEN__
        emscripten_sleep(SOAK_TICK_DT * 1000);
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
