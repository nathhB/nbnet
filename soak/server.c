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
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "nbnet.h"
#include "soak.h"
#include "log.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

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
    bool error;
    bool is_closed;
    SoakChannel *channels;
} SoakClient;

static void HandleNewConnection(void) {
    NBN_Server_AcceptIncomingConnection();

    NBN_ConnectionHandle *conn = NBN_Server_GetIncomingConnection();
    SoakClient *soak_client = (SoakClient *)malloc(sizeof(SoakClient));

    soak_client->error = false;
    soak_client->is_closed = false;
    soak_client->channels = (SoakChannel *)malloc(sizeof(SoakChannel) * SOAK_CHANNEL_COUNT);

    for (unsigned int c = 0; c < SOAK_CHANNEL_COUNT; c++) {
        SoakChannel *channel = &soak_client->channels[c];

        channel->id = 2 + c;
        channel->recved_messages_count = 0;
        channel->last_recved_message_id = 0;

        // init the soak message queue for that channel

        channel->echo_queue.head = 0;
        channel->echo_queue.tail = 0;
        channel->echo_queue.count = 0;
        memset(channel->echo_queue.messages, 0, sizeof(channel->echo_queue.messages));
    }

    conn->user_data = soak_client;

    log_info("Client has connected (ID: %llu)", conn->id);
}

static void HandleClientDisconnection(NBN_DisconnectionInfo info) {
    SoakClient *soak_client = (SoakClient *)info.user_data;

    assert(soak_client != NULL);

    log_info("Client has disconnected (ID: %lld)", info.conn_id);

    free(soak_client->channels);
    free(soak_client);
}

static void EchoReceivedSoakMessages(void) {
    NBN_Client_Iterator it = 0;
    NBN_ConnectionHandle *conn;

    while ((conn = NBN_Server_GetNextClient(&it))) {

        SoakClient *soak_client = (SoakClient *)conn->user_data;

        assert(soak_client != NULL);

        if (soak_client->is_closed)
            continue;

        for (unsigned int c = 0; c < SOAK_CHANNEL_COUNT; c++) {
            SoakChannel *channel = &soak_client->channels[c];
            int send_count = NBN_Server_GetChannelCurrentCapacity(channel->id, conn);

            // make sure that we don't exceed channel capacity
            while (channel->echo_queue.count > 0 && --send_count >= 0) {
                Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.head];
                NBN_Writer *writer =
                    NBN_Server_CreateMessage(SOAK_MESSAGE_SMALL, channel->id, conn); // TODO: support big

                if (!writer) {
                    log_error("Failed to send soak message to client %llu, closing client", conn->id);

                    if (NBN_Server_CloseClient(conn) < 0) {
                        log_error("Failed to close client %llu", conn->id);
                        abort();
                    }

                    soak_client->is_closed = true;
                    return;
                }

                SoakMessage_Write(writer, msg_entry->msg_id, msg_entry->data, msg_entry->length);

                log_info("Send soak message %d's echo (length: %d) to client %llu", msg_entry->msg_id,
                         msg_entry->length, conn->id);

                msg_entry->length = 0;

                channel->echo_queue.head = (channel->echo_queue.head + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
                channel->echo_queue.count--;
            }
        }
    }
}

