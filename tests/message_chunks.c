#include <stdio.h>
#include <stdlib.h>

#include "CuTest.h"

#define NBNET_IMPL

#define NBN_LogInfo printf
#define NBN_LogTrace printf
#define NBN_LogDebug printf
#define NBN_LogError printf

#define NBN_Allocator malloc
#define NBN_Deallocator free

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

static NBN_Endpoint endpoint;

NBN_Connection *Begin(NBN_Endpoint *endpoint)
{
    NBN_Endpoint_Init(endpoint, (NBN_Config){ .protocol_name = "tests" });
    NBN_Endpoint_RegisterMessageBuilder(endpoint, (NBN_MessageBuilder)BigMessage_Create, BIG_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(endpoint, (NBN_MessageSerializer)BigMessage_Serialize, BIG_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(endpoint, (NBN_MessageDestructor)BigMessage_Destroy, BIG_MESSAGE_TYPE);

    return NBN_Endpoint_CreateConnection(endpoint, 0);
}

static void End(NBN_Connection *conn, NBN_Endpoint *endpoint)
{
    NBN_ListNode *current_node = conn->send_queue->head;

    for (int i = 0; current_node != NULL; i++)
    {
        NBN_Message *m = current_node->data;

        current_node = current_node->next;

        NBN_List_Remove(conn->send_queue, m);
    }

    NBN_Connection_Destroy(conn);
    NBN_Endpoint_Deinit(endpoint);
}

void Test_ChunksGeneration(CuTest *tc)
{
    NBN_Endpoint endpoint;
    NBN_Connection *conn = Begin(&endpoint);
    BigMessage *msg = NBN_Endpoint_CreateOutgoingMessage(&endpoint, BIG_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_RELIABLE);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg->data); i++)
        msg->data[i] = rand() % 255 + 1;

    NBN_MeasureStream m_stream;

    NBN_MeasureStream_Init(&m_stream);

    NBN_Message message = {
        .header = {.type = BIG_MESSAGE_TYPE, .channel_id = NBN_CHANNEL_RESERVED_RELIABLE},
        .serializer = (NBN_MessageSerializer)BigMessage_Serialize,
        .data = msg};
    unsigned int message_size = (NBN_Message_Measure(&message, &m_stream) - 1) / 8 + 1;
    uint8_t buffer[4096 * 8];
    NBN_WriteStream w_stream;

    NBN_WriteStream_Init(&w_stream, buffer, message_size);

    CuAssertIntEquals(tc, 0, NBN_Message_SerializeHeader(
                &message.header, (NBN_Stream *)&w_stream));
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
        NBN_MessageChunk *chunk = ((NBN_OutgoingMessageInfo *)chunk_msg->data)->data;

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

    free(r_buffer);

    End(conn, &endpoint);
}

