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

#define NBNET_IMPL

#include "soak.h"

#ifdef __EMSCRIPTEN__
/* Use WebRTC driver */
#include "../net_drivers/webrtc.h"
#else
/* Use UDP driver */
#include "../net_drivers/udp.h"
#endif

static unsigned int sent_message_count = 0;
static unsigned int next_msg_id = 1;
static unsigned int last_recved_message_id = 0;
static unsigned int last_sent_message_id = 0;
static bool connected = false;
static uint8_t **messages_data;

static void GenerateRandomBytes(uint8_t *data, unsigned int length)
{
    for (int i = 0; i < length; i++)
        data[i] = rand() % 255 + 1;
}

static int SendSoakMessages(void)
{
    if (sent_message_count < Soak_GetOptions().message_count)
    {
        // number of messages yet to send
        unsigned int remaining_message_count = Soak_GetOptions().message_count - sent_message_count;

        // number of messages sent but not yet acked
        unsigned int pending_message_count = last_sent_message_id - last_recved_message_id;

        // don't send anything on this tick if we have reached the max number of unacked messages
        if (pending_message_count >= SOAK_CLIENT_MAX_PENDING_MESSAGES)
            return 0;

        // max number of messages to send on this tick
        unsigned int max_send_message_count = MIN(
                SOAK_CLIENT_MAX_PENDING_MESSAGES - pending_message_count, remaining_message_count);

        // number of messages to send on this tick
        unsigned int count = (rand() % max_send_message_count) + 1;

        for (int i = 0; i < count; i++)
        {
            SoakMessage *msg = NBN_GameClient_CreateReliableMessage(SOAK_MESSAGE);

            if (msg == NULL)
            {
                Soak_LogError("Failed to create soak message");

                return -1;
            }

            msg->data_length = rand() % (SOAK_MESSAGE_MAX_DATA_LENGTH - SOAK_MESSAGE_MIN_DATA_LENGTH) + SOAK_MESSAGE_MIN_DATA_LENGTH;

            if (!NBN_GameClient_CanSendMessage())
            {
                SoakMessage_Destroy(msg);

                return 0;
            }

            msg->id = next_msg_id++;

            GenerateRandomBytes(msg->data, msg->data_length);

            messages_data[msg->id - 1] = malloc(msg->data_length);
            memcpy(messages_data[msg->id - 1], msg->data, msg->data_length);

            Soak_LogInfo("Send soak message (id: %d, data length: %d)", msg->id, msg->data_length);

            if (NBN_GameClient_SendMessage() < 0)
                return -1;

            sent_message_count++;
            last_sent_message_id = msg->id;
        }
    }

    return 0;
}

static int HandleReceivedSoakMessage(SoakMessage *msg)
{
    if (msg->id != last_recved_message_id + 1)
    {
        Soak_LogError("Expected to receive message %d but received message %d", last_recved_message_id + 1, msg->id);

        return -1;
    }

    if (memcmp(msg->data, messages_data[msg->id - 1], msg->data_length) != 0)
    {
        Soak_LogError("Received invalid data for message %d (data length: %d)", msg->id, msg->data_length);

        return -1;
    }

    free(messages_data[msg->id - 1]);

    messages_data[msg->id - 1] = NULL;
    last_recved_message_id = msg->id;

    Soak_LogInfo("Received soak message (%d/%d)", msg->id, Soak_GetOptions().message_count);

    SoakMessage_Destroy(msg);

    if (last_recved_message_id == Soak_GetOptions().message_count)
    {
        Soak_LogInfo("Received all soak message echoes");
        Soak_Stop();

        return SOAK_DONE;
    }

    return 0;
}

static int HandleReceivedMessage(void)
{
    NBN_MessageInfo msg = NBN_GameClient_GetMessageInfo();

    switch (msg.type)
    {
        case SOAK_MESSAGE:
            return HandleReceivedSoakMessage((SoakMessage *)msg.data);

        default:
            Soak_LogError("Received unexpected message (type: %d)", msg.type);

            return -1;
    }

    return 0;
}

static int Tick(void)
{
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
                if (HandleReceivedMessage() < 0)
                    return -1;
                break;

            case NBN_MESSAGE_RECYCLED:
                SoakMessage_Destroy(NBN_GameClient_GetMessageInfo().data);
                break;
        }
    }

    if (connected)
    {
        if (SendSoakMessages() < 0)
            return -1;
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

    NBN_GameClient_Init(SOAK_PROTOCOL_NAME, "127.0.0.1", SOAK_PORT);

    if (Soak_Init(argc, argv) < 0)
    {
        NBN_GameClient_Deinit();

        return 1;
    }

    messages_data = malloc(sizeof(uint8_t *) * Soak_GetOptions().message_count);

    for (int i = 0; i < Soak_GetOptions().message_count; i++)
        messages_data[i] = NULL;

    NBN_GameClient_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, Soak_Debug_PrintAddedToRecvQueue);

    if (NBN_GameClient_Start() < 0)
    {
        Soak_LogError("Failed to start game client. Exit");

        NBN_GameClient_Deinit();

#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        return 1;
#endif
    } 

    int ret = Soak_MainLoop(Tick); 

    Soak_LogInfo("Soak messages created: %d", Soak_GetCreatedSoakMessageCount());
    Soak_LogInfo("Soak messages destroyed: %d", Soak_GetDestroyedSoakMessageCount());

    for (int i = 0; i < Soak_GetOptions().message_count; i++)
    {
        if (messages_data[i])
            free(messages_data[i]);
    }

    free(messages_data);

    NBN_GameClient_Stop();
    Soak_Deinit();
    NBN_GameClient_Deinit();

#ifdef __EMSCRIPTEN__
    emscripten_force_exit(ret);
#else
    return ret;
#endif
}
