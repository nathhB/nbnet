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
#include <limits.h>
#include <getopt.h>

// nbnet implementation
#define NBNET_IMPL 

#include "shared.h"

// Command line options
enum
{
    OPT_MESSAGES_COUNT,
    OPT_PACKET_LOSS,
    OPT_PACKET_DUPLICATION,
    OPT_PING,
    OPT_JITTER
};

static Options options = {0};

ChangeColorMessage *ChangeColorMessage_Create(void)
{
    return malloc(sizeof(ChangeColorMessage));
}

void ChangeColorMessage_Destroy(ChangeColorMessage *msg)
{
    free(msg);
}

int ChangeColorMessage_Serialize(ChangeColorMessage *msg, NBN_Stream *stream)
{
    NBN_SerializeUInt(stream, msg->color, 0, MAX_COLORS - 1);

    return 0;
}

UpdateStateMessage *UpdateStateMessage_Create(void)
{
    return malloc(sizeof(UpdateStateMessage));
}

void UpdateStateMessage_Destroy(UpdateStateMessage *msg)
{
    free(msg);
}

int UpdateStateMessage_Serialize(UpdateStateMessage *msg, NBN_Stream *stream)
{
    NBN_SerializeUInt(stream, msg->x, 0, GAME_WIDTH);
    NBN_SerializeUInt(stream, msg->y, 0, GAME_HEIGHT);
    NBN_SerializeFloat(stream, msg->val, MIN_FLOAT_VAL, MAX_FLOAT_VAL, 3);

    return 0;
}

GameStateMessage *GameStateMessage_Create(void)
{
    return malloc(sizeof(GameStateMessage));
}

void GameStateMessage_Destroy(GameStateMessage *msg)
{
    free(msg);
}

int GameStateMessage_Serialize(GameStateMessage *msg, NBN_Stream *stream)
{
    NBN_SerializeUInt(stream, msg->client_count, 0, MAX_CLIENTS);

    for (unsigned int i = 0; i < msg->client_count; i++)
    {
        NBN_SerializeUInt(stream, msg->client_states[i].client_id, 0, UINT_MAX);
        NBN_SerializeUInt(stream, msg->client_states[i].color, 0, MAX_COLORS - 1);
        NBN_SerializeUInt(stream, msg->client_states[i].x, 0, GAME_WIDTH);
        NBN_SerializeUInt(stream, msg->client_states[i].y, 0, GAME_HEIGHT);
        NBN_SerializeFloat(stream, msg->client_states[i].val, MIN_FLOAT_VAL, MAX_FLOAT_VAL, 3);
    }

    return 0;
}

// Parse the command line
int ReadCommandLine(int argc, char *argv[])
{
    int opt;
    int option_index;
    struct option long_options[] = {
        { "packet_loss", required_argument, NULL, OPT_PACKET_LOSS },
        { "packet_duplication", required_argument, NULL, OPT_PACKET_DUPLICATION },
        { "ping", required_argument, NULL, OPT_PING },
        { "jitter", required_argument, NULL, OPT_JITTER }
    };

    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
            case OPT_PACKET_LOSS:
                options.packet_loss = atof(optarg);
                break;

            case OPT_PACKET_DUPLICATION:
                options.packet_duplication = atof(optarg);
                break;

            case OPT_PING:
                options.ping = atof(optarg);
                break;

            case OPT_JITTER:
                options.jitter = atof(optarg);
                break;

            case '?':
                return -1;

            default:
                return -1;
        }
    }

    return 0;
}

// Return the command line options
Options GetOptions(void)
{
    return options;
}
