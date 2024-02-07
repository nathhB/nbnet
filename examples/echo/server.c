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
#include <stdbool.h>
#include <string.h>

// Has to be defined in exactly *one* source file before including the nbnet header
#define NBNET_IMPL

#include "shared.h"

static NBN_ConnectionHandle client = 0;

// Echo the received message
static int EchoReceivedMessage(void)
{
    // Get info about the received message
    NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

    assert(msg_info.sender == client);
    assert(msg_info.type == ECHO_MESSAGE_TYPE);

    // Retrieve the received message
    EchoMessage *msg = (EchoMessage *)msg_info.data;

    // Create an echo message
    EchoMessage *echo = EchoMessage_Create();

    // Fill it with the received message data and length
    memcpy(echo->data, msg->data, msg->length);
    echo->length = msg->length;

    // Reliably send it to the client
    // If the send fails the client will be disconnected and a NBN_CLIENT_DISCONNECTED event
    // will be received (see event polling in main)
    NBN_GameServer_SendReliableMessageTo(client, ECHO_MESSAGE_TYPE, echo);

    EchoMessage_Destroy(msg); // Destroy the received echo message

    return 0;
}

static bool error = false;

int main(int argc, const char **argv)
{
#ifdef __EMSCRIPTEN__

    // Register the WebRTC driver
#ifdef NBN_TLS 

    if (argc != 3)
    {
        printf("Usage: server CERT_PATH KEY_PATH\n");
        return 1;
    }

    const char *cert_path = argv[1];
    const char *key_path = argv[2];

    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = true, .cert_path = cert_path, .key_path = key_path});
#else
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = false});
#endif // NBN_TLS

#elif defined(NBN_WEBRTC_NATIVE)

    // Register native WebRTC driver

#ifdef NBN_TLS
    bool enable_tls = true;
#else
    bool enable_tls = false;
#endif // NBN_TLS

    const char *ice_servers[] = { "stun:stun01.sipphone.com" };
    NBN_WebRTC_C_Config cfg = {
        .ice_servers = ice_servers,
        .ice_servers_count = 1,
        .enable_tls = enable_tls,
        .cert_path = NULL,
        .key_path = NULL,
        .passphrase = NULL,
        .log_level = RTC_LOG_VERBOSE};

    NBN_WebRTC_C_Register(cfg);
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__ 

#ifdef NBN_ENCRYPTION
    bool enable_encryption = true;
#else
    bool enable_encryption = false;
#endif

    // Start the server with a protocol name, a port, and with packet encryption on or off
    if (NBN_GameServer_StartEx(ECHO_PROTOCOL_NAME, ECHO_EXAMPLE_PORT, enable_encryption) < 0)
    {
        Log(LOG_ERROR, "Failed to start the server");

        // Error, quit the server application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    // Registering messages, have to be done after NBN_GameServer_StartEx
    NBN_GameServer_RegisterMessage(ECHO_MESSAGE_TYPE,
            (NBN_MessageBuilder)EchoMessage_Create,
            (NBN_MessageDestructor)EchoMessage_Destroy,
            (NBN_MessageSerializer)EchoMessage_Serialize); 

    // Number of seconds between server ticks
    double dt = 1.0 / ECHO_TICK_RATE;

    while (true)
    {
        int ev;

        // Poll for server events
        while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
        {
            if (ev < 0)
            {
                Log(LOG_ERROR, "Something went wrong");

                // Error, quit the server application
                error = true;
                break;
            }

            switch (ev)
            {
                // New connection request...
                case NBN_NEW_CONNECTION:
                    // Echo server work with one single client at a time
                    if (client)
                    {
                        NBN_GameServer_RejectIncomingConnectionWithCode(ECHO_SERVER_BUSY_CODE);
                    }
                    else
                    {
                        NBN_GameServer_AcceptIncomingConnection();
                        client = NBN_GameServer_GetIncomingConnection();
                    }

                    break;

                    // The client has disconnected
                case NBN_CLIENT_DISCONNECTED:
                    assert(NBN_GameServer_GetDisconnectedClient() == client);

                    client = 0;
                    break;

                    // A message has been received from the client
                case NBN_CLIENT_MESSAGE_RECEIVED:
                    if (EchoReceivedMessage() < 0)
                    {
                        Log(LOG_ERROR, "Failed to echo received message");

                        // Error, quit the server application
                        error = true;
                    }
                    break;
            }
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameServer_SendPackets() < 0)
        {
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
