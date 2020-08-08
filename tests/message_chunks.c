#include <stdio.h>

#include "CuTest.h"

#define NBNET_IMPL

/* nbnet logging */
#define NBN_LogInfo printf
#define NBN_LogTrace printf
#define NBN_LogDebug printf
#define NBN_LogError printf

#include "../nbnet.h"

typedef struct
{
    uint8_t data[4096];
} BigMessage;

#define BIG_MESSAGE_TYPE 0

BigMessage *BigMessage_Create()
{
    return malloc(sizeof(BigMessage));
}

int BigMessage_Serialize(BigMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_BYTES(msg->data, 4096);

    return 0;
}

void BigMessage_Destroy(BigMessage *msg)
{
    free(msg);
}

NBN_Connection *Begin(void)
{
    NBN_MessageBuilder message_builders[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageDestructor message_destructors[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageSerializer message_serializers[NBN_MAX_MESSAGE_TYPES];

    message_builders[NBN_MESSAGE_CHUNK_TYPE] = (NBN_MessageBuilder)NBN_MessageChunk_Create;
    message_builders[BIG_MESSAGE_TYPE] = (NBN_MessageBuilder)BigMessage_Create;

    message_destructors[NBN_MESSAGE_CHUNK_TYPE] = (NBN_MessageDestructor)NBN_MessageChunk_Destroy;
    message_destructors[BIG_MESSAGE_TYPE] = (NBN_MessageDestructor)BigMessage_Destroy;

    message_serializers[NBN_MESSAGE_CHUNK_TYPE] = (NBN_MessageSerializer)NBN_MessageChunk_Serialize;
    message_serializers[BIG_MESSAGE_TYPE] = (NBN_MessageSerializer)BigMessage_Serialize;

    NBN_Connection *conn = NBN_Connection_Create(0, 0, true, message_builders, message_destructors, message_serializers);

    NBN_Connection_CreateChannel(conn, NBN_CHANNEL_RELIABLE_ORDERED, 0);

    return conn;
}

static void End(NBN_Connection *conn)
{
    NBN_ListNode *current_node = conn->send_queue->head;

    for (int i = 0; current_node != NULL; i++)
    {
        NBN_Message *m = current_node->data;

        current_node = current_node->next;

        NBN_List_Remove(conn->send_queue, m);
    }

    NBN_Connection_Destroy(conn);
}

void Test_ChunksGeneration(CuTest *tc)
{
    NBN_Connection *conn = Begin();
    BigMessage *msg = NBN_Connection_CreateOutgoingMessage(conn, BIG_MESSAGE_TYPE, 0);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg->data); i++)
        msg->data[i] = rand() % 255 + 1;

    NBN_MeasureStream m_stream;

    NBN_MeasureStream_Init(&m_stream);

    unsigned int message_size = (NBN_Message_Measure(conn->message, (NBN_MessageSerializer)BigMessage_Serialize, &m_stream) - 1) / 8 + 1;
    uint8_t *buffer = malloc(message_size);
    NBN_WriteStream w_stream;

    NBN_WriteStream_Init(&w_stream, buffer, message_size);

    CuAssertIntEquals(tc, 0, NBN_Message_SerializeHeader(&conn->message->header, (NBN_Stream *)&w_stream));
    CuAssertIntEquals(tc, 0, BigMessage_Serialize(msg, (NBN_Stream *)&w_stream));

    CuAssertIntEquals(tc, 0, NBN_Connection_EnqueueOutgoingMessage(conn));

    /* Should have generated 5 chunks */
    CuAssertIntEquals(tc, 5, conn->send_queue->count); 

    /* Merging the chunks together should reconstruct the initial message */
    uint8_t *r_buffer = malloc(message_size); /* used to merge chunks together */
    NBN_ListNode *current_node = conn->send_queue->head;

    for (int i = 0; current_node != NULL; i++)
    {
        NBN_Message *chunk_msg = current_node->data;
        NBN_MessageChunk *chunk = chunk_msg->data;

        CuAssertIntEquals(tc, NBN_MESSAGE_CHUNK_TYPE, chunk_msg->header.type);
        CuAssertIntEquals(tc, i, chunk->id);
        CuAssertIntEquals(tc, 5, chunk->total);

        unsigned int cpy_size = MIN(
                message_size - (i * NBN_MESSAGE_CHUNK_SIZE),
                NBN_MESSAGE_CHUNK_SIZE);

        NBN_LogDebug("Read chunk %d (bytes: %d)", i, cpy_size);

        memcpy(r_buffer + (i * NBN_MESSAGE_CHUNK_SIZE), chunk->data, cpy_size);

        current_node = current_node->next;
    }

    CuAssertIntEquals(tc, 0, memcmp(r_buffer, buffer, message_size));

    free(buffer);
    free(r_buffer);

    End(conn);
}

