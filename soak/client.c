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
        unsigned int count = (rand() % MIN(50, Soak_GetOptions().messages_count - sent_messages_count)) + 1;

        for (int i = 0; i < count; i++)
        {
            SoakMessage *msg = NBN_GameClient_CreateMessage(SOAK_MESSAGE, SOAK_CHAN_RELIABLE_ORDERED_1);

            if (msg == NULL)
            {
                log_error("Failed to create soak message");

                return -1;
            }

            msg->id = next_msg_id++;
            msg->data_length = rand() % SOAK_MESSAGE_MAX_DATA_LENGTH + 1;

            uint8_t *bytes = generate_random_bytes(msg->data_length);

            messages_data[msg->id - 1] = malloc(msg->data_length);

            memcpy(msg->data, bytes, msg->data_length);
            memcpy(messages_data[msg->id - 1], msg->data, msg->data_length);
            free(bytes); // TODO: use a message destructor

            sent_messages_count++;

            NBN_GameClient_EnqueueMessage();

            log_info("Soak message sent (id: %d, data length: %d)", msg->id, msg->data_length);
        }
    }

    return 0;
}

static int handle_soak_message(SoakMessage *msg)
{
    if (msg->id != last_recved_message_id + 1)
    {
        log_error("Expected to receive message %d but received message %d", last_recved_message_id + 1, msg->id);

        return -1;
    }

    if (memcmp(msg->data, messages_data[msg->id - 1], msg->data_length) != 0)
    {
        log_error("Received invalid data for message %d (data length: %d)", msg->id, msg->data_length);

        return -1;
    }

    free(messages_data[msg->id - 1]);

    messages_data[msg->id - 1] = NULL;
    last_recved_message_id = msg->id;

    log_info("Received soak message (%d/%d)", msg->id, Soak_GetOptions().messages_count);

    if (last_recved_message_id == Soak_GetOptions().messages_count)
    {
        Soak_Stop();

        return -1;
    }

    return 0;
}

static int handle_message(void)
{
    NBN_Message msg;

    NBN_GameClient_ReadReceivedMessage(&msg);

    switch (msg.header.type)
    {
    case SOAK_MESSAGE:
        if (handle_soak_message((SoakMessage *)msg.data) < 0)
            return -1;
        break;

    default:
        log_error("Received unexpected message (type: %d)", msg.header.type);

        return -1;
    }

    NBN_GameClient_RecycleMessage(&msg);

    return 0;
}

static int tick(void)
{
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
        }
    }

    if (connected)
    {
        if (send_messages() < 0)
            return -1;
    }

    if (NBN_GameClient_Flush() < 0)
    {
        log_error("Failed to flush game client send queue. Exit");

        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (Soak_ReadCommandLine(argc, argv) < 0)
        return 1;

    messages_data = malloc(sizeof(uint8_t *) * Soak_GetOptions().messages_count);

    for (int i = 0; i < Soak_GetOptions().messages_count; i++)
        messages_data[i] = NULL;

    NBN_GameClient_Init(SOAK_PROTOCOL_NAME);
    Soak_Init();

    NBN_GameClient_Debug_SetMinPacketLossRatio(Soak_GetOptions().min_packet_loss);
    NBN_GameClient_Debug_SetMaxPacketLossRatio(Soak_GetOptions().min_packet_loss);
    NBN_GameClient_Debug_SetPacketDuplicationRatio(Soak_GetOptions().packet_duplication);
    NBN_GameClient_Debug_SetPing(Soak_GetOptions().ping);
    NBN_GameClient_Debug_SetJitter(Soak_GetOptions().jitter);
    NBN_GameClient_Debug_RegisterCallback(NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE, Soak_Debug_PrintAddedToRecvQueue);

    if (NBN_GameClient_Start("127.0.0.1", SOAK_PORT) < 0)
    {
        log_error("Failed to start game client. Exit");

        return 1;
    }

    int ret = Soak_MainLoop(tick);

    NBN_GameClient_Stop();

    for (int i = 0; i < Soak_GetOptions().messages_count; i++)
    {
        if (messages_data[i])
            free(messages_data[i]);
    }

    free(messages_data);

    return ret;
}
