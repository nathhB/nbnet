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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#elif !defined(_WIN32) && !defined(_WIN64)
// we are on unix or osx
#include <time.h>
#endif

#include "cargs.h"
#include "soak.h"

static bool running = true;
static SoakOptions soak_options = {0};
static unsigned int created_outgoing_soak_message_count = 0;
static unsigned int created_incoming_soak_message_count = 0;
static unsigned int destroyed_outgoing_soak_message_count = 0;
static unsigned int destroyed_incoming_soak_message_count = 0;

static void Usage(void) {
#ifdef SOAK_CLIENT

#ifdef WEBRTC_NATIVE
    printf("Usage: client --message_count=<value> [--packet_loss=<value>] \
[--packet_duplication=<value>] [--ping=<value>] [--jitter=<value>] [--webrtc]\n");
#else
    printf("Usage: client --message_count=<value> [--packet_loss=<value>] \
[--packet_duplication=<value>] [--ping=<value>] [--jitter=<value>]\n");
#endif // WEBRTC_NATIVE

#endif // SOAK_CLIENT

#ifdef SOAK_SERVER
    printf("Usage: server [--packet_loss=<value>] \
[--packet_duplication=<value>] [--ping=<value>] [--jitter=<value>]\n");
#endif
}

int Soak_ReadCommandLine(int argc, char *argv[]) {
    struct cag_option options[] = {
#ifdef SOAK_CLIENT

        {'m', NULL, "message_count", "VALUE", "Number of messages to send"},

#ifdef WEBRTC_NATIVE

        {'w', NULL, "webrtc", NULL, "Use the native WebRTC driver instead of the UDP driver"},

#endif // WEBRTC_NATIVE

#endif // SOAK_CLIENT

        {'l', NULL, "packet_loss", "VALUE", "Packet loss frenquency (0-1)"},
        {'d', NULL, "packet_duplication", "VALUE", "Packet duplication frequency (0-1)"},
        {'p', NULL, "ping", "VALUE", "Ping in seconds"},
        {'j', NULL, "jitter", "VALUE", "Jitter in seconds"}};

    cag_option_context context;

    cag_option_prepare(&context, options, CAG_ARRAY_SIZE(options), argc, argv);

    while (cag_option_fetch(&context)) {
        char option = cag_option_get(&context);

#ifdef SOAK_CLIENT
        if (option == 'm') {
            const char *val = cag_option_get_value(&context);

            if (val) {
                soak_options.message_count = atoi(val);
            }
        } else if (option == 'w') {
            soak_options.webrtc = true;
        }
#else
        if (false) {
        }
#endif
        else if (option == 'l') {
            soak_options.packet_loss = atof(cag_option_get_value(&context));
        } else if (option == 'd') {
            soak_options.packet_duplication = atof(cag_option_get_value(&context));
        } else if (option == 'p') {
            soak_options.ping = atof(cag_option_get_value(&context));
        } else if (option == 'j') {
            soak_options.jitter = atof(cag_option_get_value(&context));
        }
    }

#ifdef SOAK_CLIENT
    if (soak_options.message_count <= 0) {
        Usage();
        return -1;
    }
#endif

    return 0;
}

int Soak_MainLoop(int (*Tick)(void *), void *data) {
    while (running) {
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
        struct timespec t = {.tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999};

        nanosleep(&t, &t);
#endif
    }

    return 0;
}

void Soak_Stop(void) {
    running = false;

    log_info("Soak test stopped");
}

SoakOptions Soak_GetOptions(void) { return soak_options; }

unsigned int Soak_GetCreatedOutgoingSoakMessageCount(void) { return created_outgoing_soak_message_count; }

unsigned int Soak_GetDestroyedOutgoingSoakMessageCount(void) { return destroyed_outgoing_soak_message_count; }

unsigned int Soak_GetCreatedIncomingSoakMessageCount(void) { return created_incoming_soak_message_count; }

unsigned int Soak_GetDestroyedIncomingSoakMessageCount(void) { return destroyed_incoming_soak_message_count; }

void SoakMessage_Write(NBN_Writer *writer, unsigned int msg_id, uint8_t *data, unsigned int data_length) {
    NBN_Writer_WriteUInt32(writer, msg_id);
    NBN_Writer_WriteUInt32(writer, data_length);
    NBN_Writer_WriteBytes(writer, data, data_length);
}

int SoakMessage_Read(NBN_Reader *reader, unsigned int *msg_id, uint8_t *data, unsigned int *data_length) {
    if (NBN_Reader_ReadUInt32(reader, msg_id) < 0)
        return -1;
    if (NBN_Reader_ReadUInt32(reader, data_length) < 0)
        return -1;
    if (NBN_Reader_ReadBytes(reader, data, *data_length) < 0)
        return -1;

    return 0;
}
