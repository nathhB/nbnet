#define NBNET_IMPL

#include "../../nbnet.h"
#include "../../net_drivers/udp.h"

#include "shared.h"

int main(void)
{
    NBN_GameServer_Init(ECHO_PROTOCOL_NAME, ECHO_EXAMPLE_PORT);

    // Register messages, have to be called after NBN_GameServer_Init and before NBN_GameServer_Start
    RegisterMessages();

    if (NBN_GameServer_Start() < 0)
        return 1; // Error, quit the server application

    // Game loop
    while (true)
    {
        // Update the server clock
        NBN_GameServer_AddTime(GetElapsedSecondsSinceLastTick());

        NBN_GameServerEvent ev;

        // Poll for server events
        while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
        {
            switch (ev)
            {
            // New connection request...
            case NBN_NEW_CONNECTION:
                // either accept it
                NBN_Connection *connection = NBN_GameServer_AcceptConnection();

                // or reject it
                NBN_GameServer_RejectConnection(-1);
                break;

            // A previously accepted client has disconnected
            case NBN_CLIENT_DISCONNECTED:
                uint32_t connection_id = NBN_GameServer_GetDisconnectedClientId();

                // Do something with the disconnected client id...
                break;

            case NBN_CLIENT_MESSAGE_RECEIVED:
                NBN_MessageInfo msg_info = NBN_GameServer_GetReceivedMessageInfo();

                // Do something with received message info
                break;

            // Error, quit the server application
            case NBN_ERROR:
                return 1; 
            }
        }

        // Pack enqueued messages inside packets and send them
        if (NBN_GameServer_SendPackets() < 0)
            return 1; // Error, quit the server application
    }

    return 0;
}