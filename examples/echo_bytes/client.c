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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Has to be defined in exactly *one* source file before including the nbnet header
#define NBNET_IMPL

#include "shared.h"

static bool running = true;
static bool connected = false;
static bool disconnected = false;

void OnConnected(void)
{
    Log(LOG_INFO, "Connected");

    connected = true; // Start sending messages
}

void OnDisconnected(void)
{
    Log(LOG_INFO, "Disconnected");

    // Stop the main loop
    disconnected = true;
    running = false;

    // Retrieve the server code used when closing our client connection
    if (NBN_GameClient_GetServerCloseCode() == ECHO_SERVER_BUSY_CODE)
    {
        Log(LOG_INFO, "Another client is already connected");
    }
}

void OnMessageReceived(void)
{
    // Get info about the received message
    NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();

    assert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

    // Retrieve the received message
    NBN_ByteArrayMessage *msg = (NBN_ByteArrayMessage *)msg_info.data;

    Log(LOG_INFO, "Received echo: %s (%d bytes)", msg->bytes, msg->length);

    // Destroy the received message
    NBN_ByteArrayMessage_Destroy(msg);
}

int SendEchoMessage(const char *msg)
{
    unsigned int length = strlen(msg); // Compute message length

    // Reliably send bytes to the server
    if (NBN_GameClient_SendReliableByteArray((uint8_t *)msg, length) < 0)
        return -1;

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: client MSG\n");

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    const char *msg = argv[1];

    if (strlen(msg) > NBN_BYTE_ARRAY_MAX_SIZE - 1)
    {
        Log(LOG_ERROR, "Message length cannot exceed %d. Exit", NBN_BYTE_ARRAY_MAX_SIZE - 1);

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register(); // Register the WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__

    // Initialize the client with a protocol name (must be the same than the one used by the server), the server ip address and port
    NBN_GameClient_Init(ECHO_PROTOCOL_NAME, "127.0.0.1", ECHO_EXAMPLE_PORT, false, NULL);

    if (NBN_GameClient_Start() < 0)
    {
        Log(LOG_ERROR, "Failed to start client");

// Error, quit the client application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif 
    }

    // Number of seconds between client ticks
    double dt = 1.0 / ECHO_TICK_RATE;

    while (running)
    {
        // Update client clock
        NBN_GameClient_AddTime(dt);

        int ev;

        // Poll for client events
        while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
        {
            if (ev < 0)
            {
                Log(LOG_ERROR, "An error occured while polling client events. Exit");

                // Stop main loop
                running = false;
                break;
            }

            switch (ev)
            {
                // Client is connected to the server
                case NBN_CONNECTED:
                    OnConnected();
                    break;

                    // Client has disconnected from the server
                case NBN_DISCONNECTED:
                    OnDisconnected();
                    break;

                    // A message has been received from the server
                case NBN_MESSAGE_RECEIVED:
                    OnMessageReceived();
                    break;
            }
        }

        if (disconnected)
            break;

        if (connected)
        {
            if (SendEchoMessage(msg) < 0)
            {
                Log(LOG_ERROR, "Failed to send message. Exit");

                // Stop main loop
                running = false;
                break;
            }
        }

        // Pack all enqueued messages as packets and send them
        if (NBN_GameClient_SendPackets() < 0)
        {
            Log(LOG_ERROR, "Failed to send packets. Exit");

            // Stop main loop
            running = false;
            break;
        }

        // Cap the client tick rate
        Sleep(dt);
    }

    // Stop the client
    NBN_GameClient_Stop();

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(0);
#else
    return 0;
#endif
}
