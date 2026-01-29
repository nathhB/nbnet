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

#define NBN_UDP
#define NBN_IMPLEMENTATION

#include "soak.h"

#include <assert.h>

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

static bool connected = false;
static unsigned int done_channel_count = 0;

static void GenerateRandomBytes(uint8_t *data, unsigned int length) {
    for (int i = 0; i < length; i++)
        data[i] = rand() % 255 + 1;
}

static int SendSoakMessages(SoakChannel *channel, uint8_t channel_id) {
    unsigned int msg_count = channel->message_count;

    if (channel->sent_message_count < msg_count) {
        // number of messages yet to be sent
        unsigned int remaining_message_count = msg_count - channel->sent_message_count;

        // number of messages sent but not yet to be acked
        unsigned int pending_message_count = channel->last_sent_message_id - channel->last_recved_message_id;

        Soak_LogInfo("Compute number of soak messages to send (sent: %d, pending: %d, remaining: %d)",
                     channel->sent_message_count, pending_message_count, remaining_message_count);

        // don't send anything on this tick if we have reached the max number of unacked messages
        if (pending_message_count >= SOAK_CLIENT_MAX_PENDING_MESSAGES) {
            Soak_LogInfo("Max number of pending messages has been reached, not sending anything this tick");

            return 0;
        }

        // number of messages to send on this tick
        unsigned int send_message_count =
            MIN(SOAK_CLIENT_MAX_PENDING_MESSAGES - pending_message_count, remaining_message_count);

        Soak_LogInfo("Will send %d soak messages this tick", send_message_count);

        for (int i = 0; i < send_message_count; i++) {
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

            Soak_LogInfo("Send soak message (id: %d, data length: %d)", msg_id, data_length);

            // TODO: support big messages
            NBN_Writer *writer = NBN_GameClient_CreateMessage(SOAK_MESSAGE_SMALL, channel->id);

            SoakMessage_Write(writer, msg_id, entry->data, entry->length);

            if (NBN_GameClient_EnqueueMessage() < 0)
                return -1;

            channel->sent_message_count++;
            channel->last_sent_message_id = msg_id;
        }
    }

    return 0;
}