static int HandleReceivedSoakMessage(NBN_Reader *reader, NBN_ConnectionHandle *sender, uint8_t channel_id) {
    SoakClient *soak_client = (SoakClient *)sender->user_data;

    assert(soak_client != NULL);

    if (soak_client->error)
        return 0;

    SoakChannel *channel = &soak_client->channels[channel_id - 2];
    unsigned int msg_id;
    unsigned int data_length;
    static uint8_t recv_buffer[SOAK_MESSAGE_BIG_MAX_DATA_LENGTH];

    if (SoakMessage_Read(reader, &msg_id, recv_buffer, &data_length) < 0) {
        log_error("Failed to read soak message");

        return -1;
    }

    if (msg_id != channel->last_recved_message_id + 1) {
        log_error("Expected to receive message %d but received message %d (from client: %d)",
                  channel->last_recved_message_id + 1, msg_id, sender);

        soak_client->error = true;

        return -1;
    }

    log_info("Received soak message %d (length: %d) from client %llu on channel %d", msg_id, data_length, sender->id,
             channel_id);

    channel->recved_messages_count++;
    channel->last_recved_message_id = msg_id;

    Soak_MessageEntry *msg_entry = &channel->echo_queue.messages[channel->echo_queue.tail];

    assert(channel->echo_queue.count < SOAK_CLIENT_MAX_PENDING_MESSAGES);
    assert(msg_entry->length == 0);

    log_info("Enqueue soak message %d's echo for client %llu on channel %d", msg_id, sender->id, channel_id);

    memcpy(msg_entry->data, recv_buffer, data_length);
    msg_entry->msg_id = msg_id;
    msg_entry->length = data_length;
    msg_entry->channel_id = channel_id;

    channel->echo_queue.tail = (channel->echo_queue.tail + 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES;
    channel->echo_queue.count++;

    return 0;
}

static void HandleReceivedMessage(void) {
    NBN_MessageInfo msg_info = NBN_Server_GetMessageInfo();
    NBN_Reader *reader = NBN_Server_ReadMessage();
    SoakClient *soak_client = (SoakClient *)msg_info.sender->user_data;

    switch (msg_info.type) {
    case SOAK_MESSAGE_SMALL:
        if (HandleReceivedSoakMessage(reader, msg_info.sender, msg_info.channel_id) < 0) {
            if (NBN_Server_CloseClient(msg_info.sender) < 0) {
                log_error("Failed to close client %llu", msg_info.sender->id);
                abort();
            }

            soak_client->is_closed = true;
        }
        break;

        // TODO: support big messages

    default:
        log_error("Received unexpected message (type: %d, channel_id: %d)", msg_info.type, msg_info.channel_id);

        if (NBN_Server_CloseClient(msg_info.sender) < 0) {
            log_error("Failed to close client %llu", msg_info.sender->id);
            abort();
        }

        soak_client->is_closed = true;
        break;
    }
}

static int Tick(void *data) {
    (void)data;

    int ev;

    while ((ev = NBN_Server_Poll()) != NBN_SERVER_NO_EVENT) {
        if (ev < 0)
            return -1;

        switch (ev) {
        case NBN_SERVER_NEW_CONNECTION:
            HandleNewConnection();
            break;

        case NBN_SERVER_DISCONNECTION:
            HandleClientDisconnection(NBN_Server_GetDisconnectionInfo());
            break;

        case NBN_SERVER_MESSAGE_RECEIVED:
            HandleReceivedMessage();
            break;
        }
    }

    EchoReceivedSoakMessages();

    if (NBN_Server_Flush() < 0) {
        log_error("Failed to flush game server send queue. Exit");

        return -1;
    }

    return 0;
}

static void SigintHandler(int dummy) { Soak_Stop(); }

int main(int argc, char *argv[]) {
    signal(SIGINT, SigintHandler);

    NBN_SetLogLevel(NBN_LOG_DEBUG);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

    SoakOptions options = Soak_GetOptions();

    NBN_Server_Init(SOAK_PROTOCOL_NAME, SOAK_PORT);

    for (uint8_t c = 0; c < SOAK_CHANNEL_COUNT; c++) {
        uint8_t channel_id =
            NBN_Server_CreateChannel(NBN_CHANNEL_RELIABLE, SOAK_CHANNEL_BUFFER_SIZE, SOAK_MAX_MESSAGE_SIZE);

        // channels 0 and 1 are the default nbnet channels
        assert(channel_id == 2 + c);
    }

    if (NBN_Server_Start()) {
        log_error("Failed to start game server");

        return 1;
    }

    if (Soak_Init(argc, argv) < 0) {
        log_error("Failed to initialize soak test");

        return 1;
    }

    int ret = Soak_MainLoop(Tick, NULL);

    NBN_Server_Stop();
    Soak_Deinit();

#ifdef WEBRTC_NATIVE
    NBN_WebRTC_Native_Unregister();
#endif

    return ret;
}
