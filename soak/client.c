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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nbnet.h"
#include "soak.h"
#include "log.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

typedef struct {
    uint8_t data[SOAK_MESSAGE_BIG_MAX_LENGTH];
    uint8_t channel_id;
    unsigned int length;
    bool free;
} Soak_MessageEntry;

typedef struct {
    uint8_t id;
    unsigned int message_count;
    unsigned int sent_message_count;
    unsigned int next_msg_id;
    unsigned int last_recved_message_id;
    unsigned int last_sent_message_id;
    Soak_MessageEntry messages[SOAK_CLIENT_MAX_PENDING_MESSAGES];
} SoakChannel;

typedef struct {
    unsigned int done_channel_count;
    SoakChannel *channels;
    NBN_Client *client;
} Soak_Client_State;

static void GenerateRandomBytes(uint8_t *data, unsigned int length) {
    for (unsigned int i = 0; i < length; i++)
        data[i] = rand() % 255 + 1;
}

static int SendSoakMessages(NBN_Client *client, SoakChannel *channel, uint8_t channel_id) {
    unsigned int msg_count = channel->message_count;

    if (channel->sent_message_count < msg_count) {
        // number of messages yet to be sent
        unsigned int remaining_message_count = msg_count - channel->sent_message_count;

        // number of messages sent but not yet to be acked
        unsigned int pending_message_count = channel->last_sent_message_id - channel->last_recved_message_id;

        log_info("Compute number of soak messages to send (sent: %d, pending: %d, remaining: %d)",
                 channel->sent_message_count, pending_message_count, remaining_message_count);

        unsigned int capacity = NBN_Client_GetChannelCurrentCapacity(client, channel->id);
        // make sure that we don't exceed channel capacity
        unsigned int max_pending_messages = (unsigned int)fmin(SOAK_CLIENT_MAX_PENDING_MESSAGES, capacity);

        // don't send anything on this tick if we have reached the max number of unacked messages
        if (pending_message_count >= max_pending_messages) {
            log_info("Max number of pending messages has been reached, not sending anything this tick");

            return 0;
        }

        // number of messages to send on this tick
        unsigned int send_message_count = fmin(max_pending_messages - pending_message_count, remaining_message_count);

        log_info("Will send %d soak messages this tick", send_message_count);

        for (unsigned int i = 0; i < send_message_count; i++) {
            int percent = rand() % 100 + 1;
            unsigned int min_len;
            unsigned int max_len;

            if (percent <= SOAK_BIG_MESSAGE_PERCENTAGE) {
                min_len = SOAK_MESSAGE_BIG_MIN_DATA_LENGTH;
                max_len = SOAK_MESSAGE_BIG_MAX_DATA_LENGTH;
            } else {
                min_len = SOAK_MESSAGE_SMALL_MIN_DATA_LENGTH;
                max_len = SOAK_MESSAGE_SMALL_MAX_DATA_LENGTH;
            }

            unsigned int data_length = rand() % (max_len - min_len) + min_len;
            unsigned int msg_id = channel->next_msg_id++;
            Soak_MessageEntry *entry = &channel->messages[(msg_id - 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES];

            assert(entry->free);

            GenerateRandomBytes(entry->data, data_length);

            // entry->msg_id = msg_id;
            entry->length = data_length;
            entry->free = false;
            entry->channel_id = channel_id;

            log_info("Send soak message (id: %d, data length: %d)", msg_id, data_length);

            // TODO: support big messages
            NBN_Writer *writer = NBN_Client_CreateMessage(client, SOAK_MESSAGE_SMALL, channel->id);

            if (!writer) {
                return -1;
            }

            SoakMessage_Write(writer, msg_id, entry->data, entry->length);

            channel->sent_message_count++;
            channel->last_sent_message_id = msg_id;
        }
    }

    return 0;
}

static int HandleReceivedSoakMessage(Soak_Client_State *state, uint8_t channel_id) {
    assert(channel_id >= 2 && channel_id < SOAK_CHANNEL_COUNT + 2);

    SoakChannel *channel = &state->channels[channel_id - 2];
    unsigned int msg_id;
    unsigned int data_length;
    static uint8_t recv_buffer[SOAK_MESSAGE_BIG_MAX_DATA_LENGTH];

    NBN_Reader *reader = NBN_Client_ReadMessage(state->client);

    if (SoakMessage_Read(reader, &msg_id, recv_buffer, &data_length) < 0) {
        log_error("Failed to read soak message");

        return -1;
    }

    if (msg_id != channel->last_recved_message_id + 1) {
        log_error("Expected to receive message %d but received message %d (channel_id: %d)",
                  channel->last_recved_message_id + 1, msg_id, channel_id);

        return -1;
    }

    Soak_MessageEntry *entry = &channel->messages[(msg_id - 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES];

    assert(!entry->free);
    assert(entry->channel_id == channel_id);

    if (data_length != entry->length) {
        log_error("Expected message %d to have length %d but was %d (channel_id: %d)", msg_id, entry->length,
                  data_length, channel_id);

        return -1;
    }

    if (memcmp(recv_buffer, entry->data, data_length) != 0) {
        log_error("Received invalid data for message %d (data length: %d, channel_id: %d)", msg_id, data_length,
                  channel_id);

        return -1;
    }

    entry->free = true;

    channel->last_recved_message_id = msg_id;

    SoakOptions options = Soak_GetOptions();

    log_info("Received soak message (length: %d, %d/%d) on channel %d", data_length, msg_id, channel->message_count,
             channel_id);

    if (channel->last_recved_message_id == channel->message_count) {
        log_info("Received all soak message echoes on channel %d", channel_id);
        state->done_channel_count++;
    }

    if (state->done_channel_count >= SOAK_CHANNEL_COUNT) {
        log_info("Received all soak message echoes on all channels");
        Soak_Stop();

        return SOAK_DONE;
    }

    return 0;
}

static int HandleReceivedMessage(Soak_Client_State *state) {
    NBN_MessageInfo msg = NBN_Client_GetMessageInfo(state->client);

    int ret;

    if (msg.type == SOAK_MESSAGE_SMALL) {
        ret = HandleReceivedSoakMessage(state, msg.channel_id);
    } else {
        log_error("Received unexpected message (type: %d, channel_id: %d)", msg.type, msg.channel_id);

        ret = -1;
    }

    return ret;
}

static int Tick(void *data) {
    Soak_Client_State *state = (Soak_Client_State *)data;

    int ev;

    while ((ev = NBN_Client_Poll(state->client)) != NBN_CLIENT_NO_EVENT) {
        if (ev < 0) {
            log_error("Error while poling client events");
            return -1;
        }

        switch (ev) {
        case NBN_CLIENT_DISCONNECTED:
            log_info("Disconnected from server (code: %d)", NBN_Client_GetServerCloseCode(state->client));
            Soak_Stop();
            return 0;

        case NBN_CLIENT_CONNECTED:
            log_info("Connected to server");
            break;

        case NBN_CLIENT_MESSAGE_RECEIVED:
            if (HandleReceivedMessage(state) < 0) {
                log_error("Error processing received message");
                return -1;
            }
            break;
        }
    }

    if (NBN_Client_IsConnected(state->client)) {
        for (unsigned int c = 0; c < SOAK_CHANNEL_COUNT; c++) {
            SoakChannel *channel = &state->channels[c];

            if (SendSoakMessages(state->client, channel, channel->id) < 0) {
                log_error("An error occured while sending messages on channel %d", c);
                return -1;
            }
        }
    }

    if (NBN_Client_Flush(state->client) < 0) {
        log_error("Failed to flush game client send queue. Exit");

        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    srand(SOAK_SEED);
    NBN_SetLogLevel(NBN_LOG_DEBUG);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

    SoakOptions options = Soak_GetOptions();

    log_info("Starting soak test client... (Packet loss: %f, Packet duplication: %f, Ping: %f, Jitter: %f)",
             options.packet_loss, options.packet_duplication, options.ping, options.jitter);

    NBN_Client *client = NBN_Client_Create(SOAK_PROTOCOL_NAME, "127.0.0.1", SOAK_PORT);

    for (uint8_t c = 0; c < SOAK_CHANNEL_COUNT; c++) {
        uint8_t channel_id =
            NBN_Client_CreateChannel(client, NBN_CHANNEL_RELIABLE, SOAK_CHANNEL_BUFFER_SIZE, SOAK_MAX_MESSAGE_SIZE);

        // channels 0 and 1 are the default nbnet channels
        assert(channel_id == 2 + c);
    }

    if (NBN_Client_Start(client) < 0) {
        log_error("Failed to start client. Exit");

#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    NBN_Client_SetPing(client, options.ping);
    NBN_Client_SetJitter(client, options.jitter);
    NBN_Client_SetPacketLoss(client, options.packet_loss);
    NBN_Client_SetPacketDuplication(client, options.packet_duplication);

    unsigned int message_count = options.message_count;
    unsigned int message_per_channel = message_count / SOAK_CHANNEL_COUNT;
    unsigned int leftover_message_count = message_count % SOAK_CHANNEL_COUNT;

    Soak_Client_State state;
    state.channels = (SoakChannel *)malloc(sizeof(SoakChannel) * SOAK_CHANNEL_COUNT);
    state.done_channel_count = 0;
    state.client = client;

    for (int c = 0; c < SOAK_CHANNEL_COUNT; c++) {
        SoakChannel *channel = &state.channels[c];

        channel->id = 2 + c; // channels 0 and 1 are the default nbnet channels
        channel->next_msg_id = 1;
        channel->sent_message_count = 0;
        channel->last_recved_message_id = 0;
        channel->last_sent_message_id = 0;
        channel->message_count = message_per_channel;

        for (int i = 0; i < SOAK_CLIENT_MAX_PENDING_MESSAGES; i++) {
            channel->messages[i].free = true;
        }
    }

    state.channels[SOAK_CHANNEL_COUNT - 1].message_count += leftover_message_count;

    int ret = Soak_MainLoop(Tick, &state);

    NBN_Client_Stop(client);
    free(state.channels);

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(ret);
#else
    return ret;
#endif
}
