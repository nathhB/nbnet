/*

   Copyright (C) 2024 BIAGINI Nathan

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
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>

// nbnet implementation
#define NBNET_IMPL

#include "shared.h"

// Command line options
enum { OPT_MESSAGES_COUNT, OPT_PACKET_LOSS, OPT_PACKET_DUPLICATION, OPT_PING, OPT_JITTER };

static Options options = {0};

void ChangeColorMessage_Write(NBN_Writer *writer, ClientColor color) {
    NBN_Writer_WriteUInt32(writer, (uint32_t)color);
}

int ChangeColorMessage_Read(NBN_Reader *reader, ClientColor *color) {
    return NBN_Reader_ReadUInt32(reader, (uint32_t *)color);
}

void UpdateClientStateMessage_Write(NBN_Writer *writer, ClientState state) {
    NBN_Writer_WriteInt32(writer, state.x);
    NBN_Writer_WriteInt32(writer, state.y);
    NBN_Writer_WriteFloat(writer, state.val);
}

int UpdateClientStateMessage_Read(NBN_Reader *reader, ClientState *state) {
    if (NBN_Reader_ReadInt32(reader, &state->x) < 0) {
        return -1;
    }

    if (NBN_Reader_ReadInt32(reader, &state->y) < 0) {
        return -1;
    }

    if (NBN_Reader_ReadFloat(reader, &state->val) < 0) {
        return -1;
    }

    return 0;
}

void GameStateMessage_Write(NBN_Writer *writer, GameState *state) {
    NBN_Writer_WriteUInt32(writer, state->client_count);

    for (unsigned int i = 0; i < state->client_count; i++) {
        ClientState cli_state = state->client_states[i];

        NBN_Writer_WriteUInt32(writer, cli_state.client_id);
        NBN_Writer_WriteUInt32(writer, (uint32_t)cli_state.color);
        NBN_Writer_WriteInt32(writer, cli_state.x);
        NBN_Writer_WriteInt32(writer, cli_state.y);
        NBN_Writer_WriteFloat(writer, cli_state.val);
        NBN_Writer_WriteString(writer, cli_state.name, CLIENT_NAME_MAX_LEN);
    }
}

int GameStateMessage_Read(NBN_Reader *reader, GameState *state) {
    if (NBN_Reader_ReadUInt32(reader, &state->client_count) < 0) {
        return -1;
    }

    if (state->client_count > MAX_CLIENTS) {
        return -1;
    }

    for (unsigned int i = 0; i < state->client_count; i++) {
        ClientState *cli_state = &state->client_states[i];

        if (NBN_Reader_ReadUInt32(reader, &cli_state->client_id) < 0) {
            return -1;
        }

        if (NBN_Reader_ReadUInt32(reader, (uint32_t *)&cli_state->color) < 0) {
            return -1;
        }

        if (NBN_Reader_ReadInt32(reader, &cli_state->x) < 0) {
            return -1;
        }

        if (NBN_Reader_ReadInt32(reader, &cli_state->y) < 0) {
            return -1;
        }

        if (NBN_Reader_ReadFloat(reader, &cli_state->val) < 0) {
            return -1;
        }

        if (NBN_Reader_ReadString(reader, cli_state->name, CLIENT_NAME_MAX_LEN) < 0) {
            return -1;
        }
    }

    return 0;
}

// Parse the command line
int ReadCommandLine(int argc, char *argv[]) {
    int opt;
    int option_index;
    struct option long_options[] = {{"packet_loss", required_argument, NULL, OPT_PACKET_LOSS},
                                    {"packet_duplication", required_argument, NULL, OPT_PACKET_DUPLICATION},
                                    {"ping", required_argument, NULL, OPT_PING},
                                    {"jitter", required_argument, NULL, OPT_JITTER}};

    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (opt) {
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
Options GetOptions(void) { return options; }
