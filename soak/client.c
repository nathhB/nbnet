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
    uint8_t data[SOAK_MESSAGE_MAX_DATA_LENGTH];
    uint8_t channel_id;
    unsigned int length;
    bool free;
} Soak_MessageEntry;

typedef struct
{
    unsigned int message_count;
    unsigned int sent_message_count;
    unsigned int next_msg_id;
    unsigned int last_recved_message_id;
    unsigned int last_sent_message_id;
    Soak_MessageEntry messages[SOAK_CLIENT_MAX_PENDING_MESSAGES];
} SoakChannel;

static bool connected = false;
static unsigned int done_channel_count = 0;

static void GenerateRandomBytes(uint8_t *data, unsigned int length)
{
    for (int i = 0; i < length; i++)
        data[i] = rand() % 255 + 1;
}

static int SendSoakMessages(SoakChannel *channel, uint8_t channel_id)
{
    unsigned int msg_count = channel->message_count;

    if (channel->sent_message_count < msg_count)
    {
        // number of messages yet to be sent
        unsigned int remaining_message_count = msg_count - channel->sent_message_count;

        // number of messages sent but not yet to be acked
        unsigned int pending_message_count = channel->last_sent_message_id - channel->last_recved_message_id;

        Soak_LogInfo("Compute number of soak messages to send (sent: %d, pending: %d, remaining: %d)",
                channel->sent_message_count, pending_message_count, remaining_message_count);

        // don't send anything on this tick if we have reached the max number of unacked messages
        if (pending_message_count >= SOAK_CLIENT_MAX_PENDING_MESSAGES)
        {
            Soak_LogInfo("Max number of pending messages has been reached, not sending anything this tick");

            return 0;
        }

        // number of messages to send on this tick
        unsigned int send_message_count = MIN(
                SOAK_CLIENT_MAX_PENDING_MESSAGES - pending_message_count, remaining_message_count);

        Soak_LogInfo("Will send %d soak messages this tick", send_message_count);

        for (int i = 0; i < send_message_count; i++)
        {
            SoakMessage *msg = SoakMessage_CreateOutgoing();

            if (msg == NULL)
            {
                Soak_LogError("Failed to create soak message");

                return -1;
            }

            int percent = rand() % 100 + 1;

            if (percent <= SOAK_BIG_MESSAGE_PERCENTAGE)
            {
                // chunked
                msg->data_length = rand() % (SOAK_MESSAGE_MAX_DATA_LENGTH - 1024) + 1024;
            }
            else
            {
                // not chuncked
                msg->data_length = rand() % (200 - SOAK_MESSAGE_MIN_DATA_LENGTH) + SOAK_MESSAGE_MIN_DATA_LENGTH;
            }

            msg->id = channel->next_msg_id++;

            GenerateRandomBytes(msg->data, msg->data_length);

            Soak_MessageEntry *entry = &channel->messages[(msg->id - 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES];

            assert(entry->free);

            entry->length = msg->data_length;
            entry->free = false;
            entry->channel_id = channel_id;
            memcpy(entry->data, msg->data, msg->data_length);

            Soak_LogInfo("Send soak message (id: %d, data length: %d)", msg->id, msg->data_length);

            if (NBN_GameClient_SendMessage(SOAK_MESSAGE, channel_id, msg) < 0)
                return -1;

            channel->sent_message_count++;
            channel->last_sent_message_id = msg->id;
        }
    }

    return 0;
}

static int HandleReceivedSoakMessage(SoakMessage *msg, uint8_t channel_id, SoakChannel *channels)
{
    SoakChannel *channel = &channels[channel_id];

    if (msg->id != channel->last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d (channel_id: %d)", channel->last_recved_message_id + 1, msg->id, channel_id);

        return -1;
    }

    Soak_MessageEntry *entry = &channel->messages[(msg->id - 1) % SOAK_CLIENT_MAX_PENDING_MESSAGES];

    assert(!entry->free);
    assert(entry->channel_id == channel_id);

    if (memcmp(msg->data, entry->data, msg->data_length) != 0)
    {
        Soak_LogError("Received invalid data for message %d (data length: %d, channel_id: %d)", msg->id, msg->data_length, channel_id);

        return -1;
    }

    entry->free = true;

    channel->last_recved_message_id = msg->id;

    SoakOptions options = Soak_GetOptions();
    unsigned int channel_count = options.channel_count;

    Soak_LogInfo("Received soak message (length: %d, %d/%d) on channel %d", msg->data_length, msg->id, channel->message_count, channel_id);

    SoakMessage_Destroy(msg);

    if (channel->last_recved_message_id == channel->message_count)
    {
        Soak_LogInfo("Received all soak message echoes on channel %d", channel_id);
        done_channel_count++;
    }

    if (done_channel_count >= channel_count)
    {
        Soak_LogInfo("Received all soak message echoes on all channels");
        Soak_Stop();

        return SOAK_DONE;
    }

    return 0;
}

static int HandleReceivedMessage(SoakChannel *channels)
{
    NBN_MessageInfo msg = NBN_GameClient_GetMessageInfo();

    switch (msg.type)
    {
        case SOAK_MESSAGE:
            return HandleReceivedSoakMessage((SoakMessage *)msg.data, msg.channel_id, channels);

        default:
            Soak_LogError("Received unexpected message (type: %d, channel_id: %d)", msg.type, msg.channel_id);

            return -1;
    }

    return 0;
}

static int Tick(void *data)
{
    SoakChannel *channels = data;
    NBN_GameClient_AddTime(SOAK_TICK_DT);

    int ev;

    while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
    {
        if (ev < 0)
            return -1;

        switch (ev)
        {
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

    if (connected)
    {
        unsigned int channel_count = Soak_GetOptions().channel_count;

        for (unsigned int c = 0; c < channel_count; c++)
        {
            SoakChannel *channel = &channels[c];

            if (SendSoakMessages(channel, c) < 0)
            {
                Soak_LogError("An error occured while sending messages on channel %d", c);
                return -1;
            }
        }
    }

    if (NBN_GameClient_SendPackets() < 0)
    {
        Soak_LogError("Failed to flush game client send queue. Exit");

        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    Soak_SetLogLevel(LOG_TRACE);

#ifdef __EMSCRIPTEN__
    NBN_WebRTC_Register(); // Register the WebRTC driver
#else
    NBN_UDP_Register(); // Register the UDP driver
#endif // __EMSCRIPTEN__ 

    NBN_GameClient_Init(SOAK_PROTOCOL_NAME, "127.0.0.1", SOAK_PORT, false, NULL);

    if (Soak_Init(argc, argv) < 0)
    {
        Soak_LogError("Failed to initialize soak test");
        return 1;
    } 

    SoakOptions options = Soak_GetOptions();
    unsigned int channel_count = options.channel_count;
    unsigned int message_count = options.message_count;
    unsigned int message_per_channel = message_count / channel_count;
    unsigned int leftover_message_count = message_count % channel_count;
    SoakChannel *channels = malloc(sizeof(SoakChannel) * channel_count);

    if (NBN_GameClient_Start() < 0)
    {
        Soak_LogError("Failed to start game client. Exit");

#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    }

    for (int c = 0; c < channel_count; c++)
    {
        SoakChannel *channel = &channels[c];
        
        channel->next_msg_id = 1;
        channel->sent_message_count = 0;
        channel->last_recved_message_id = 0;
        channel->last_sent_message_id = 0;
        channel->message_count = message_per_channel;

        for (int i = 0; i < SOAK_CLIENT_MAX_PENDING_MESSAGES; i++)
        {
            channel->messages[i].free = true;
        }
    }

    channels[channel_count - 1].message_count += leftover_message_count;

    NBN_GameClient_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, (void *)Soak_Debug_PrintAddedToRecvQueue); 

    int ret = Soak_MainLoop(Tick, channels);

    NBN_GameClient_Stop();
    free(channels);

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
        return 1;
    }

    if (created_incoming_message_count != destroyed_incoming_message_count)
    {
        Soak_LogError("created_incoming_message_count != destroyed_incoming_message_count (potential memory leak !)");
        return 1;
    }

    Soak_LogInfo("No memory leak detected! Cool... cool cool cool");
    Soak_Deinit();

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(ret);
#else
    return ret;
#endif
}
