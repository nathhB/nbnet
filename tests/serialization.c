#include <stdio.h>
#include <stdlib.h>

#include "CuTest.h"

#define NBNET_IMPL

#define NBN_LogInfo printf
#define NBN_LogTrace printf
#define NBN_LogDebug printf
#define NBN_LogError printf
#define NBN_LogWarning printf

#define NBN_Allocator malloc
#define NBN_Deallocator free

#include "../nbnet.h"

typedef struct
{
    float v1;
    float v2;
    float v3;
} BogusMessage;

int BogusMessage_Serialize(BogusMessage *msg, NBN_Stream *stream)
{
    NBN_SerializeFloat(stream, msg->v1, -100, 100, 1);
    NBN_SerializeFloat(stream, msg->v2, -100, 100, 2);
    NBN_SerializeFloat(stream, msg->v3, -100, 100, 3);

    return 0;
}

typedef struct
{
    uint64_t v1;
    uint64_t v2;
    uint64_t v3;
    uint64_t v4;
} BogusMessage2;

int BogusMessage2_Serialize(BogusMessage2 *msg, NBN_Stream *stream)
{
    NBN_SerializeUInt64(stream, msg->v1);
    NBN_SerializeUInt64(stream, msg->v2);
    NBN_SerializeUInt64(stream, msg->v3);
    NBN_SerializeUInt64(stream, msg->v4);

    return 0;
}

void Test_SerializeFloat(CuTest *tc)
{
    BogusMessage msg = { .v1 = 42.5, .v2 = -12.42, .v3 = -89.123 };
    NBN_WriteStream w_stream;
    uint8_t buffer[32];

    NBN_WriteStream_Init(&w_stream, buffer, sizeof(buffer));

    CuAssertIntEquals(tc, 0, BogusMessage_Serialize(&msg, (NBN_Stream *)&w_stream));

    NBN_WriteStream_Flush(&w_stream);

    BogusMessage r_msg;
    NBN_ReadStream r_stream;

    NBN_ReadStream_Init(&r_stream, buffer, sizeof(buffer));

    CuAssertIntEquals(tc, 0, BogusMessage_Serialize(&r_msg, (NBN_Stream *)&r_stream));
    CuAssertTrue(tc, r_msg.v1 == 42.5);
    CuAssertIntEquals(tc, -1242, r_msg.v2 * 100);
    CuAssertIntEquals(tc, -89123, r_msg.v3 * 1000);
}

void Test_SerializeUInt64(CuTest *tc)
{
    BogusMessage2 msg = {
        .v1 = 0xFFFFFFFFFFFFFFFF,
        .v2 = 9223372036854775807,
        .v3 = 4611686018427387903,
        .v4 = 42000
    };
    NBN_WriteStream w_stream;
    uint8_t buffer[32];

    NBN_WriteStream_Init(&w_stream, buffer, sizeof(buffer));
    CuAssertIntEquals(tc, 0, BogusMessage2_Serialize(&msg, (NBN_Stream *)&w_stream));
    NBN_WriteStream_Flush(&w_stream);

    BogusMessage2 r_msg;
    NBN_ReadStream r_stream;

    NBN_ReadStream_Init(&r_stream, buffer, sizeof(buffer));

    CuAssertIntEquals(tc, 0, BogusMessage2_Serialize(&r_msg, (NBN_Stream *)&r_stream));
    CuAssertTrue(tc, r_msg.v1 == 0xFFFFFFFFFFFFFFFF);
    CuAssertTrue(tc, r_msg.v2 == 9223372036854775807);
    CuAssertTrue(tc, r_msg.v3 == 4611686018427387903);
    CuAssertTrue(tc, r_msg.v4 == 42000);
}

int main(int argc, char *argv[])
{
    CuString *output = CuStringNew();
    CuSuite* suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, Test_SerializeFloat);
    SUITE_ADD_TEST(suite, Test_SerializeUInt64);

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);

    printf("%s\n", output->buffer);

    return suite->failCount;
}
