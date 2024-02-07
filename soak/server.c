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

#include <signal.h>
#include <string.h>

#define NBNET_IMPL

#include "soak.h"

#ifdef __EMSCRIPTEN__
#include "../net_drivers/webrtc.h"
#else
#include "../net_drivers/udp.h"

#ifdef WEBRTC_NATIVE
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
    NBN_ConnectionHandle connection_handle;
    bool error;
    bool is_closed;
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

    NBN_ConnectionHandle connection_handle = NBN_GameServer_GetIncomingConnection();

    assert(clients[connection_handle - 1] == NULL);

    NBN_GameServer_AcceptIncomingConnection();

    SoakClient *soak_client = (SoakClient *)malloc(sizeof(SoakClient));
    unsigned int channel_count = Soak_GetOptions().channel_count;

    soak_client->connection_handle = connection_handle;
    soak_client->error = false;
    soak_client->is_closed = false;
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

    clients[connection_handle - 1] = soak_client;
    client_count++; 

    Soak_LogInfo("Client has connected (ID: %d)", soak_client->connection_handle);
}

static void HandleClientDisconnection(NBN_ConnectionHandle connection_handle)
{
    SoakClient *soak_client = clients[connection_handle - 1];

    assert(soak_client != NULL);

    Soak_LogInfo("Client has disconnected (ID: %d)", connection_handle);

    free(soak_client->channels);
    free(soak_client);

    clients[connection_handle - 1] = NULL;
    client_count--;
}

static void EchoReceivedSoakMessages(void)
{
    unsigned int channel_count = Soak_GetOptions().channel_count;

    for (unsigned int i = 0; i < SOAK_MAX_CLIENTS; i++)
    {
        SoakClient *soak_client = clients[i];

        if (soak_client == NULL || soak_client->is_closed)
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

                    if (NBN_GameServer_CloseClient(soak_client->connection_handle) < 0)
                    {
                        Soak_LogError("Failed to close client %d", soak_client->connection_handle);
                        abort();
                    }

                    soak_client->is_closed = true;
                    return;
                }

                echo_msg->id = msg_entry->msg->id;
                echo_msg->data_length = msg_entry->msg->data_length; 

                memcpy(echo_msg->data, msg_entry->msg->data, msg_entry->msg->data_length);

                Soak_LogInfo("Send soak message %d's echo to client %d", echo_msg->id, soak_client->connection_handle);

                if (NBN_GameServer_SendMessageTo(soak_client->connection_handle, SOAK_MESSAGE, msg_entry->channel_id, echo_msg) < 0)
                    break;

                SoakMessage_Destroy(msg_entry->msg);

                msg_entry->msg = NULL;
                channel->echo_queue.head = (channel->echo_queue.head + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
                channel->echo_queue.count--;
            }
        }
    }
}

static int HandleReceivedSoakMessage(SoakMessage *msg, NBN_ConnectionHandle sender, uint8_t channel_id)
{
    SoakClient *soak_client = clients[sender - 1];

    if (!soak_client || soak_client->error)
        return 0;

    SoakChannel *channel = &soak_client->channels[channel_id];

    if (msg->id != channel->last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d (from client: %d)",
                channel->last_recved_message_id + 1, msg->id, sender);

        soak_client->error = true;

        return -1;
    }

    Soak_LogInfo("Received soak message %d from client %d on channel %d", msg->id, sender, channel_id);

    channel->recved_messages_count++;
    channel->last_recved_message_id = msg->id;

    Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.tail];

    assert(channel->echo_queue.count < SOAK_CLIENT_MAX_PENDING_MESSAGES);
    assert(!msg_entry->msg);

    Soak_LogInfo("Enqueue soak message %d's echo for client %d on channel %d", msg->id, soak_client->connection_handle, channel_id);

    msg_entry->msg = msg;
    msg_entry->channel_id = channel_id;
    channel->echo_queue.tail = (channel->echo_queue.tail + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
    channel->echo_queue.count++;

    return 0;
}

static void HandleReceivedMessage(void)
{
    NBN_MessageInfo msg = NBN_GameServer_GetMessageInfo();
    SoakClient *soak_client = clients[msg.sender - 1];

    switch (msg.type)
    {
        case SOAK_MESSAGE:
            if (HandleReceivedSoakMessage((SoakMessage *)msg.data, msg.sender, msg.channel_id) < 0)
            {
                if (NBN_GameServer_CloseClient(msg.sender) < 0)
                {
                    Soak_LogError("Failed to close client %d", msg.sender);
                    abort();
                }

                soak_client->is_closed = true;
            }
            break;

        default:
            Soak_LogError("Received unexpected message (type: %d, channel_id: %d)", msg.type, msg.channel_id);

            if (NBN_GameServer_CloseClient(msg.sender) < 0)
            {
                Soak_LogError("Failed to close client %d", msg.sender);
                abort();
            }

            soak_client->is_closed = true;
            break;
    }
}

static int Tick(void *data)
{
    (void)data;

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
                HandleClientDisconnection(NBN_GameServer_GetDisconnectedClient());
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

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = false}); // Register JS WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__

#ifdef WEBRTC_NATIVE
    // Register native WebRTC driver
    const char *ice_servers[] = { "stun:stun01.sipphone.com" };
    NBN_WebRTC_C_Config cfg = {
        .ice_servers = ice_servers,
        .ice_servers_count = 1,
        .enable_tls = false,
        .cert_path = NULL,
        .key_path = NULL,
        .passphrase = NULL,
        .log_level = RTC_LOG_VERBOSE};

    NBN_WebRTC_C_Register(cfg);
#endif // WEBRTC_NATIVE

    if (NBN_GameServer_Start(SOAK_PROTOCOL_NAME, SOAK_PORT))
    {
        Soak_LogError("Failed to start game server");

        return 1;
    }

    if (Soak_Init(argc, argv) < 0)
    {
        Soak_LogError("Failed to initialize soak test");

        return 1;
    }

    NBN_GameServer_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, (void *)Soak_Debug_PrintAddedToRecvQueue);

    int ret = Soak_MainLoop(Tick, NULL);

    NBN_GameServer_Stop();
    Soak_Deinit();

#ifdef WEBRTC_NATIVE
    NBN_WebRTC_C_Unregister();
#endif

    return ret;
}
