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

typedef struct {
    uint8_t channel_id;
    unsigned int msg_id;
    uint8_t data[SOAK_MESSAGE_BIG_MAX_LENGTH];
    unsigned int length;
} Soak_MessageEntry;

typedef struct {
    unsigned int head;
    unsigned int tail;
    unsigned int count;
    Soak_MessageEntry messages[SOAK_CLIENT_MAX_PENDING_MESSAGES];
} EchoMessageQueue;

typedef struct {
    uint8_t id;
    unsigned int recved_messages_count;
    unsigned int last_recved_message_id;
    EchoMessageQueue echo_queue;
} SoakChannel;

typedef struct {
    NBN_Connection *conn;
    bool error;
    bool is_closed;
    SoakChannel *channels;
} SoakClient;

static SoakClient *clients[SOAK_MAX_CLIENTS] = {NULL};
static unsigned int client_count = 0;

static void HandleNewConnection(void) {
    if (client_count == SOAK_MAX_CLIENTS) {
        NBN_LogInfo("Connection rejected");

        NBN_GameServer_RejectIncomingConnectionWithCode(SOAK_SERVER_FULL_CODE);

        return;
    }

    NBN_Connection *conn = NBN_GameServer_GetIncomingConnection();

    assert(clients[conn->id - 1] == NULL);

    NBN_GameServer_AcceptIncomingConnection();

    SoakClient *soak_client = (SoakClient *)malloc(sizeof(SoakClient));

    soak_client->conn = conn;
    soak_client->error = false;
    soak_client->is_closed = false;
    soak_client->channels = (SoakChannel *)malloc(sizeof(SoakChannel) * NBN_CHANNEL_COUNT);

    for (unsigned int c = 0; c < NBN_CHANNEL_COUNT; c++) {
        SoakChannel *channel = &soak_client->channels[c];

        channel->id = c;
        channel->recved_messages_count = 0;
        channel->last_recved_message_id = 0;

        // init the soak message queue for that channel

        channel->echo_queue.head = 0;
        channel->echo_queue.tail = 0;
        channel->echo_queue.count = 0;
        memset(channel->echo_queue.messages, 0, sizeof(channel->echo_queue.messages));
    }

    clients[conn->id - 1] = soak_client;
    client_count++;

    Soak_LogInfo("Client has connected (ID: %d)", soak_client->conn->id);
}

static void HandleClientDisconnection(NBN_DisconnectionInfo info) {
    SoakClient *soak_client = clients[info.conn_id - 1];

    assert(soak_client != NULL);

    Soak_LogInfo("Client has disconnected (ID: %d)", info.conn_id);

    free(soak_client->channels);
    free(soak_client);

    clients[info.conn_id - 1] = NULL;
    client_count--;
}

static void EchoReceivedSoakMessages(void) {
    for (unsigned int i = 0; i < SOAK_MAX_CLIENTS; i++) {
        SoakClient *soak_client = clients[i];

        if (soak_client == NULL || soak_client->is_closed)
            continue;

        for (unsigned int c = 0; c < NBN_CHANNEL_COUNT; c++) {
            SoakChannel *channel = &soak_client->channels[c];

            while (channel->echo_queue.count > 0) {
                Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.head];
                NBN_Writer *writer = NBN_GameServer_CreateMessage(SOAK_MESSAGE_SMALL, channel->id); // TODO: support big

                SoakMessage_Write(writer, msg_entry->msg_id, msg_entry->data, msg_entry->length);

                Soak_LogInfo("Send soak message %d's echo (length: %d) to client %d", msg_entry->msg_id,
                             msg_entry->length, soak_client->conn->id);

                if (NBN_GameServer_EnqueueMessageFor(soak_client->conn) < 0) {
                    Soak_LogError("Failed to send soak message to client %d, closing client", soak_client->conn->id);

                    if (NBN_GameServer_CloseClient(soak_client->conn) < 0) {
                        Soak_LogError("Failed to close client %d", soak_client->conn->id);
                        abort();
                    }

                    soak_client->is_closed = true;
                    return;
                }

                msg_entry->length = 0;

                channel->echo_queue.head = (channel->echo_queue.head + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
                channel->echo_queue.count--;
            }
        }
    }
}