static int HandleReceivedSoakMessage(uint8_t channel_id, SoakChannel *channels) {
    SoakChannel *channel = &channels[channel_id];
    unsigned int msg_id;
    unsigned int data_length;
    static uint8_t recv_buffer[SOAK_MESSAGE_BIG_MAX_DATA_LENGTH];

    NBN_Reader *reader = NBN_GameClient_GetMessageReader();

    if (SoakMessage_Read(reader, &msg_id, recv_buffer, &data_length) < 0) {
        Soak_LogError("Failed to read soak message");

        return -1;
    }

    if (msg_id != channel->last_recved_message_id + 1) {
        Soak_LogError("Expected to receive message %d but received message %d (channel_id: %d)",
                      channel->last_recved_message_id + 1, msg_id, channel_id);

        return -1;
    }

    Soak_MessageEntry *entry = &channel->messages[(msg_id - 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES];

    assert(!entry->free);
    assert(entry->channel_id == channel_id);

    if (data_length != entry->length) {
        Soak_LogError("Expected message %d to have length %d but was %d (channel_id: %d)", msg_id, entry->length,
                      data_length);

        return -1;
    }

    if (memcmp(recv_buffer, entry->data, data_length) != 0) {
        Soak_LogError("Received invalid data for message %d (data length: %d, channel_id: %d)", msg_id, data_length,
                      channel_id);

        return -1;
    }

    entry->free = true;

    channel->last_recved_message_id = msg_id;

    SoakOptions options = Soak_GetOptions();

    Soak_LogInfo("Received soak message (length: %d, %d/%d) on channel %d", data_length, msg_id, channel->message_count,
                 channel_id);

    if (channel->last_recved_message_id == channel->message_count) {
        Soak_LogInfo("Received all soak message echoes on channel %d", channel_id);
        done_channel_count++;
    }

    if (done_channel_count >= NBN_CHANNEL_COUNT) {
        Soak_LogInfo("Received all soak message echoes on all channels");
        Soak_Stop();

        return SOAK_DONE;
    }

    return 0;
}

static int HandleReceivedMessage(SoakChannel *channels) {
    NBN_MessageInfo msg = NBN_GameClient_GetMessageInfo();

    int ret;

    if (msg.type == SOAK_MESSAGE_SMALL) {
        ret = HandleReceivedSoakMessage(msg.channel_id, channels);
    } else {
        Soak_LogError("Received unexpected message (type: %d, channel_id: %d)", msg.type, msg.channel_id);

        ret = -1;
    }

    return ret;
}

static int Tick(void *data) {
    SoakChannel *channels = (SoakChannel *)data;

    int ev;

    while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT) {
        if (ev < 0)
            return -1;

        switch (ev) {
        case NBN_DISCONNECTED:
            connected = false;

            Soak_LogInfo("Disconnected from server (code: %d)", NBN_GameClient_GetServerCloseCode());
            Soak_Stop();
            return 0;

        case NBN_CONNECTED:
            Soak_LogInfo("Connected to server");
            connected = true;
            break;

        case NBN_MESSAGE_RECEIVED:
            if (HandleReceivedMessage(channels) < 0)
                return -1;
            break;
        }
    }

    if (connected) {
        for (unsigned int c = 0; c < NBN_CHANNEL_COUNT; c++) {
            SoakChannel *channel = &channels[c];

            if (SendSoakMessages(channel, channel->id) < 0) {
                Soak_LogError("An error occured while sending messages on channel %d", c);
                return -1;
            }
        }
    }

    if (NBN_GameClient_Flush() < 0) {
        Soak_LogError("Failed to flush game client send queue. Exit");

        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    Soak_SetLogLevel(LOG_TRACE);

    if (Soak_ReadCommandLine(argc, argv) < 0)
        return -1;

    SoakOptions options = Soak_GetOptions();

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register((NBN_WebRTC_Config){.enable_tls = false}); // Register the JS WebRTC driver
#else

#ifdef WEBRTC_NATIVE

    if (options.webrtc) {
        // Register the native WebRTC driver
        const char *ice_servers[] = {"stun:stun01.sipphone.com"};
        NBN_WebRTC_Native_Config cfg = {.ice_servers = ice_servers,
                                        .ice_servers_count = 1,
                                        .enable_tls = false,
                                        .cert_path = NULL,
                                        .key_path = NULL,
                                        .passphrase = NULL,
                                        .log_level = RTC_LOG_VERBOSE};

        NBN_WebRTC_Native_Register(cfg);
    } else {
        NBN_UDP_Register();
    }

#else

#endif // WEBRTC_NATIVE

#endif // __EMSCRIPTEN__

    NBN_GameClient_Init(SOAK_PROTOCOL_NAME, "127.0.0.1", SOAK_PORT);

    if (NBN_GameClient_Start() < 0) {
        Soak_LogError("Failed to start game client. Exit");

#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    if (Soak_Init(argc, argv) < 0) {
        Soak_LogError("Failed to initialize soak test");
        return 1;
    }

    unsigned int message_count = options.message_count;
    unsigned int message_per_channel = message_count / NBN_CHANNEL_COUNT;
    unsigned int leftover_message_count = message_count % NBN_CHANNEL_COUNT;
    SoakChannel *channels = (SoakChannel *)malloc(sizeof(SoakChannel) * NBN_CHANNEL_COUNT);

    for (int c = 0; c < NBN_CHANNEL_COUNT; c++) {
        SoakChannel *channel = &channels[c];

        channel->id = c; // channels 0 and 1 are reserved by the library
        channel->next_msg_id = 1;
        channel->sent_message_count = 0;
        channel->last_recved_message_id = 0;
        channel->last_sent_message_id = 0;
        channel->message_count = message_per_channel;

        for (int i = 0; i < SOAK_CLIENT_MAX_PENDING_MESSAGES; i++) {
            channel->messages[i].free = true;
        }
    }

    channels[NBN_CHANNEL_COUNT - 1].message_count += leftover_message_count;

    NBN_GameClient_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE,
                                          (void *)Soak_Debug_PrintAddedToRecvQueue);

    int ret = Soak_MainLoop(Tick, channels);

    NBN_GameClient_Stop();
    free(channels);

#ifdef WEBRTC_NATIVE

    if (options.webrtc) {
        NBN_WebRTC_Native_Unregister();
    }

#endif // WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(ret);
#else
    return ret;
#endif
}
