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
static unsigned int created_outgoing_soak_message_count = 0;
static unsigned int created_incoming_soak_message_count = 0;
static unsigned int destroyed_outgoing_soak_message_count = 0;
static unsigned int destroyed_incoming_soak_message_count = 0;

int Soak_Init(int argc, char *argv[])
{
    srand(SOAK_SEED);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

#ifdef SOAK_CLIENT

#ifdef SOAK_ENCRYPTION
    NBN_GameClient_EnableEncryption();
#endif

    NBN_GameClient_RegisterMessage(SOAK_MESSAGE,
            (NBN_MessageBuilder)SoakMessage_CreateIncoming,
            (NBN_MessageDestructor)SoakMessage_Destroy,
            (NBN_MessageSerializer)SoakMessage_Serialize);

#endif

#ifdef SOAK_SERVER

#ifdef SOAK_ENCRYPTION
    NBN_GameServer_EnableEncryption();
#endif

    NBN_GameServer_RegisterMessage(SOAK_MESSAGE,
            (NBN_MessageBuilder)SoakMessage_CreateIncoming,
            (NBN_MessageDestructor)SoakMessage_Destroy,
            (NBN_MessageSerializer)SoakMessage_Serialize);

#endif

    /* Packet simulator configuration */
#ifdef SOAK_CLIENT
    NBN_GameClient_SetPing(soak_options.ping);
    NBN_GameClient_SetJitter(soak_options.jitter);
    NBN_GameClient_SetPacketLoss(soak_options.packet_loss);
    NBN_GameClient_SetPacketDuplication(soak_options.packet_duplication);
#endif

#ifdef SOAK_SERVER
    NBN_GameServer_SetPing(soak_options.ping);
    NBN_GameServer_SetJitter(soak_options.jitter);
    NBN_GameServer_SetPacketLoss(soak_options.packet_loss);
    NBN_GameServer_SetPacketDuplication(soak_options.packet_duplication);
#endif

    return 0;
}

void Soak_Deinit(void)
{
    Soak_LogInfo("Done.");
    Soak_LogInfo("Memory report:\n");
    // TODO
}

int Soak_ReadCommandLine(int argc, char *argv[])
{
#ifdef SOAK_CLIENT
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
        { "message_count", required_argument, NULL, OPT_MESSAGES_COUNT },
        { "packet_loss", required_argument, NULL, OPT_PACKET_LOSS },
        { "packet_duplication", required_argument, NULL, OPT_PACKET_DUPLICATION },
        { "ping", required_argument, NULL, OPT_PING },
        { "jitter", required_argument, NULL, OPT_JITTER }
    };

    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
#ifdef SOAK_CLIENT
            case OPT_MESSAGES_COUNT:
                soak_options.message_count = atoi(optarg);
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
        NBN_MessageChunk *chunk = msg->data;

        Soak_LogDebug("Soak message chunk added to recv queue (chunk id: %d, chunk total: %d)",
                chunk->id, chunk->total);
    }
    else
    {
        SoakMessage *soak_message = (SoakMessage *)msg->data;

        Soak_LogDebug("Soak message added to recv queue (conn id: %d, msg id: %d, soak msg id: %d)",
                conn->id, msg->header.id, soak_message->id);
    }
}

unsigned int Soak_GetCreatedOutgoingSoakMessageCount(void)
{
    return created_outgoing_soak_message_count;
}

unsigned int Soak_GetDestroyedOutgoingSoakMessageCount(void)
{
    return destroyed_outgoing_soak_message_count;
}

unsigned int Soak_GetCreatedIncomingSoakMessageCount(void)
{
    return created_incoming_soak_message_count;
}

unsigned int Soak_GetDestroyedIncomingSoakMessageCount(void)
{
    return destroyed_incoming_soak_message_count;
}

SoakMessage *SoakMessage_CreateIncoming(void)
{
    SoakMessage *msg = malloc(sizeof(SoakMessage));

    msg->outgoing = false;

    created_incoming_soak_message_count++;

    return msg;
}

SoakMessage *SoakMessage_CreateOutgoing(void)
{
    SoakMessage *msg = malloc(sizeof(SoakMessage));

    msg->outgoing = true;

    created_outgoing_soak_message_count++;

    return msg;
}

void SoakMessage_Destroy(SoakMessage *msg)
{
    if (msg->outgoing)
        destroyed_outgoing_soak_message_count++;
    else
        destroyed_incoming_soak_message_count++;

    free(msg);
}