/* TODO: add more tests for cases like missing chunks etc. */
void Test_NBN_Channel_AddChunk(CuTest *tc)
{
    NBN_Connection *conn = Begin();
    BigMessage *msg = NBN_Connection_CreateOutgoingMessage(conn, BIG_MESSAGE_TYPE, 0);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg->data); i++)
        msg->data[i] = rand() % 255 + 1;

    NBN_Connection_EnqueueOutgoingMessage(conn);

    BigMessage *msg2 = NBN_Connection_CreateOutgoingMessage(conn, BIG_MESSAGE_TYPE, 0);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg2->data); i++)
        msg2->data[i] = rand() % 255 + 1;

    NBN_Connection_EnqueueOutgoingMessage(conn);

    /* Should have generated 10 chunks */
    CuAssertIntEquals(tc, 10, conn->send_queue->count); 

    NBN_Message *chunk_msg1 = NBN_List_GetAt(conn->send_queue, 0);
    NBN_Message *chunk_msg2 = NBN_List_GetAt(conn->send_queue, 1);
    NBN_Message *chunk_msg3 = NBN_List_GetAt(conn->send_queue, 2);
    NBN_Message *chunk_msg4 = NBN_List_GetAt(conn->send_queue, 3);
    NBN_Message *chunk_msg5 = NBN_List_GetAt(conn->send_queue, 4);
    NBN_Message *chunk_msg6 = NBN_List_GetAt(conn->send_queue, 5);
    NBN_Message *chunk_msg7 = NBN_List_GetAt(conn->send_queue, 6);
    NBN_Message *chunk_msg8 = NBN_List_GetAt(conn->send_queue, 7);
    NBN_Message *chunk_msg9 = NBN_List_GetAt(conn->send_queue, 8);
    NBN_Message *chunk_msg10 = NBN_List_GetAt(conn->send_queue, 9);

    NBN_Channel *channel = conn->channels[0];

    /* First message chunks */

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg1));
    CuAssertIntEquals(tc, 0, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 1, channel->chunks_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg2));
    CuAssertIntEquals(tc, 1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 2, channel->chunks_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg3));
    CuAssertIntEquals(tc, 2, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 3, channel->chunks_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg4));
    CuAssertIntEquals(tc, 3, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 4, channel->chunks_count);

    /* This is the last chunk of the first message so it should return true */
    CuAssertTrue(tc, NBN_Channel_AddChunk(channel, chunk_msg5));
    CuAssertIntEquals(tc, -1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 5, channel->chunks_count);

    /* Reconstruct the message so the chunks buffer gets cleared */
    NBN_Channel_ReconstructMessageFromChunks(channel, conn);

    /* Second message chunks */

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg6));
    CuAssertIntEquals(tc, 0, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 1, channel->chunks_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg7));
    CuAssertIntEquals(tc, 1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 2, channel->chunks_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg8));
    CuAssertIntEquals(tc, 2, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 3, channel->chunks_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg9));
    CuAssertIntEquals(tc, 3, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 4, channel->chunks_count);

    /* This is the last chunk of the second message so it should return true */
    CuAssertTrue(tc, NBN_Channel_AddChunk(channel, chunk_msg10));
    CuAssertIntEquals(tc, -1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 5, channel->chunks_count);

    End(conn);
}

void Test_NBN_Channel_ReconstructMessageFromChunks(CuTest *tc)
{
    NBN_Connection *conn = Begin();
    BigMessage *msg = NBN_Connection_CreateOutgoingMessage(conn, BIG_MESSAGE_TYPE, 0);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg->data); i++)
        msg->data[i] = rand() % 255 + 1;

    NBN_Connection_EnqueueOutgoingMessage(conn);

    /* Should have generated 5 chunks */
    CuAssertIntEquals(tc, 5, conn->send_queue->count);

    NBN_Message *chunk_msg1 = NBN_List_GetAt(conn->send_queue, 0);
    NBN_Message *chunk_msg2 = NBN_List_GetAt(conn->send_queue, 1);
    NBN_Message *chunk_msg3 = NBN_List_GetAt(conn->send_queue, 2);
    NBN_Message *chunk_msg4 = NBN_List_GetAt(conn->send_queue, 3);
    NBN_Message *chunk_msg5 = NBN_List_GetAt(conn->send_queue, 4);

    NBN_Channel *channel = conn->channels[0];

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg1));
    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg2));
    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg3));
    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, chunk_msg4));
    CuAssertTrue(tc, NBN_Channel_AddChunk(channel, chunk_msg5));

    NBN_Message *r_msg = NBN_Channel_ReconstructMessageFromChunks(channel, conn);

    CuAssertIntEquals(tc, 0, r_msg->header.type);
    CuAssertIntEquals(tc, 0, r_msg->header.channel_id);
    CuAssertIntEquals(tc, 0, memcmp(r_msg->data, msg->data, 4096));

    End(conn);
}

int main(int argc, char *argv[])
{
    CuString *output = CuStringNew();
    CuSuite* suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, Test_ChunksGeneration);
    SUITE_ADD_TEST(suite, Test_NBN_Channel_AddChunk);
    SUITE_ADD_TEST(suite, Test_NBN_Channel_ReconstructMessageFromChunks);

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);

    printf("%s\n", output->buffer);

    return suite->failCount;
}
