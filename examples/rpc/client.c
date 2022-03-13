/*

   Copyright (C) 2022 BIAGINI Nathan

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
}

int main(int argc, char *argv[])
{
    // Start the client with a protocol name (must be the same than the one used by the server), the server ip address
    // and port
#ifdef NBN_ENCRYPTION
    if (NBN_GameClient_Start(RPC_PROTOCOL_NAME, "127.0.0.1", RPC_EXAMPLE_PORT, true, NULL) < 0)
#else
    if (NBN_GameClient_Start(RPC_PROTOCOL_NAME, "127.0.0.1", RPC_EXAMPLE_PORT, false, NULL) < 0)
#endif
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
    double dt = 1.0 / RPC_TICK_RATE;

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
            }
        }

        if (disconnected)
            break;

        // Pack all enqueued messages as packets and send them
        if (NBN_GameClient_SendPackets() < 0)
        {
            Log(LOG_ERROR, "Failed to send packets. Exit");

            // Stop main loop
            running = false;
            break;
        }

        // Cap the client tick rate
        ExampleSleep(dt);
    }

    // Stop the client
    NBN_GameClient_Stop();

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(0);
#else
    return 0;
#endif
}
