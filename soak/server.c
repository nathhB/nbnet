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

#include <signal.h>
#include <string.h>

#define NBNET_IMPL

#include "soak.h"

#ifdef __EMSCRIPTEN__
#include "../net_drivers/webrtc.h"
#else
#include "../net_drivers/udp.h"

#ifdef SOAK_WEBRTC_C_DRIVER
#include "../net_drivers/webrtc_c.h"
#endif

#endif // __EMSCRIPTEN__

typedef struct
{
    uint8_t channel_id;
    SoakMessage *msg;
} Soak_MessageEntry;

typedef struct
{
    unsigned int head;
    unsigned int tail;
    unsigned int count;
    Soak_MessageEntry messages[SOAK_CLIENT_MAX_PENDING_MESSAGES];
} EchoMessageQueue;

typedef struct
{
    unsigned int recved_messages_count;
    unsigned int last_recved_message_id;
    EchoMessageQueue echo_queue;
} SoakChannel;

typedef struct
{
    unsigned int id; 
    bool error;
    NBN_Connection *connection;
    SoakChannel *channels;
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

    NBN_Connection *connection = NBN_GameServer_GetIncomingConnection();

    assert(clients[connection->id] == NULL);

    NBN_GameServer_AcceptIncomingConnection();

    SoakClient *soak_client = (SoakClient *)malloc(sizeof(SoakClient));
    unsigned int channel_count = Soak_GetOptions().channel_count;

    soak_client->id = connection->id; 
    soak_client->error = false;
    soak_client->connection = connection;
    soak_client->channels = (SoakChannel *)malloc(sizeof(SoakChannel) * channel_count);

    for (unsigned int i = 0; i < channel_count; i++)
    {
        SoakChannel *channel = &soak_client->channels[i];

        channel->recved_messages_count = 0;
        channel->last_recved_message_id = 0;

        // init the soak message queue for that channel

        channel->echo_queue.head = 0;
        channel->echo_queue.tail = 0;
        channel->echo_queue.count = 0;
        memset(channel->echo_queue.messages, 0, sizeof(channel->echo_queue.messages));
    }

    clients[connection->id] = soak_client;
    client_count++; 

    Soak_LogInfo("Client has connected (ID: %d)", soak_client->id);
}

static void HandleClientDisconnection(uint32_t client_id)
{
    SoakClient *soak_client = clients[client_id];

    assert(soak_client != NULL);

    Soak_LogInfo("Client has disconnected (ID: %d)", client_id);

    free(soak_client->channels);
    free(soak_client);

    clients[client_id] = NULL;
    // client_count--;
}

static void EchoReceivedSoakMessages(void)
{
    unsigned int channel_count = Soak_GetOptions().channel_count;

    for (unsigned int i = 0; i < SOAK_MAX_CLIENTS; i++)
    {
        SoakClient *soak_client = clients[i];

        if (soak_client == NULL || soak_client->connection->is_closed)
            continue;

        for (unsigned int c = 0; c < channel_count; c++)
        {
            SoakChannel *channel = &soak_client->channels[c];

            while (channel->echo_queue.count > 0)
            {
                Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.head];

                assert(msg_entry->msg);
                assert(!msg_entry->msg->outgoing);

                SoakMessage *echo_msg = SoakMessage_CreateOutgoing();

                if (echo_msg == NULL)
                {
                    Soak_LogError("Failed to create soak message");
                    NBN_GameServer_CloseClient(soak_client->connection);

                    return;
                }

                echo_msg->id = msg_entry->msg->id;
                echo_msg->data_length = msg_entry->msg->data_length; 

                memcpy(echo_msg->data, msg_entry->msg->data, msg_entry->msg->data_length);

                Soak_LogInfo("Send soak message %d's echo to client %d", echo_msg->id, soak_client->connection->id);

                if (NBN_GameServer_SendMessageTo(soak_client->connection, SOAK_MESSAGE, msg_entry->channel_id, echo_msg) < 0)
                    break;

                SoakMessage_Destroy(msg_entry->msg);

                msg_entry->msg = NULL;
                channel->echo_queue.head = (channel->echo_queue.head + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
                channel->echo_queue.count--;
            }
        }
    }
}

