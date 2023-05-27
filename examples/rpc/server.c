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

static NBN_Connection *client = NULL;

static bool error = false;

static void TestRPC(unsigned int param_count, NBN_RPC_Param params[NBN_RPC_MAX_PARAM_COUNT], NBN_Connection *sender)
{
    Log(LOG_INFO, "TestRPC called ! (Sender: %d)", sender->id);
    Log(LOG_INFO, "Parameter 1 (int): %d", NBN_RPC_GetInt(params, 0));
    Log(LOG_INFO, "Parameter 2 (float): %f", NBN_RPC_GetFloat(params, 1));
    Log(LOG_INFO, "Parameter 3 (bool): %d", NBN_RPC_GetBool(params, 2));

    NBN_GameServer_CallRPC(
        TEST_RPC_2_ID,
        sender,
        NBN_RPC_GetInt(params, 0) * NBN_RPC_GetFloat(params, 1),
        "Some test string");
}

int main(void)
{
#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register(); // Register the WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__

    // Initialize the server with a protocol name and a port, must be done first
#ifdef NBN_ENCRYPTION
    NBN_GameServer_Init(RPC_PROTOCOL_NAME, RPC_EXAMPLE_PORT, true);
#else
    NBN_GameServer_Init(RPC_PROTOCOL_NAME, RPC_EXAMPLE_PORT, false);
#endif

    if (NBN_GameServer_Start() < 0)
    {
        Log(LOG_ERROR, "Failed to start the server");

        // Error, quit the server application
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    int ret = NBN_GameServer_RegisterRPC(TEST_RPC_ID, TEST_RPC_SIGNATURE, TestRPC);

    assert(ret == 0);

    ret = NBN_GameServer_RegisterRPC(TEST_RPC_2_ID, TEST_RPC_2_SIGNATURE, NULL);

    assert(ret == 0);

    // Number of seconds between server ticks
    double dt = 1.0 / RPC_TICK_RATE;

    while (true)
    {
        // Update the server clock
        NBN_GameServer_AddTime(dt);

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
                    NBN_GameServer_AcceptIncomingConnection();

                    break;

                // A client has disconnected
                case NBN_CLIENT_DISCONNECTED:
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
        ExampleSleep(dt);
    }

    // Stop the server
    NBN_GameServer_Stop();

    ret = error ? 1 : 0;

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(ret);
#else
    return ret;
#endif
}
