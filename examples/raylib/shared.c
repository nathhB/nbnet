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

#include <limits.h>
#include <getopt.h>

// nbnet implementation
#define NBNET_IMPL 

#include "shared.h"

// Register all messages, called on both client and server side
void RegisterMessages(void)
{
    NBN_RegisterMessage(CHANGE_COLOR_MESSAGE, ChangeColorMessage);
    NBN_RegisterMessage(UPDATE_STATE_MESSAGE, UpdateStateMessage);
    NBN_RegisterMessage(GAME_STATE_MESSAGE, GameStateMessage);
}

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
