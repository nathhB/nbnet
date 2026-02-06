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
#include "logging.h"

static bool running = true;
static bool connected = false;
static bool disconnected = false;

void OnConnected(void) {
    LogInfo("Connected");

    connected = true; // Start sending messages
}

void OnDisconnected(void) {
    LogInfo("Disconnected");

    // Stop the main loop
    disconnected = true;
    running = false;

    // Retrieve the server code used when closing our client connection
    if (NBN_GameClient_GetServerCloseCode() == ECHO_SERVER_BUSY_CODE) {
        LogInfo("Another client is already connected");
    }
}

void OnMessageReceived(void) {
    // Get info about the received message
    NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();

    assert(msg_info.type == ECHO_MESSAGE_TYPE);

    NBN_Reader *reader = NBN_GameClient_ReadMessage();
    unsigned int length;
    int res;

    res = NBN_Reader_ReadUInt32(reader, &length);
    assert(res == 0);
    static char msg_str[ECHO_MESSAGE_MAX_LENGTH];

    res = NBN_Reader_ReadBytes(reader, (uint8_t *)msg_str, length);
    assert(res == 0);
    msg_str[length] = 0;

    LogInfo("Received echo: %s (length: %d, channel: %d)", msg_str, msg_info.length, msg_info.channel_id);
}

int SendEcho(const char *msg) {
    NBN_Writer *writer = NBN_GameClient_CreateReliableMessage(ECHO_MESSAGE_TYPE);
    unsigned int length = strlen(msg);

    NBN_Writer_WriteUInt32(writer, length);
    NBN_Writer_WriteBytes(writer, (uint8_t *)msg, length);

    return NBN_GameClient_EnqueueMessage();
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

    InitLogging();
    SetLogLevel(NBN_LOG_DEBUG);

    const char *msg = argv[1];
    // reserve 4 bytes to write the message length in the message (see the SendEcho function)
    unsigned int msg_max_len = ECHO_MESSAGE_MAX_LENGTH - 4;

    if (strlen(msg) > msg_max_len) {
        LogError("Message length cannot exceed %d. Exit", msg_max_len);

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

#ifdef __EMSCRIPTEN__

    // Register the WebRTC driver
#ifdef NBN_TLS
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = true});
#else
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = false});
#endif // NBN_TLS

#endif // __EMSCRIPTEN__

#ifdef NBN_WEBRTC_NATIVE

#ifdef NBN_TLS
    bool enable_tls = true;
#else
    bool enable_tls = false;
#endif // NBN_TLS

    const char *ice_servers[] = {"stun:stun01.sipphone.com"};
    NBN_WebRTC_C_Config cfg = {.ice_servers = ice_servers,
                               .ice_servers_count = 1,
                               .enable_tls = enable_tls,
                               .cert_path = NULL,
                               .key_path = NULL,
                               .passphrase = NULL,
                               .log_level = RTC_LOG_VERBOSE};

    NBN_WebRTC_C_Register(cfg);

#endif // NBN_WEBRTC_NATIVE

    // Initialize the client

    // Start the client with a protocol name (must be the same than the one used by the server)
    // the server host and port

    NBN_GameClient_Init(ECHO_PROTOCOL_NAME, "127.0.0.1", ECHO_EXAMPLE_PORT);

    if (NBN_GameClient_Start() < 0) {
        LogError("Failed to start client");

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
        while ((ev = NBN_GameClient_Poll()) != NBN_CLIENT_NO_EVENT) {
            if (ev < 0) {
                LogError("An error occured while polling client events. Exit");

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
                OnDisconnected();
                break;

                // A message has been received from the server
            case NBN_CLIENT_MESSAGE_RECEIVED:
                OnMessageReceived();
                break;
            }
        }

        if (disconnected)
            break;

        if (connected) {
            if (SendEcho(msg) < 0) {
                LogError("Failed to send message. Exit");

                // Stop main loop
                running = false;
                break;
            }
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameClient_Flush() < 0) {
            LogError("Failed to send packets. Exit");

            // Stop main loop
            running = false;
            break;
        }

        // Cap the client tick rate
        EchoSleep(dt);
    }

    // Stop and deinitialize the client
    NBN_GameClient_Stop();

#ifdef NBN_WEBRTC_NATIVE
    NBN_WebRTC_C_Unregister();
#endif

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(0);
#else
    return 0;
#endif
}
