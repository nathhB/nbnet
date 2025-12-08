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

// Has to be defined in exactly *one* source file before including the nbnet header
#define NBNET_IMPL

#include "shared.h"

static NBN_Connection *connection = NULL;
static uint32_t conn_id;

// Echo the received message
static int EchoReceivedMessage(void) {
    // Get info about the received message
    NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

    assert(msg_info.sender->id == conn_id);
    assert(msg_info.type == ECHO_MESSAGE_TYPE);

    // read message data
    NBN_Reader *reader = NBN_GameServer_GetMessageReader();
    unsigned int length;
    int res;

    res = NBN_Reader_ReadUInt32(reader, &length);
    assert(res == 0);
    static char msg_str[ECHO_MESSAGE_MAX_LENGTH];

    res = NBN_Reader_ReadBytes(reader, (uint8_t *)msg_str, length);
    assert(res == 0);
    msg_str[length] = 0;

    Log(LOG_INFO, "Received message: %s, send echo (length: %d, channel: %d)", msg_str, msg_info.length,
        msg_info.channel_id);

    // create and send an echo of the received message
    NBN_GameServer_CreateReliableMessage(ECHO_MESSAGE_TYPE);
    NBN_Writer *writer = NBN_GameServer_GetMessageWriter();

    NBN_Writer_WriteUInt32(writer, length);
    NBN_Writer_WriteBytes(writer, (uint8_t *)msg_str, length);

    return NBN_GameServer_SendMessageTo(connection);
}

static bool error = false;

int main(int argc, const char **argv) {
#ifdef __EMSCRIPTEN__

    // Register the WebRTC driver
#ifdef NBN_TLS

    if (argc != 3) {
        printf("Usage: server CERT_PATH KEY_PATH\n");
        return 1;
    }

    const char *cert_path = argv[1];
    const char *key_path = argv[2];

    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = true, .cert_path = cert_path, .key_path = key_path});
#else
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = false});
#endif // NBN_TLS

#endif // __EMSCRIPTEN__

#ifdef NBN_WEBRTC_NATIVE

    // Register native WebRTC driver

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

#if !defined(__EMSCRIPTEN__) && !defined(NBN_WEBRTC_NATIVE)
    NBN_UDP_Register(); // Register the UDP driver
#endif

    // Start the server with a protocol name and a port

    NBN_GameServer_Config config =
        NBN_GameServer_CreateConfig(ECHO_PROTOCOL_NAME, ECHO_EXAMPLE_PORT, AllocateMessage, DeallocateMessage);

    if (NBN_GameServer_Start(config) < 0) {
        Log(LOG_ERROR, "Failed to start the server");

        // Error, quit the server application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    // Number of seconds between server ticks
    double dt = 1.0 / ECHO_TICK_RATE;

    while (true) {
        int ev;
        NBN_DisconnectionInfo disconnect_info;

        // Poll for server events
        while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT) {
            if (ev < 0) {
                Log(LOG_ERROR, "Something went wrong");

                // Error, quit the server application
                error = true;
                break;
            }

            switch (ev) {
            // New connection request...
            case NBN_NEW_CONNECTION:
                // Echo server work with one single client at a time
                if (connection) {
                    NBN_GameServer_RejectIncomingConnectionWithCode(ECHO_SERVER_BUSY_CODE);
                } else {
                    NBN_GameServer_AcceptIncomingConnection();
                    connection = NBN_GameServer_GetIncomingConnection();
                    conn_id = connection->id;
                }

                break;

                // The client has disconnected
            case NBN_CLIENT_DISCONNECTED:
                disconnect_info = NBN_GameServer_GetDisconnectionInfo();

                assert(disconnect_info.conn_id == conn_id);
                connection = NULL;
                break;

                // A message has been received from the client
            case NBN_CLIENT_MESSAGE_RECEIVED:
                if (EchoReceivedMessage() < 0) {
                    Log(LOG_ERROR, "Failed to echo received message");

                    // Error, quit the server application
                    error = true;
                }
                break;
            }
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameServer_SendPackets() < 0) {
            Log(LOG_ERROR, "Failed to send packets");

            // Error, quit the server application
            error = true;
            break;
        }

        // Cap the server tick rate
        EchoSleep(dt);
    }

    // Stop the server
    NBN_GameServer_Stop();

#ifdef NBN_WEBRTC_NATIVE
    NBN_WebRTC_C_Unregister();
#endif

    int ret = error ? 1 : 0;

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(ret);
#else
    return ret;
#endif
}
