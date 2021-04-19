/*

   Copyright (C) 2020 BIAGINI Nathan

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

#include <signal.h>

#define NBNET_IMPL

#include "soak.h"

#ifdef __EMSCRIPTEN__
/* Use WebRTC driver */
#include "../net_drivers/webrtc.h"
#else
/* Use UDP driver */
#include "../net_drivers/udp.h"
#endif

typedef struct
{
    SoakMessage *messages[SOAK_CLIENT_MAX_PENDING_MESSAGES];
    unsigned int head;
    unsigned int tail;
    unsigned int count;
} EchoMessageQueue;

typedef struct
{
    unsigned int id;
    unsigned int recved_messages_count;
    unsigned int last_recved_message_id;
    bool error;
    EchoMessageQueue echo_queue;
    NBN_Connection *connection;
} SoakClient;

static SoakClient *clients[SOAK_MAX_CLIENTS] = {NULL};
static unsigned int client_count = 0;

static void HandleNewConnection(void)
{
    if (client_count == SOAK_MAX_CLIENTS)
    {
        NBN_LogInfo("Connection rejected");

        NBN_GameServer_RejectIncomingConnectionWithCode(SOAK_SERVER_FULL_CODE);

        return;
    }

    assert(clients[client_count] == NULL);

    NBN_Connection *connection = NBN_GameServer_GetIncomingConnection();

    NBN_GameServer_AcceptIncomingConnection(NULL);

    SoakClient *soak_client = malloc(sizeof(SoakClient));

    soak_client->id = connection->id;
    soak_client->recved_messages_count = 0;
    soak_client->last_recved_message_id = 0;
    soak_client->error = false;
    soak_client->echo_queue = (EchoMessageQueue){ .messages = { NULL }, .head = 0, .tail = 0, .count = 0 };
    soak_client->connection = connection;

    clients[client_count++] = soak_client;

    Soak_LogInfo("Client has connected (ID: %d)", soak_client->id);
}

static void HandleClientDisconnection(uint32_t client_id)
{
    SoakClient *soak_client = clients[client_id];

    assert(soak_client != NULL);

    Soak_LogInfo("Client has disconnected (ID: %d)", client_id);

    free(soak_client);

    clients[client_id] = NULL;
    // client_count--;
}

static void EchoReceivedSoakMessages(void)
{
    for (int i = 0; i < client_count; i++)
    {
        SoakClient *soak_client = clients[i];

        if (soak_client == NULL)
            continue;

        while (soak_client->echo_queue.count > 0)
        {
            SoakMessage *msg = soak_client->echo_queue.messages[soak_client->echo_queue.head];

            assert(msg);

            SoakMessage *echo_msg = NBN_GameServer_CreateReliableMessage(SOAK_MESSAGE);

            if (echo_msg == NULL)
            {
                Soak_LogError("Failed to create soak message");
                NBN_GameServer_CloseClient(soak_client->connection);

                return;
            }

            echo_msg->id = msg->id;
            echo_msg->data_length = msg->data_length; 

            memcpy(echo_msg->data, msg->data, msg->data_length);

            if (!NBN_GameServer_CanSendMessageTo(soak_client->connection))
            {
                SoakMessage_Destroy(echo_msg);

                break;
            }

            Soak_LogInfo("Send soak message %d's echo to client %d", echo_msg->id, soak_client->connection->id);

            NBN_GameServer_SendMessageTo(soak_client->connection);

            SoakMessage_Destroy(msg);

            soak_client->echo_queue.messages[soak_client->echo_queue.head] = NULL;
            soak_client->echo_queue.head = (soak_client->echo_queue.head + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
            soak_client->echo_queue.count--;
        }
    }
}

static int HandleReceivedSoakMessage(SoakMessage *msg, NBN_Connection *sender)
{
    SoakClient *soak_client = clients[sender->id];

    if (soak_client->error)
        return 0;

    if (msg->id != soak_client->last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d (from client: %d)",
                soak_client->last_recved_message_id + 1, msg->id, sender->id);

        soak_client->error = true;

        return -1;
    }

    Soak_LogInfo("Received message %d from client %d", msg->id, sender->id);

    soak_client->recved_messages_count++;
    soak_client->last_recved_message_id = msg->id;

    assert(soak_client->echo_queue.count < SOAK_CLIENT_MAX_PENDING_MESSAGES);
    assert(!soak_client->echo_queue.messages[soak_client->echo_queue.tail]);

    soak_client->echo_queue.messages[soak_client->echo_queue.tail] = msg;
    soak_client->echo_queue.tail = (soak_client->echo_queue.tail + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
    soak_client->echo_queue.count++;

    return 0;
}

static void HandleReceivedMessage(void)
{
    NBN_MessageInfo msg = NBN_GameServer_GetMessageInfo();

    switch (msg.type)
    {
        case SOAK_MESSAGE:
            if (HandleReceivedSoakMessage((SoakMessage *)msg.data, msg.sender) < 0)
                NBN_GameServer_CloseClient(msg.sender);
            break;

        default:
            Soak_LogError("Received unexpected message (type: %d)", msg.type);

            NBN_GameServer_CloseClient(msg.sender);
            break;
    }
}

static int Tick(void)
{
    NBN_GameServer_AddTime(SOAK_TICK_DT);

    int ev;

    while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT)
    {
        if (ev < 0)
            return -1;

        switch (ev)
        {
            case NBN_NEW_CONNECTION:
                HandleNewConnection();
                break;

            case NBN_CLIENT_DISCONNECTED:
                HandleClientDisconnection(NBN_GameServer_GetDisconnectedClientId());
                break;

            case NBN_CLIENT_MESSAGE_RECEIVED:
                HandleReceivedMessage();
                break;

            case NBN_CLIENT_MESSAGE_RECYCLED:
                assert(NBN_GameServer_GetMessageInfo().type == SOAK_MESSAGE);

                SoakMessage_Destroy(NBN_GameServer_GetMessageInfo().data);
                break;
        }
    }

    EchoReceivedSoakMessages();

    if (NBN_GameServer_SendPackets() < 0)
    {
        Soak_LogError("Failed to flush game server send queue. Exit");

        return -1;
    }

    return 0;
}

static void SigintHandler(int dummy)
{
    Soak_LogInfo("Soak messages created: %d", Soak_GetCreatedSoakMessageCount());
    Soak_LogInfo("Soak messages destroyed: %d", Soak_GetDestroyedSoakMessageCount());

    Soak_Stop();
}

int main(int argc, char *argv[])
{
    signal(SIGINT, SigintHandler);

    Soak_SetLogLevel(LOG_TRACE);

    NBN_GameServer_Init(SOAK_PROTOCOL_NAME, SOAK_PORT);

    if (Soak_Init(argc, argv) < 0)
    {
        NBN_GameServer_Deinit();
        NBN_GameServer_Stop();

        return 1;
    }

    NBN_GameServer_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, Soak_Debug_PrintAddedToRecvQueue);

    if (NBN_GameServer_Start())
    {
        Soak_LogError("Failed to start game server");

        NBN_GameServer_Deinit();

        return 1;
    }

    int ret = Soak_MainLoop(Tick);

    NBN_GameServer_Stop();
    NBN_GameServer_Deinit();
    Soak_Deinit();

    return ret;
}