static int HandleReceivedSoakMessage(NBN_Reader *reader, NBN_Connection *sender, uint8_t channel_id) {
    SoakClient *soak_client = clients[sender->id - 1];

    if (!soak_client || soak_client->error)
        return 0;

    SoakChannel *channel = &soak_client->channels[channel_id];

    unsigned int msg_id;
    unsigned int data_length;
    static uint8_t recv_buffer[SOAK_MESSAGE_BIG_MAX_DATA_LENGTH];

    if (SoakMessage_Read(reader, &msg_id, recv_buffer, &data_length) < 0) {
        Soak_LogError("Failed to read soak message");

        return -1;
    }

    if (msg_id != channel->last_recved_message_id + 1) {
        Soak_LogError("Expected to receive message %d but received message %d (from client: %d)",
                      channel->last_recved_message_id + 1, msg_id, sender);

        soak_client->error = true;

        return -1;
    }

    Soak_LogInfo("Received soak message %d (length: %d) from client %d on channel %d", msg_id, data_length, sender->id,
                 channel_id);

    channel->recved_messages_count++;
    channel->last_recved_message_id = msg_id;

    Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.tail];

    assert(channel->echo_queue.count < SOAK_CLIENT_MAX_PENDING_MESSAGES);
    assert(msg_entry->length == 0);

    Soak_LogInfo("Enqueue soak message %d's echo for client %d on channel %d", msg_id, soak_client->conn->id,
                 channel_id);

    memcpy(msg_entry->data, recv_buffer, data_length);
    msg_entry->msg_id = msg_id;
    msg_entry->length = data_length;
    msg_entry->channel_id = channel_id;

    channel->echo_queue.tail = (channel->echo_queue.tail + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
    channel->echo_queue.count++;

    return 0;
}

static void HandleReceivedMessage(void) {
    NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();
    NBN_Reader *reader = NBN_GameServer_GetMessageReader();
    SoakClient *soak_client = clients[msg_info.sender->id - 1];

    switch (msg_info.type) {
    case SOAK_MESSAGE_SMALL:
        if (HandleReceivedSoakMessage(reader, msg_info.sender, msg_info.channel_id) < 0) {
            if (NBN_GameServer_CloseClient(msg_info.sender) < 0) {
                Soak_LogError("Failed to close client %d", msg_info.sender->id);
                abort();
            }

            soak_client->is_closed = true;
        }
        break;

        // TODO: support big messages

    default:
        Soak_LogError("Received unexpected message (type: %d, channel_id: %d)", msg_info.type, msg_info.channel_id);

        if (NBN_GameServer_CloseClient(msg_info.sender) < 0) {
            Soak_LogError("Failed to close client %d", msg_info.sender->id);
            abort();
        }

        soak_client->is_closed = true;
        break;
    }
}

static int Tick(void *data) {
    (void)data;

    int ev;

    while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT) {
        if (ev < 0)
            return -1;

        switch (ev) {
        case NBN_NEW_CONNECTION:
            HandleNewConnection();
            break;

        case NBN_CLIENT_DISCONNECTED:
            HandleClientDisconnection(NBN_GameServer_GetDisconnectionInfo());
            break;

        case NBN_CLIENT_MESSAGE_RECEIVED:
            HandleReceivedMessage();
            break;
        }
    }

    EchoReceivedSoakMessages();

    if (NBN_GameServer_Flush() < 0) {
        Soak_LogError("Failed to flush game server send queue. Exit");

        return -1;
    }

    return 0;
}

static void SigintHandler(int dummy) { Soak_Stop(); }

int main(int argc, char *argv[]) {
    signal(SIGINT, SigintHandler);

    Soak_SetLogLevel(LOG_DEBUG);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = false}); // Register JS WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__

#ifdef WEBRTC_NATIVE
    // Register native WebRTC driver
    const char *ice_servers[] = {"stun:stun01.sipphone.com"};
    NBN_WebRTC_C_Config cfg = {.ice_servers = ice_servers,
                               .ice_servers_count = 1,
                               .enable_tls = false,
                               .cert_path = NULL,
                               .key_path = NULL,
                               .passphrase = NULL,
                               .log_level = RTC_LOG_VERBOSE};

    NBN_WebRTC_C_Register(cfg);
#endif // WEBRTC_NATIVE

    SoakOptions options = Soak_GetOptions();

    NBN_GameServer_Init(SOAK_PROTOCOL_NAME, SOAK_PORT);

    if (NBN_GameServer_Start()) {
        Soak_LogError("Failed to start game server");

        return 1;
    }

    if (Soak_Init(argc, argv) < 0) {
        Soak_LogError("Failed to initialize soak test");

        return 1;
    }

    NBN_GameServer_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE,
                                          (void *)Soak_Debug_PrintAddedToRecvQueue);

    int ret = Soak_MainLoop(Tick, NULL);

    NBN_GameServer_Stop();
    Soak_Deinit();

#ifdef WEBRTC_NATIVE
    NBN_WebRTC_C_Unregister();
#endif

    return ret;
}
