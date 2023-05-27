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

#include <stdlib.h>
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#elif !defined(_WIN32) && !defined(_WIN64)
// we are on unix or osx
#include <time.h>
#endif

#include "soak.h"
#include "cargs.h"

static bool running = true;
static SoakOptions soak_options = {0};
static unsigned int created_outgoing_soak_message_count = 0;
static unsigned int created_incoming_soak_message_count = 0;
static unsigned int destroyed_outgoing_soak_message_count = 0;
static unsigned int destroyed_incoming_soak_message_count = 0;

static void Usage(void)
{
#ifdef SOAK_CLIENT
        printf("Usage: client --message_count=<value> --channel_count=<value> [--packet_loss=<value>] \
[--packet_duplication=<value>] [--ping=<value>] [--jitter=<value>]\n");
#endif // SOAK_CLIENT

#ifdef SOAK_SERVER
        printf("Usage: server --channel_count=<value> [--packet_loss=<value>] \
[--packet_duplication=<value>] [--ping=<value>] [--jitter=<value>]\n");
#endif
}

int Soak_Init(int argc, char *argv[])
{
    srand(SOAK_SEED);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

    SoakOptions options = Soak_GetOptions();

    Soak_LogInfo("Soak test initialized (Packet loss: %f, Packet duplication: %f, Ping: %f, Jitter: %f)",
        options.packet_loss, options.packet_duplication, options.ping, options.jitter);

#ifdef SOAK_CLIENT 

    for (int i = 0; i < options.channel_count; i++)
    {
        NBN_GameClient_RegisterReliableChannel(i);
    }

#ifdef SOAK_ENCRYPTION
    NBN_GameClient_EnableEncryption();
#endif

    NBN_GameClient_RegisterMessage(SOAK_MESSAGE,
            (NBN_MessageBuilder)SoakMessage_CreateIncoming,
            (NBN_MessageDestructor)SoakMessage_Destroy,
            (NBN_MessageSerializer)SoakMessage_Serialize);

#endif

#ifdef SOAK_SERVER 

    for (int i = 0; i < options.channel_count; i++)
    {
        NBN_GameServer_RegisterReliableChannel(i);
    }

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
    struct cag_option options[] = {
#ifdef SOAK_CLIENT
        {'m', NULL, "message_count", "VALUE", "Number of messages to send"},
#endif
        {'c', NULL, "channel_count", "VALUE", "Number of channels (1-NBN_MAX_CHANNELS)"},
        {'l', NULL, "packet_loss", "VALUE", "Packet loss frenquency (0-1)"},
        {'d', NULL, "packet_duplication", "VALUE", "Packet duplication frequency (0-1)"},
        {'p', NULL, "ping", "VALUE", "Ping in seconds"},
        {'j', NULL, "jitter", "VALUE", "Jitter in seconds"}
    };

    cag_option_context context;

    cag_option_prepare(&context, options, CAG_ARRAY_SIZE(options), argc, argv);

    while (cag_option_fetch(&context))
    {
        char option = cag_option_get(&context);

#ifdef SOAK_CLIENT
        if (option == 'm')
        {
            const char *val = cag_option_get_value(&context);

            if (val)
            {
                soak_options.message_count = atoi(val);
            }
        }
#else
        if (false) {}
#endif
        else if (option == 'c')
        {
            const char *val = cag_option_get_value(&context);

            if (val)
            {
                soak_options.channel_count = atoi(val);
            }
        }
        else if (option == 'l')
        {
            soak_options.packet_loss = atof(cag_option_get_value(&context));
        }
        else if (option == 'd')
        {
            soak_options.packet_duplication = atof(cag_option_get_value(&context));
        }
        else if (option == 'p')
        {
            soak_options.ping = atof(cag_option_get_value(&context));
        }
        else if (option == 'j')
        {
            soak_options.jitter = atof(cag_option_get_value(&context));
        }
    }

    if (soak_options.channel_count <= 0)
    {
        Usage();
        return -1;
    }

#ifdef SOAK_CLIENT
    if (soak_options.message_count <= 0)
    {
        Usage();
        return -1;
    }
#endif

    if (soak_options.channel_count > SOAK_MAX_CHANNELS)
    {
        Soak_LogError("Too many channels (max: %d)", SOAK_MAX_CHANNELS);
        return -1;
    }

    return 0;
}

int Soak_MainLoop(int (*Tick)(void *), void *data)
{
    while (running)
    {
        int ret = Tick(data);

        if (ret < 0) // Error
            return 1;

        if (ret == SOAK_DONE) // All soak messages have been received
            return 0;

#ifdef __EMSCRIPTEN__
        emscripten_sleep(SOAK_TICK_DT * 1000);
#elif defined(_WIN32) || defined(_WIN64)
        Sleep(SOAK_TICK_DT * 1000);
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
        NBN_MessageChunk *chunk = (NBN_MessageChunk *)msg->data;

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
    SoakMessage *msg = (SoakMessage *)malloc(sizeof(SoakMessage));

    msg->outgoing = false;

    created_incoming_soak_message_count++;

    return msg;
}

SoakMessage *SoakMessage_CreateOutgoing(void)
{
    SoakMessage *msg = (SoakMessage *)malloc(sizeof(SoakMessage));

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

int SoakMessage_Serialize(SoakMessage *msg, NBN_Stream *stream)
{
    NBN_SerializeUInt(stream, msg->id, 0, UINT32_MAX);
    NBN_SerializeUInt(stream, msg->data_length, 1, SOAK_MESSAGE_MAX_DATA_LENGTH);
    NBN_SerializeBytes(stream, msg->data, msg->data_length);

    return 0;
}
