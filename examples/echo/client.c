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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Has to be defined in exactly *one* source file before including the nbnet header
#define NBNET_IMPL

#include "shared.h"
#include "log.h"

static bool running = true;
static bool connected = false;
static bool disconnected = false;

void OnConnected(void) {
    log_info("Connected");

    connected = true; // Start sending messages
}

void OnDisconnected(NBN_Client *client) {
    log_info("Disconnected");

    // Stop the main loop
    disconnected = true;
    running = false;

    // Retrieve the server code used when closing our client connection
    if (NBN_Client_GetServerCloseCode(client) == ECHO_SERVER_BUSY_CODE) {
        log_info("Another client is already connected");
    }
}

void OnMessageReceived(NBN_Client *client) {
    // Get info about the received message
    NBN_MessageInfo msg_info = NBN_Client_GetMessageInfo(client);

    assert(msg_info.type == ECHO_MESSAGE_TYPE);

    NBN_Reader *reader = NBN_Client_ReadMessage(client);
    unsigned int length;
    int res;

    res = NBN_Reader_ReadUInt32(reader, &length);
    assert(res == 0);
    static char msg_str[ECHO_MESSAGE_MAX_LENGTH];

    res = NBN_Reader_ReadBytes(reader, (uint8_t *)msg_str, length);
    assert(res == 0);
    msg_str[length] = 0;

    log_info("Received echo: %s (length: %d, channel: %d)", msg_str, msg_info.length, msg_info.channel_id);
}

int SendEcho(NBN_Client *client, const char *msg) {
    NBN_Writer *writer = NBN_Client_CreateReliableMessage(client, ECHO_MESSAGE_TYPE);

    if (!writer) {
        return -1;
    }

    unsigned int length = strlen(msg);

    NBN_Writer_WriteUInt32(writer, length);
    NBN_Writer_WriteBytes(writer, (uint8_t *)msg, length);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: client MSG\n");

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    const char *msg = argv[1];
    // reserve 4 bytes to write the message length in the message (see the SendEcho function)
    unsigned int msg_max_len = ECHO_MESSAGE_MAX_LENGTH - 4;

    if (strlen(msg) > msg_max_len) {
        log_error("Message length cannot exceed %d. Exit", msg_max_len);

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    // Start the client with a protocol name (must be the same than the one used by the server)
    // the server host and port

    NBN_Client *client = NBN_Client_Create(ECHO_PROTOCOL_NAME, "127.0.0.1", ECHO_EXAMPLE_PORT);

    if (NBN_Client_Start(client) < 0) {
        log_error("Failed to start client");

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    // Number of seconds between client ticks
    double dt = 1.0 / ECHO_TICK_RATE;

    while (running) {
        int ev;

        // Poll for client events
        while ((ev = NBN_Client_Poll(client)) != NBN_CLIENT_NO_EVENT) {
            if (ev < 0) {
                log_error("An error occured while polling client events. Exit");

                // Stop main loop
                running = false;
                break;
            }

            switch (ev) {
            // Client is connected to the server
            case NBN_CLIENT_CONNECTED:
                OnConnected();
                break;

                // Client has disconnected from the server
            case NBN_CLIENT_DISCONNECTED:
                OnDisconnected(client);
                break;

                // A message has been received from the server
            case NBN_CLIENT_MESSAGE_RECEIVED:
                OnMessageReceived(client);
                break;
            }
        }

        if (disconnected)
            break;

        if (connected) {
            if (SendEcho(client, msg) < 0) {
                log_error("Failed to send message. Exit");

                // Stop main loop
                running = false;
                break;
            }
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_Client_Flush(client) < 0) {
            log_error("Failed to send packets. Exit");

            // Stop main loop
            running = false;
            break;
        }

        // Cap the client tick rate
        EchoSleep(dt);
    }

    // Stop and deinitialize the client
    NBN_Client_Stop(client);

#ifdef NBN_WEBRTC_NATIVE
    NBN_WebRTC_C_Unregister();
#endif

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(0);
#else
    return 0;
#endif
}