static int HandleReceivedSoakMessage(SoakMessage *msg, NBN_Connection *sender, uint8_t channel_id)
{
    SoakClient *soak_client = clients[sender->id];

    if (!soak_client || soak_client->error)
        return 0;

    SoakChannel *channel = &soak_client->channels[channel_id];

    if (msg->id != channel->last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d (from client: %d)",
                channel->last_recved_message_id + 1, msg->id, sender->id);

        soak_client->error = true;

        return -1;
    }

    Soak_LogInfo("Received soak message %d from client %d on channel %d", msg->id, sender->id, channel_id);

    channel->recved_messages_count++;
    channel->last_recved_message_id = msg->id;

    Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.tail];

    assert(channel->echo_queue.count < SOAK_CLIENT_MAX_PENDING_MESSAGES);
    assert(!msg_entry->msg);

    Soak_LogInfo("Enqueue soak message %d's echo for client on channel %d", msg->id, soak_client->connection->id, channel_id);

    msg_entry->msg = msg;
    msg_entry->channel_id = channel_id;
    channel->echo_queue.tail = (channel->echo_queue.tail + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
    channel->echo_queue.count++;

    return 0;
}

static void HandleReceivedMessage(void)
{
    NBN_MessageInfo msg = NBN_GameServer_GetMessageInfo();

    switch (msg.type)
    {
        case SOAK_MESSAGE:
            if (HandleReceivedSoakMessage((SoakMessage *)msg.data, msg.sender, msg.channel_id) < 0)
                NBN_GameServer_CloseClient(msg.sender);
            break;

        default:
            Soak_LogError("Received unexpected message (type: %d, channel_id: %d)", msg.type, msg.channel_id);

            NBN_GameServer_CloseClient(msg.sender);
            break;
    }
}

static int Tick(void *data)
{
    (void)data;
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
                HandleClientDisconnection(NBN_GameServer_GetDisconnectedClient()->id);
                break;

            case NBN_CLIENT_MESSAGE_RECEIVED:
                HandleReceivedMessage();
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
    unsigned int created_outgoing_message_count = Soak_GetCreatedOutgoingSoakMessageCount();
    unsigned int destroyed_outgoing_message_count = Soak_GetDestroyedOutgoingSoakMessageCount();
    unsigned int created_incoming_message_count = Soak_GetCreatedIncomingSoakMessageCount();
    unsigned int destroyed_incoming_message_count = Soak_GetDestroyedIncomingSoakMessageCount();

    Soak_LogInfo("Outgoing soak messages created: %d", created_outgoing_message_count);
    Soak_LogInfo("Outgoing soak messages destroyed: %d", destroyed_outgoing_message_count);
    Soak_LogInfo("Incoming soak messages created: %d", created_incoming_message_count);
    Soak_LogInfo("Incoming soak messages destroyed: %d", destroyed_incoming_message_count);

    if (created_outgoing_message_count != destroyed_outgoing_message_count)
    {
        Soak_LogError("created_outgoing_message_count != destroyed_outgoing_message_count (potential memory leak !)");
    }
    else if (created_incoming_message_count != destroyed_incoming_message_count)
    {
        Soak_LogError("created_incoming_message_count != destroyed_incoming_message_count (potential memory leak !)");
    }
    else
    {
        Soak_LogInfo("No memory leak detected! Cool... cool cool cool");
    }

    Soak_Stop();
}

int main(int argc, char *argv[])
{
    signal(SIGINT, SigintHandler);

    Soak_SetLogLevel(LOG_TRACE);

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register(); // Register the WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver

#ifdef SOAK_WEBRTC_C_DRIVER
    NBN_WebRTC_C_Register(); // Register native WebRTC driver
#endif

#endif // __EMSCRIPTEN__ 

    NBN_GameServer_Init(SOAK_PROTOCOL_NAME, SOAK_PORT, false); 

    if (Soak_Init(argc, argv) < 0)
    {
        Soak_LogError("Failed to initialize soak test");

        return 1;
    }

    NBN_GameServer_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, (void *)Soak_Debug_PrintAddedToRecvQueue); 

    if (NBN_GameServer_Start())
    {
        Soak_LogError("Failed to start game server");

        return 1;
    }

    int ret = Soak_MainLoop(Tick, NULL);

    NBN_GameServer_Stop();
    Soak_Deinit();

    return ret;
}
