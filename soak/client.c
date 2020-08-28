#define NBNET_IMPL

#include "soak.h"

static unsigned int sent_messages_count = 0;
static unsigned int next_msg_id = 1;
static unsigned int last_recved_message_id = 0;
static bool connected = false;
static uint8_t **messages_data;

static uint8_t *generate_random_bytes(unsigned int length)
{
    uint8_t *bytes = malloc(length);

    for (int i = 0; i < length; i++)
        bytes[i] = rand() % 255 + 1;

    return bytes;
}

static int send_messages(void)
{
    if (sent_messages_count < Soak_GetOptions().messages_count)
    {
        unsigned int count = MIN((rand() % 64) + 1, Soak_GetOptions().messages_count - sent_messages_count);

        for (int i = 0; i < count; i++)
        {
            SoakMessage *msg = NBN_GameClient_CreateMessage(SOAK_MESSAGE);

            if (msg == NULL)
            {
                Soak_LogError("Failed to create soak message");

                return -1;
            }

            msg->data_length = rand() % (SOAK_MESSAGE_MAX_DATA_LENGTH - SOAK_MESSAGE_MIN_DATA_LENGTH) + SOAK_MESSAGE_MIN_DATA_LENGTH;

            if (!NBN_GameClient_CanSendMessage(SOAK_CHAN_RELIABLE_ORDERED_1))
            {
                NBN_Message_Destroy(NBN_GameClient_GetOutgoingMessage(), true);

                return 0;
            }

            msg->id = next_msg_id++;

            uint8_t *bytes = generate_random_bytes(msg->data_length);

            messages_data[msg->id - 1] = bytes;

            memcpy(msg->data, bytes, msg->data_length);
            memcpy(messages_data[msg->id - 1], bytes, msg->data_length);

            Soak_LogInfo("Send soak message (id: %d, data length: %d)", msg->id, msg->data_length);

            if (NBN_GameClient_SendMessage(SOAK_CHAN_RELIABLE_ORDERED_1) < 0)
                return -1;

            sent_messages_count++;
        }
    }

    return 0;
}

static int handle_soak_message(SoakMessage *msg)
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

    Soak_LogInfo("Received soak message (%d/%d)", msg->id, Soak_GetOptions().messages_count);

    if (last_recved_message_id == Soak_GetOptions().messages_count)
    {
        Soak_Stop();

        return -1;
    }

    return 0;
}

static int handle_message(void)
{
    NBN_MessageInfo msg;

    NBN_GameClient_GetReceivedMessageInfo(&msg);

    switch (msg.type)
    {
    case SOAK_MESSAGE:
        if (handle_soak_message((SoakMessage *)msg.data) < 0)
            return -1;
        break;

    default:
        Soak_LogError("Received unexpected message (type: %d)", msg.type);

        return -1;
    }

    return 0;
}

static int tick(void)
{
    NBN_GameClient_AddTime(1000 / SOAK_TICK_RATE);

    NBN_GameClientEvent ev;

    while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT)
    {
        switch (ev)
        {
        case NBN_DISCONNECTED:
            connected = false;

            Soak_Stop();
            return 0;

        case NBN_CONNECTED: 
            connected = true;
            break;

        case NBN_MESSAGE_RECEIVED:
            if (handle_message() < 0)
                return -1;
            break;

        case NBN_ERROR:
            return -1;
        }
    }

    if (connected)
    {
        if (send_messages() < 0)
            return -1;
    }

    if (NBN_GameClient_Flush() < 0)
    {
        Soak_LogError("Failed to flush game client send queue. Exit");

        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    NBN_GameClient_Init(SOAK_PROTOCOL_NAME);
    
    if (Soak_Init(argc, argv) < 0)
    {
        NBN_GameClient_Stop();

        return 1;
    }

    messages_data = malloc(sizeof(uint8_t *) * Soak_GetOptions().messages_count);

    for (int i = 0; i < Soak_GetOptions().messages_count; i++)
        messages_data[i] = NULL;

    NBN_GameClient_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, Soak_Debug_PrintAddedToRecvQueue);

    if (NBN_GameClient_Start("127.0.0.1", SOAK_PORT) < 0)
    {
        Soak_LogError("Failed to start game client. Exit");

        return 1;
    } 

    int ret = Soak_MainLoop(tick);

    NBN_GameClient_Poll(); /* poll one last time to clear the events queue */
    NBN_GameClient_Stop();
    Soak_Deinit();

    for (int i = 0; i < Soak_GetOptions().messages_count; i++)
    {
        if (messages_data[i])
            free(messages_data[i]);
    }

    free(messages_data);

    return ret;
}