/* TODO: add more tests for cases like missing chunks etc. */
void Test_NBN_Channel_AddChunk(CuTest *tc)
{
    NBN_Endpoint endpoint;
    NBN_Connection *conn = Begin(&endpoint);
    BigMessage *msg = NBN_Endpoint_CreateOutgoingMessage(&endpoint, BIG_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_RELIABLE);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg->data); i++)
        msg->data[i] = rand() % 255 + 1;

    NBN_Connection_EnqueueOutgoingMessage(conn);

    BigMessage *msg2 = NBN_Endpoint_CreateOutgoingMessage(&endpoint, BIG_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_RELIABLE);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg2->data); i++)
        msg2->data[i] = rand() % 255 + 1;

    NBN_Connection_EnqueueOutgoingMessage(conn);

    /* Should have generated 10 chunks */
    CuAssertIntEquals(tc, 10, conn->send_queue->count); 

    NBN_Message *msg_chunks[10];

    for (int i = 0; i < 10; i++)
    {
        NBN_Message *m = NBN_List_GetAt(conn->send_queue, i);
        NBN_MessageChunk *chunk = ((NBN_OutgoingMessageInfo *)m->data)->data;

        msg_chunks[i] = NBN_Message_Create(
                NBN_MESSAGE_CHUNK_TYPE,
                NBN_CHANNEL_RESERVED_RELIABLE,
                (NBN_MessageSerializer)NBN_MessageChunk_Serialize,
                (NBN_MessageDestructor)NBN_MessageChunk_Destroy,
                false,
                chunk);
    }

    NBN_Channel *channel = conn->channels[NBN_CHANNEL_RESERVED_RELIABLE];

    /* First message chunks */

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[0]));
    CuAssertIntEquals(tc, 0, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 1, channel->chunk_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[1]));
    CuAssertIntEquals(tc, 1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 2, channel->chunk_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[2]));
    CuAssertIntEquals(tc, 2, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 3, channel->chunk_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[3]));
    CuAssertIntEquals(tc, 3, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 4, channel->chunk_count);

    /* This is the last chunk of the first message so it should return true */
    CuAssertTrue(tc, NBN_Channel_AddChunk(channel, msg_chunks[4]));
    CuAssertIntEquals(tc, -1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 5, channel->chunk_count);

    /* Reconstruct the message so the chunks buffer gets cleared */
    NBN_Channel_ReconstructMessageFromChunks(channel, conn);

    /* Second message chunks */

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[5]));
    CuAssertIntEquals(tc, 0, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 1, channel->chunk_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[6]));
    CuAssertIntEquals(tc, 1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 2, channel->chunk_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[7]));
    CuAssertIntEquals(tc, 2, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 3, channel->chunk_count);

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[8]));
    CuAssertIntEquals(tc, 3, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 4, channel->chunk_count);

    /* This is the last chunk of the second message so it should return true */
    CuAssertTrue(tc, NBN_Channel_AddChunk(channel, msg_chunks[9]));
    CuAssertIntEquals(tc, -1, channel->last_received_chunk_id);
    CuAssertIntEquals(tc, 5, channel->chunk_count);

    End(conn, &endpoint);
}

void Test_NBN_Channel_ReconstructMessageFromChunks(CuTest *tc)
{
    NBN_Endpoint endpoint;
    NBN_Connection *conn = Begin(&endpoint);
    BigMessage *msg = NBN_Endpoint_CreateOutgoingMessage(&endpoint, BIG_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_RELIABLE);

    CuAssertPtrNotNull(tc, msg);

    /* Fill the "big message" with random bytes */
    for (int i = 0; i < sizeof(msg->data); i++)
        msg->data[i] = rand() % 255 + 1;

    CuAssertIntEquals(tc, 0, NBN_Connection_EnqueueOutgoingMessage(conn));

    /* Should have generated 5 chunks */
    CuAssertIntEquals(tc, 5, conn->send_queue->count);

    NBN_Message *msg_chunks[5];

    for (int i = 0; i < 5; i++)
    {
        NBN_Message *m = NBN_List_GetAt(conn->send_queue, i);
        NBN_MessageChunk *chunk = ((NBN_OutgoingMessageInfo *)m->data)->data;

        msg_chunks[i] = NBN_Message_Create(
                NBN_MESSAGE_CHUNK_TYPE,
                NBN_CHANNEL_RESERVED_RELIABLE,
                (NBN_MessageSerializer)NBN_MessageChunk_Serialize,
                (NBN_MessageDestructor)NBN_MessageChunk_Destroy,
                false,
                chunk);
    }

    NBN_Channel *channel = conn->channels[NBN_CHANNEL_RESERVED_RELIABLE];

    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[0]));
    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[1]));
    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[2]));
    CuAssertTrue(tc, !NBN_Channel_AddChunk(channel, msg_chunks[3]));
    CuAssertTrue(tc, NBN_Channel_AddChunk(channel, msg_chunks[4]));

    NBN_Message *r_msg = NBN_Channel_ReconstructMessageFromChunks(channel, conn);

    CuAssertIntEquals(tc, 0, r_msg->header.type);
    CuAssertIntEquals(tc, NBN_CHANNEL_RESERVED_RELIABLE, r_msg->header.channel_id);
    CuAssertIntEquals(tc, 0, memcmp(r_msg->data, msg->data, 4096));

    End(conn, &endpoint);
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
