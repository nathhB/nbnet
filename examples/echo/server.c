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
#include <assert.h>
#include "shared.h"
#include "log.h"

static NBN_ConnectionHandle *connection = NULL;
static NBN_Connection_ID conn_id;

// Echo the received message
static int EchoReceivedMessage(NBN_Server *server) {
    // Get info about the received message
    NBN_MessageInfo msg_info = NBN_Server_GetMessageInfo(server);

    assert(msg_info.type == ECHO_MESSAGE_TYPE);

    log_info("Received message of type %d from %lld", msg_info.type, msg_info.sender->id);

    assert(msg_info.sender->id == conn_id);

    // read message data
    NBN_Reader *reader = NBN_Server_ReadMessage(server);
    unsigned int length;
    int res;

    res = NBN_Reader_ReadUInt32(reader, &length);
    assert(res == 0);
    static char msg_str[ECHO_MESSAGE_MAX_LENGTH];

    res = NBN_Reader_ReadBytes(reader, (uint8_t *)msg_str, length);
    assert(res == 0);
    msg_str[length] = 0;

    log_info("Received message: %s, send echo (length: %d, channel: %d)", msg_str, msg_info.length,
             msg_info.channel_id);

    // create and send an echo of the received message
    NBN_Writer *writer = NBN_Server_CreateReliableMessage(server, ECHO_MESSAGE_TYPE, connection);

    if (!writer) {
        return -1;
    }

    NBN_Writer_WriteUInt32(writer, length);
    NBN_Writer_WriteBytes(writer, (uint8_t *)msg_str, length);

    return 0;
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

    // Start the server with a protocol name and a port

    NBN_Server *server = NBN_Server_Create(ECHO_PROTOCOL_NAME, ECHO_EXAMPLE_PORT);

    if (NBN_Server_Start(server) < 0) {
        log_error("Failed to start the server");

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
        while ((ev = NBN_Server_Poll(server)) != NBN_SERVER_NO_EVENT) {
            if (ev < 0) {
                log_error("Something went wrong");

                // Error, quit the server application
                error = true;
                break;
            }

            switch (ev) {
            // New connection request...
            case NBN_SERVER_NEW_CONNECTION:
                // Echo server work with one single client at a time
                if (connection) {
                    NBN_Server_RejectIncomingConnectionWithCode(server, ECHO_SERVER_BUSY_CODE);
                } else {
                    NBN_Server_AcceptIncomingConnection(server);
                    connection = NBN_Server_GetIncomingConnection(server);
                    conn_id = connection->id;
                }

                break;

                // The client has disconnected
            case NBN_SERVER_DISCONNECTION:
                disconnect_info = NBN_Server_GetDisconnectionInfo(server);

                assert(disconnect_info.conn_id == conn_id);
                connection = NULL;
                break;

                // A message has been received from the client
            case NBN_SERVER_MESSAGE_RECEIVED:
                if (EchoReceivedMessage(server) < 0) {
                    log_error("Failed to echo received message");

                    // Error, quit the server application
                    error = true;
                }
                break;
            }
        }

        if (error) {
            break;
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_Server_Flush(server) < 0) {
            log_error("Failed to send packets");

            // Error, quit the server application
            error = true;
            break;
        }

        // Cap the server tick rate
        EchoSleep(dt);
    }

    // Stop the server
    NBN_Server_Stop(server);

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
