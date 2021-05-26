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

#ifndef NBNET_H
#define NBNET_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#pragma region Declarations

#define NBN_Abort abort /* TODO: custom abort mechanism */
#define NBN_ERROR -1

#pragma region Memory management

enum
{
    NBN_MEM_MESSAGE_CHUNK,
    NBN_MEM_BYTE_ARRAY_MESSAGE,
    NBN_MEM_CONNECTION,

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_MEM_PACKET_SIMULATOR_ENTRY
#endif
};

typedef struct NBN_MemPoolFreeBlock
{
    struct NBN_MemPoolFreeBlock *next;
} NBN_MemPoolFreeBlock;

typedef struct
{
    uint8_t **blocks;
    size_t block_size;
    unsigned int block_count;
    unsigned int block_idx;
    NBN_MemPoolFreeBlock *free;
} NBN_MemPool;

typedef struct
{
    NBN_MemPool mem_pools[16];
} NBN_MemoryManager;

extern NBN_MemoryManager __mem_manager;

#pragma endregion /* Memory management */

#pragma region Serialization

typedef uint32_t Word;

unsigned int GetRequiredNumberOfBitsFor(unsigned int);

#define WORD_BYTES (sizeof(Word))
#define WORD_BITS (WORD_BYTES * 8)
#define BITS_REQUIRED(min, max) (min == max) ? 0 : GetRequiredNumberOfBitsFor(max - min)

#define B_MASK(n) (1 << (n))
#define B_SET(mask, n) (mask |= B_MASK(n))
#define B_UNSET(mask, n) (mask &= ~B_MASK(n))
#define B_IS_SET(mask, n) ((B_MASK(n) & mask) == B_MASK(n))
#define B_IS_UNSET(mask, n) ((B_MASK(n) & mask) == 0)

#define ASSERT_VALUE_IN_RANGE(v, min, max) assert(v >= min && v <= max)
#define ASSERTED_SERIALIZE(v, min, max, code)       \
{                                                   \
    if (stream->type == NBN_STREAM_WRITE)           \
    ASSERT_VALUE_IN_RANGE(v, min, max);             \
    if (code < 0)                                   \
    return NBN_ERROR;                               \
    if (stream->type == NBN_STREAM_READ)            \
    ASSERT_VALUE_IN_RANGE(v, min, max);             \
}

#define SERIALIZE_UINT(v, min, max) \
    ASSERTED_SERIALIZE(v, min, max, stream->serialize_uint_func(stream, (unsigned int *)&(v), min, max))
#define SERIALIZE_INT(v, min, max) \
    ASSERTED_SERIALIZE(v, min, max, stream->serialize_int_func(stream, &(v), min, max))
#define SERIALIZE_FLOAT(v, min, max, precision) \
    ASSERTED_SERIALIZE(v, min, max, stream->serialize_float_func(stream, &(v), min, max, precision))
#define SERIALIZE_BOOL(v) ASSERTED_SERIALIZE(v, 0, 1, stream->serialize_bool_func(stream, &(v)))
#define SERIALIZE_STRING(v, length) SERIALIZE_BYTES(v, length)
#define SERIALIZE_BYTES(v, length) stream->serialize_bytes_func(stream, (uint8_t *)v, length)
#define SERIALIZE_PADDING() stream->serialize_padding_func(stream)

#pragma region NBN_BitReader

typedef struct
{
    unsigned int size;
    uint8_t *buffer;
    uint64_t scratch;
    unsigned int scratch_bits_count;
    unsigned int byte_cursor;
} NBN_BitReader;

void NBN_BitReader_Init(NBN_BitReader *, uint8_t *, unsigned int);
int NBN_BitReader_Read(NBN_BitReader *, Word *, unsigned int);

#pragma endregion /* NBN_BitReader */

#pragma region NBN_BitWriter

typedef struct
{
    unsigned int size;
    uint8_t *buffer;
    uint64_t scratch;
    unsigned int scratch_bits_count;
    unsigned int byte_cursor;
} NBN_BitWriter;

void NBN_BitWriter_Init(NBN_BitWriter*, uint8_t *, unsigned int);
int NBN_BitWriter_Write(NBN_BitWriter *, Word, unsigned int);
int NBN_BitWriter_Flush(NBN_BitWriter *);

#pragma endregion /* NBN_BitWriter */

#pragma region NBN_Stream

typedef struct NBN_Stream NBN_Stream;

typedef int (*NBN_Stream_SerializUint)(NBN_Stream *, unsigned int *, unsigned int, unsigned int);
typedef int (*NBN_Stream_SerializeInt)(NBN_Stream *, int *, int, int);
typedef int (*NBN_Stream_SerializeFloat)(NBN_Stream *, float *, float, float, int);
typedef int (*NBN_Stream_SerializeBool)(NBN_Stream *, unsigned int *);
typedef int (*NBN_Stream_SerializePadding)(NBN_Stream *);
typedef int (*NBN_Stream_SerializeBytes)(NBN_Stream *, uint8_t *, unsigned int);

typedef enum
{
    NBN_STREAM_WRITE,
    NBN_STREAM_READ,
    NBN_STREAM_MEASURE
} NBN_StreamType;

struct NBN_Stream
{
    NBN_StreamType type;
    NBN_Stream_SerializUint serialize_uint_func;
    NBN_Stream_SerializeInt serialize_int_func;
    NBN_Stream_SerializeFloat serialize_float_func;
    NBN_Stream_SerializeBool serialize_bool_func;
    NBN_Stream_SerializePadding serialize_padding_func;
    NBN_Stream_SerializeBytes serialize_bytes_func;
};

#pragma endregion /* NBN_Stream */

#pragma region NBN_ReadStream

typedef struct
{
    NBN_Stream base;
    NBN_BitReader bit_reader;
} NBN_ReadStream;

void NBN_ReadStream_Init(NBN_ReadStream *, uint8_t *, unsigned int);
int NBN_ReadStream_SerializeUint(NBN_ReadStream *, unsigned int *, unsigned int, unsigned int);
int NBN_ReadStream_SerializeInt(NBN_ReadStream *, int *, int, int);
int NBN_ReadStream_SerializeFloat(NBN_ReadStream *, float *, float, float, int);
int NBN_ReadStream_SerializeBool(NBN_ReadStream *, unsigned int *);
int NBN_ReadStream_SerializePadding(NBN_ReadStream *);
int NBN_ReadStream_SerializeBytes(NBN_ReadStream *, uint8_t *, unsigned int);

#pragma endregion /* NBN_ReadStream */

#pragma region NBN_WriteStream

typedef struct
{
    NBN_Stream base;
    NBN_BitWriter bit_writer;
} NBN_WriteStream;

void NBN_WriteStream_Init(NBN_WriteStream *, uint8_t *, unsigned int);
int NBN_WriteStream_SerializeUint(NBN_WriteStream *, unsigned int *, unsigned int, unsigned int);
int NBN_WriteStream_SerializeInt(NBN_WriteStream *, int *, int, int);
int NBN_WriteStream_SerializeFloat(NBN_WriteStream *, float *, float, float, int);
int NBN_WriteStream_SerializeBool(NBN_WriteStream *, unsigned int *);
int NBN_WriteStream_SerializePadding(NBN_WriteStream *);
int NBN_WriteStream_SerializeBytes(NBN_WriteStream *, uint8_t *, unsigned int);
int NBN_WriteStream_Flush(NBN_WriteStream *);

#pragma endregion /* NBN_WriteStream */

#pragma region NBN_MeasureStream

typedef struct
{
    NBN_Stream base;
    unsigned int number_of_bits;
} NBN_MeasureStream;

void NBN_MeasureStream_Init(NBN_MeasureStream *);
int NBN_MeasureStream_SerializeUint(NBN_MeasureStream *, unsigned int *, unsigned int, unsigned int);
int NBN_MeasureStream_SerializeInt(NBN_MeasureStream *, int *, int, int);
int NBN_MeasureStream_SerializeFloat(NBN_MeasureStream *, float *, float, float, int);
int NBN_MeasureStream_SerializeBool(NBN_MeasureStream *, unsigned int *);
int NBN_MeasureStream_SerializePadding(NBN_MeasureStream *);
int NBN_MeasureStream_SerializeBytes(NBN_MeasureStream *, uint8_t *, unsigned int);
void NBN_MeasureStream_Reset(NBN_MeasureStream *);

#pragma endregion /* NBN_MeasureStream */

#pragma endregion /* Serialization */

#pragma region NBN_Message

#define NBN_MAX_CHANNELS 8 /* Maximum value of uint8_t, see message header */
#define NBN_MAX_MESSAGE_TYPES 255 /* Maximum value of uint8_t, see message header */
#define NBN_MESSAGE_RESEND_DELAY 0.1 /* Number of seconds before a message is resent (reliable messages redundancy) */

/*
 * Message definition macro, use it like this in an header file shared between your client and server code:
 *
 * typedef struct
 * {
 *     ...
 * } SomeMessage;
 *
 * BEGIN_MESSAGE(SomeMessage)
 *     < USE SERIALIZATION MACROS HERE >
 * END_MESSAGE
 */

#define BEGIN_MESSAGE(name) \
static inline int name##_Serialize(name *msg, NBN_Stream *stream) {

#define END_MESSAGE \
    return 0; \
}

typedef int (*NBN_MessageSerializer)(void *, NBN_Stream *);
typedef void *(*NBN_MessageBuilder)(void);
typedef void (*NBN_MessageDestructor)(void *);

typedef struct
{
    uint16_t id;
    uint8_t type;
    uint8_t channel_id;
} NBN_MessageHeader;

typedef struct __NBN_Endpoint NBN_Endpoint;
typedef struct __NBN_Connection NBN_Connection;
typedef struct __NBN_Channel NBN_Channel;

/*
 * Holds the user message's data as well as a reference count for message recycling
 */
typedef struct
{
    uint8_t type;
    unsigned int ref_count;
    void *data;
} NBN_OutgoingMessage;

typedef struct
{
    NBN_MessageHeader header;
    NBN_Connection *sender;
    NBN_OutgoingMessage *outgoing_msg; /* NULL for incoming messages */
    void *data;
} NBN_Message;

/*
 * Used in user code to hold information about a received message.
 */
typedef struct
{
    uint8_t type;
    void *data;
    NBN_Connection *sender;
} NBN_MessageInfo;

int NBN_Message_SerializeHeader(NBN_MessageHeader *, NBN_Stream *);
int NBN_Message_Measure(NBN_Message *, NBN_MeasureStream *, NBN_MessageSerializer);
int NBN_Message_SerializeData(NBN_Message *, NBN_Stream *, NBN_MessageSerializer);

#pragma endregion /* NBN_Message */

#pragma region Encryption

/*
 * All "low-level" cryptography definitions used for nbnet packet encryption and authentication.
 *
 * I did not write any of the code in this section, I only regrouped it in there to keep nbnet contained
 * into a single header. I used a total of three different open source libraries:
 *
 * ECDH:        https://github.com/kokke/tiny-ECDH-c
 * AES:         https://github.com/kokke/tiny-AES-c
 * Poly1305:    https://github.com/floodyberry/poly1305-donna
 */

#pragma region ECDH

#define NIST_B163  1
#define NIST_K163  2
#define NIST_B233  3
#define NIST_K233  4
#define NIST_B283  5
#define NIST_K283  6
#define NIST_B409  7
#define NIST_K409  8
#define NIST_B571  9
#define NIST_K571 10

#define ECC_CURVE NIST_B233

#if defined(ECC_CURVE) && (ECC_CURVE != 0)
#if   (ECC_CURVE == NIST_K163) || (ECC_CURVE == NIST_B163)
#define CURVE_DEGREE       163
#define ECC_PRV_KEY_SIZE   24
#elif (ECC_CURVE == NIST_K233) || (ECC_CURVE == NIST_B233)
#define CURVE_DEGREE       233
#define ECC_PRV_KEY_SIZE   32
#elif (ECC_CURVE == NIST_K283) || (ECC_CURVE == NIST_B283)
#define CURVE_DEGREE       283
#define ECC_PRV_KEY_SIZE   36
#elif (ECC_CURVE == NIST_K409) || (ECC_CURVE == NIST_B409)
#define CURVE_DEGREE       409
#define ECC_PRV_KEY_SIZE   52
#elif (ECC_CURVE == NIST_K571) || (ECC_CURVE == NIST_B571)
#define CURVE_DEGREE       571
#define ECC_PRV_KEY_SIZE   72
#endif
#else
#error Must define a curve to use
#endif

#define ECC_PUB_KEY_SIZE     (2 * ECC_PRV_KEY_SIZE)

#pragma endregion /* ECDH */

#pragma region AES

#define AES128 1
//#define AES192 1
//#define AES256 1

#define AES_BLOCKLEN 16 //Block length in bytes AES is 128b block only

#if defined(AES256) && (AES256 == 1)
    #define AES_KEYLEN 32
    #define AES_keyExpSize 240
#elif defined(AES192) && (AES192 == 1)
    #define AES_KEYLEN 24
    #define AES_keyExpSize 208
#else
    #define AES_KEYLEN 16   // Key length in bytes
    #define AES_keyExpSize 176
#endif

struct AES_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
  uint8_t Iv[AES_BLOCKLEN];
};

#pragma endregion /* AES */

#pragma region Poly1305

#define POLY1305_KEYLEN 32
#define POLY1305_TAGLEN 16

#pragma endregion /* Poly1305 */

#pragma region Pseudo random generator

typedef void* CSPRNG;

#ifdef _WIN32

#include <windows.h>
#include <wincrypt.h>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif

typedef union
{
    CSPRNG     object;
    HCRYPTPROV hCryptProv;
}
CSPRNG_TYPE;

#else

#include <stdio.h>

typedef union
{
    CSPRNG object;
    FILE*  urandom;
}
CSPRNG_TYPE;

#endif /* _WIN32 */

#pragma endregion /* Pseudo random generator */

#pragma endregion /* Encryption */

#pragma region NBN_Packet

/*  
 * Maximum allowed packet size (including header) in bytes.
 * 1024 is an arbitrary value chosen to be below the MTU in order to avoid packet fragmentation.
 */
#define NBN_PACKET_MAX_SIZE 1024
#define NBN_MAX_MESSAGES_PER_PACKET 255 /* Maximum value of uint8_t, see packet header */

#define NBN_PACKET_HEADER_SIZE sizeof(NBN_PacketHeader)

/* Maximum size of packet's data (NBN_PACKET_MAX_DATA_SIZE + NBN_PACKET_HEADER_SIZE = NBN_PACKET_MAX_SIZE) */
#define NBN_PACKET_MAX_DATA_SIZE (NBN_PACKET_MAX_SIZE - NBN_PACKET_HEADER_SIZE)

/*
 * Maximum size of user's data. This is the NBN_PACKET_MAX_DATA_SIZE minus the size of an AES block so we don't
 * exceed the maximum packet size after AES encryption
 */
#define NBN_PACKET_MAX_USER_DATA_SIZE (NBN_PACKET_MAX_DATA_SIZE - AES_BLOCKLEN)

enum
{
    NBN_PACKET_WRITE_ERROR = -1,
    NBN_PACKET_WRITE_OK,
    NBN_PACKET_WRITE_NO_SPACE,
};

typedef enum
{
    NBN_PACKET_MODE_WRITE = 1,
    NBN_PACKET_MODE_READ
} NBN_PacketMode;

typedef struct
{
    uint32_t protocol_id;
    uint16_t seq_number;
    uint16_t ack;
    uint32_t ack_bits;
    uint8_t messages_count;

    /*
     * Encryption part
     *
     * 17 bytes overhead (not used when encryption is disabled)
     */

    uint8_t is_encrypted;
    uint8_t auth_tag[POLY1305_TAGLEN]; /* Poly1305 auth tag */
} NBN_PacketHeader;

typedef struct
{
    NBN_PacketHeader header;
    NBN_PacketMode mode;
    struct __NBN_Connection *sender; /* not serialized, fill by the network driver upon reception */
    uint8_t buffer[NBN_PACKET_MAX_SIZE];
    unsigned int size; /* in bytes */
    bool sealed;

    // streams
    NBN_WriteStream w_stream;
    NBN_ReadStream r_stream;
    NBN_MeasureStream m_stream;

    /* AES IV, different for each packet (computed from packet's sequence number) */
    uint8_t aes_iv[AES_BLOCKLEN];  
} NBN_Packet;

void NBN_Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
int NBN_Packet_InitRead(NBN_Packet *, NBN_Connection *, uint8_t[NBN_PACKET_MAX_SIZE], unsigned int);
uint32_t NBN_Packet_ReadProtocolId(uint8_t[NBN_PACKET_MAX_SIZE], unsigned int);
int NBN_Packet_WriteMessage(NBN_Packet *, NBN_Message *, NBN_MessageSerializer);
int NBN_Packet_Seal(NBN_Packet *, NBN_Connection *);

/* Encryption related functions */

void NBN_Packet_Encrypt(NBN_Packet *, NBN_Connection *);
void NBN_Packet_Decrypt(NBN_Packet *, NBN_Connection *);
void NBN_Packet_ComputeIV(NBN_Packet *, NBN_Connection *);
void NBN_Packet_Authenticate(NBN_Packet *, NBN_Connection *);
bool NBN_Packet_CheckAuthentication(NBN_Packet *, NBN_Connection *);
void NBN_Packet_ComputePoly1305Key(NBN_Packet *, NBN_Connection *, uint8_t *);

#pragma endregion /* NBN_Packet */

#pragma region NBN_MessageChunk

/* Chunk max size is the number of bytes of data a packet can hold minus the size of a message header minus 2 bytes
 * to hold the chunk id and total number of chunks.
 */
#define NBN_MESSAGE_CHUNK_SIZE (NBN_PACKET_MAX_USER_DATA_SIZE - sizeof(NBN_MessageHeader) - 2)
#define NBN_MESSAGE_CHUNK_TYPE (NBN_MAX_MESSAGE_TYPES - 1) /* Reserved message type for chunks */

typedef struct
{
    uint8_t id;
    uint8_t total;
    uint8_t data[NBN_MESSAGE_CHUNK_SIZE];
    NBN_OutgoingMessage *outgoing_msg;
} NBN_MessageChunk;

NBN_MessageChunk *NBN_MessageChunk_Create(void);
void NBN_MessageChunk_Destroy(NBN_MessageChunk *);

BEGIN_MESSAGE(NBN_MessageChunk)
    SERIALIZE_BYTES(&msg->id, 1);
    SERIALIZE_BYTES(&msg->total, 1);
    SERIALIZE_BYTES(msg->data, NBN_MESSAGE_CHUNK_SIZE);
END_MESSAGE

#pragma endregion /* NBN_MessageChunk */

#pragma region NBN_ClientClosedMessage

#define NBN_CLIENT_CLOSED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 2) /* Reserved message type */

typedef struct
{
    int code;
} NBN_ClientClosedMessage;

BEGIN_MESSAGE(NBN_ClientClosedMessage)
    SERIALIZE_INT(msg->code, SHRT_MIN, SHRT_MAX);
END_MESSAGE

NBN_ClientClosedMessage *NBN_ClientClosedMessage_Create(void);
void NBN_ClientClosedMessage_Destroy(NBN_ClientClosedMessage *);

#pragma endregion /* NBN_ClientClosedMessage */

#pragma region NBN_ClientAcceptedMessage

#define NBN_CLIENT_ACCEPTED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 3) /* Reserved message type */
#define NBN_ACCEPT_DATA_MAX_SIZE 255

typedef struct
{
    uint8_t data[NBN_ACCEPT_DATA_MAX_SIZE];
} NBN_ClientAcceptedMessage;

BEGIN_MESSAGE(NBN_ClientAcceptedMessage)
    SERIALIZE_BYTES(msg->data, NBN_ACCEPT_DATA_MAX_SIZE);
END_MESSAGE

NBN_ClientAcceptedMessage *NBN_ClientAcceptedMessage_Create(void);
void NBN_ClientAcceptedMessage_Destroy(NBN_ClientAcceptedMessage *);

#pragma endregion /* NBN_ClientAcceptedMessage */

#pragma region NBN_ByteArrayMessage

#define NBN_BYTE_ARRAY_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 4) /* Reserved message type */
#define NBN_BYTE_ARRAY_MAX_SIZE 4096

typedef struct
{
    uint8_t bytes[NBN_BYTE_ARRAY_MAX_SIZE];
    unsigned int length;
} NBN_ByteArrayMessage;

BEGIN_MESSAGE(NBN_ByteArrayMessage)
    SERIALIZE_UINT(msg->length, 0, NBN_BYTE_ARRAY_MAX_SIZE);
    SERIALIZE_BYTES(msg->bytes, msg->length);
END_MESSAGE

NBN_ByteArrayMessage *NBN_ByteArrayMessage_Create(void);
void NBN_ByteArrayMessage_Destroy(NBN_ByteArrayMessage *);

#pragma endregion /* NBN_ByteArrayMessage */

#pragma region NBN_PublicCryptoInfoMessage

#define NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 5) /* Reserved message type */

typedef struct
{
    uint8_t pub_key1[ECC_PUB_KEY_SIZE];
    uint8_t pub_key2[ECC_PUB_KEY_SIZE];
    uint8_t pub_key3[ECC_PUB_KEY_SIZE];
    uint8_t aes_iv[AES_BLOCKLEN];
} NBN_PublicCryptoInfoMessage;

BEGIN_MESSAGE(NBN_PublicCryptoInfoMessage)
    SERIALIZE_BYTES(msg->pub_key1, ECC_PUB_KEY_SIZE);
    SERIALIZE_BYTES(msg->pub_key2, ECC_PUB_KEY_SIZE);
    SERIALIZE_BYTES(msg->pub_key3, ECC_PUB_KEY_SIZE);
    SERIALIZE_BYTES(msg->aes_iv, AES_BLOCKLEN);
END_MESSAGE

NBN_PublicCryptoInfoMessage *NBN_PublicCryptoInfoMessage_Create(void);
void NBN_PublicCryptoInfoMessage_Destroy(NBN_PublicCryptoInfoMessage *);

#pragma endregion /* NBN_PublicCryptoInfoMessage */

#pragma region NBN_StartEncryptMessage

#define NBN_START_ENCRYPT_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 6) /* Reserved message type */

typedef struct {} NBN_StartEncryptMessage;

BEGIN_MESSAGE(NBN_StartEncryptMessage)
    (void)msg;
    (void)stream;
END_MESSAGE

NBN_StartEncryptMessage *NBN_StartEncryptMessage_Create(void);
void NBN_StartEncryptMessage_Destroy(NBN_StartEncryptMessage *);

#pragma endregion /* NBN_StartEncryptMessage */

#pragma region NBN_Channel

#define NBN_CHANNEL_BUFFER_SIZE 1024
#define NBN_CHANNEL_CHUNKS_BUFFER_SIZE 255
#define NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE 2048

/* Library reserved unreliable ordered channel */
#define NBN_CHANNEL_RESERVED_UNRELIABLE (NBN_MAX_CHANNELS - 1)

/* Library reserved reliable ordered channel */
#define NBN_CHANNEL_RESERVED_RELIABLE (NBN_MAX_CHANNELS - 2)

/* Library reserved messages reliable ordered channel */
#define NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES (NBN_MAX_CHANNELS - 3)

typedef enum
{
    NBN_CHANNEL_TYPE_UNDEFINED = -1,
    NBN_CHANNEL_TYPE_UNRELIABLE_ORDERED,
    NBN_CHANNEL_TYPE_RELIABLE_ORDERED
} NBN_ChannelType;

typedef struct
{
    NBN_Message message;
    double last_send_time;
    bool free;
} NBN_MessageSlot;

struct __NBN_Channel
{
    uint8_t id;
    NBN_ChannelType type;
    NBN_Connection *connection;
    uint16_t next_outgoing_message_id;
    uint16_t next_recv_message_id;
    unsigned int outgoing_message_count;
    unsigned int chunk_count;
    int last_received_chunk_id;
    double time;
    uint8_t *write_chunk_buffer;
    uint8_t *read_chunk_buffer;
    unsigned int write_chunk_buffer_size;
    unsigned int read_chunk_buffer_size;
    unsigned int next_outgoing_chunked_message;
    NBN_MessageSlot outgoing_message_slot_buffer[NBN_CHANNEL_BUFFER_SIZE];
    NBN_MessageSlot recved_message_slot_buffer[NBN_CHANNEL_BUFFER_SIZE];
    NBN_MessageChunk *recv_chunk_buffer[NBN_CHANNEL_CHUNKS_BUFFER_SIZE];

    bool (*AddReceivedMessage)(NBN_Channel *, NBN_Message *);
    bool (*AddOutgoingMessage)(NBN_Channel *, NBN_Message *);
    NBN_Message *(*GetNextRecvedMessage)(NBN_Channel *);
    NBN_Message *(*GetNextOutgoingMessage)(NBN_Channel *);
    int (*OnOutgoingMessageAcked)(NBN_Channel *, uint16_t);
};

void NBN_Channel_Destroy(NBN_Channel *);
void NBN_Channel_AddTime(NBN_Channel *, double);
bool NBN_Channel_AddChunk(NBN_Channel *, NBN_Message *);
int NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *, NBN_Connection *, NBN_Message *);
void NBN_Channel_ResizeWriteChunkBuffer(NBN_Channel *, unsigned int);
void NBN_Channel_ResizeReadChunkBuffer(NBN_Channel *, unsigned int);
void NBN_Channel_UpdateMessageLastSendTime(NBN_Channel *, NBN_Message *, double);

/*
   Unreliable ordered

   Guarantee that messages will be received in order, does not however guarantee that all message will be received when
   packets get lost. This is meant to be used for time critical messages when it does not matter that much if they
   end up getting lost. A good example would be game snaphosts when any newly received snapshot is more up to date
   than the previous one.
   */
typedef struct
{
    NBN_Channel base;
    uint16_t last_received_message_id;
    unsigned int next_outgoing_message_slot;
} NBN_UnreliableOrderedChannel;

NBN_UnreliableOrderedChannel *NBN_UnreliableOrderedChannel_Create(void);

/*
   Reliable ordered

   Will guarantee that all messages will be received in order. Use this when you want to make sure a message
   will be received, for example for chat messages or initial game world state.
   */
typedef struct
{
    NBN_Channel base;
    uint16_t oldest_unacked_message_id;
    uint16_t most_recent_message_id;
    bool ack_buffer[NBN_CHANNEL_BUFFER_SIZE];
} NBN_ReliableOrderedChannel;

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(void);

#pragma endregion /* NBN_Channel */

#pragma region NBN_Config

typedef struct
{
    const char *protocol_name;
    const char *ip_address;
    uint16_t port;
    bool is_encryption_enabled;
} NBN_Config;

#pragma endregion

#pragma region NBN_Connection

#define NBN_MAX_PACKET_ENTRIES 1024

/* Maximum number of packets that can be sent in a single flush
 *
 * IMPORTANT: do not increase this, it will break packet acks
*/
#define NBN_CONNECTION_MAX_SENT_PACKET_COUNT 16

/* Number of seconds before the connection is considered stale and get closed */
#define NBN_CONNECTION_STALE_TIME_THRESHOLD 3

typedef struct
{
    uint16_t id;
    uint8_t channel_id;
} NBN_MessageEntry;

typedef struct
{
    bool acked;
    unsigned int messages_count;
    NBN_MessageEntry messages[NBN_MAX_MESSAGES_PER_PACKET];
    double send_time;
} NBN_PacketEntry;

typedef struct
{
    double ping;
    float packet_loss;
    float upload_bandwidth;
    float download_bandwidth;
} NBN_ConnectionStats;

#ifdef NBN_DEBUG

typedef enum
{
    NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE
} NBN_ConnectionDebugCallback;

#endif /* NBN_DEBUG */

typedef struct
{
    uint8_t pub_key[ECC_PUB_KEY_SIZE]; /* Public key */
    uint8_t prv_key[ECC_PRV_KEY_SIZE]; /* Private key */
    uint8_t shared_key[ECC_PUB_KEY_SIZE]; /* Shared key */
} NBN_ConnectionKeySet;

struct __NBN_Connection
{
    uint32_t id;
    uint32_t protocol_id;
    double last_recv_packet_time; /* Used to detect stale connections */
    double last_flush_time; /* Last time the send queue was flushed */
    double last_read_packets_time; /* Last time packets were read from the socket */
    double time; /* Current time */
    unsigned int downloaded_bytes; /* Keep track of bytes read from the socket (used for download bandwith calculation) */
    bool is_accepted;
    bool is_stale;
    bool is_closed;
    struct __NBN_Endpoint *endpoint;
    NBN_ConnectionStats stats;
    void *driver_data; /* Data attached to the connection by the underlying driver */

#ifdef NBN_DEBUG
    /* Debug callbacks */
    void (*OnMessageAddedToRecvQueue)(struct __NBN_Connection *, NBN_Message *);
#endif /* NBN_DEBUG */

    /*
     *  Packet sequencing & acking
     */
    uint16_t next_packet_seq_number;
    uint16_t last_received_packet_seq_number;
    uint32_t packet_send_seq_buffer[NBN_MAX_PACKET_ENTRIES];
    NBN_PacketEntry packet_send_buffer[NBN_MAX_PACKET_ENTRIES];
    uint32_t packet_recv_seq_buffer[NBN_MAX_PACKET_ENTRIES];

    /*
     *  Messages channeling (sending & receiving)
     */
    NBN_Channel *channels[NBN_MAX_CHANNELS];

    /*
     * Encryption related fields
     */
    NBN_ConnectionKeySet keys1; /* Used for message encryption */
    NBN_ConnectionKeySet keys2; /* Used for packets IV */
    NBN_ConnectionKeySet keys3; /* Used for poly1305 keys generation */
    uint8_t aes_iv[AES_BLOCKLEN]; /* AES IV */

    bool can_decrypt;
    bool can_encrypt;
};

NBN_Connection *NBN_Connection_Create(uint32_t, uint32_t, void *, NBN_Endpoint *);
void NBN_Connection_Destroy(NBN_Connection *);
int NBN_Connection_ProcessReceivedPacket(NBN_Connection *, NBN_Packet *);
int NBN_Connection_EnqueueOutgoingMessage(NBN_Connection *, NBN_Channel *, NBN_Message *);
int NBN_Connection_FlushSendQueue(NBN_Connection *);
int NBN_Connection_CreateChannel(NBN_Connection *, NBN_ChannelType, uint8_t);
bool NBN_Connection_CheckIfStale(NBN_Connection *);
void NBN_Connection_AddTime(NBN_Connection *, double);

#pragma endregion /* NBN_Connection */

#pragma region NBN_AcceptData

typedef struct
{
    uint8_t buffer[NBN_ACCEPT_DATA_MAX_SIZE];
    NBN_WriteStream write_stream;
    NBN_ReadStream read_stream;
} NBN_AcceptData;

NBN_AcceptData *NBN_AcceptData_Create(void);
NBN_AcceptData *NBN_AcceptData_Read(uint8_t *);
void NBN_AcceptData_Destroy(NBN_AcceptData *);
void NBN_AcceptData_WriteUInt(NBN_AcceptData *, unsigned int);
void NBN_AcceptData_WriteInt(NBN_AcceptData *, int);
void NBN_AcceptData_WriteFloat(NBN_AcceptData *, float);
void NBN_AcceptData_WriteBytes(NBN_AcceptData *, uint8_t *, unsigned int);
unsigned int NBN_AcceptData_ReadUInt(NBN_AcceptData *);
int NBN_AcceptData_ReadInt(NBN_AcceptData *);
float NBN_AcceptData_ReadFloat(NBN_AcceptData *);
uint8_t *NBN_AcceptData_ReadBytes(NBN_AcceptData *, uint8_t *, unsigned int);

#pragma endregion /* NBN_AcceptData */

#pragma region NBN_EventQueue

#define NBN_NO_EVENT 0 /* No event left in the events queue */
#define NBN_EVENT_QUEUE_CAPACITY 1024

typedef struct
{
    int type;

    union
    {
        NBN_MessageInfo message_info;
        NBN_Connection *connection;
    } data;
} NBN_Event;

typedef struct
{
    NBN_Event events[NBN_EVENT_QUEUE_CAPACITY];
    unsigned int head;
    unsigned int tail;
    unsigned int count;
} NBN_EventQueue;

NBN_EventQueue *NBN_EventQueue_Create(void);
void NBN_EventQueue_Destroy(NBN_EventQueue *);
bool NBN_EventQueue_Enqueue(NBN_EventQueue *, NBN_Event);
bool NBN_EventQueue_Dequeue(NBN_EventQueue *, NBN_Event *);
bool NBN_EventQueue_IsEmpty(NBN_EventQueue *);

#pragma endregion /* NBN_EventQueue */

#pragma region Packet simulator

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#if defined(_WIN32) || defined(_WIN64)
#define NBNET_WINDOWS
#endif

/*
   Threading.

   Windows headers need need to be included by the user of the library before
   the nbnet header because of some winsock2.h / windows.h dependencies.
   */
#ifndef NBNET_WINDOWS
#include <pthread.h>
#endif /* NBNET_WINDOWS */

#define NBN_GameClient_SetPing(v) { __game_client.endpoint.packet_simulator.ping = v; }
#define NBN_GameClient_SetJitter(v) { __game_client.endpoint.packet_simulator.jitter = v; }
#define NBN_GameClient_SetPacketLoss(v) { __game_client.endpoint.packet_simulator.packet_loss_ratio = v; }
#define NBN_GameClient_SetPacketDuplication(v) { __game_client.endpoint.packet_simulator.packet_duplication_ratio = v; }

#define NBN_GameServer_SetPing(v) { __game_server.endpoint.packet_simulator.ping = v; }
#define NBN_GameServer_SetJitter(v) { __game_server.endpoint.packet_simulator.jitter = v; }
#define NBN_GameServer_SetPacketLoss(v) { __game_server.endpoint.packet_simulator.packet_loss_ratio = v; }
#define NBN_GameServer_SetPacketDuplication(v) { __game_server.endpoint.packet_simulator.packet_duplication_ratio = v; }

typedef struct __NBN_PacketSimulatorEntry NBN_PacketSimulatorEntry;

struct __NBN_PacketSimulatorEntry
{
    NBN_Packet packet;
    NBN_Connection *receiver;
    double delay;
    double enqueued_at;
    struct __NBN_PacketSimulatorEntry *next;
    struct __NBN_PacketSimulatorEntry *prev;
};

typedef struct
{
    NBN_PacketSimulatorEntry *head_packet;
    NBN_PacketSimulatorEntry *tail_packet;
    unsigned int packet_count;
    double time;

#ifdef NBNET_WINDOWS
    HANDLE queue_mutex;
    HANDLE thread; 
#else
    pthread_mutex_t queue_mutex;
    pthread_t thread;
#endif

    bool running;

    /* Settings */
    float packet_loss_ratio;
    float current_packet_loss_ratio;
    float packet_duplication_ratio;
    double ping;
    double jitter;
} NBN_PacketSimulator;

void NBN_PacketSimulator_Init(NBN_PacketSimulator *);
int NBN_PacketSimulator_EnqueuePacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *);
void NBN_PacketSimulator_Start(NBN_PacketSimulator *);
void NBN_PacketSimulator_Stop(NBN_PacketSimulator *);
void NBN_PacketSimulator_AddTime(NBN_PacketSimulator *, double);

#else

#define NBN_GameClient_SetPing(v) NBN_LogInfo("NBN_Debug_SetPing: packet simulator is not enabled, ignore")
#define NBN_GameClient_SetJitter(v) NBN_LogInfo("NBN_Debug_SetJitter: packet simulator is not enabled, ignore")
#define NBN_GameClient_SetPacketLoss(v) NBN_LogInfo("NBN_Debug_SetPacketLoss: packet simulator is not enabled, ignore")
#define NBN_GameClient_SetPacketDuplication(v) NBN_LogInfo("NBN_Debug_SetPacketDuplication: packet simulator is not enabled, ignore")

#define NBN_GameServer_SetPing(v) NBN_LogInfo("NBN_Debug_SetPing: packet simulator is not enabled, ignore")
#define NBN_GameServer_SetJitter(v) NBN_LogInfo("NBN_Debug_SetJitter: packet simulator is not enabled, ignore")
#define NBN_GameServer_SetPacketLoss(v) NBN_LogInfo("NBN_Debug_SetPacketLoss: packet simulator is not enabled, ignore")
#define NBN_GameServer_SetPacketDuplication(v) NBN_LogInfo("NBN_Debug_SetPacketDuplication: packet simulator is not enabled, ignore")

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */

#pragma region NBN_Endpoint

#define NBN_ENDPOINT_OUTGOING_MESSAGE_BUFFER_SIZE 1024

#define NBN_IsReservedMessage(type) (type == NBN_MESSAGE_CHUNK_TYPE || type == NBN_CLIENT_CLOSED_MESSAGE_TYPE \
|| type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE || type == NBN_BYTE_ARRAY_MESSAGE_TYPE \
|| type == NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE || type == NBN_START_ENCRYPT_MESSAGE_TYPE)

struct __NBN_Endpoint
{
    NBN_Config config;
    NBN_ChannelType channels[NBN_MAX_CHANNELS];
    NBN_MessageBuilder message_builders[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageDestructor message_destructors[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageSerializer message_serializers[NBN_MAX_MESSAGE_TYPES];
    NBN_OutgoingMessage outgoing_message_buffer[NBN_ENDPOINT_OUTGOING_MESSAGE_BUFFER_SIZE];
    NBN_EventQueue event_queue;
    bool is_server;
    unsigned int next_outgoing_message;

#ifdef NBN_DEBUG
    /* Debug callbacks */
    void (*OnMessageAddedToRecvQueue)(NBN_Connection *, NBN_Message *);
#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator packet_simulator;
#endif
};

void NBN_Endpoint_Init(NBN_Endpoint *, NBN_Config, bool);
void NBN_Endpoint_Deinit(NBN_Endpoint *);
void NBN_Endpoint_RegisterMessageBuilder(NBN_Endpoint *, NBN_MessageBuilder, uint8_t);
void NBN_Endpoint_RegisterMessageDestructor(NBN_Endpoint *, NBN_MessageDestructor, uint8_t);
void NBN_Endpoint_RegisterMessageSerializer(NBN_Endpoint *, NBN_MessageSerializer, uint8_t);
void NBN_Endpoint_RegisterChannel(NBN_Endpoint *, NBN_ChannelType, uint8_t);
NBN_Connection *NBN_Endpoint_CreateConnection(NBN_Endpoint *, uint32_t, void *);

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

enum
{
    /* Client is connected to server */
    NBN_CONNECTED = 1,

    /* Client is disconnected from the server */
    NBN_DISCONNECTED,

    /* Client has received a message from the server */
    NBN_MESSAGE_RECEIVED
};

typedef struct
{
    NBN_Endpoint endpoint;
    NBN_Connection *server_connection;
    NBN_AcceptData *accept_data;
} NBN_GameClient;

extern NBN_GameClient __game_client;

void NBN_GameClient_Init(const char *, const char *, uint16_t);
void NBN_GameClient_Deinit(void);
int NBN_GameClient_Start(void);
void NBN_GameClient_Stop(void);
void NBN_GameClient_RegisterMessage(uint8_t, NBN_MessageBuilder, NBN_MessageDestructor, NBN_MessageSerializer);
void NBN_GameClient_RegisterChannel(uint8_t, uint8_t);
void NBN_GameClient_AddTime(double);
int NBN_GameClient_Poll(void);
int NBN_GameClient_SendPackets(void);
NBN_OutgoingMessage *NBN_GameClient_CreateMessage(uint8_t, void *);
NBN_OutgoingMessage *NBN_GameClient_CreateByteArrayMessage(uint8_t *, unsigned int);
int NBN_GameClient_SendMessage(NBN_OutgoingMessage *, uint8_t);
int NBN_GameClient_SendUnreliableMessage(NBN_OutgoingMessage *);
int NBN_GameClient_SendReliableMessage(NBN_OutgoingMessage *);
NBN_Connection *NBN_GameClient_CreateServerConnection(void *);
NBN_MessageInfo NBN_GameClient_GetMessageInfo(void);
NBN_ConnectionStats NBN_GameClient_GetStats(void);
int NBN_GameClient_GetServerCloseCode(void);
NBN_AcceptData *NBN_GameClient_GetAcceptData(void);
bool NBN_GameClient_IsEncryptionEnabled(void);
void NBN_GameClient_EnableEncryption(void);

#ifdef NBN_DEBUG

void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#pragma endregion /* NBN_GameClient */

#pragma region NBN_GameServer

#define NBN_MAX_CLIENTS 32

enum
{
    /* A new client has connected */
    NBN_NEW_CONNECTION = 1,

    /* A client has disconnected */
    NBN_CLIENT_DISCONNECTED,

    /* A message has been received from a client */
    NBN_CLIENT_MESSAGE_RECEIVED
};

typedef struct
{
    float upload_bandwidth; /* Total upload bandwith of the game server */
    float download_bandwidth; /* Total download bandwith of the game server */
} NBN_GameServerStats;

typedef struct
{
    NBN_Endpoint endpoint;
    NBN_Connection *clients[NBN_MAX_CLIENTS];
    unsigned int client_count;
    NBN_GameServerStats stats;
} NBN_GameServer;

extern NBN_GameServer __game_server;

void NBN_GameServer_Init(const char *, uint16_t);
void NBN_GameServer_Deinit(void);
int NBN_GameServer_Start(void);
void NBN_GameServer_Stop(void);
void NBN_GameServer_RegisterMessage(uint8_t, NBN_MessageBuilder, NBN_MessageDestructor, NBN_MessageSerializer);
void NBN_GameServer_RegisterChannel(uint8_t, uint8_t);
void NBN_GameServer_AddTime(double);
int NBN_GameServer_Poll(void);
int NBN_GameServer_SendPackets(void);
NBN_Connection *NBN_GameServer_CreateClientConnection(uint32_t, void *);
int NBN_GameServer_CloseClient(NBN_Connection *);
int NBN_GameServer_CloseClientWithCode(NBN_Connection *, int);
NBN_OutgoingMessage *NBN_GameServer_CreateMessage(uint8_t, void *);
NBN_OutgoingMessage *NBN_GameServer_CreateByteArrayMessage(uint8_t *, unsigned int);
int NBN_GameServer_SendMessageTo(NBN_Connection *, NBN_OutgoingMessage *, uint8_t);
int NBN_GameServer_SendUnreliableMessageTo(NBN_Connection *, NBN_OutgoingMessage *);
int NBN_GameServer_SendReliableMessageTo(NBN_Connection *, NBN_OutgoingMessage *);
int NBN_GameServer_BroadcastMessage(NBN_OutgoingMessage *, uint8_t);
int NBN_GameServer_BroadcastUnreliableMessage(NBN_OutgoingMessage *);
int NBN_GameServer_BroadcastReliableMessage(NBN_OutgoingMessage *);
int NBN_GameServer_AcceptIncomingConnection(NBN_AcceptData *);
int NBN_GameServer_RejectIncomingConnectionWithCode(int);
int NBN_GameServer_RejectIncomingConnection(void);
NBN_Connection *NBN_GameServer_GetIncomingConnection(void);
NBN_Connection *NBN_GameServer_GetDisconnectedClient(void);
NBN_Connection *NBN_GameServer_FindClientById(uint32_t);
NBN_MessageInfo NBN_GameServer_GetMessageInfo(void);
NBN_GameServerStats NBN_GameServer_GetStats(void);
void NBN_GameServer_EnableEncryption(void);
bool NBN_GameServer_IsEncryptionEnabled(void);

#ifdef NBN_DEBUG

void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#pragma endregion /* NBN_GameServer */

#pragma region Network driver

/*
 * Game client driver
 */

typedef enum
{
    NBN_DRIVER_GCLI_CONNECTED,
    NBN_DRIVER_GCLI_SERVER_PACKET_RECEIVED
} NBN_Driver_GCli_EventType;

int NBN_Driver_GCli_Start(uint32_t, const char *, uint16_t);
void NBN_Driver_GCli_Stop(void);
int NBN_Driver_GCli_RecvPackets(void);
int NBN_Driver_GCli_SendPacket(NBN_Packet *);
void NBN_Driver_GCli_RaiseEvent(NBN_Driver_GCli_EventType, void *);

/*
 * Game server driver
 */

typedef enum
{
    NBN_DRIVER_GSERV_CLIENT_CONNECTED,
    NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED
} NBN_Driver_GServ_EventType;

int NBN_Driver_GServ_Start(uint32_t, uint16_t);
void NBN_Driver_GServ_Stop(void);
int NBN_Driver_GServ_RecvPackets(void);
void NBN_Driver_GServ_DestroyClientConnection(NBN_Connection *);
int NBN_Driver_GServ_SendPacketTo(NBN_Packet *, NBN_Connection *);
int NBN_Driver_GServ_RaiseEvent(NBN_Driver_GServ_EventType, void *);

#pragma endregion /* Network driver */

#pragma region Utils

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef ABS
#define ABS(v) (((v) > 0) ? (v) : -(v))
#endif

#define SEQUENCE_NUMBER_GT(seq1, seq2) \
    ((seq1 > seq2 && (seq1 - seq2) <= 32767) || (seq1 < seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_GTE(seq1, seq2) \
    ((seq1 >= seq2 && (seq1 - seq2) <= 32767) || (seq1 <= seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_LT(seq1, seq2) \
    ((seq1 < seq2 && (seq2 - seq1) <= 32767) || (seq1 > seq2 && (seq1 - seq2) >= 32767))

#pragma endregion /* Utils */

#pragma endregion /* Declarations */

#endif /* NBNET_H */

#pragma region Implementations

#ifdef NBNET_IMPL

#pragma region Memory management

NBN_MemoryManager __mem_manager;

static void NBN_MemoryManager_Init(void);
static void NBN_MemoryManager_Deinit(void);
static void *NBN_MemoryManager_Alloc(unsigned int);
static void NBN_MemoryManager_Dealloc(void *, unsigned int);

static void NBN_MemPool_Init(NBN_MemPool *, size_t, unsigned int);
static void NBN_MemPool_Deinit(NBN_MemPool *);
static void NBN_MemPool_Grow(NBN_MemPool *, unsigned int);
static void *NBN_MemPool_Alloc(NBN_MemPool *);
static void NBN_MemPool_Dealloc(NBN_MemPool *, void *);
static void NBN_MemPool_Grow(NBN_MemPool *, unsigned int);

static void NBN_MemoryManager_Init(void)
{
    NBN_MemPool_Init(&__mem_manager.mem_pools[NBN_MEM_MESSAGE_CHUNK], sizeof(NBN_MessageChunk), 256);
    NBN_MemPool_Init(&__mem_manager.mem_pools[NBN_MEM_BYTE_ARRAY_MESSAGE], sizeof(NBN_ByteArrayMessage), 256);
    NBN_MemPool_Init(&__mem_manager.mem_pools[NBN_MEM_CONNECTION], sizeof(NBN_Connection), 16);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_MemPool_Init(&__mem_manager.mem_pools[NBN_MEM_PACKET_SIMULATOR_ENTRY], sizeof(NBN_PacketSimulatorEntry), 512);
#endif
}

static void NBN_MemoryManager_Deinit(void)
{
    NBN_MemPool_Deinit(&__mem_manager.mem_pools[NBN_MEM_MESSAGE_CHUNK]);
    NBN_MemPool_Deinit(&__mem_manager.mem_pools[NBN_MEM_BYTE_ARRAY_MESSAGE]);
    // NBN_MemPool_Deinit(&__mem_manager.mem_pools[NBN_MEM_CONNECTION]);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_MemPool_Deinit(&__mem_manager.mem_pools[NBN_MEM_PACKET_SIMULATOR_ENTRY]);
#endif
}

static void *NBN_MemoryManager_Alloc(unsigned int mem_tag)
{
    return NBN_MemPool_Alloc(&__mem_manager.mem_pools[mem_tag]);
}

static void NBN_MemoryManager_Dealloc(void *ptr, unsigned int mem_tag)
{
    NBN_MemPool_Dealloc(&__mem_manager.mem_pools[mem_tag], ptr);
}

static void NBN_MemPool_Init(NBN_MemPool *pool, size_t block_size, unsigned int initial_block_count)
{
    pool->block_size = MAX(block_size, sizeof(NBN_MemPoolFreeBlock));
    pool->block_idx = 0;
    pool->block_count = 0;
    pool->free = NULL;

    NBN_MemPool_Grow(pool, initial_block_count);
}

static void NBN_MemPool_Deinit(NBN_MemPool *pool)
{
    for (unsigned int i = 0; i < pool->block_count; i++)
        NBN_Deallocator(pool->blocks[i]);

    NBN_Deallocator(pool->blocks);
}

static void *NBN_MemPool_Alloc(NBN_MemPool *pool)
{
    if (pool->free)
    {
        void *block = pool->free;

        pool->free = pool->free->next;

        return block;
    }

    if (pool->block_idx == pool->block_count)
        NBN_MemPool_Grow(pool, pool->block_count * 2);

    void *block = pool->blocks[pool->block_idx];

    pool->block_idx++;

    return block;
}

static void NBN_MemPool_Dealloc(NBN_MemPool *pool, void *ptr)
{
    NBN_MemPoolFreeBlock *free = pool->free;

    pool->free = ptr;
    pool->free->next = free;
}

static void NBN_MemPool_Grow(NBN_MemPool *pool, unsigned int block_count)
{
    pool->blocks = NBN_Reallocator(pool->blocks, sizeof(uint8_t *) * block_count);

    for (unsigned int i = 0; i < block_count - pool->block_count; i++)
        pool->blocks[pool->block_idx + i] = NBN_Allocator(pool->block_size);

    pool->block_count = block_count;
}

#pragma endregion /* Memory management */

#pragma region Serialization

unsigned int GetRequiredNumberOfBitsFor(unsigned int v)
{
    unsigned int a = v | (v >> 1);
    unsigned int b = a | (a >> 2);
    unsigned int c = b | (b >> 4);
    unsigned int d = c | (c >> 8);
    unsigned int e = d | (d >> 16);
    unsigned int f = e >> 1;

    unsigned int i = f - ((f >> 1) & 0x55555555);
    unsigned int j = (((i >> 2) & 0x33333333) + (i & 0x33333333));
    unsigned int k = (((j >> 4) + j) & 0x0f0f0f0f);
    unsigned int l = k + (k >> 8);
    unsigned int m = l + (l >> 16);

    return (m & 0x0000003f) + 1;
}

#pragma region NBN_BitReader

static void NBN_BitReader_ReadFromBuffer(NBN_BitReader *);

void NBN_BitReader_Init(NBN_BitReader *bit_reader, uint8_t *buffer, unsigned int size)
{
    bit_reader->size = size;
    bit_reader->buffer = buffer;
    bit_reader->scratch = 0;
    bit_reader->scratch_bits_count = 0;
    bit_reader->byte_cursor = 0;
}

int NBN_BitReader_Read(NBN_BitReader *bit_reader, Word *word, unsigned int number_of_bits)
{
    *word = 0;

    if (number_of_bits > bit_reader->scratch_bits_count)
    {
        unsigned int needed_bytes = (number_of_bits - bit_reader->scratch_bits_count - 1) / 8 + 1;

        if (bit_reader->byte_cursor + needed_bytes > bit_reader->size)
            return NBN_ERROR;

        NBN_BitReader_ReadFromBuffer(bit_reader);
    }

    *word |= (bit_reader->scratch & (((uint64_t)1 << number_of_bits) - 1));
    bit_reader->scratch >>= number_of_bits;
    bit_reader->scratch_bits_count -= number_of_bits;

    return 0;
}

static void NBN_BitReader_ReadFromBuffer(NBN_BitReader *bit_reader)
{
    unsigned int bytes_count = MIN(bit_reader->size - bit_reader->byte_cursor, WORD_BYTES);
    Word word = 0;

    memcpy(&word, bit_reader->buffer + bit_reader->byte_cursor, bytes_count);

    bit_reader->scratch |= (uint64_t)word << bit_reader->scratch_bits_count;
    bit_reader->scratch_bits_count += bytes_count * 8;
    bit_reader->byte_cursor += bytes_count;
}

#pragma endregion /* NBN_BitReader */

#pragma region NBN_BitWriter

static int NBN_BitWriter_FlushScratchBits(NBN_BitWriter *, unsigned int);

void NBN_BitWriter_Init(NBN_BitWriter *bit_writer, uint8_t *buffer, unsigned int size)
{
    bit_writer->size = size;
    bit_writer->buffer = buffer;
    bit_writer->scratch = 0;
    bit_writer->scratch_bits_count = 0;
    bit_writer->byte_cursor = 0;
}

int NBN_BitWriter_Write(NBN_BitWriter *bit_writer, Word value, unsigned int number_of_bits)
{
    bit_writer->scratch |= ((uint64_t)value << bit_writer->scratch_bits_count);

    if ((bit_writer->scratch_bits_count += number_of_bits) >= WORD_BITS)
        return NBN_BitWriter_FlushScratchBits(bit_writer, WORD_BITS);

    return 0;
}

int NBN_BitWriter_Flush(NBN_BitWriter *bit_writer)
{
    return NBN_BitWriter_FlushScratchBits(bit_writer, bit_writer->scratch_bits_count);
}

static int NBN_BitWriter_FlushScratchBits(NBN_BitWriter *bit_writer, unsigned int number_of_bits)
{
    if (bit_writer->scratch_bits_count < 1)
        return 0;

    unsigned int bytes_count = (number_of_bits - 1) / 8 + 1;

    assert(bytes_count <= WORD_BYTES);

    if (bit_writer->byte_cursor + bytes_count > bit_writer->size)
        return NBN_ERROR;

    Word word = 0 | (bit_writer->scratch & (((uint64_t)1 << number_of_bits) - 1));

    memcpy(bit_writer->buffer + bit_writer->byte_cursor, &word, bytes_count);

    bit_writer->scratch >>= number_of_bits;
    bit_writer->scratch_bits_count -= number_of_bits;
    bit_writer->byte_cursor += bytes_count;

    return 0;
}

#pragma endregion /* NBN_BitWriter */

#pragma region NBN_ReadStream

void NBN_ReadStream_Init(NBN_ReadStream *read_stream, uint8_t *buffer, unsigned int size)
{
    read_stream->base.type = NBN_STREAM_READ;
    read_stream->base.serialize_uint_func = (NBN_Stream_SerializUint)NBN_ReadStream_SerializeUint;
    read_stream->base.serialize_int_func = (NBN_Stream_SerializeInt)NBN_ReadStream_SerializeInt;
    read_stream->base.serialize_float_func = (NBN_Stream_SerializeFloat)NBN_ReadStream_SerializeFloat;
    read_stream->base.serialize_bool_func = (NBN_Stream_SerializeBool)NBN_ReadStream_SerializeBool;
    read_stream->base.serialize_padding_func = (NBN_Stream_SerializePadding)NBN_ReadStream_SerializePadding;
    read_stream->base.serialize_bytes_func = (NBN_Stream_SerializeBytes)NBN_ReadStream_SerializeBytes;

    NBN_BitReader_Init(&read_stream->bit_reader, buffer, size);
}

int NBN_ReadStream_SerializeUint(NBN_ReadStream *read_stream, unsigned int *value, unsigned int min, unsigned int max)
{
    assert(min <= max);

    if (NBN_BitReader_Read(&read_stream->bit_reader, value, BITS_REQUIRED(min, max)) < 0)
        return NBN_ERROR;

    *value += min;

#ifdef NBN_DEBUG
    assert(*value >= min && *value <= max);
#else
    if (*value < min || *value > max)
        return NBN_ERROR;
#endif

    return 0;
}

int NBN_ReadStream_SerializeInt(NBN_ReadStream *read_stream, int *value, int min, int max)
{
    assert(min <= max);

    unsigned int isNegative = 0;
    unsigned int abs_min = MIN(abs(min), abs(max));
    unsigned int abs_max = MAX(abs(min), abs(max));

    isNegative = *value < 0; /* TODO: useless, remove this ? */
    *value = abs(*value);

    if (NBN_ReadStream_SerializeBool(read_stream, &isNegative) < 0)
        return NBN_ERROR;

    if (NBN_ReadStream_SerializeUint(
                read_stream, (unsigned int *)value, (min < 0 && max > 0) ? 0 : abs_min, abs_max) < 0)
        return NBN_ERROR;

    if (isNegative)
        *value *= -1;

    return 0;
}

int NBN_ReadStream_SerializeFloat(NBN_ReadStream *read_stream, float *value, float min, float max, int precision)
{
    assert(min <= max);

    unsigned int mult = pow(10, precision);
    int i_min = min * mult;
    int i_max = max * mult;
    int i_val;

    if (NBN_ReadStream_SerializeInt(read_stream, &i_val, i_min, i_max) < 0)
        return NBN_ERROR;

    *value = (float)i_val / mult;

    return 0;
}

int NBN_ReadStream_SerializeBool(NBN_ReadStream *read_stream, unsigned int *value)
{
    return NBN_ReadStream_SerializeUint(read_stream, (unsigned int *)value, 0, 1);
}

int NBN_ReadStream_SerializePadding(NBN_ReadStream *read_stream)
{
    // if we are at the beginning of a new byte, no need to pad
    if (read_stream->bit_reader.scratch_bits_count % 8 == 0)
        return 0;

    Word value;
    unsigned int padding = read_stream->bit_reader.scratch_bits_count % 8;
    int ret = NBN_BitReader_Read(&read_stream->bit_reader, &value, padding);

#ifdef NBN_DEBUG
    assert(value == 0);
#else
    if (value != 0)
        return NBN_ERROR;
#endif

    return ret;
}

int NBN_ReadStream_SerializeBytes(NBN_ReadStream *read_stream, uint8_t *bytes, unsigned int length)
{
    if (length == 0)
        return NBN_ERROR;

    if (NBN_ReadStream_SerializePadding(read_stream) < 0)
        return NBN_ERROR;

    NBN_BitReader *bit_reader = &read_stream->bit_reader;

    // make sure we are at the start of a new byte after applying padding
    assert(bit_reader->scratch_bits_count % 8 == 0);

    if (length * 8 <= bit_reader->scratch_bits_count)
    {
        // the byte array is fully contained inside the read word

        Word word;

        if (NBN_BitReader_Read(bit_reader, &word, length * 8) < 0)
            return NBN_ERROR;

        memcpy(bytes, &word, length);
    }
    else
    {
        // reading is done word by word and in this case, the start of byte array has already been read
        // therefore we need to "roll back" the byte cursor so it is at the very begining of the byte array
        // inside the buffer

        bit_reader->byte_cursor -= (bit_reader->scratch_bits_count / 8);
        bit_reader->scratch_bits_count = 0;
        bit_reader->scratch = 0;

        memcpy(bytes, bit_reader->buffer + bit_reader->byte_cursor, length);

        bit_reader->byte_cursor += length;
    }

    return 0;
}

#pragma endregion /* NBN_ReadStream */

#pragma region NBN_WriteStream

void NBN_WriteStream_Init(NBN_WriteStream *write_stream, uint8_t *buffer, unsigned int size)
{
    write_stream->base.type = NBN_STREAM_WRITE;
    write_stream->base.serialize_uint_func = (NBN_Stream_SerializUint)NBN_WriteStream_SerializeUint;
    write_stream->base.serialize_int_func = (NBN_Stream_SerializeInt)NBN_WriteStream_SerializeInt;
    write_stream->base.serialize_float_func = (NBN_Stream_SerializeFloat)NBN_WriteStream_SerializeFloat;
    write_stream->base.serialize_bool_func = (NBN_Stream_SerializeBool)NBN_WriteStream_SerializeBool;
    write_stream->base.serialize_padding_func = (NBN_Stream_SerializePadding)NBN_WriteStream_SerializePadding;
    write_stream->base.serialize_bytes_func = (NBN_Stream_SerializeBytes)NBN_WriteStream_SerializeBytes;

    NBN_BitWriter_Init(&write_stream->bit_writer, buffer, size);
}

int NBN_WriteStream_SerializeUint(
        NBN_WriteStream *write_stream, unsigned int *value, unsigned int min, unsigned int max)
{
    assert(min <= max);
    assert(*value >= min && *value <= max);

    if (NBN_BitWriter_Write(&write_stream->bit_writer, *value - min, BITS_REQUIRED(min, max)) < 0)
        return NBN_ERROR;

    return 0;
}

int NBN_WriteStream_SerializeInt(NBN_WriteStream *write_stream, int *value, int min, int max)
{
    assert(min <= max);

    unsigned int isNegative = 0;
    unsigned int abs_min = MIN(abs(min), abs(max));
    unsigned int abs_max = MAX(abs(min), abs(max));

    isNegative = *value < 0;
    *value = abs(*value);

    if (NBN_WriteStream_SerializeBool(write_stream, &isNegative) < 0)
        return NBN_ERROR;

    if (NBN_WriteStream_SerializeUint(
                write_stream, (unsigned int *)value, (min < 0 && max > 0) ? 0 : abs_min, abs_max) < 0)
        return NBN_ERROR;

    if (isNegative)
        *value *= -1;

    return 0;
}

int NBN_WriteStream_SerializeFloat(NBN_WriteStream *write_stream, float *value, float min, float max, int precision)
{
    assert(min <= max);

    unsigned int mult = pow(10, precision);
    int i_min = min * mult;
    int i_max = max * mult;
    int i_val = *value * mult;

    if (NBN_WriteStream_SerializeInt(write_stream, &i_val, i_min, i_max) < 0)
        return NBN_ERROR;

    return 0;
}

int NBN_WriteStream_SerializeBool(NBN_WriteStream *write_stream, unsigned int *value)
{
    return NBN_WriteStream_SerializeUint(write_stream, value, 0, 1);
}

int NBN_WriteStream_SerializePadding(NBN_WriteStream *write_stream)
{
    // if we are at the beginning of a new byte, no need to pad
    if (write_stream->bit_writer.scratch_bits_count % 8 == 0) 
        return 0;

    unsigned int padding = 8 - (write_stream->bit_writer.scratch_bits_count % 8);

    return NBN_BitWriter_Write(&write_stream->bit_writer, 0, padding);
}

int NBN_WriteStream_SerializeBytes(NBN_WriteStream *write_stream, uint8_t *bytes, unsigned int length)
{
    if (length == 0)
        return NBN_ERROR;

    if (NBN_WriteStream_SerializePadding(write_stream) < 0)
        return NBN_ERROR;

    NBN_BitWriter *bit_writer = &write_stream->bit_writer;

    // make sure we are at the start of a new byte after applying padding
    assert(bit_writer->scratch_bits_count % 8 == 0);

    if (NBN_WriteStream_Flush(write_stream) < 0)
        return NBN_ERROR;

    // make sure everything has been flushed to the buffer before writing the byte array
    assert(bit_writer->scratch_bits_count == 0);

    if (bit_writer->byte_cursor + length > bit_writer->size)
        return NBN_ERROR;

    memcpy(bit_writer->buffer + bit_writer->byte_cursor, bytes, length);

    bit_writer->byte_cursor += length;

    return 0;
}

int NBN_WriteStream_Flush(NBN_WriteStream *write_stream)
{
    return NBN_BitWriter_Flush(&write_stream->bit_writer);
}

#pragma endregion /* NBN_WriteStream */

#pragma region NBN_MeasureStream

void NBN_MeasureStream_Init(NBN_MeasureStream *measure_stream)
{
    measure_stream->base.type = NBN_STREAM_MEASURE;
    measure_stream->base.serialize_uint_func = (NBN_Stream_SerializUint)NBN_MeasureStream_SerializeUint;
    measure_stream->base.serialize_int_func = (NBN_Stream_SerializeInt)NBN_MeasureStream_SerializeInt;
    measure_stream->base.serialize_float_func = (NBN_Stream_SerializeFloat)NBN_MeasureStream_SerializeFloat;
    measure_stream->base.serialize_bool_func = (NBN_Stream_SerializeBool)NBN_MeasureStream_SerializeBool;
    measure_stream->base.serialize_padding_func = (NBN_Stream_SerializePadding)NBN_MeasureStream_SerializePadding;
    measure_stream->base.serialize_bytes_func = (NBN_Stream_SerializeBytes)NBN_MeasureStream_SerializeBytes;

    measure_stream->number_of_bits = 0;
}

int NBN_MeasureStream_SerializeUint(
        NBN_MeasureStream *measure_stream, unsigned int *value, unsigned int min, unsigned int max)
{
    assert(min <= max);
    // assert(*value >= min && *value <= max);

    unsigned int number_of_bits = BITS_REQUIRED(min, max);

    measure_stream->number_of_bits += number_of_bits;

    return number_of_bits;
}

int NBN_MeasureStream_SerializeInt(NBN_MeasureStream *measure_stream, int *value, int min, int max)
{
    assert(min <= max);
    assert(*value >= min && *value <= max);

    unsigned int abs_min = MIN(abs(min), abs(max));
    unsigned int abs_max = MAX(abs(min), abs(max));
    unsigned int abs_value = abs(*value);
    unsigned number_of_bits = NBN_MeasureStream_SerializeUint(
            measure_stream, &abs_value, (min < 0 && max > 0) ? 0 : abs_min, abs_max);

    measure_stream->number_of_bits++; // +1 for int sign

    return number_of_bits + 1;
}

int NBN_MeasureStream_SerializeFloat(
        NBN_MeasureStream *measure_stream, float *value, float min, float max, int precision)
{
    assert(min <= max);
    assert(*value >= min && *value <= max);

    unsigned int mult = pow(10, precision);
    int i_min = min * mult;
    int i_max = max * mult;
    int i_val = *value * mult;

    return NBN_MeasureStream_SerializeInt(measure_stream, &i_val, i_min, i_max);
}

int NBN_MeasureStream_SerializeBool(NBN_MeasureStream *measure_stream, unsigned int *value)
{
    (void)value;

    measure_stream->number_of_bits++;

    return 1;
}

int NBN_MeasureStream_SerializePadding(NBN_MeasureStream *measure_stream)
{
    if (measure_stream->number_of_bits % 8 == 0)
        return 0;

    unsigned int padding = 8 - (measure_stream->number_of_bits % 8);

    measure_stream->number_of_bits += padding;

    return padding;
}

int NBN_MeasureStream_SerializeBytes(NBN_MeasureStream *measure_stream, uint8_t *bytes, unsigned int length)
{
    (void)bytes;

    NBN_MeasureStream_SerializePadding(measure_stream);

    unsigned int bits = length * 8;

    measure_stream->number_of_bits += bits;

    return bits;
}

void NBN_MeasureStream_Reset(NBN_MeasureStream *measure_stream)
{
    measure_stream->number_of_bits = 0;
}

#pragma endregion /* NBN_MeasureStream */

#pragma endregion /* Serialization */

#pragma region NBN_Packet

static int NBN_Packet_SerializeHeader(NBN_PacketHeader *, NBN_Stream *);

static void AES_init_ctx_iv(struct AES_ctx*, const uint8_t*, const uint8_t*);
static void AES_CBC_encrypt_buffer(struct AES_ctx *,uint8_t*, uint32_t);
static void AES_CBC_decrypt_buffer(struct AES_ctx*, uint8_t*, uint32_t);

void poly1305_auth(uint8_t out[POLY1305_TAGLEN], const uint8_t *m, size_t inlen,
                   const uint8_t key[POLY1305_KEYLEN])
    __attribute__((__bounded__(__minbytes__, 1, POLY1305_TAGLEN)))
    __attribute__((__bounded__(__buffer__, 2, 3)))
    __attribute__((__bounded__(__minbytes__, 4, POLY1305_KEYLEN)));

void NBN_Packet_InitWrite(
        NBN_Packet *packet, uint32_t protocol_id, uint16_t seq_number, uint16_t ack, uint32_t ack_bits)
{
    packet->header.protocol_id = protocol_id;
    packet->header.messages_count = 0;
    packet->header.seq_number = seq_number;
    packet->header.ack = ack;
    packet->header.ack_bits = ack_bits;

    packet->mode = NBN_PACKET_MODE_WRITE;
    packet->sender = NULL;
    packet->size = 0;
    packet->sealed = false;
    packet->m_stream.number_of_bits = 0;

    NBN_WriteStream_Init(&packet->w_stream, packet->buffer + NBN_PACKET_HEADER_SIZE, NBN_PACKET_MAX_USER_DATA_SIZE);
    NBN_MeasureStream_Init(&packet->m_stream);
}

int NBN_Packet_InitRead(
        NBN_Packet *packet, NBN_Connection *sender, uint8_t buffer[NBN_PACKET_MAX_SIZE], unsigned int size)
{
    packet->mode = NBN_PACKET_MODE_READ;
    packet->sender = sender;
    packet->size = size;
    packet->sealed = false;

    memcpy(packet->buffer, buffer, size);

    NBN_ReadStream header_r_stream;

    NBN_ReadStream_Init(&header_r_stream, packet->buffer, NBN_PACKET_HEADER_SIZE);

    if (NBN_Packet_SerializeHeader(&packet->header, (NBN_Stream *)&header_r_stream) < 0)
        return NBN_ERROR;

    if (sender->endpoint->config.is_encryption_enabled && packet->header.is_encrypted)
    {
        if (!sender->can_decrypt)
        {
            NBN_LogError("Discard encrypted packet %d", packet->header.seq_number);

            return NBN_ERROR;
        }

        NBN_Packet_ComputeIV(packet, packet->sender);

        if (!NBN_Packet_CheckAuthentication(packet, packet->sender))
        {
            NBN_LogError("Authentication check failed for packet %d", packet->header.seq_number);

            return NBN_ERROR;
        }

        NBN_Packet_Decrypt(packet, packet->sender);
    }

    NBN_ReadStream_Init(&packet->r_stream, packet->buffer + NBN_PACKET_HEADER_SIZE, packet->size);

    return 0;
}

uint32_t NBN_Packet_ReadProtocolId(uint8_t buffer[NBN_PACKET_MAX_SIZE], unsigned int size)
{
    if (size < NBN_PACKET_HEADER_SIZE)
        return 0;

    NBN_ReadStream r_stream;

    NBN_ReadStream_Init(&r_stream, buffer, NBN_PACKET_HEADER_SIZE);

    NBN_PacketHeader header;

    if (NBN_Packet_SerializeHeader(&header, (NBN_Stream *)&r_stream) < 0)
        return 0;

    return header.protocol_id;
}

int NBN_Packet_WriteMessage(NBN_Packet *packet, NBN_Message *message, NBN_MessageSerializer msg_serializer)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE || packet->sealed)
        return NBN_PACKET_WRITE_ERROR;

    int current_number_of_bits = packet->m_stream.number_of_bits;

    if (NBN_Message_Measure(message, &packet->m_stream, msg_serializer) < 0)
        return NBN_PACKET_WRITE_ERROR;

    if (
            packet->header.messages_count >= NBN_MAX_MESSAGES_PER_PACKET ||
            packet->m_stream.number_of_bits > NBN_PACKET_MAX_USER_DATA_SIZE * 8)
    {
        packet->m_stream.number_of_bits = current_number_of_bits;

        return NBN_PACKET_WRITE_NO_SPACE;
    }

    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)&packet->w_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    if (NBN_Message_SerializeData(message, (NBN_Stream *)&packet->w_stream, msg_serializer) < 0)
        return NBN_PACKET_WRITE_ERROR;

    packet->size = (packet->m_stream.number_of_bits - 1) / 8 + 1;
    packet->header.messages_count++;

    return NBN_PACKET_WRITE_OK;
}

int NBN_Packet_Seal(NBN_Packet *packet, NBN_Connection *connection)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE)
        return NBN_ERROR;

    if (NBN_WriteStream_Flush(&packet->w_stream) < 0)
        return NBN_ERROR;

    bool is_encrypted = connection->endpoint->config.is_encryption_enabled && connection->can_encrypt;

    packet->header.is_encrypted = is_encrypted;
    packet->size += NBN_PACKET_HEADER_SIZE;

    if (is_encrypted)
    {
        NBN_Packet_ComputeIV(packet, connection);
        NBN_Packet_Encrypt(packet, connection);
        NBN_Packet_Authenticate(packet, connection);
    }

    NBN_WriteStream header_w_stream;

    NBN_WriteStream_Init(&header_w_stream, packet->buffer, NBN_PACKET_HEADER_SIZE);

    if (NBN_Packet_SerializeHeader(&packet->header, (NBN_Stream *)&header_w_stream) < 0)
        return NBN_ERROR;

    if (NBN_WriteStream_Flush(&header_w_stream) < 0)
        return NBN_ERROR;

    packet->sealed = true;

    return 0;
}

static int NBN_Packet_SerializeHeader(NBN_PacketHeader *header, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&header->protocol_id, sizeof(header->protocol_id));
    SERIALIZE_BYTES(&header->seq_number, sizeof(header->seq_number));
    SERIALIZE_BYTES(&header->ack, sizeof(header->ack));
    SERIALIZE_BYTES(&header->ack_bits, sizeof(header->ack_bits));
    SERIALIZE_BYTES(&header->messages_count, sizeof(header->messages_count));
    SERIALIZE_BYTES(&header->is_encrypted, sizeof(header->is_encrypted));

    /* Do not serialize authentication tag when packet is not encrypted to save some bandwith */
    if (header->is_encrypted)
        SERIALIZE_BYTES(&header->auth_tag, sizeof(header->auth_tag));

    return 0;
}

void NBN_Packet_Encrypt(NBN_Packet *packet, NBN_Connection *connection)
{
    struct AES_ctx aes_ctx;

    AES_init_ctx_iv(&aes_ctx, connection->keys1.shared_key, packet->aes_iv);

    unsigned int bytes_to_encrypt = packet->size - NBN_PACKET_HEADER_SIZE;
    unsigned int added_bytes = (bytes_to_encrypt % AES_BLOCKLEN == 0) ? 0 : (AES_BLOCKLEN - bytes_to_encrypt % AES_BLOCKLEN);

    bytes_to_encrypt += added_bytes;

    assert(bytes_to_encrypt % AES_BLOCKLEN == 0);
    assert(bytes_to_encrypt < NBN_PACKET_MAX_DATA_SIZE);

    packet->size = NBN_PACKET_HEADER_SIZE + bytes_to_encrypt;

    assert(packet->size < NBN_PACKET_MAX_SIZE); 

    memset((packet->buffer + packet->size) - added_bytes, 0, added_bytes);

    AES_CBC_encrypt_buffer(&aes_ctx, packet->buffer + NBN_PACKET_HEADER_SIZE, bytes_to_encrypt);

    NBN_LogTrace("Encrypted packet %d (%d bytes)", packet->header.seq_number, packet->size);  
}

void NBN_Packet_Decrypt(NBN_Packet *packet, NBN_Connection *connection)
{
    struct AES_ctx aes_ctx;

    AES_init_ctx_iv(&aes_ctx, connection->keys1.shared_key, packet->aes_iv);

    unsigned int bytes_to_decrypt = packet->size - NBN_PACKET_HEADER_SIZE;

    assert(bytes_to_decrypt % AES_BLOCKLEN == 0);
    assert(bytes_to_decrypt < NBN_PACKET_MAX_DATA_SIZE);

    AES_CBC_decrypt_buffer(&aes_ctx, packet->buffer + NBN_PACKET_HEADER_SIZE, bytes_to_decrypt);

    NBN_LogTrace("Decrypted packet %d (%d bytes)", packet->header.seq_number, packet->size);
}

void NBN_Packet_ComputeIV(NBN_Packet *packet, NBN_Connection *connection)
{
    struct AES_ctx aes_ctx;

    AES_init_ctx_iv(&aes_ctx, connection->keys2.shared_key, connection->aes_iv);

    memset(packet->aes_iv, 0, AES_BLOCKLEN);
    memcpy(packet->aes_iv, (uint8_t *)&packet->header.seq_number, sizeof(packet->header.seq_number));

    AES_CBC_encrypt_buffer(&aes_ctx, packet->aes_iv, AES_BLOCKLEN);
}

void NBN_Packet_Authenticate(NBN_Packet *packet, NBN_Connection *connection)
{ 
    uint8_t poly1305_key[POLY1305_KEYLEN] = {0};

    NBN_Packet_ComputePoly1305Key(packet, connection, poly1305_key);

    memset(packet->header.auth_tag, 0, POLY1305_TAGLEN);

    poly1305_auth(
            packet->header.auth_tag,
            packet->buffer + NBN_PACKET_HEADER_SIZE,
            packet->size - NBN_PACKET_HEADER_SIZE,
            poly1305_key);
}

bool NBN_Packet_CheckAuthentication(NBN_Packet *packet, NBN_Connection *connection)
{
    uint8_t poly1305_key[POLY1305_KEYLEN] = {0};
    uint8_t auth_tag[POLY1305_TAGLEN] = {0};

    NBN_Packet_ComputePoly1305Key(packet, connection, poly1305_key);

    poly1305_auth(
            auth_tag,
            packet->buffer + NBN_PACKET_HEADER_SIZE,
            packet->size - NBN_PACKET_HEADER_SIZE,
            poly1305_key);

    return memcmp(packet->header.auth_tag, auth_tag, POLY1305_TAGLEN) == 0;
}

void NBN_Packet_ComputePoly1305Key(NBN_Packet *packet, NBN_Connection *connection, uint8_t *poly1305_key)
{
    struct AES_ctx aes_ctx;

    memcpy(poly1305_key, (uint8_t *)&packet->header.seq_number, sizeof(packet->header.seq_number));

    AES_init_ctx_iv(&aes_ctx, connection->keys3.shared_key, connection->aes_iv);
    AES_CBC_encrypt_buffer(&aes_ctx, poly1305_key, POLY1305_KEYLEN);
}

#pragma endregion /* NBN_Packet */

#pragma region NBN_Message

int NBN_Message_SerializeHeader(NBN_MessageHeader *message_header, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&message_header->id, sizeof(message_header->id));
    SERIALIZE_BYTES(&message_header->type, sizeof(message_header->type));
    SERIALIZE_BYTES(&message_header->channel_id, sizeof(message_header->channel_id));

    return 0;
}

int NBN_Message_Measure(NBN_Message *message, NBN_MeasureStream *m_stream, NBN_MessageSerializer msg_serializer)
{
    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)m_stream) < 0)
        return NBN_ERROR;

    if (NBN_Message_SerializeData(message, (NBN_Stream *)m_stream, msg_serializer) < 0)
        return NBN_ERROR;

    return m_stream->number_of_bits;
}

int NBN_Message_SerializeData(NBN_Message *message, NBN_Stream *stream, NBN_MessageSerializer msg_serializer)
{
    return msg_serializer(message->data, stream);
}

#pragma endregion /* NBN_Message */

#pragma region NBN_MessageChunk

NBN_MessageChunk *NBN_MessageChunk_Create(void)
{
    NBN_MessageChunk *chunk = NBN_MemoryManager_Alloc(NBN_MEM_MESSAGE_CHUNK);

    chunk->outgoing_msg = NULL;

    return chunk;
}

void NBN_MessageChunk_Destroy(NBN_MessageChunk *chunk)
{
    NBN_MemoryManager_Dealloc(chunk, NBN_MEM_MESSAGE_CHUNK);
}

#pragma endregion /* NBN_MessageChunk */

#pragma region NBN_ClientClosedMessage

NBN_ClientClosedMessage *NBN_ClientClosedMessage_Create(void)
{
    return NBN_Allocator(sizeof(NBN_ClientClosedMessage));
}

void NBN_ClientClosedMessage_Destroy(NBN_ClientClosedMessage *msg)
{
    return NBN_Deallocator(msg);
}

#pragma endregion /* NBN_ClientClosedMessage */

#pragma region NBN_ClientAcceptedMessage

NBN_ClientAcceptedMessage *NBN_ClientAcceptedMessage_Create(void)
{
    return NBN_Allocator(sizeof(NBN_ClientAcceptedMessage));
}

void NBN_ClientAcceptedMessage_Destroy(NBN_ClientAcceptedMessage *msg)
{
    return NBN_Deallocator(msg);
}

#pragma endregion /* NBN_ClientAcceptedMessage */

#pragma region NBN_ByteArrayMessage

NBN_ByteArrayMessage *NBN_ByteArrayMessage_Create(void)
{
    return NBN_MemoryManager_Alloc(NBN_MEM_BYTE_ARRAY_MESSAGE);
}

void NBN_ByteArrayMessage_Destroy(NBN_ByteArrayMessage *msg)
{
    NBN_MemoryManager_Dealloc(msg, NBN_MEM_BYTE_ARRAY_MESSAGE);
}

#pragma endregion /* NBN_ByteArrayMessage */

#pragma region NBN_PublicCryptoInfoMessage

NBN_PublicCryptoInfoMessage *NBN_PublicCryptoInfoMessage_Create(void)
{
    return NBN_Allocator(sizeof(NBN_PublicCryptoInfoMessage));
}

void NBN_PublicCryptoInfoMessage_Destroy(NBN_PublicCryptoInfoMessage *msg)
{
    return NBN_Deallocator(msg);
}

#pragma endregion /* NBN_PublicCryptoInfoMessage */

#pragma region NBN_StartEncryptMessage

NBN_StartEncryptMessage *NBN_StartEncryptMessage_Create(void)
{
    return NBN_Allocator(sizeof(NBN_StartEncryptMessage));
}

void NBN_StartEncryptMessage_Destroy(NBN_StartEncryptMessage *msg)
{
    return NBN_Deallocator(msg);
}

#pragma endregion /* NBN_StartEncryptMessage */

#pragma region NBN_Connection

static uint32_t NBN_Connection_BuildPacketAckBits(NBN_Connection *);
static int NBN_Connection_DecodePacketHeader(NBN_Connection *, NBN_Packet *);
static int NBN_Connection_AckPacket(NBN_Connection *, uint16_t);
static void NBN_Connection_InitOutgoingPacket(NBN_Connection *, NBN_Packet *, NBN_PacketEntry **);
static NBN_PacketEntry *NBN_Connection_InsertOutgoingPacketEntry(NBN_Connection *, uint16_t);
static bool NBN_Connection_InsertReceivedPacketEntry(NBN_Connection *, uint16_t);
static NBN_PacketEntry *NBN_Connection_FindSendPacketEntry(NBN_Connection *, uint16_t);
static bool NBN_Connection_IsPacketReceived(NBN_Connection *, uint16_t);
static int NBN_Connection_SendPacket(NBN_Connection *, NBN_Packet *, NBN_PacketEntry *);
static int NBN_Connection_ReadNextMessageFromStream(NBN_Connection *, NBN_ReadStream *, NBN_Message *);
static int NBN_Connection_ReadNextMessageFromPacket(NBN_Connection *, NBN_Packet *, NBN_Message *);
static int NBN_Connection_RecycleMessage(NBN_Connection *, NBN_Message *);
static void NBN_Connection_UpdateAveragePing(NBN_Connection *, double);
static void NBN_Connection_UpdateAveragePacketLoss(NBN_Connection *, uint16_t);
static void NBN_Connection_UpdateAverageUploadBandwidth(NBN_Connection *, float);
static void NBN_Connection_UpdateAverageDownloadBandwidth(NBN_Connection *);

/* Encryption related functions */

static int NBN_Connection_GenerateKeys(NBN_Connection *);
static int NBN_Connection_GenerateKeySet(NBN_ConnectionKeySet *, CSPRNG *);
static int NBN_Connection_BuildSharedKey(NBN_ConnectionKeySet *, uint8_t *);
static void NBN_Connection_StartEncryption(NBN_Connection *);

static int ecdh_generate_keys(uint8_t*, uint8_t*);
static int ecdh_shared_secret(const uint8_t*, const uint8_t*, uint8_t*);

static CSPRNG csprng_create();
static CSPRNG csprng_destroy(CSPRNG object);
static int csprng_get(CSPRNG, void*, unsigned long long);

NBN_Connection *NBN_Connection_Create(uint32_t id, uint32_t protocol_id, void *driver_data, NBN_Endpoint *endpoint)
{
    NBN_Connection *connection = NBN_MemoryManager_Alloc(NBN_MEM_CONNECTION);

    connection->id = id;
    connection->protocol_id = protocol_id;
    connection->driver_data = driver_data;
    connection->endpoint = endpoint;
    connection->last_recv_packet_time = 0;
    connection->next_packet_seq_number = 1;
    connection->last_received_packet_seq_number = 0;
    connection->last_flush_time = 0;
    connection->last_read_packets_time = 0;
    connection->downloaded_bytes = 0;
    connection->time = 0;
    connection->is_accepted = false;
    connection->is_stale = false;
    connection->is_closed = false;

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
        connection->channels[i] = NULL;

    for (int i = 0; i < NBN_MAX_PACKET_ENTRIES; i++)
    {
        connection->packet_send_seq_buffer[i] = 0xFFFFFFFF;
        connection->packet_recv_seq_buffer[i] = 0xFFFFFFFF;
    }

    connection->stats = (NBN_ConnectionStats){0};
    connection->can_decrypt = false;
    connection->can_encrypt = false;

    if (endpoint->config.is_encryption_enabled)
    {
        if (NBN_Connection_GenerateKeys(connection)  < 0)
        {
            NBN_LogError("Failed to generate keys");

            NBN_Deallocator(connection);

            return NULL;
        }
    } 

    return connection;
}

void NBN_Connection_Destroy(NBN_Connection *connection)
{
    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        if (connection->channels[i])
            NBN_Deallocator(connection->channels[i]);
    }

    NBN_MemoryManager_Dealloc(connection, NBN_MEM_CONNECTION);
}

int NBN_Connection_ProcessReceivedPacket(NBN_Connection *connection, NBN_Packet *packet)
{
    if (NBN_Connection_DecodePacketHeader(connection, packet) < 0)
    {
        NBN_LogError("Failed to decode packet %d header", packet->header.seq_number);

        return NBN_ERROR;
    }

    NBN_Connection_UpdateAveragePacketLoss(connection, packet->header.ack);

    if (!NBN_Connection_InsertReceivedPacketEntry(connection, packet->header.seq_number))
        return 0;

    if (SEQUENCE_NUMBER_GT(packet->header.seq_number, connection->last_received_packet_seq_number))
        connection->last_received_packet_seq_number = packet->header.seq_number;

    for (int i = 0; i < packet->header.messages_count; i++)
    {
        NBN_Message message;

        message.outgoing_msg = NULL;

        if (NBN_Connection_ReadNextMessageFromPacket(connection, packet, &message) < 0)
        {
            NBN_LogError("Failed to read message from packet");

            return NBN_ERROR;
        }

        NBN_Channel *channel = connection->channels[message.header.channel_id];

        if (channel->AddReceivedMessage(channel, &message))
        {
            NBN_LogTrace("Received message %d on channel %d : added to recv queue", message.header.id, channel->id);

#ifdef NBN_DEBUG
            if (connection->OnMessageAddedToRecvQueue)
                connection->OnMessageAddedToRecvQueue(connection, &message);
#endif
        }
        else
        {
            NBN_LogTrace("Received message %d : discarded", message.header.id);

            NBN_Connection_RecycleMessage(connection, &message);
        }
    }

    return 0;
}

int NBN_Connection_EnqueueOutgoingMessage(NBN_Connection *connection, NBN_Channel *channel, NBN_Message *message)
{
    assert(!connection->is_closed || message->header.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    assert(!connection->is_stale);

    NBN_LogTrace("Enqueue message of type %d on channel %d", message->header.type, channel->id);

    if (!channel->AddOutgoingMessage(channel, message))
    {
        NBN_LogError("Failed to enqueue outgoing message of type %d on channel %d",
                message->header.type, message->header.channel_id);

        return NBN_ERROR;
    }

    return 0;
}

int NBN_Connection_FlushSendQueue(NBN_Connection *connection)
{
    NBN_LogTrace("Flushing the send queue");

    NBN_Packet packet;
    NBN_PacketEntry *packet_entry;
    unsigned int sent_packet_count = 0;
    unsigned int sent_bytes = 0;

    NBN_Connection_InitOutgoingPacket(connection, &packet, &packet_entry);

    for (unsigned int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        NBN_Channel *channel = connection->channels[i];

        if (channel == NULL)
            continue;

        NBN_LogTrace("Flushing channel %d (message count: %d)", channel->id, channel->outgoing_message_count);

        NBN_Message *message;

        while (
                sent_packet_count < NBN_CONNECTION_MAX_SENT_PACKET_COUNT &&
                (message = channel->GetNextOutgoingMessage(channel)) != NULL
              )
        {
            bool message_sent = false;
            NBN_MessageSerializer msg_serializer = connection->endpoint->message_serializers[message->header.type];

            assert(msg_serializer);

            int ret = NBN_Packet_WriteMessage(&packet, message, msg_serializer);

            if (ret == NBN_PACKET_WRITE_OK)
            {
                message_sent = true;
            }
            else if (ret == NBN_PACKET_WRITE_NO_SPACE)
            {
                if (NBN_Connection_SendPacket(connection, &packet, packet_entry) < 0)
                {
                    NBN_LogError("Failed to send packet %d", packet.header.seq_number);

                    return NBN_ERROR;
                }

                sent_packet_count++;
                sent_bytes += packet.size;

                NBN_Connection_InitOutgoingPacket(connection, &packet, &packet_entry);

                int ret = NBN_Packet_WriteMessage(&packet, message, msg_serializer);

                if (ret != NBN_PACKET_WRITE_OK)
                {
                    NBN_LogError("Failed to send packet %d", packet.header.seq_number);

                    return NBN_ERROR;
                }

                message_sent = true;
            }
            else if (ret == NBN_PACKET_WRITE_ERROR)
            {
                NBN_LogError("Failed to write message %d of type %d to packet %d",
                        message->header.id, message->header.type, packet.header.seq_number);

                return NBN_ERROR;
            }

            if (message_sent)
            {
                NBN_LogTrace("Message %d added to packet %d", message->header.id, packet.header.seq_number);

                NBN_Channel_UpdateMessageLastSendTime(channel, message, connection->time);

                packet_entry->messages[packet_entry->messages_count++] = (NBN_MessageEntry){
                    .id = message->header.id, .channel_id = channel->id
                };

                if (channel->type == NBN_CHANNEL_TYPE_UNRELIABLE_ORDERED)
                    NBN_Connection_RecycleMessage(connection, message);
            }
        }
    }

    if (NBN_Connection_SendPacket(connection, &packet, packet_entry) < 0)
    {
        NBN_LogError("Failed to send packet %d", packet.header.seq_number);

        return NBN_ERROR;
    }

    sent_bytes += packet.size;
    sent_packet_count++;

    double t = connection->time - connection->last_flush_time;

    if (t > 0)
        NBN_Connection_UpdateAverageUploadBandwidth(connection, sent_bytes / t);

    connection->last_flush_time = connection->time;

    return 0;
}

int NBN_Connection_CreateChannel(NBN_Connection *connection, NBN_ChannelType type, uint8_t id)
{
    if (id > NBN_MAX_CHANNELS - 1)
        return NBN_ERROR;

    NBN_Channel *channel;

    switch (type)
    {
        case NBN_CHANNEL_TYPE_UNRELIABLE_ORDERED:
            channel = (NBN_Channel *)NBN_UnreliableOrderedChannel_Create();
            break;

        case NBN_CHANNEL_TYPE_RELIABLE_ORDERED:
            channel = (NBN_Channel *)NBN_ReliableOrderedChannel_Create();
            break;

        default:
            NBN_LogError("Unsupported message channel type: %d", type);

            return NBN_ERROR;
    }

    channel->id = id;
    channel->type = type;
    channel->connection = connection;

    channel->read_chunk_buffer = NBN_Allocator(NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE);
    channel->write_chunk_buffer = NBN_Allocator(NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE);

    channel->read_chunk_buffer_size = NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE;
    channel->write_chunk_buffer_size = NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE;
    channel->next_outgoing_chunked_message = 0;
    channel->next_outgoing_message_id = 0;
    channel->next_recv_message_id = 0;
    channel->outgoing_message_count = 0;
    channel->chunk_count = 0;
    channel->last_received_chunk_id = -1;

    for (unsigned int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
    {
        channel->recved_message_slot_buffer[i].free = true;
        channel->outgoing_message_slot_buffer[i].free = true;
    }

    for (int i = 0; i < NBN_CHANNEL_CHUNKS_BUFFER_SIZE; i++)
        channel->recv_chunk_buffer[i] = NULL;

    connection->channels[id] = channel;

    return 0;
}

bool NBN_Connection_CheckIfStale(NBN_Connection *connection)
{
#if defined(NBN_DEBUG) && defined(NBN_DISABLE_STALE_CONNECTION_DETECTION)
    /* When testing under bad network conditions (in soak test for instance), we don't want to deal
       with stale connections */
    return false;
#else
    return connection->time - connection->last_recv_packet_time > NBN_CONNECTION_STALE_TIME_THRESHOLD;
#endif
}

void NBN_Connection_AddTime(NBN_Connection *connection, double time)
{
    connection->time += time;

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        NBN_Channel *channel = connection->channels[i];

        if (channel != NULL)
            NBN_Channel_AddTime(channel, time);
    }
}

static int NBN_Connection_DecodePacketHeader(NBN_Connection *connection, NBN_Packet *packet)
{
    if (NBN_Connection_AckPacket(connection, packet->header.ack) < 0)
    {
        NBN_LogError("Failed to ack packet %d", packet->header.seq_number);

        return NBN_ERROR;
    }

    for (unsigned int i = 0; i < 32; i++)
    {
        if (B_IS_UNSET(packet->header.ack_bits, i))
            continue;

        if (NBN_Connection_AckPacket(connection, packet->header.ack - (i + 1)) < 0)
        {
            NBN_LogError("Failed to ack packet %d", packet->header.seq_number);

            return NBN_ERROR;
        }
    }

    return 0;
}

static uint32_t NBN_Connection_BuildPacketAckBits(NBN_Connection *connection)
{
    uint32_t ack_bits = 0;

    for (int i = 0; i < 32; i++)
    {
        /* 
           when last_received_packet_seq_number is lower than 32, the value of acked_packet_seq_number will eventually
           wrap around, which means the packets from before the wrap around will naturally be acked
           */

        uint16_t acked_packet_seq_number = connection->last_received_packet_seq_number - (i + 1);

        if (NBN_Connection_IsPacketReceived(connection, acked_packet_seq_number))
            B_SET(ack_bits, i);
    }

    return ack_bits;
}

static int NBN_Connection_AckPacket(NBN_Connection *connection, uint16_t ack_packet_seq_number)
{
    NBN_PacketEntry *packet_entry = NBN_Connection_FindSendPacketEntry(connection, ack_packet_seq_number);

    if (packet_entry && !packet_entry->acked)
    {
        NBN_LogTrace("Packet %d acked (connection: %d)", ack_packet_seq_number, connection->id);

        packet_entry->acked = true;

        NBN_Connection_UpdateAveragePing(connection, connection->time - packet_entry->send_time);

        for (unsigned int i = 0; i < packet_entry->messages_count; i++)
        {
            NBN_MessageEntry *msg_entry = &packet_entry->messages[i];
            NBN_Channel *channel = connection->channels[msg_entry->channel_id];

            assert(channel != NULL);

            if (channel->OnOutgoingMessageAcked)
            {
                if (channel->OnOutgoingMessageAcked(channel, msg_entry->id) < 0)
                    return NBN_ERROR;
            }
        }
    }

    return 0;
}

static void NBN_Connection_InitOutgoingPacket(
        NBN_Connection *connection, NBN_Packet *outgoing_packet, NBN_PacketEntry **packet_entry)
{
    NBN_Packet_InitWrite(
            outgoing_packet,
            connection->protocol_id,
            connection->next_packet_seq_number++,
            connection->last_received_packet_seq_number,
            NBN_Connection_BuildPacketAckBits(connection));

    *packet_entry = NBN_Connection_InsertOutgoingPacketEntry(connection, outgoing_packet->header.seq_number);
}

static NBN_PacketEntry *NBN_Connection_InsertOutgoingPacketEntry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    connection->packet_send_seq_buffer[index] = seq_number;
    connection->packet_send_buffer[index] = (NBN_PacketEntry){.acked = false, .messages_count = 0};

    return &connection->packet_send_buffer[index];
}

static bool NBN_Connection_InsertReceivedPacketEntry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    /* Ignore duplicated packets */
    if (connection->packet_recv_seq_buffer[index] != 0xFFFFFFFF &&
            connection->packet_recv_seq_buffer[index] == seq_number)
        return false;

    /*
       Clear entries between the previous highest sequence numbers and new highest one
       to avoid entries staying inside the sequence buffer from before the sequence wrap around
       and break the packet acking logic.
       */
    if (SEQUENCE_NUMBER_GT(seq_number, connection->last_received_packet_seq_number))
    {
        for (uint16_t seq = connection->last_received_packet_seq_number + 1; SEQUENCE_NUMBER_LT(seq, seq_number); seq++)
            connection->packet_recv_seq_buffer[seq % NBN_MAX_PACKET_ENTRIES] = 0xFFFFFFFF;
    }

    connection->packet_recv_seq_buffer[index] = seq_number;

    return true;
}

static NBN_PacketEntry *NBN_Connection_FindSendPacketEntry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    if (connection->packet_send_seq_buffer[index] == seq_number)
        return &connection->packet_send_buffer[index];

    return NULL;
}

static bool NBN_Connection_IsPacketReceived(NBN_Connection *connection, uint16_t packet_seq_number)
{
    uint16_t index = packet_seq_number % NBN_MAX_PACKET_ENTRIES;

    return connection->packet_recv_seq_buffer[index] == packet_seq_number;
}

static int NBN_Connection_SendPacket(NBN_Connection *connection, NBN_Packet *packet, NBN_PacketEntry *packet_entry)
{
    NBN_LogTrace("Send packet %d to connection %d (messages count: %d)",
            packet->header.seq_number, connection->id, packet->header.messages_count);

    assert(packet_entry->messages_count == packet->header.messages_count);

    if (NBN_Packet_Seal(packet, connection) < 0)
    {
        NBN_LogError("Failed to seal packet");

        return NBN_ERROR;
    }

    packet_entry->send_time = connection->time;

    if (connection->endpoint->is_server)
    {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return NBN_PacketSimulator_EnqueuePacket(&__game_server.endpoint.packet_simulator, packet, connection);
#else
        if (connection->is_stale)
            return 0;

        return NBN_Driver_GServ_SendPacketTo(packet, connection);
#endif
    }
    else
    {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return NBN_PacketSimulator_EnqueuePacket(&__game_client.endpoint.packet_simulator, packet, connection);
#else
        return NBN_Driver_GCli_SendPacket(packet);
#endif
    }
}

static int NBN_Connection_ReadNextMessageFromStream(
        NBN_Connection *connection, NBN_ReadStream *r_stream, NBN_Message *message)
{
    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)r_stream) < 0)
    {
        NBN_LogError("Failed to read message header");

        return -1;
    }
    
    uint8_t msg_type = message->header.type;
    NBN_MessageBuilder msg_builder = connection->endpoint->message_builders[msg_type];

    if (msg_builder == NULL)
    {
        NBN_LogError("No message builder is registered for messages of type %d", msg_type);

        return -1;
    }

    NBN_MessageSerializer msg_serializer = connection->endpoint->message_serializers[msg_type];

    if (msg_serializer == NULL)
    {
        NBN_LogError("No message serializer attached to message of type %d", msg_type);

        return -1;
    }

    NBN_Channel *channel = connection->channels[message->header.channel_id];

    if (channel == NULL)
    {
        NBN_LogError("Channel %d does not exist", message->header.channel_id);

        return -1;
    }

    message->data = msg_builder();

    if (msg_serializer(message->data, (NBN_Stream *)r_stream) < 0)
    {
        NBN_LogError("Failed to read message body");

        return -1;
    }

    return 0;
}

int NBN_Connection_ReadNextMessageFromPacket(NBN_Connection *connection, NBN_Packet *packet, NBN_Message *message)
{
    return NBN_Connection_ReadNextMessageFromStream(connection, &packet->r_stream, message);
}

static int NBN_Connection_RecycleMessage(NBN_Connection *connection, NBN_Message *message)
{
    assert(message->outgoing_msg == NULL || message->outgoing_msg->ref_count > 0);

    if (message->outgoing_msg == NULL || --message->outgoing_msg->ref_count == 0)
    {
        if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
        {
            NBN_MessageChunk *chunk = message->data;

            if (chunk->outgoing_msg)
            {
                assert(chunk->outgoing_msg->ref_count > 0);

                if (--chunk->outgoing_msg->ref_count == 0)
                {
                    NBN_MessageDestructor msg_destructor =
                        connection->endpoint->message_destructors[chunk->outgoing_msg->type];

                    if (msg_destructor)
                        msg_destructor(chunk->outgoing_msg->data);
                }
            }
        }

        NBN_MessageDestructor msg_destructor = connection->endpoint->message_destructors[message->header.type];

        if (msg_destructor)
            msg_destructor(message->data);
    }

    return 0;
}

static void NBN_Connection_UpdateAveragePing(NBN_Connection *connection, double ping)
{
    /* exponential smoothing with a factor of 0.05 */
    connection->stats.ping = connection->stats.ping + .05f * (ping - connection->stats.ping);
}

static void NBN_Connection_UpdateAveragePacketLoss(NBN_Connection *connection, uint16_t seq)
{
    unsigned int lost_packet_count = 0;
    uint16_t start_seq = seq - 64;

    for (int i = 0; i < 100; i++)
    {
        uint16_t s = start_seq - i;
        NBN_PacketEntry *entry = NBN_Connection_FindSendPacketEntry(connection, s);

        if (entry && !entry->acked)
            lost_packet_count++;
    }

    float packet_loss = lost_packet_count / 100.f;

    /* exponential smoothing with a factor of 0.1 */
    connection->stats.packet_loss = connection->stats.packet_loss + .1f * (packet_loss - connection->stats.packet_loss);
}

static void NBN_Connection_UpdateAverageUploadBandwidth(NBN_Connection *connection, float bytes_per_sec)
{
    /* exponential smoothing with a factor of 0.1 */
    connection->stats.upload_bandwidth =
        connection->stats.upload_bandwidth + .1f * (bytes_per_sec - connection->stats.upload_bandwidth);
}

static void NBN_Connection_UpdateAverageDownloadBandwidth(NBN_Connection *connection)
{
    double t = connection->time - connection->last_read_packets_time;

    if (t == 0)
        return;

    float bytes_per_sec = connection->downloaded_bytes / t;

    /* exponential smoothing with a factor of 0.1 */
    connection->stats.download_bandwidth =
        connection->stats.download_bandwidth + .1f * (bytes_per_sec - connection->stats.download_bandwidth);

    connection->downloaded_bytes = 0;
}

static int NBN_Connection_GenerateKeys(NBN_Connection *connection)
{
    CSPRNG *prng = csprng_create();

    if (!prng)
    {
        NBN_LogError("Failed to initialize pseudo random number generator");

        return -1;
    }

    if (NBN_Connection_GenerateKeySet(&connection->keys1, prng) < 0)
        return -1;

    if (NBN_Connection_GenerateKeySet(&connection->keys2, prng) < 0)
        return -1; 

    if (NBN_Connection_GenerateKeySet(&connection->keys3, prng) < 0)
        return -1;

    csprng_get(prng, connection->aes_iv, AES_BLOCKLEN);
    csprng_destroy(prng);

    return 0;
}

static int NBN_Connection_GenerateKeySet(NBN_ConnectionKeySet *key_set, CSPRNG *prng)
{
    /* Generate a random private key */
    csprng_get(prng, key_set->prv_key, ECC_PRV_KEY_SIZE);

    if (!ecdh_generate_keys(key_set->pub_key, key_set->prv_key))
    {
        NBN_LogError("Failed to generate public and private keys");

        return -1;
    }

    return 0;
}

static int NBN_Connection_BuildSharedKey(NBN_ConnectionKeySet *key_set, uint8_t *pub_key)
{
    if (!ecdh_shared_secret(key_set->prv_key, pub_key, key_set->shared_key))
        return -1;

    return 0;
}

static void NBN_Connection_StartEncryption(NBN_Connection *connection)
{
    connection->can_encrypt = true;
    
    NBN_LogDebug("Encryption started for connection %d", connection->id);
}

#pragma endregion /* NBN_Connection */

#pragma region NBN_AcceptData

NBN_AcceptData *NBN_AcceptData_Create(void)
{
    NBN_AcceptData *accept_data = NBN_Allocator(sizeof(NBN_AcceptData));

    memset(accept_data->buffer, 0, NBN_ACCEPT_DATA_MAX_SIZE);
    NBN_WriteStream_Init(&accept_data->write_stream, accept_data->buffer, NBN_ACCEPT_DATA_MAX_SIZE);

    return accept_data;
}

NBN_AcceptData *NBN_AcceptData_Read(uint8_t *buffer)
{
    NBN_AcceptData *accept_data = NBN_Allocator(sizeof(NBN_AcceptData));

    memcpy(accept_data->buffer, buffer, NBN_ACCEPT_DATA_MAX_SIZE);
    NBN_ReadStream_Init(&accept_data->read_stream, accept_data->buffer, NBN_ACCEPT_DATA_MAX_SIZE);

    return accept_data;
}

void NBN_AcceptData_Destroy(NBN_AcceptData *accept_data)
{
    NBN_Deallocator(accept_data);
}

void NBN_AcceptData_WriteUInt(NBN_AcceptData *accept_data, unsigned int v)
{
    int ret = NBN_WriteStream_SerializeUint(&accept_data->write_stream, &v, 0, UINT_MAX);

    assert(ret == 0);
}

void NBN_AcceptData_WriteInt(NBN_AcceptData *accept_data, int v)
{
    int ret = NBN_WriteStream_SerializeInt(&accept_data->write_stream, &v, INT_MIN, INT_MAX);

    assert(ret == 0);
}

void NBN_AcceptData_WriteFloat(NBN_AcceptData *accept_data, float v)
{
    unsigned int mult = pow(10, 3);
    int ret = NBN_WriteStream_SerializeFloat(&accept_data->write_stream, &v, INT_MIN / mult, INT_MAX / mult, 3);

    assert(ret == 0);
}

void NBN_AcceptData_WriteBytes(NBN_AcceptData *accept_data, uint8_t *bytes, unsigned int length)
{
    int ret = NBN_WriteStream_SerializeBytes(&accept_data->write_stream, bytes, length);

    assert(ret == 0);
}

unsigned int NBN_AcceptData_ReadUInt(NBN_AcceptData *accept_data)
{
    uint32_t v;
    int ret = NBN_ReadStream_SerializeUint(&accept_data->read_stream, &v, 0, UINT_MAX);

    assert(ret == 0);

    return v;
}

int NBN_AcceptData_ReadInt(NBN_AcceptData *accept_data)
{
    int v;
    int ret = NBN_ReadStream_SerializeInt(&accept_data->read_stream, &v, INT_MIN, INT_MAX);

    assert(ret == 0);

    return v;
}

float NBN_AcceptData_ReadFloat(NBN_AcceptData *accept_data)
{
    float v;
    unsigned int mult = pow(10, 3);
    int ret = NBN_ReadStream_SerializeFloat(&accept_data->read_stream, &v, INT_MIN / mult, INT_MAX / mult, 3);

    assert(ret == 0);

    return v;
}

uint8_t *NBN_AcceptData_ReadBytes(NBN_AcceptData *accept_data, uint8_t *buffer, unsigned int length)
{
    int ret = NBN_ReadStream_SerializeBytes(&accept_data->read_stream, buffer, length);

    assert(ret == 0);

    return buffer;
}

#pragma endregion /* NBN_AcceptData */

#pragma region NBN_Channel

void NBN_Channel_Destroy(NBN_Channel *channel)
{
    for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
    {
        NBN_MessageSlot *slot = &channel->recved_message_slot_buffer[i];

        if (!slot->free)
            NBN_Connection_RecycleMessage(channel->connection, &slot->message);

        slot = &channel->outgoing_message_slot_buffer[i];

        if (!slot->free)
            NBN_Connection_RecycleMessage(channel->connection, &slot->message);
    }

    NBN_Deallocator(channel);
}

void NBN_Channel_AddTime(NBN_Channel *channel, double time)
{
    channel->time += time;
}

bool NBN_Channel_AddChunk(NBN_Channel *channel, NBN_Message *chunk_msg)
{
    assert(chunk_msg->header.type == NBN_MESSAGE_CHUNK_TYPE);

    NBN_MessageChunk *chunk = chunk_msg->data;

    NBN_LogTrace("Add chunk %d to channel %d (current chunk count: %d, last recved chunk id: %d)",
            chunk->id, channel->id, channel->chunk_count, channel->last_received_chunk_id);

    if (chunk->id == channel->last_received_chunk_id + 1)
    {
        assert(channel->recv_chunk_buffer[chunk->id] == NULL);

        channel->recv_chunk_buffer[chunk->id] = chunk;
        channel->last_received_chunk_id++;
        channel->chunk_count++;

        NBN_LogTrace("Chunk added (%d/%d)", channel->chunk_count, chunk->total);

        if (channel->chunk_count == chunk->total)
        {
            channel->last_received_chunk_id = -1;

            return true;
        }

        return false;
    }
    else
    {
        NBN_LogTrace("Chunk ignored");
    }

    /* Clear the chunks buffer */
    for (unsigned int i = 0; i < channel->chunk_count; i++)
    {
        assert(channel->recv_chunk_buffer[i] != NULL);

        NBN_MessageChunk_Destroy(channel->recv_chunk_buffer[i]);

        channel->recv_chunk_buffer[i] = NULL;
    }

    channel->chunk_count = 0;
    channel->last_received_chunk_id = -1;

    if (chunk->id == 0)
        return NBN_Channel_AddChunk(channel, chunk_msg);

    return false;
}

int NBN_Channel_ReconstructMessageFromChunks(
        NBN_Channel *channel, NBN_Connection *connection, NBN_Message *message)
{
    unsigned int message_size = channel->chunk_count * NBN_MESSAGE_CHUNK_SIZE;

    if (message_size > channel->read_chunk_buffer_size)
        NBN_Channel_ResizeReadChunkBuffer(channel, message_size);

    NBN_LogTrace("Reconstructing message (chunk count: %d, size: %d) from channel %d",
            channel->chunk_count, message_size, channel->id);

    for (unsigned int i = 0; i < channel->chunk_count; i++)
    {
        NBN_MessageChunk *chunk = channel->recv_chunk_buffer[i];

        memcpy(channel->read_chunk_buffer + i * NBN_MESSAGE_CHUNK_SIZE, chunk->data, NBN_MESSAGE_CHUNK_SIZE);

        NBN_MessageChunk_Destroy(chunk);

        channel->recv_chunk_buffer[i] = NULL;
    }

    channel->chunk_count = 0;

    NBN_ReadStream r_stream;

    NBN_ReadStream_Init(&r_stream, channel->read_chunk_buffer, message_size);

    if (NBN_Connection_ReadNextMessageFromStream(connection, &r_stream, message) < 0)
        return NBN_ERROR;

    NBN_LogTrace("Reconstructed message %d of type %d", message->header.id, message->header.type);

    return 0;
}

void NBN_Channel_ResizeWriteChunkBuffer(NBN_Channel *channel, unsigned int size)
{
    channel->write_chunk_buffer = NBN_Reallocator(channel->write_chunk_buffer, size);

    channel->write_chunk_buffer_size = size;
}

void NBN_Channel_ResizeReadChunkBuffer(NBN_Channel *channel, unsigned int size)
{
    channel->read_chunk_buffer = NBN_Reallocator(channel->read_chunk_buffer, size);

    channel->read_chunk_buffer_size = size;
}

void NBN_Channel_UpdateMessageLastSendTime(NBN_Channel *channel, NBN_Message *message, double time)
{
    NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[message->header.id % NBN_CHANNEL_BUFFER_SIZE];

    assert(slot->message.header.id == message->header.id);

    slot->last_send_time = time;
}

/* Unreliable ordered */

static bool NBN_UnreliableOrderedChannel_AddReceivedMessage(NBN_Channel *, NBN_Message *);
static bool NBN_UnreliableOrderedChannel_AddOutgoingMessage(NBN_Channel *, NBN_Message *);
static NBN_Message *NBN_UnreliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *);
static NBN_Message *NBN_UnreliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *);

NBN_UnreliableOrderedChannel *NBN_UnreliableOrderedChannel_Create(void)
{
    NBN_UnreliableOrderedChannel *channel = NBN_Allocator(sizeof(NBN_UnreliableOrderedChannel));

    channel->base.AddReceivedMessage = NBN_UnreliableOrderedChannel_AddReceivedMessage;
    channel->base.AddOutgoingMessage = NBN_UnreliableOrderedChannel_AddOutgoingMessage;
    channel->base.GetNextRecvedMessage = NBN_UnreliableOrderedChannel_GetNextRecvedMessage;
    channel->base.GetNextOutgoingMessage = NBN_UnreliableOrderedChannel_GetNextOutgoingMessage;
    channel->base.OnOutgoingMessageAcked = NULL;

    channel->last_received_message_id = 0;
    channel->next_outgoing_message_slot = 0;

    return channel;
}

static bool NBN_UnreliableOrderedChannel_AddReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    NBN_UnreliableOrderedChannel *unreliable_ordered_channel = (NBN_UnreliableOrderedChannel *)channel; 

    if (SEQUENCE_NUMBER_GT(message->header.id, unreliable_ordered_channel->last_received_message_id))
    {
        NBN_MessageSlot *slot = &channel->recved_message_slot_buffer[message->header.id % NBN_CHANNEL_BUFFER_SIZE];

        memcpy(&slot->message, message, sizeof(NBN_Message));

        slot->free = false;

        unreliable_ordered_channel->last_received_message_id = message->header.id;

        return true;
    }

    return false;
}

static bool NBN_UnreliableOrderedChannel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *message)
{
    uint16_t msg_id = channel->next_outgoing_message_id;
    NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE];

    memcpy(&slot->message, message, sizeof(NBN_Message));

    slot->message.header.id = msg_id;
    slot->free = false;

    channel->next_outgoing_message_id++;
    channel->outgoing_message_count++;

    return true;
}

static NBN_Message *NBN_UnreliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *channel)
{
    NBN_UnreliableOrderedChannel *unreliable_ordered_channel = (NBN_UnreliableOrderedChannel *)channel;

    while (SEQUENCE_NUMBER_LT(channel->next_recv_message_id, unreliable_ordered_channel->last_received_message_id))
    {
        NBN_MessageSlot *slot = &channel->recved_message_slot_buffer[channel->next_recv_message_id % NBN_CHANNEL_BUFFER_SIZE];

        if (!slot->free && slot->message.header.id == channel->next_recv_message_id)
        {
            slot->free = true;

            return &slot->message;
        }

        channel->next_recv_message_id++;
    }

    return NULL;
}

static NBN_Message *NBN_UnreliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *channel)
{
    NBN_UnreliableOrderedChannel *unreliable_ordered_channel = (NBN_UnreliableOrderedChannel *)channel;

    NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[unreliable_ordered_channel->next_outgoing_message_slot];

    if (slot->free)
        return NULL;

    slot->free = true;

    unreliable_ordered_channel->next_outgoing_message_slot =
        (unreliable_ordered_channel->next_outgoing_message_slot + 1) % NBN_CHANNEL_BUFFER_SIZE;

    return &slot->message;
}

/* Reliable ordered */

static bool NBN_ReliableOrderedChannel_AddReceivedMessage(NBN_Channel *, NBN_Message *);
static bool NBN_ReliableOrderedChannel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *);
static NBN_Message *NBN_ReliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *);
static NBN_Message *NBN_ReliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *);
static int NBN_ReliableOrderedChannel_OnOutgoingMessageAcked(NBN_Channel *, uint16_t);

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(void)
{
    NBN_ReliableOrderedChannel *channel = NBN_Allocator(sizeof(NBN_ReliableOrderedChannel));

    channel->base.AddReceivedMessage = NBN_ReliableOrderedChannel_AddReceivedMessage;
    channel->base.AddOutgoingMessage = NBN_ReliableOrderedChannel_AddOutgoingMessage;
    channel->base.GetNextRecvedMessage = NBN_ReliableOrderedChannel_GetNextRecvedMessage;
    channel->base.GetNextOutgoingMessage = NBN_ReliableOrderedChannel_GetNextOutgoingMessage;
    channel->base.OnOutgoingMessageAcked = NBN_ReliableOrderedChannel_OnOutgoingMessageAcked;

    memset(channel->base.recved_message_slot_buffer, 0, sizeof(channel->base.recved_message_slot_buffer));

    for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
        channel->ack_buffer[i] = false;

    channel->oldest_unacked_message_id = 0;
    channel->most_recent_message_id = 0;

    return channel;
}

static unsigned int ComputeMessageIdDelta(uint16_t id1, uint16_t id2)
{
    if (SEQUENCE_NUMBER_GT(id1, id2))
        return (id1 >= id2) ? id1 - id2 : ((0xFFFF + 1) - id2) + id1;
    else
        return (id2 >= id1) ? id2 - id1 : (((0xFFFF + 1) - id1) + id2) % 0xFFFF;
}

static bool NBN_ReliableOrderedChannel_AddReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    unsigned int dt = ComputeMessageIdDelta(message->header.id, reliable_ordered_channel->most_recent_message_id);

    NBN_LogTrace("Add recved message %d of type %d to channel %d (most recent msg id: %d, dt: %d)",
            message->header.id, message->header.type, channel->id, reliable_ordered_channel->most_recent_message_id, dt);

    if (SEQUENCE_NUMBER_GT(message->header.id, reliable_ordered_channel->most_recent_message_id))
    {
        assert(dt < NBN_CHANNEL_BUFFER_SIZE);

        reliable_ordered_channel->most_recent_message_id = message->header.id;
    }
    else
    {
        /* This is an old message that has already been received, probably coming from
           an out of order late packet. */
        if (dt >= NBN_CHANNEL_BUFFER_SIZE)
            return false;
    }

    NBN_MessageSlot *slot = &channel->recved_message_slot_buffer[message->header.id % NBN_CHANNEL_BUFFER_SIZE]; 

    if (!slot->free)
        NBN_Connection_RecycleMessage(channel->connection, &slot->message);

    memcpy(&slot->message, message, sizeof(NBN_Message));

    slot->free = false;

    return true;
}

static bool NBN_ReliableOrderedChannel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *message)
{
    uint16_t msg_id = channel->next_outgoing_message_id;
    int index = msg_id % NBN_CHANNEL_BUFFER_SIZE;
    NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[index];

    if (!slot->free)
    {
        NBN_LogTrace("Slot %d is not free on channel %d (slot msg id: %d, outgoing message count: %d)",
                index, channel->id, slot->message.header.id, channel->outgoing_message_count);

        return false;
    }

    memcpy(&slot->message, message, sizeof(NBN_Message));

    slot->message.header.id = msg_id;
    slot->last_send_time = -1;
    slot->free = false;

    channel->next_outgoing_message_id++;
    channel->outgoing_message_count++;

    NBN_LogTrace("Added outgoing message %d of type %d to channel %d (index: %d)",
            slot->message.header.id, slot->message.header.type, channel->id, index);

    return true;
}

static NBN_Message *NBN_ReliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *channel)
{
    NBN_MessageSlot *slot = &channel->recved_message_slot_buffer[channel->next_recv_message_id % NBN_CHANNEL_BUFFER_SIZE];

    if (!slot->free && slot->message.header.id == channel->next_recv_message_id)
    {
        slot->free = true;
        channel->next_recv_message_id++;

        return &slot->message;
    }

    return NULL;
}

static NBN_Message *NBN_ReliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *channel)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;

    int max_message_id = (reliable_ordered_channel->oldest_unacked_message_id + (NBN_CHANNEL_BUFFER_SIZE - 1)) % (0xFFFF + 1);

    if (SEQUENCE_NUMBER_LT(channel->next_outgoing_message_id, max_message_id))
        max_message_id = channel->next_outgoing_message_id;

    uint16_t msg_id = reliable_ordered_channel->oldest_unacked_message_id;

    while (SEQUENCE_NUMBER_LT(msg_id, max_message_id))
    {
        NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE];

        if (
                !slot->free &&
                (slot->last_send_time < 0 || channel->time - slot->last_send_time >= NBN_MESSAGE_RESEND_DELAY)
           )
        {
            return &slot->message;
        }

        msg_id++;
    }

    return NULL;
}

static int NBN_ReliableOrderedChannel_OnOutgoingMessageAcked(NBN_Channel *channel, uint16_t msg_id)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    NBN_MessageSlot *slot = &reliable_ordered_channel->base.outgoing_message_slot_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE];

    if (slot->free || slot->message.header.id != msg_id)
        return 0;

    slot->free = true;

    NBN_LogTrace("Message %d acked on channel %d (buffer index: %d, oldest unacked: %d)",
        msg_id, channel->id, msg_id % NBN_CHANNEL_BUFFER_SIZE, reliable_ordered_channel->oldest_unacked_message_id);

    reliable_ordered_channel->ack_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE] = true;
    channel->outgoing_message_count--;

    if (msg_id == reliable_ordered_channel->oldest_unacked_message_id)
    {
        for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
        {
            uint16_t ack_msg_id = msg_id + i;
            int index = ack_msg_id % NBN_CHANNEL_BUFFER_SIZE;

            if (reliable_ordered_channel->ack_buffer[index])
            {
                reliable_ordered_channel->ack_buffer[index] = false;
                reliable_ordered_channel->oldest_unacked_message_id++;
            }
            else
            {
                break;
            }
        }

        NBN_LogTrace("Updated oldest unacked message id on channel %d: %d",
                channel->id, reliable_ordered_channel->oldest_unacked_message_id);
    }

    return NBN_Connection_RecycleMessage(channel->connection, &slot->message);
}

#pragma endregion /* NBN_MessageChannel */

#pragma region NBN_EventQueue

void NBN_EventQueue_Init(NBN_EventQueue *event_queue)
{
    event_queue->head = 0;
    event_queue->tail = 0;
    event_queue->count = 0;
}

bool NBN_EventQueue_Enqueue(NBN_EventQueue *event_queue, NBN_Event ev)
{
    if (event_queue->count >= NBN_EVENT_QUEUE_CAPACITY)
        return false;

    event_queue->events[event_queue->tail] = ev;

    event_queue->tail = (event_queue->tail + 1) % NBN_EVENT_QUEUE_CAPACITY;
    event_queue->count++;

    return true;
}

bool NBN_EventQueue_Dequeue(NBN_EventQueue *event_queue, NBN_Event *ev)
{
    if (NBN_EventQueue_IsEmpty(event_queue))
        return false;

    memcpy(ev, &event_queue->events[event_queue->head], sizeof(NBN_Event));
    event_queue->head = (event_queue->head + 1) % NBN_EVENT_QUEUE_CAPACITY;
    event_queue->count--;

    return true;
}

bool NBN_EventQueue_IsEmpty(NBN_EventQueue *event_queue)
{
    return event_queue->count == 0;
}

#pragma endregion /* NBN_EventQueue */

#pragma region NBN_Endpoint

static uint32_t NBN_Endpoint_BuildProtocolId(const char *);
static int NBN_Endpoint_ProcessReceivedPacket(NBN_Endpoint *, NBN_Packet *, NBN_Connection *);
static NBN_OutgoingMessage *NBN_Endpoint_CreateOutgoingMessage(NBN_Endpoint *, uint8_t, void *);
static int NBN_Endpoint_EnqueueOutgoingMessage(NBN_Endpoint *, NBN_Connection *, NBN_OutgoingMessage *, uint8_t);
static int NBN_Endpoint_SplitMessageIntoChunks(
        NBN_Message *, NBN_OutgoingMessage *, NBN_Channel *, NBN_MessageSerializer, unsigned int, NBN_MessageChunk **);

void NBN_Endpoint_Init(NBN_Endpoint *endpoint, NBN_Config config, bool is_server)
{
    NBN_MemoryManager_Init();

    endpoint->config = config;
    endpoint->is_server = is_server;
    endpoint->next_outgoing_message = 0;

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
        endpoint->channels[i] = NBN_CHANNEL_TYPE_UNDEFINED;

    for (int i = 0; i < NBN_MAX_MESSAGE_TYPES; i++)
    {
        endpoint->message_builders[i] = NULL;
        endpoint->message_destructors[i] = NULL;
        endpoint->message_serializers[i] = NULL;
    }

    NBN_EventQueue_Init(&endpoint->event_queue);

    /* Register library reserved channels */
    NBN_Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_TYPE_UNRELIABLE_ORDERED, NBN_CHANNEL_RESERVED_UNRELIABLE);
    NBN_Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_TYPE_RELIABLE_ORDERED, NBN_CHANNEL_RESERVED_RELIABLE);
    NBN_Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_TYPE_RELIABLE_ORDERED, NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES);

    /* Register NBN_MessageChunk library message */
    NBN_Endpoint_RegisterMessageBuilder(
            endpoint, (NBN_MessageBuilder)NBN_MessageChunk_Create, NBN_MESSAGE_CHUNK_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
            endpoint, (NBN_MessageSerializer)NBN_MessageChunk_Serialize, NBN_MESSAGE_CHUNK_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
            endpoint, (NBN_MessageDestructor)NBN_MessageChunk_Destroy, NBN_MESSAGE_CHUNK_TYPE);

    /* Register NBN_ClientClosedMessage library message */
    NBN_Endpoint_RegisterMessageBuilder(
            endpoint, (NBN_MessageBuilder)NBN_ClientClosedMessage_Create, NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
            endpoint, (NBN_MessageSerializer)NBN_ClientClosedMessage_Serialize, NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
            endpoint, (NBN_MessageDestructor)NBN_ClientClosedMessage_Destroy, NBN_CLIENT_CLOSED_MESSAGE_TYPE);

    /* Register NBN_ClientAcceptedMessage library message */
    NBN_Endpoint_RegisterMessageBuilder(
            endpoint, (NBN_MessageBuilder)NBN_ClientAcceptedMessage_Create, NBN_CLIENT_ACCEPTED_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
            endpoint, (NBN_MessageSerializer)NBN_ClientAcceptedMessage_Serialize, NBN_CLIENT_ACCEPTED_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
            endpoint, (NBN_MessageDestructor)NBN_ClientAcceptedMessage_Destroy, NBN_CLIENT_ACCEPTED_MESSAGE_TYPE);

    /* Register NBN_ByteArrayMessage library message */
    NBN_Endpoint_RegisterMessageBuilder(
            endpoint, (NBN_MessageBuilder)NBN_ByteArrayMessage_Create, NBN_BYTE_ARRAY_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
            endpoint, (NBN_MessageSerializer)NBN_ByteArrayMessage_Serialize, NBN_BYTE_ARRAY_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
            endpoint, (NBN_MessageDestructor)NBN_ByteArrayMessage_Destroy, NBN_BYTE_ARRAY_MESSAGE_TYPE);

    /* Register NBN_PublicCryptoInfoMessage library message */
    NBN_Endpoint_RegisterMessageBuilder(
            endpoint, (NBN_MessageBuilder)NBN_PublicCryptoInfoMessage_Create, NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
            endpoint, (NBN_MessageSerializer)NBN_PublicCryptoInfoMessage_Serialize, NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
            endpoint, (NBN_MessageDestructor)NBN_PublicCryptoInfoMessage_Destroy, NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE);

    /* Register NBN_StartEncryptMessage library message */
    NBN_Endpoint_RegisterMessageBuilder(
            endpoint, (NBN_MessageBuilder)NBN_StartEncryptMessage_Create, NBN_START_ENCRYPT_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
            endpoint, (NBN_MessageSerializer)NBN_StartEncryptMessage_Serialize, NBN_START_ENCRYPT_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
            endpoint, (NBN_MessageDestructor)NBN_StartEncryptMessage_Destroy, NBN_START_ENCRYPT_MESSAGE_TYPE);

#ifdef NBN_DEBUG
    endpoint->OnMessageAddedToRecvQueue = NULL;
#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_Init(&endpoint->packet_simulator);
    NBN_PacketSimulator_Start(&endpoint->packet_simulator);
#endif
}

void NBN_Endpoint_Deinit(NBN_Endpoint *endpoint)
{
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_Stop(&endpoint->packet_simulator);
#endif

    NBN_MemoryManager_Deinit();
}

void NBN_Endpoint_RegisterMessageBuilder(NBN_Endpoint *endpoint, NBN_MessageBuilder msg_builder, uint8_t msg_type)
{
    endpoint->message_builders[msg_type] = msg_builder;
}

void NBN_Endpoint_RegisterMessageDestructor(NBN_Endpoint *endpoint, NBN_MessageDestructor msg_destructor, uint8_t msg_type)
{
    endpoint->message_destructors[msg_type] = msg_destructor;
}

void NBN_Endpoint_RegisterMessageSerializer(
        NBN_Endpoint *endpoint, NBN_MessageSerializer msg_serializer, uint8_t msg_type)
{
    endpoint->message_serializers[msg_type] = msg_serializer;
}

void NBN_Endpoint_RegisterChannel(NBN_Endpoint *endpoint, NBN_ChannelType type, uint8_t id)
{
    assert(endpoint->channels[id] == NBN_CHANNEL_TYPE_UNDEFINED);

    endpoint->channels[id] = type;
}

NBN_Connection *NBN_Endpoint_CreateConnection(NBN_Endpoint *endpoint, uint32_t id, void *driver_data)
{
    NBN_Connection *connection = NBN_Connection_Create(
            id, NBN_Endpoint_BuildProtocolId(endpoint->config.protocol_name), driver_data, endpoint);

    for (int chan_id = 0; chan_id < NBN_MAX_CHANNELS; chan_id++)
    {
        NBN_ChannelType channel_type = endpoint->channels[chan_id];

        if (channel_type != NBN_CHANNEL_TYPE_UNDEFINED)
            NBN_Connection_CreateChannel(connection, channel_type, chan_id);
    }

    return connection;
}

static uint32_t NBN_Endpoint_BuildProtocolId(const char *protocol_name)
{
    uint32_t protocol_id = 2166136261;

    for (unsigned int i = 0; i < strlen(protocol_name); i++)
    {
        protocol_id *= 16777619;
        protocol_id ^= protocol_name[i];
    }

    return protocol_id;
}

static int NBN_Endpoint_ProcessReceivedPacket(NBN_Endpoint *endpoint, NBN_Packet *packet, NBN_Connection *connection)
{
    (void)endpoint;

    NBN_LogTrace("Received packet %d (conn id: %d, ack: %d, messages count: %d)", packet->header.seq_number,
            connection->id, packet->header.ack, packet->header.messages_count);

    if (NBN_Connection_ProcessReceivedPacket(connection, packet) < 0)
        return NBN_ERROR;

    connection->last_recv_packet_time = connection->time;
    connection->downloaded_bytes += packet->size;

    return 0;
}

static NBN_OutgoingMessage *NBN_Endpoint_CreateOutgoingMessage(NBN_Endpoint *endpoint, uint8_t msg_type, void *data)
{
    if (endpoint->message_serializers[msg_type] == NULL)
    {
        NBN_LogError("No message serializer is registered for messages of type %d", msg_type);

        return NULL;
    }

    NBN_OutgoingMessage *outgoing_message = &endpoint->outgoing_message_buffer[endpoint->next_outgoing_message];

    outgoing_message->type = msg_type;
    outgoing_message->data = data;
    outgoing_message->ref_count = 0;

    endpoint->next_outgoing_message = (endpoint->next_outgoing_message + 1) % NBN_ENDPOINT_OUTGOING_MESSAGE_BUFFER_SIZE;

    return outgoing_message;
}

static int NBN_Endpoint_EnqueueOutgoingMessage(
        NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_OutgoingMessage *outgoing_msg, uint8_t channel_id)
{
    NBN_Channel *channel = connection->channels[channel_id];

    if (channel == NULL)
    {
        NBN_LogError("Channel %d does not exist", channel_id);

        return NBN_ERROR;
    }

    NBN_MessageSerializer msg_serializer = endpoint->message_serializers[outgoing_msg->type];

    assert(msg_serializer); 

    NBN_Message message = {
        .header = { 0, outgoing_msg->type, channel_id },
        .outgoing_msg = outgoing_msg,
        .data = outgoing_msg->data
    };

    NBN_MeasureStream m_stream;

    NBN_MeasureStream_Init(&m_stream);

    unsigned int message_size = (NBN_Message_Measure(&message, &m_stream, msg_serializer) - 1) / 8 + 1;

    if (message_size > NBN_PACKET_MAX_USER_DATA_SIZE)
    {
        NBN_MessageChunk *chunks[NBN_CHANNEL_CHUNKS_BUFFER_SIZE];
        int chunk_count = NBN_Endpoint_SplitMessageIntoChunks(
                &message, outgoing_msg, channel, msg_serializer, message_size, chunks);

        assert(chunk_count <= NBN_CHANNEL_CHUNKS_BUFFER_SIZE);

        if (chunk_count < 0)
        {
            NBN_LogError("Failed to split message into chunks");

            return NBN_ERROR;
        }

        outgoing_msg->ref_count += chunk_count;

        for (int i = 0; i < chunk_count; i++)
        {
            NBN_OutgoingMessage *chunk_outgoing_msg = NBN_Endpoint_CreateOutgoingMessage(
                    endpoint, NBN_MESSAGE_CHUNK_TYPE, chunks[i]);

            if (chunk_outgoing_msg == NULL)
                return NBN_ERROR;

            if (NBN_Endpoint_EnqueueOutgoingMessage(endpoint, connection, chunk_outgoing_msg, channel_id) < 0)
            {
                NBN_LogError("Failed to enqueue message chunk");

                return NBN_ERROR;
            }
        }

        return 0;
    }
    else
    {
        outgoing_msg->ref_count++;

        return NBN_Connection_EnqueueOutgoingMessage(connection, channel, &message);
    }
}

static int NBN_Endpoint_SplitMessageIntoChunks(
        NBN_Message *message,
        NBN_OutgoingMessage *outgoing_msg,
        NBN_Channel *channel,
        NBN_MessageSerializer msg_serializer,
        unsigned int message_size,
        NBN_MessageChunk **chunks)
{
    unsigned int chunk_count = ((message_size - 1) / NBN_MESSAGE_CHUNK_SIZE) + 1;

    NBN_LogTrace("Split message into %d chunks (message size: %d)", chunk_count, message_size);

    if (chunk_count > NBN_CHANNEL_CHUNKS_BUFFER_SIZE)
    {
        NBN_LogError("The maximum number of chunks is 255");

        return NBN_ERROR;
    }

    if (message_size > channel->write_chunk_buffer_size)
        NBN_Channel_ResizeWriteChunkBuffer(channel, message_size);

    NBN_WriteStream w_stream;

    NBN_WriteStream_Init(&w_stream, channel->write_chunk_buffer, message_size);

    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)&w_stream) < 0)
        return NBN_ERROR;

    if (NBN_Message_SerializeData(message, (NBN_Stream *)&w_stream, msg_serializer) < 0)
        return NBN_ERROR;

    for (unsigned int i = 0; i < chunk_count; i++)
    {
        NBN_MessageChunk *chunk = NBN_MessageChunk_Create();

        chunk->id = i;
        chunk->total = chunk_count;
        chunk->outgoing_msg = outgoing_msg;

        unsigned int offset = i * NBN_MESSAGE_CHUNK_SIZE;
        unsigned int chunk_size = MIN(NBN_MESSAGE_CHUNK_SIZE, message_size - offset); 

        assert(chunk_size <= NBN_MESSAGE_CHUNK_SIZE);

        memcpy(chunk->data, channel->write_chunk_buffer + offset, chunk_size);

        NBN_LogTrace("Enqueue chunk %d (size: %d, total: %d) for message %d of type %d",
                chunk->id, chunk_size, chunk->total, message->header.id, message->header.type);

        chunks[i] = chunk;
    }

    return chunk_count;
}

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

NBN_GameClient __game_client;

NBN_Event client_last_event;
static uint8_t client_last_received_message_type;
static int client_closed_code = -1;

static int NBN_GameClient_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static int NBN_GameClient_HandleEvent(void);
static int NBN_GameClient_HandleMessageReceivedEvent(void);

static int NBN_GameClient_SendCryptoPublicInfo(void);
static void NBN_GameClient_StartEncryption(void);

void NBN_GameClient_Init(const char *protocol_name, const char *ip_address, uint16_t port)
{
    NBN_Config config = {
        .protocol_name = protocol_name,
        .ip_address = ip_address,
        .port = port,
        .is_encryption_enabled = false
    };

    NBN_Endpoint_Init(&__game_client.endpoint, config, false);
    __game_client.accept_data = NULL;
    __game_client.server_connection = NULL;
}

void NBN_GameClient_Deinit(void)
{
    NBN_Endpoint_Deinit(&__game_client.endpoint);

    if (__game_client.server_connection)
        NBN_Connection_Destroy(__game_client.server_connection);

    if (__game_client.accept_data != NULL)
        NBN_AcceptData_Destroy(__game_client.accept_data);
}

int NBN_GameClient_Start(void)
{
    NBN_Config config = __game_client.endpoint.config;

    if (NBN_Driver_GCli_Start(NBN_Endpoint_BuildProtocolId(config.protocol_name), config.ip_address, config.port) < 0)
        return NBN_ERROR;

    NBN_LogInfo("Started");

    return 0;
}

void NBN_GameClient_Stop(void)
{
    NBN_Driver_GCli_Stop();
    NBN_GameClient_Poll(); /* Poll one last time to clear remaining events */

    NBN_LogInfo("Stopped");
}

void NBN_GameClient_RegisterMessage(
        uint8_t msg_type,
        NBN_MessageBuilder msg_builder,
        NBN_MessageDestructor msg_destructor,
        NBN_MessageSerializer msg_serializer)
{
    if (NBN_IsReservedMessage(msg_type))
    {
        NBN_LogError("Message type %d is reserved by the library", msg_type);
        NBN_Abort();
    }

    NBN_Endpoint_RegisterMessageBuilder(&__game_client.endpoint, msg_builder, msg_type);
    NBN_Endpoint_RegisterMessageDestructor(&__game_client.endpoint, msg_destructor, msg_type);
    NBN_Endpoint_RegisterMessageSerializer(&__game_client.endpoint, msg_serializer, msg_type);
}

void NBN_GameClient_RegisterChannel(uint8_t type, uint8_t id)
{
    if (id == NBN_CHANNEL_RESERVED_UNRELIABLE || id == NBN_CHANNEL_RESERVED_RELIABLE || id == NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES)
    {
        NBN_LogError("Channel id %d is reserved by the library", type);
        NBN_Abort();
    }

    NBN_Endpoint_RegisterChannel(&__game_client.endpoint, type, id);
}

void NBN_GameClient_AddTime(double time)
{
    NBN_Connection_AddTime(__game_client.server_connection, time);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_AddTime(&__game_client.endpoint.packet_simulator, time);
#endif
}

int NBN_GameClient_Poll(void)
{
    if (__game_client.server_connection->is_stale)
        return NBN_NO_EVENT;

    if (NBN_EventQueue_IsEmpty(&__game_client.endpoint.event_queue))
    {
        if (NBN_Connection_CheckIfStale(__game_client.server_connection))
        {
            __game_client.server_connection->is_stale = true;

            NBN_LogInfo("Server connection is stale. Disconnected.");

            if (!NBN_EventQueue_Enqueue(&__game_client.endpoint.event_queue, (NBN_Event){ NBN_DISCONNECTED }))
                return NBN_ERROR;
        }
        else
        {
            if (NBN_Driver_GCli_RecvPackets() < 0)
                return NBN_ERROR;

            for (unsigned int i = 0; i < NBN_MAX_CHANNELS; i++)
            {
                NBN_Channel *channel = __game_client.server_connection->channels[i];

                if (channel)
                {
                    NBN_Message *msg;

                    while ((msg = channel->GetNextRecvedMessage(channel)) != NULL)
                    {
                        NBN_LogTrace("Got message %d of type %d from the recv queue", msg->header.id, msg->header.type);

                        if (NBN_GameClient_ProcessReceivedMessage(msg, __game_client.server_connection) < 0)
                        {
                            NBN_LogError("Failed to process received message");

                            return NBN_ERROR;
                        }
                    }
                }
            }

            NBN_Connection_UpdateAverageDownloadBandwidth(__game_client.server_connection);

            __game_client.server_connection->last_read_packets_time = __game_client.server_connection->time;
        }
    }

    bool ret = NBN_EventQueue_Dequeue(&__game_client.endpoint.event_queue, &client_last_event);

    return ret ? NBN_GameClient_HandleEvent() : NBN_NO_EVENT;
}

int NBN_GameClient_SendPackets(void)
{
    return NBN_Connection_FlushSendQueue(__game_client.server_connection);
}

NBN_OutgoingMessage *NBN_GameClient_CreateMessage(uint8_t msg_type, void *msg_data)
{
    return NBN_Endpoint_CreateOutgoingMessage(&__game_client.endpoint, msg_type, msg_data);
}

int NBN_GameClient_SendMessage(NBN_OutgoingMessage *outgoing_msg, uint8_t channel_id)
{
    if (NBN_Endpoint_EnqueueOutgoingMessage(
                &__game_client.endpoint, __game_client.server_connection, outgoing_msg, channel_id) < 0)
    {
        NBN_LogError("Failed to create outgoing message");

        return -1;
    }

    return 0;
}

NBN_OutgoingMessage *NBN_GameClient_CreateByteArrayMessage(uint8_t *bytes, unsigned int length)
{
    if (length > NBN_BYTE_ARRAY_MAX_SIZE)
    {
        NBN_LogError("Byte array cannot exceed %d bytes", NBN_BYTE_ARRAY_MAX_SIZE);

        return NULL;
    }

    NBN_ByteArrayMessage *msg = NBN_ByteArrayMessage_Create();

    memcpy(msg->bytes, bytes, length);

    msg->length = length;

    return NBN_GameClient_CreateMessage(NBN_BYTE_ARRAY_MESSAGE_TYPE, msg);
}

int NBN_GameClient_SendUnreliableMessage(NBN_OutgoingMessage *outgoing_msg)
{
    return NBN_GameClient_SendMessage(outgoing_msg, NBN_CHANNEL_RESERVED_UNRELIABLE);
}

int NBN_GameClient_SendReliableMessage(NBN_OutgoingMessage *outgoing_msg)
{
    return NBN_GameClient_SendMessage(outgoing_msg, NBN_CHANNEL_RESERVED_RELIABLE);
}

NBN_Connection *NBN_GameClient_CreateServerConnection(void *driver_data)
{
    NBN_Connection *server_connection = NBN_Endpoint_CreateConnection(&__game_client.endpoint, 0, driver_data);

#ifdef NBN_DEBUG
    server_connection->OnMessageAddedToRecvQueue = __game_client.endpoint.OnMessageAddedToRecvQueue;
#endif

    __game_client.server_connection = server_connection;

    return server_connection;
}

NBN_MessageInfo NBN_GameClient_GetMessageInfo(void)
{
    assert(client_last_event.type == NBN_MESSAGE_RECEIVED);

    return client_last_event.data.message_info;
}

NBN_ConnectionStats NBN_GameClient_GetStats(void)
{
    return __game_client.server_connection->stats;
}

int NBN_GameClient_GetServerCloseCode(void)
{
    return client_closed_code;
}

NBN_AcceptData *NBN_GameClient_GetAcceptData(void)
{
    return __game_client.accept_data;
}

bool NBN_GameClient_IsEncryptionEnabled(void)
{
    return __game_client.endpoint.config.is_encryption_enabled;
}

void NBN_GameClient_EnableEncryption(void)
{
    __game_client.endpoint.config.is_encryption_enabled = true;
}

#ifdef NBN_DEBUG

void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
        case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
            __game_client.endpoint.OnMessageAddedToRecvQueue = cb;
            break;
    }
}

#endif /* NBN_DEBUG */

static int NBN_GameClient_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *server_connection)
{
    assert(__game_client.server_connection == server_connection);

    NBN_Event ev = { NBN_MESSAGE_RECEIVED };

    if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        NBN_Channel *channel = server_connection->channels[message->header.channel_id];

        if (!NBN_Channel_AddChunk(channel, message))
            return 0;

        NBN_Message complete_message;

        if (NBN_Channel_ReconstructMessageFromChunks(channel, server_connection, &complete_message) < 0)
        {
            NBN_LogError("Failed to reconstruct message from chunks");

            return -1;
        }

        ev.data.message_info = (NBN_MessageInfo){ complete_message.header.type, complete_message.data, NULL };
    }
    else
    {
        ev.data.message_info = (NBN_MessageInfo){ message->header.type, message->data, NULL };
    }

    if (!NBN_EventQueue_Enqueue(&__game_client.endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static int NBN_GameClient_HandleEvent(void)
{
    switch (client_last_event.type)
    {
        case NBN_MESSAGE_RECEIVED:
            return NBN_GameClient_HandleMessageReceivedEvent();

        default:
            return client_last_event.type;
    }
}

static int NBN_GameClient_HandleMessageReceivedEvent(void)
{
    NBN_MessageInfo message_info = client_last_event.data.message_info;

    int ret = NBN_NO_EVENT;

    if (message_info.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE)
    {
        client_closed_code = ((NBN_ClientClosedMessage *)message_info.data)->code;

        ret = NBN_DISCONNECTED;
    }
    else if (message_info.type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE)
    {
        __game_client.accept_data = NBN_AcceptData_Read(((NBN_ClientAcceptedMessage *)message_info.data)->data); 

        ret = NBN_CONNECTED;
    }
    else if (NBN_GameClient_IsEncryptionEnabled() && message_info.type == NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE)
    {
        NBN_LogDebug("Received server's crypto public info");

        if (NBN_GameClient_SendCryptoPublicInfo() < 0)
        {
            NBN_LogError("Failed to send public key to server");
            NBN_Abort();
        }

        NBN_PublicCryptoInfoMessage *pub_crypto_msg = message_info.data;

        if (NBN_Connection_BuildSharedKey(&__game_client.server_connection->keys1, pub_crypto_msg->pub_key1) < 0)
        {
            NBN_LogError("Failed to build shared key (first key)");
            NBN_Abort();
        }

        if (NBN_Connection_BuildSharedKey(&__game_client.server_connection->keys2, pub_crypto_msg->pub_key2) < 0)
        {
            NBN_LogError("Failed to build shared key (second key)");
            NBN_Abort();
        }

        if (NBN_Connection_BuildSharedKey(&__game_client.server_connection->keys3, pub_crypto_msg->pub_key3) < 0)
        {
            NBN_LogError("Failed to build shared key (third key)");
            NBN_Abort();
        }

        NBN_LogTrace("Client can now decrypt packets");

        memcpy(__game_client.server_connection->aes_iv, pub_crypto_msg->aes_iv, AES_BLOCKLEN);
        __game_client.server_connection->can_decrypt = true;
    }
    else if (NBN_GameClient_IsEncryptionEnabled() && message_info.type == NBN_START_ENCRYPT_MESSAGE_TYPE)
    {
        NBN_GameClient_StartEncryption();
    }
    else
    {
        ret = NBN_MESSAGE_RECEIVED;
    }

    return ret;
}

static int NBN_GameClient_SendCryptoPublicInfo(void)
{
    assert(__game_client.server_connection);

    NBN_PublicCryptoInfoMessage *msg = NBN_PublicCryptoInfoMessage_Create(); 

    memcpy(msg->pub_key1, __game_client.server_connection->keys1.pub_key, ECC_PUB_KEY_SIZE);
    memcpy(msg->pub_key2, __game_client.server_connection->keys2.pub_key, ECC_PUB_KEY_SIZE);
    memcpy(msg->pub_key3, __game_client.server_connection->keys3.pub_key, ECC_PUB_KEY_SIZE);

    /* Client does not send an AES IV to the server */
    uint8_t zero_aes_iv[AES_BLOCKLEN] = {0};

    memcpy(msg->aes_iv, zero_aes_iv, AES_BLOCKLEN);

    NBN_OutgoingMessage *outgoing_msg = NBN_GameClient_CreateMessage(NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE, msg);

    if (outgoing_msg == NULL)
        return NBN_ERROR;

    if (NBN_GameClient_SendReliableMessage(outgoing_msg) < 0)
        return NBN_ERROR;

    NBN_LogDebug("Sent client's public key to the server");

    return 0;
}

static void NBN_GameClient_StartEncryption(void)
{
    NBN_Connection_StartEncryption(__game_client.server_connection);
}

#pragma endregion /* NBN_GameClient */

#pragma region Game client driver

static void NBN_Driver_GCli_OnConnected(void);
static void NBN_Driver_GCli_OnPacketReceived(NBN_Packet *packet);

void NBN_Driver_GCli_RaiseEvent(NBN_Driver_GCli_EventType ev, void *data)
{
    switch (ev)
    {
        case NBN_DRIVER_GCLI_CONNECTED:
            NBN_Driver_GCli_OnConnected();
            break;

        case NBN_DRIVER_GCLI_SERVER_PACKET_RECEIVED:
            NBN_Driver_GCli_OnPacketReceived(data);
            break;
    }
}

static void NBN_Driver_GCli_OnConnected(void)
{
}

static void NBN_Driver_GCli_OnPacketReceived(NBN_Packet *packet)
{
    int ret = NBN_Endpoint_ProcessReceivedPacket(&__game_client.endpoint, packet, __game_client.server_connection);

    /* packets from server should always be valid */
    assert(ret == 0);
}

#pragma endregion /* Game Client driver */

#pragma region NBN_GameServer

NBN_GameServer __game_server;

static NBN_Event server_last_event;

static int NBN_GameServer_AddClient(NBN_Connection *);
static bool NBN_GameServer_RemoveClient(NBN_Connection *client);
static unsigned int NBN_GameServer_GetClientCount(void);
static int NBN_GameServer_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static int NBN_GameServer_CloseStaleClientConnections(void);
static void NBN_GameServer_RemoveClosedClientConnections(void);
static int NBN_GameServer_HandleEvent(void);
static int NBN_GameServer_HandleMessageReceivedEvent(void);

static int NBN_GameServer_SendCryptoPublicInfoTo(NBN_Connection *);
static int NBN_GameServer_StartEncryption(NBN_Connection *);

void NBN_GameServer_Init(const char *protocol_name, uint16_t port)
{
    NBN_Config config = { .protocol_name = protocol_name, .port = port, .is_encryption_enabled = false };

    NBN_Endpoint_Init(&__game_server.endpoint, config, true);

    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
        __game_server.clients[i] = NULL;

    __game_server.client_count = 0;
}

void NBN_GameServer_Deinit(void)
{
    NBN_Endpoint_Deinit(&__game_server.endpoint);

    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
            NBN_Connection_Destroy(__game_server.clients[i]);
    }

    __game_server.client_count = 0;
}

int NBN_GameServer_Start(void)
{
    NBN_Config config = __game_server.endpoint.config;

    if (NBN_Driver_GServ_Start(NBN_Endpoint_BuildProtocolId(config.protocol_name), config.port) < 0)
    {
        NBN_LogError("Failed to start network driver");

        return NBN_ERROR;
    }

    NBN_LogInfo("Started");

    return 0;
}

void NBN_GameServer_Stop(void)
{
    NBN_Driver_GServ_Stop();
    NBN_GameServer_Poll(); /* Poll one last time to clear remaining events */

    NBN_LogInfo("Stopped");
}

void NBN_GameServer_RegisterMessage(
        uint8_t msg_type,
        NBN_MessageBuilder msg_builder,
        NBN_MessageDestructor msg_destructor,
        NBN_MessageSerializer msg_serializer)
{
    if (NBN_IsReservedMessage(msg_type))
    {
        NBN_LogError("Message type %d is reserved by the library", msg_type);
        NBN_Abort();
    }

    NBN_Endpoint_RegisterMessageBuilder(&__game_server.endpoint, msg_builder, msg_type);
    NBN_Endpoint_RegisterMessageDestructor(&__game_server.endpoint, msg_destructor, msg_type);
    NBN_Endpoint_RegisterMessageSerializer(&__game_server.endpoint, msg_serializer, msg_type);
}

void NBN_GameServer_RegisterChannel(uint8_t type, uint8_t id)
{
    if (id == NBN_CHANNEL_RESERVED_UNRELIABLE || id == NBN_CHANNEL_RESERVED_RELIABLE || id == NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES)
    {
        NBN_LogError("Channel id %d is reserved by the library", type);
        NBN_Abort();
    }

    NBN_Endpoint_RegisterChannel(&__game_server.endpoint, type, id);
}

void NBN_GameServer_AddTime(double time)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
            NBN_Connection_AddTime(__game_server.clients[i], time);
    }

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_AddTime(&__game_server.endpoint.packet_simulator, time);
#endif
}

int NBN_GameServer_Poll(void)
{
    if (NBN_EventQueue_IsEmpty(&__game_server.endpoint.event_queue))
    {
        if (NBN_GameServer_CloseStaleClientConnections() < 0)
            return NBN_ERROR;

        if (NBN_Driver_GServ_RecvPackets() < 0)
            return NBN_ERROR;

        __game_server.stats.download_bandwidth = 0;

        for (int i = 0; i < NBN_MAX_CLIENTS; i++)
        {
            if (__game_server.clients[i])
            {
                NBN_Connection *client = __game_server.clients[i];

                for (unsigned int i = 0; i < NBN_MAX_CHANNELS; i++)
                {
                    NBN_Channel *channel = client->channels[i];

                    if (channel)
                    {
                        NBN_Message *msg;

                        while ((msg = channel->GetNextRecvedMessage(channel)) != NULL)
                        {
                            if (NBN_GameServer_ProcessReceivedMessage(msg, client) < 0)
                            {
                                NBN_LogError("Failed to process received message");

                                return NBN_ERROR;
                            }
                        }
                    }
                }

                if (!client->is_closed)
                    NBN_Connection_UpdateAverageDownloadBandwidth(client);

                __game_server.stats.download_bandwidth += client->stats.download_bandwidth;
                client->last_read_packets_time = client->time;
            }
        }

        NBN_GameServer_RemoveClosedClientConnections();
    }

    bool ret = NBN_EventQueue_Dequeue(&__game_server.endpoint.event_queue, &server_last_event);

    return ret ? NBN_GameServer_HandleEvent() : NBN_NO_EVENT;
}

int NBN_GameServer_SendPackets(void)
{
    __game_server.stats.upload_bandwidth = 0;

    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
        {
            NBN_Connection *client = __game_server.clients[i];

            if (!client->is_stale && NBN_Connection_FlushSendQueue(client) < 0)
                return NBN_ERROR;

            __game_server.stats.upload_bandwidth += client->stats.upload_bandwidth;
        }
    }

    return 0;
}

NBN_Connection *NBN_GameServer_CreateClientConnection(uint32_t id, void *driver_data)
{
    NBN_Connection *client = NBN_Endpoint_CreateConnection(&__game_server.endpoint, id, driver_data);

#ifdef NBN_DEBUG
    client->OnMessageAddedToRecvQueue = __game_server.endpoint.OnMessageAddedToRecvQueue;
#endif

    return client;
}

int NBN_GameServer_CloseClientWithCode(NBN_Connection *client, int code)
{
    NBN_LogTrace("Closing connection %d", client->id);

    if (!client->is_closed && client->is_accepted)
    {
        if (!NBN_EventQueue_Enqueue(
                &__game_server.endpoint.event_queue,
                (NBN_Event) { NBN_CLIENT_DISCONNECTED, .data = { .connection = client } }))
            return NBN_ERROR;
    }

    if (client->is_stale)
    {
        client->is_closed = true;

        return 0;
    }

    client->is_closed = true;

    NBN_LogDebug("Send close message for client %d (code: %d)", client->id, code);

    NBN_ClientClosedMessage *msg = NBN_ClientClosedMessage_Create();

    msg->code = code;

    NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateMessage(NBN_CLIENT_CLOSED_MESSAGE_TYPE, msg);

    if (outgoing_msg == NULL)
        return NBN_ERROR;

    NBN_GameServer_SendMessageTo(client, outgoing_msg, NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES);

    return 0;
}

int NBN_GameServer_CloseClient(NBN_Connection *client)
{
    return NBN_GameServer_CloseClientWithCode(client, -1);
}

NBN_OutgoingMessage *NBN_GameServer_CreateMessage(uint8_t msg_type, void *msg_data)
{
    return NBN_Endpoint_CreateOutgoingMessage(&__game_server.endpoint, msg_type, msg_data);
}

NBN_OutgoingMessage *NBN_GameServer_CreateByteArrayMessage(uint8_t *bytes, unsigned int length)
{
    if (length > NBN_BYTE_ARRAY_MAX_SIZE)
    {
        NBN_LogError("Byte array cannot exceed %d bytes", NBN_BYTE_ARRAY_MAX_SIZE);

        return NULL;
    }

    NBN_ByteArrayMessage *msg = NBN_ByteArrayMessage_Create();

    memcpy(msg->bytes, bytes, length);

    msg->length = length;

    return NBN_GameServer_CreateMessage(NBN_BYTE_ARRAY_MESSAGE_TYPE, msg);
}

int NBN_GameServer_SendMessageTo(NBN_Connection *client, NBN_OutgoingMessage *outgoing_msg, uint8_t channel_id)
{
    /* The only message type we can send to an unaccepted client is a NBN_ClientAcceptedMessage message
     * or a NBN_PublicCryptoInfoMessage */
    assert(client->is_accepted ||
            outgoing_msg->type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE || NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE);

    if (NBN_Endpoint_EnqueueOutgoingMessage(&__game_server.endpoint, client, outgoing_msg, channel_id) < 0)
    {
        NBN_LogError("Failed to create outgoing message for client %d");

        /* Do not close the client if we failed to send the close client message to avoid infinite loops */
        if (outgoing_msg->type != NBN_CLIENT_CLOSED_MESSAGE_TYPE)
        {
            NBN_GameServer_CloseClient(client);

            return NBN_ERROR;
        }
    }

    return 0;
}

int NBN_GameServer_SendUnreliableMessageTo(NBN_Connection *client, NBN_OutgoingMessage *outgoing_msg)
{
    return NBN_GameServer_SendMessageTo(client, outgoing_msg, NBN_CHANNEL_RESERVED_UNRELIABLE);
}

int NBN_GameServer_SendReliableMessageTo(NBN_Connection *client, NBN_OutgoingMessage *outgoing_msg)
{
    return NBN_GameServer_SendMessageTo(client, outgoing_msg, NBN_CHANNEL_RESERVED_RELIABLE);
}

int NBN_GameServer_BroadcastMessage(NBN_OutgoingMessage *outgoing_msg, uint8_t channel_id)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
        {
            NBN_Connection *client = __game_server.clients[i];

            if (!client->is_closed && !client->is_stale && client->is_accepted)
            {
                if (NBN_GameServer_SendMessageTo(client, outgoing_msg, channel_id) < 0)
                    return NBN_ERROR;
            }
        }
    }

    return 0;
}

int NBN_GameServer_BroadcastUnreliableMessage(NBN_OutgoingMessage *outgoing_msg)
{
    return NBN_GameServer_BroadcastMessage(outgoing_msg, NBN_CHANNEL_RESERVED_UNRELIABLE);
}

int NBN_GameServer_BroadcastReliableMessage(NBN_OutgoingMessage *outgoing_msg)
{
    return NBN_GameServer_BroadcastMessage(outgoing_msg, NBN_CHANNEL_RESERVED_RELIABLE);
}

int NBN_GameServer_AcceptIncomingConnection(NBN_AcceptData *accept_data)
{
    assert(server_last_event.type == NBN_NEW_CONNECTION);
    assert(server_last_event.data.connection != NULL);

    NBN_Connection *client = server_last_event.data.connection;
    NBN_ClientAcceptedMessage *msg = NBN_ClientAcceptedMessage_Create();

    assert(msg != NULL);

    if (accept_data != NULL)
    {
        int ret = NBN_WriteStream_Flush(&accept_data->write_stream);

        assert(ret == 0);

        memcpy(msg->data, accept_data->buffer, NBN_ACCEPT_DATA_MAX_SIZE);
    }

    NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateMessage(NBN_CLIENT_ACCEPTED_MESSAGE_TYPE, msg);

    if (outgoing_msg == NULL)
        return NBN_ERROR;

    if (NBN_GameServer_SendReliableMessageTo(client, outgoing_msg) < 0)
        return NBN_ERROR;

    client->is_accepted = true;

    if (accept_data != NULL)
        NBN_AcceptData_Destroy(accept_data);

    NBN_LogTrace("Client %d has been accepted", client->id);

    return 0;
}

int NBN_GameServer_RejectIncomingConnectionWithCode(int code)
{
    assert(server_last_event.type == NBN_NEW_CONNECTION);
    assert(server_last_event.data.connection != NULL);

    return NBN_GameServer_CloseClientWithCode(server_last_event.data.connection, code);
}

int NBN_GameServer_RejectIncomingConnection(void)
{
    return NBN_GameServer_RejectIncomingConnectionWithCode(-1);
}

NBN_Connection *NBN_GameServer_GetIncomingConnection(void)
{
    assert(server_last_event.type == NBN_NEW_CONNECTION);
    assert(server_last_event.data.connection != NULL);

    return server_last_event.data.connection;
}

NBN_Connection *NBN_GameServer_GetDisconnectedClient(void)
{
    assert(server_last_event.type == NBN_CLIENT_DISCONNECTED);

    return server_last_event.data.connection;
}

NBN_Connection *NBN_GameServer_FindClientById(uint32_t client_id)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
        {
            NBN_Connection *client = __game_server.clients[i];

            if (client->id == client_id)
                return client;
        }
    }

    return NULL;
}

NBN_MessageInfo NBN_GameServer_GetMessageInfo(void)
{
    assert(server_last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    return server_last_event.data.message_info;
}

NBN_GameServerStats NBN_GameServer_GetStats(void)
{
    return __game_server.stats;
}

void NBN_GameServer_EnableEncryption(void)
{
    __game_server.endpoint.config.is_encryption_enabled = true;
}

bool NBN_GameServer_IsEncryptionEnabled(void)
{
    return __game_server.endpoint.config.is_encryption_enabled;
}

#ifdef NBN_DEBUG

void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
        case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
            __game_server.endpoint.OnMessageAddedToRecvQueue = cb;
            break;
    }
}

#endif /* NBN_DEBUG */

static int NBN_GameServer_AddClient(NBN_Connection *client)
{
    if (__game_server.client_count >= NBN_MAX_CLIENTS)
        return NBN_ERROR;

    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i] == NULL)
        {
            __game_server.clients[i] = client;
            __game_server.client_count++;

            break;
        }
    }

    return 0;
}

static bool NBN_GameServer_RemoveClient(NBN_Connection *client)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        NBN_Connection *c = __game_server.clients[i];

        if (c && c->id == client->id)
        {
            __game_server.clients[i] = NULL;
            __game_server.client_count--;

            return true;
        }
    }

    return false;
}

static unsigned int NBN_GameServer_GetClientCount(void)
{
    return __game_server.client_count;
}

static int NBN_GameServer_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *client)
{
    NBN_Event ev = { NBN_CLIENT_MESSAGE_RECEIVED };

    if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        NBN_Channel *channel = client->channels[message->header.channel_id];

        if (!NBN_Channel_AddChunk(channel, message))
            return 0;

        NBN_Message complete_message;

        if (NBN_Channel_ReconstructMessageFromChunks(channel, client, &complete_message) < 0)
        {
            NBN_LogError("Failed to reconstruct message from chunks");

            return -1;
        }

        ev.data.message_info = (NBN_MessageInfo){
            complete_message.header.type, complete_message.data, client
        };
    }
    else
    {
        ev.data.message_info = (NBN_MessageInfo){ message->header.type, message->data, client };
    }

    if (!NBN_EventQueue_Enqueue(&__game_server.endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static int NBN_GameServer_CloseStaleClientConnections(void)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
        {
            NBN_Connection *client = __game_server.clients[i];

            if (!client->is_stale && NBN_Connection_CheckIfStale(client))
            {
                NBN_LogInfo("Client %d connection is stale, closing it.", client->id);

                client->is_stale = true;

                if (NBN_GameServer_CloseClient(client) < 0)
                    return NBN_ERROR;
            }
        }
    }

    return 0;
}

static void NBN_GameServer_RemoveClosedClientConnections(void)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        if (__game_server.clients[i])
        {
            NBN_Connection *client = __game_server.clients[i];

            if (client->is_closed && client->is_stale)
            {
                NBN_LogDebug("Remove closed client connection (ID: %d)", client->id);

                NBN_GameServer_RemoveClient(client);
                NBN_Driver_GServ_DestroyClientConnection(client);
            }
        }
    }
}

static int NBN_GameServer_HandleEvent(void)
{
    switch (server_last_event.type)
    {
        case NBN_CLIENT_MESSAGE_RECEIVED:
            return NBN_GameServer_HandleMessageReceivedEvent();

        default:
            break;
    }

    return server_last_event.type;
}

static int NBN_GameServer_HandleMessageReceivedEvent(void)
{
    NBN_MessageInfo message_info = server_last_event.data.message_info;

    int ret = NBN_CLIENT_MESSAGE_RECEIVED;

    if (NBN_GameServer_IsEncryptionEnabled() && message_info.type == NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE)
    {
        ret = NBN_NO_EVENT;

        NBN_PublicCryptoInfoMessage *pub_crypto_msg = message_info.data;

        if (NBN_Connection_BuildSharedKey(&message_info.sender->keys1, pub_crypto_msg->pub_key1) < 0)
        {
            NBN_LogError("Failed to build shared key (first key)");
            NBN_Abort();
        }

        if (NBN_Connection_BuildSharedKey(&message_info.sender->keys2, pub_crypto_msg->pub_key2) < 0)
        {
            NBN_LogError("Failed to build shared key (second key)");
            NBN_Abort();
        }

        if (NBN_Connection_BuildSharedKey(&message_info.sender->keys3, pub_crypto_msg->pub_key3) < 0)
        {
            NBN_LogError("Failed to build shared key (third key)");
            NBN_Abort();
        }

        NBN_LogDebug("Received public crypto info of client %d", message_info.sender->id);

        if (NBN_GameServer_StartEncryption(message_info.sender))
        {
            NBN_LogError("Failed to start encryption of client %d", message_info.sender->id);
            NBN_Abort();
        }

        message_info.sender->can_decrypt = true;

        if (!NBN_EventQueue_Enqueue(
                &__game_server.endpoint.event_queue,
                (NBN_Event){ NBN_NEW_CONNECTION, .data = { .connection = message_info.sender } }))
            return NBN_ERROR;
    }

    return ret;
}

#pragma endregion /* NBN_GameServer */

#pragma region Game server driver

static int NBN_GServ_Driver_OnClientConnected(NBN_Connection *);
static int NBN_GServ_Driver_OnClientPacketReceived(NBN_Packet *);

int NBN_Driver_GServ_RaiseEvent(NBN_Driver_GServ_EventType ev, void *data)
{
    switch (ev)
    {
        case NBN_DRIVER_GSERV_CLIENT_CONNECTED:
            return NBN_GServ_Driver_OnClientConnected(data);

        case NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED:
            return NBN_GServ_Driver_OnClientPacketReceived(data);
    }

    return 0;
}

static int NBN_GServ_Driver_OnClientConnected(NBN_Connection *client)
{
    if (NBN_GameServer_AddClient(client) < 0)
    {
        NBN_LogError("Failed to add client");

        return NBN_ERROR;
    }

    if (NBN_GameServer_IsEncryptionEnabled())
    {
        if (NBN_GameServer_SendCryptoPublicInfoTo(client) < 0)
        {
            NBN_LogError("Failed to send public key to client %d", client->id);
            NBN_Abort();
        }
    }
    else
    {
        if (!NBN_EventQueue_Enqueue(
                &__game_server.endpoint.event_queue,
                (NBN_Event){ NBN_NEW_CONNECTION, .data = { .connection = client } }))
            return NBN_ERROR;
    }

    return 0;
}

static int NBN_GServ_Driver_OnClientPacketReceived(NBN_Packet *packet)
{
    if (NBN_Endpoint_ProcessReceivedPacket(&__game_server.endpoint, packet, packet->sender) < 0)
    {
        NBN_LogError("An error occured while processing packet from client %d, closing the client", packet->sender->id);

        return NBN_GameServer_CloseClient(packet->sender);
    }

    return 0;
}

static int NBN_GameServer_SendCryptoPublicInfoTo(NBN_Connection *client)
{
    NBN_PublicCryptoInfoMessage *msg = NBN_PublicCryptoInfoMessage_Create();

    memcpy(msg->pub_key1, client->keys1.pub_key, ECC_PUB_KEY_SIZE);
    memcpy(msg->pub_key2, client->keys2.pub_key, ECC_PUB_KEY_SIZE);
    memcpy(msg->pub_key3, client->keys3.pub_key, ECC_PUB_KEY_SIZE);
    memcpy(msg->aes_iv, client->aes_iv, AES_BLOCKLEN);

    NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateMessage(NBN_PUBLIC_CRYPTO_INFO_MESSAGE_TYPE, msg);

    if (outgoing_msg == NULL)
        return NBN_ERROR;

    if (NBN_GameServer_SendReliableMessageTo(client, outgoing_msg) < 0)
        return NBN_ERROR;

    NBN_LogDebug("Sent server's public key to the client %d", client->id);

    return 0;
}

static int NBN_GameServer_StartEncryption(NBN_Connection *client)
{
    NBN_Connection_StartEncryption(client);

    NBN_OutgoingMessage *outgoing_msg = NBN_GameServer_CreateMessage(
            NBN_START_ENCRYPT_MESSAGE_TYPE, NBN_StartEncryptMessage_Create());

    if (outgoing_msg == NULL)
        return NBN_ERROR;

    if (NBN_GameServer_SendReliableMessageTo(client, outgoing_msg) < 0)
        return NBN_ERROR;

    return 0;
}

#pragma endregion /* Game server driver */

#pragma region Packet simulator

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#define RAND_RATIO_BETWEEN(min, max) (((rand() % (int)((max * 100.f) - (min * 100.f) + 1)) + (min * 100.f)) / 100.f) 
#define RAND_RATIO RAND_RATIO_BETWEEN(0, 1)

#ifdef NBNET_WINDOWS
DWORD WINAPI NBN_PacketSimulator_Routine(LPVOID);
#else
static void *NBN_PacketSimulator_Routine(void *);
#endif

static int NBN_PacketSimulator_SendPacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *receiver);
static unsigned int NBN_PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *);

void NBN_PacketSimulator_Init(NBN_PacketSimulator *packet_simulator)
{
    packet_simulator->time = 0;
    packet_simulator->running = false;
    packet_simulator->ping = 0;
    packet_simulator->jitter = 0;
    packet_simulator->packet_loss_ratio = 0;
    packet_simulator->packet_duplication_ratio = 0;
    packet_simulator->head_packet = NULL;
    packet_simulator->tail_packet = NULL;
    packet_simulator->packet_count = 0;

#ifdef NBNET_WINDOWS
    packet_simulator->queue_mutex = CreateMutex(NULL, FALSE, NULL);
#else
    packet_simulator->queue_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif
}

int NBN_PacketSimulator_EnqueuePacket(
        NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, NBN_Connection *receiver)
{
#ifdef NBNET_WINDOWS
    WaitForSingleObject(packet_simulator->queue_mutex, INFINITE);
#else
    pthread_mutex_lock(&packet_simulator->queue_mutex);
#endif

    /* Compute jitter in range [ -jitter, +jitter ].
     * Jitter is converted from seconds to milliseconds for the random operation below.
     */

    int jitter = packet_simulator->jitter * 1000;

    jitter = (jitter > 0) ? (rand() % (jitter * 2)) - jitter : 0;


    NBN_PacketSimulatorEntry *entry = NBN_MemoryManager_Alloc(NBN_MEM_PACKET_SIMULATOR_ENTRY);

    entry->delay = packet_simulator->ping + jitter / 1000; /* and converted back to seconds */
    entry->receiver = receiver;
    entry->enqueued_at = packet_simulator->time;

    memcpy(&entry->packet, packet, sizeof(NBN_Packet));

    if (packet_simulator->packet_count > 0)
    {
        entry->prev = packet_simulator->tail_packet;
        entry->next = NULL;

        packet_simulator->tail_packet->next = entry;
        packet_simulator->tail_packet = entry;
    }
    else // the list is empty
    {
        entry->prev = NULL;
        entry->next = NULL;

        packet_simulator->head_packet = entry;
        packet_simulator->tail_packet = entry;
    }

    packet_simulator->packet_count++;

#ifdef NBNET_WINDOWS
    ReleaseMutex(packet_simulator->queue_mutex);
#else
    pthread_mutex_unlock(&packet_simulator->queue_mutex);
#endif

    return 0;
}

void NBN_PacketSimulator_Start(NBN_PacketSimulator *packet_simulator)
{
#ifdef NBNET_WINDOWS
    packet_simulator->thread = CreateThread(NULL, 0, NBN_PacketSimulator_Routine, packet_simulator, 0, NULL);
#else
    pthread_create(&packet_simulator->thread, NULL, NBN_PacketSimulator_Routine, packet_simulator);
#endif

    packet_simulator->running = true;
}

void NBN_PacketSimulator_Stop(NBN_PacketSimulator *packet_simulator)
{
    packet_simulator->running = false;

#ifdef NBNET_WINDOWS
    WaitForSingleObject(packet_simulator->thread, INFINITE);
#else
    pthread_join(packet_simulator->thread, NULL);
#endif
}

void NBN_PacketSimulator_AddTime(NBN_PacketSimulator *packet_simulator, double time)
{
    packet_simulator->time += time;
}

#ifdef NBNET_WINDOWS
DWORD WINAPI NBN_PacketSimulator_Routine(LPVOID arg)
#else
static void *NBN_PacketSimulator_Routine(void *arg)
#endif
{
    NBN_PacketSimulator *packet_simulator = arg;

    while (packet_simulator->running)
    {
#ifdef NBNET_WINDOWS
        WaitForSingleObject(packet_simulator->queue_mutex, INFINITE);
#else
        pthread_mutex_lock(&packet_simulator->queue_mutex);
#endif

        NBN_PacketSimulatorEntry *entry = packet_simulator->head_packet;

        while (entry)
        {
            NBN_PacketSimulatorEntry *next = entry->next;

            if (packet_simulator->time - entry->enqueued_at < entry->delay)
            {
                entry = next;

                continue;
            }

            NBN_PacketSimulator_SendPacket(packet_simulator, &entry->packet, entry->receiver);

            for (unsigned int i = 0; i < NBN_PacketSimulator_GetRandomDuplicatePacketCount(packet_simulator); i++)
            {
                NBN_LogDebug("Duplicate packet %d (count: %d)", entry->packet.header.seq_number, i + 1);

                NBN_PacketSimulator_SendPacket(packet_simulator, &entry->packet, entry->receiver);
            }

            // remove the entry from the packet list
            if (entry == packet_simulator->head_packet) // it's the head of the list
            {
                NBN_PacketSimulatorEntry *new_head = entry->next;

                if (new_head)
                    new_head->prev = NULL;
                else
                    packet_simulator->tail_packet = NULL;

                packet_simulator->head_packet = new_head;
            }
            else if (entry == packet_simulator->tail_packet) // it's the tail of the list
            {
                NBN_PacketSimulatorEntry *new_tail = entry->prev;

                new_tail->next = NULL;
                packet_simulator->tail_packet = new_tail;
            }
            else // it's in the middle of the list
            {
                entry->prev->next = entry->next;
                entry->next->prev = entry->prev;
            }

            packet_simulator->packet_count--;

            // release the memory allocated for the entry
            NBN_MemoryManager_Dealloc(entry, NBN_MEM_PACKET_SIMULATOR_ENTRY);

            entry = next;
        }

#ifdef NBNET_WINDOWS
        ReleaseMutex(packet_simulator->queue_mutex);
#else
        pthread_mutex_unlock(&packet_simulator->queue_mutex);
#endif
    }

#ifdef NBNET_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

static int NBN_PacketSimulator_SendPacket(
        NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, NBN_Connection *receiver)
{
    if (RAND_RATIO < packet_simulator->packet_loss_ratio)
    {
        NBN_LogDebug("Drop packet %d", packet->header.seq_number);

        return 0;
    }

    if (receiver->endpoint->is_server)
    {
        if (receiver->is_stale)
            return 0;

        return NBN_Driver_GServ_SendPacketTo(packet, receiver);
    }
    else
    {
        return NBN_Driver_GCli_SendPacket(packet);
    }
}

static unsigned int NBN_PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *packet_simulator)
{
    if (RAND_RATIO < packet_simulator->packet_duplication_ratio)
        return rand() % 10 + 1;

    return 0;
}

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */

#pragma region Encryption

/*
 * All "low-level" cryptography implementations used for nbnet packet encryption and authentication.
 *
 * I did not write any of the code in this section, I only regrouped it in there to keep nbnet contained
 * into a single header. I used a total of four different open source libraries:
 *
 * ECDH:        https://github.com/kokke/tiny-ECDH-c
 * AES:         https://github.com/kokke/tiny-AES-c
 * Poly1305:    https://github.com/floodyberry/poly1305-donna
 * CSPRNG:      https://github.com/Duthomhas/CSPRNG
 */

#pragma region ECDH

/* margin for overhead needed in intermediate calculations */
#define BITVEC_MARGIN     3
#define BITVEC_NBITS      (CURVE_DEGREE + BITVEC_MARGIN)
#define BITVEC_NWORDS     ((BITVEC_NBITS + 31) / 32)
#define BITVEC_NBYTES     (sizeof(uint32_t) * BITVEC_NWORDS)

/* Default to a (somewhat) constant-time mode?
NOTE: The library is _not_ capable of operating in constant-time and leaks information via timing.
Even if all operations are written const-time-style, it requires the hardware is able to multiply in constant time. 
Multiplication on ARM Cortex-M processors takes a variable number of cycles depending on the operands...
*/
#ifndef CONST_TIME
#define CONST_TIME 0
#endif

/* Default to using ECC_CDH (cofactor multiplication-variation) ? */
#ifndef ECDH_COFACTOR_VARIANT
#define ECDH_COFACTOR_VARIANT 0
#endif

/******************************************************************************/


/* the following type will represent bit vectors of length (CURVE_DEGREE+MARGIN) */
typedef uint32_t bitvec_t[BITVEC_NWORDS];
typedef bitvec_t gf2elem_t;           /* this type will represent field elements */
typedef bitvec_t scalar_t;


/******************************************************************************/

/* Here the curve parameters are defined. */

#if defined (ECC_CURVE) && (ECC_CURVE != 0)
#if (ECC_CURVE == NIST_K163)
#define coeff_a  1
#define cofactor 2
/* NIST K-163 */
const gf2elem_t polynomial = { 0x000000c9, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000008 }; 
const gf2elem_t coeff_b    = { 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 }; 
const gf2elem_t base_x     = { 0x5c94eee8, 0xde4e6d5e, 0xaa07d793, 0x7bbc11ac, 0xfe13c053, 0x00000002 }; 
const gf2elem_t base_y     = { 0xccdaa3d9, 0x0536d538, 0x321f2e80, 0x5d38ff58, 0x89070fb0, 0x00000002 }; 
const scalar_t  base_order = { 0x99f8a5ef, 0xa2e0cc0d, 0x00020108, 0x00000000, 0x00000000, 0x00000004 }; 
#endif

#if (ECC_CURVE == NIST_B163)
#define coeff_a  1
#define cofactor 2
/* NIST B-163 */
const gf2elem_t polynomial = { 0x000000c9, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000008 }; 
const gf2elem_t coeff_b    = { 0x4a3205fd, 0x512f7874, 0x1481eb10, 0xb8c953ca, 0x0a601907, 0x00000002 }; 
const gf2elem_t base_x     = { 0xe8343e36, 0xd4994637, 0xa0991168, 0x86a2d57e, 0xf0eba162, 0x00000003 }; 
const gf2elem_t base_y     = { 0x797324f1, 0xb11c5c0c, 0xa2cdd545, 0x71a0094f, 0xd51fbc6c, 0x00000000 }; 
const scalar_t  base_order = { 0xa4234c33, 0x77e70c12, 0x000292fe, 0x00000000, 0x00000000, 0x00000004 }; 
#endif

#if (ECC_CURVE == NIST_K233)
#define coeff_a  0
#define cofactor 4
/* NIST K-233 */
const gf2elem_t polynomial = { 0x00000001, 0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000200 };
const gf2elem_t coeff_b    = { 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
const gf2elem_t base_x     = { 0xefad6126, 0x0a4c9d6e, 0x19c26bf5, 0x149563a4, 0x29f22ff4, 0x7e731af1, 0x32ba853a, 0x00000172 };
const gf2elem_t base_y     = { 0x56fae6a3, 0x56e0c110, 0xf18aeb9b, 0x27a8cd9b, 0x555a67c4, 0x19b7f70f, 0x537dece8, 0x000001db };
const scalar_t  base_order = { 0xf173abdf, 0x6efb1ad5, 0xb915bcd4, 0x00069d5b, 0x00000000, 0x00000000, 0x00000000, 0x00000080 };
#endif

#if (ECC_CURVE == NIST_B233)
#define coeff_a  1
#define cofactor 2
/* NIST B-233 */
const gf2elem_t polynomial = { 0x00000001, 0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000200 }; 
const gf2elem_t coeff_b    = { 0x7d8f90ad, 0x81fe115f, 0x20e9ce42, 0x213b333b, 0x0923bb58, 0x332c7f8c, 0x647ede6c, 0x00000066 }; 
const gf2elem_t base_x     = { 0x71fd558b, 0xf8f8eb73, 0x391f8b36, 0x5fef65bc, 0x39f1bb75, 0x8313bb21, 0xc9dfcbac, 0x000000fa }; 
const gf2elem_t base_y     = { 0x01f81052, 0x36716f7e, 0xf867a7ca, 0xbf8a0bef, 0xe58528be, 0x03350678, 0x6a08a419, 0x00000100 }; 
const scalar_t  base_order = { 0x03cfe0d7, 0x22031d26, 0xe72f8a69, 0x0013e974, 0x00000000, 0x00000000, 0x00000000, 0x00000100 };
#endif

#if (ECC_CURVE == NIST_K283)
#define coeff_a  0
#define cofactor 4
/* NIST K-283 */
const gf2elem_t polynomial = { 0x000010a1, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08000000 };
const gf2elem_t coeff_b    = { 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 }; 
const gf2elem_t base_x     = { 0x58492836, 0xb0c2ac24, 0x16876913, 0x23c1567a, 0x53cd265f, 0x62f188e5, 0x3f1a3b81, 0x78ca4488, 0x0503213f }; 
const gf2elem_t base_y     = { 0x77dd2259, 0x4e341161, 0xe4596236, 0xe8184698, 0xe87e45c0, 0x07e5426f, 0x8d90f95d, 0x0f1c9e31, 0x01ccda38 }; 
const scalar_t  base_order = { 0x1e163c61, 0x94451e06, 0x265dff7f, 0x2ed07577, 0xffffe9ae, 0xffffffff, 0xffffffff, 0xffffffff, 0x01ffffff }; 
#endif

#if (ECC_CURVE == NIST_B283)
#define coeff_a  1
#define cofactor 2
/* NIST B-283 */
const gf2elem_t polynomial = { 0x000010a1, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08000000 }; 
const gf2elem_t coeff_b    = { 0x3b79a2f5, 0xf6263e31, 0xa581485a, 0x45309fa2, 0xca97fd76, 0x19a0303f, 0xa5a4af8a, 0xc8b8596d, 0x027b680a }; 
const gf2elem_t base_x     = { 0x86b12053, 0xf8cdbecd, 0x80e2e198, 0x557eac9c, 0x2eed25b8, 0x70b0dfec, 0xe1934f8c, 0x8db7dd90, 0x05f93925 }; 
const gf2elem_t base_y     = { 0xbe8112f4, 0x13f0df45, 0x826779c8, 0x350eddb0, 0x516ff702, 0xb20d02b4, 0xb98fe6d4, 0xfe24141c, 0x03676854 }; 
const scalar_t  base_order = { 0xefadb307, 0x5b042a7c, 0x938a9016, 0x399660fc, 0xffffef90, 0xffffffff, 0xffffffff, 0xffffffff, 0x03ffffff }; 
#endif

#if (ECC_CURVE == NIST_K409)
#define coeff_a  0
#define cofactor 4
/* NIST K-409 */
const gf2elem_t polynomial = { 0x00000001, 0x00000000, 0x00800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000 }; 
const gf2elem_t coeff_b    = { 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 }; 
const gf2elem_t base_x     = { 0xe9023746, 0xb35540cf, 0xee222eb1, 0xb5aaaa62, 0xc460189e, 0xf9f67cc2, 0x27accfb8, 0xe307c84c, 0x0efd0987, 0x0f718421, 0xad3ab189, 0x658f49c1, 0x0060f05f }; 
const gf2elem_t base_y     = { 0xd8e0286b, 0x5863ec48, 0xaa9ca27a, 0xe9c55215, 0xda5f6c42, 0xe9ea10e3, 0xe6325165, 0x918ea427, 0x3460782f, 0xbf04299c, 0xacba1dac, 0x0b7c4e42, 0x01e36905 }; 
const scalar_t  base_order = { 0xe01e5fcf, 0x4b5c83b8, 0xe3e7ca5b, 0x557d5ed3, 0x20400ec4, 0x83b2d4ea, 0xfffffe5f, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x007fffff }; 
#endif

#if (ECC_CURVE == NIST_B409)
#define coeff_a  1
#define cofactor 2
/* NIST B-409 */
const gf2elem_t polynomial = { 0x00000001, 0x00000000, 0x00800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000 }; 
const gf2elem_t coeff_b    = { 0x7b13545f, 0x4f50ae31, 0xd57a55aa, 0x72822f6c, 0xa9a197b2, 0xd6ac27c8, 0x4761fa99, 0xf1f3dd67, 0x7fd6422e, 0x3b7b476b, 0x5c4b9a75, 0xc8ee9feb, 0x0021a5c2 }; 
const gf2elem_t base_x     = { 0xbb7996a7, 0x60794e54, 0x5603aeab, 0x8a118051, 0xdc255a86, 0x34e59703, 0xb01ffe5b, 0xf1771d4d, 0x441cde4a, 0x64756260, 0x496b0c60, 0xd088ddb3, 0x015d4860 }; 
const gf2elem_t base_y     = { 0x0273c706, 0x81c364ba, 0xd2181b36, 0xdf4b4f40, 0x38514f1f, 0x5488d08f, 0x0158aa4f, 0xa7bd198d, 0x7636b9c5, 0x24ed106a, 0x2bbfa783, 0xab6be5f3, 0x0061b1cf }; 
const scalar_t  base_order = { 0xd9a21173, 0x8164cd37, 0x9e052f83, 0x5fa47c3c, 0xf33307be, 0xaad6a612, 0x000001e2, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000000 }; 
#endif

#if (ECC_CURVE == NIST_K571)
#define coeff_a  0
#define cofactor 4
/* NIST K-571 */
const gf2elem_t polynomial = { 0x00000425, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08000000 }; 
const gf2elem_t coeff_b    = { 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 }; 
const gf2elem_t base_x     = { 0xa01c8972, 0xe2945283, 0x4dca88c7, 0x988b4717, 0x494776fb, 0xbbd1ba39, 0xb4ceb08c, 0x47da304d, 0x93b205e6, 0x43709584, 0x01841ca4, 0x60248048, 0x0012d5d4, 0xac9ca297, 0xf8103fe4, 0x82189631, 0x59923fbc, 0x026eb7a8 }; 
const gf2elem_t base_y     = { 0x3ef1c7a3, 0x01cd4c14, 0x591984f6, 0x320430c8, 0x7ba7af1b, 0xb620b01a, 0xf772aedc, 0x4fbebbb9, 0xac44aea7, 0x9d4979c0, 0x006d8a2c, 0xffc61efc, 0x9f307a54, 0x4dd58cec, 0x3bca9531, 0x4f4aeade, 0x7f4fbf37, 0x0349dc80 }; 
const scalar_t  base_order = { 0x637c1001, 0x5cfe778f, 0x1e91deb4, 0xe5d63938, 0xb630d84b, 0x917f4138, 0xb391a8db, 0xf19a63e4, 0x131850e1, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000 }; 
#endif

#if (ECC_CURVE == NIST_B571)
#define coeff_a  1
#define cofactor 2
/* NIST B-571 */
const gf2elem_t polynomial = { 0x00000425, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08000000 }; 
const gf2elem_t coeff_b    = { 0x2955727a, 0x7ffeff7f, 0x39baca0c, 0x520e4de7, 0x78ff12aa, 0x4afd185a, 0x56a66e29, 0x2be7ad67, 0x8efa5933, 0x84ffabbd, 0x4a9a18ad, 0xcd6ba8ce, 0xcb8ceff1, 0x5c6a97ff, 0xb7f3d62f, 0xde297117, 0x2221f295, 0x02f40e7e }; 
const gf2elem_t base_x     = { 0x8eec2d19, 0xe1e7769c, 0xc850d927, 0x4abfa3b4, 0x8614f139, 0x99ae6003, 0x5b67fb14, 0xcdd711a3, 0xf4c0d293, 0xbde53950, 0xdb7b2abd, 0xa5f40fc8, 0x955fa80a, 0x0a93d1d2, 0x0d3cd775, 0x6c16c0d4, 0x34b85629, 0x0303001d }; 
const gf2elem_t base_y     = { 0x1b8ac15b, 0x1a4827af, 0x6e23dd3c, 0x16e2f151, 0x0485c19b, 0xb3531d2f, 0x461bb2a8, 0x6291af8f, 0xbab08a57, 0x84423e43, 0x3921e8a6, 0x1980f853, 0x009cbbca, 0x8c6c27a6, 0xb73d69d7, 0x6dccfffe, 0x42da639b, 0x037bf273 }; 
const scalar_t  base_order = { 0x2fe84e47, 0x8382e9bb, 0x5174d66e, 0x161de93d, 0xc7dd9ca1, 0x6823851e, 0x08059b18, 0xff559873, 0xe661ce18, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x03ffffff }; 
#endif
#endif

/*************************************************************************************************/
/* Private / static functions: */
/*************************************************************************************************/

/* some basic bit-manipulation routines that act on bit-vectors follow */
static int bitvec_get_bit(const bitvec_t x, const uint32_t idx)
{
    return ((x[idx / 32U] >> (idx & 31U) & 1U));
}

static void bitvec_clr_bit(bitvec_t x, const uint32_t idx)
{
    x[idx / 32U] &= ~(1U << (idx & 31U));
}

static void bitvec_copy(bitvec_t x, const bitvec_t y)
{
    int i;
    for (i = 0; i < BITVEC_NWORDS; ++i)
    {
        x[i] = y[i];
    }
}

static void bitvec_swap(bitvec_t x, bitvec_t y)
{
    bitvec_t tmp;
    bitvec_copy(tmp, x);
    bitvec_copy(x, y);
    bitvec_copy(y, tmp);
}

#if defined(CONST_TIME) && (CONST_TIME == 0)
/* fast version of equality test */
static int bitvec_equal(const bitvec_t x, const bitvec_t y)
{
    int i;
    for (i = 0; i < BITVEC_NWORDS; ++i)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}
#else
/* constant time version of equality test */
static int bitvec_equal(const bitvec_t x, const bitvec_t y)
{
    int ret = 1;
    int i;
    for (i = 0; i < BITVEC_NWORDS; ++i)
    {
        ret &= (x[i] == y[i]);
    }
    return ret;
}
#endif

static void bitvec_set_zero(bitvec_t x)
{
    int i;
    for (i = 0; i < BITVEC_NWORDS; ++i)
    {
        x[i] = 0;
    }
}

#if defined(CONST_TIME) && (CONST_TIME == 0)
/* fast implementation */
static int bitvec_is_zero(const bitvec_t x)
{
    uint32_t i = 0;
    while (i < BITVEC_NWORDS)
    {
        if (x[i] != 0)
        {
            break;
        }
        i += 1;
    }
    return (i == BITVEC_NWORDS);
}
#else
/* constant-time implementation */
static int bitvec_is_zero(const bitvec_t x)
{
    int ret = 1;
    int i = 0;
    for (i = 0; i < BITVEC_NWORDS; ++i)
    {
        ret &= (x[i] == 0);
    }
    return ret;
}
#endif

/* return the number of the highest one-bit + 1 */
static int bitvec_degree(const bitvec_t x)
{
    int i = BITVEC_NWORDS * 32;

    /* Start at the back of the vector (MSB) */
    x += BITVEC_NWORDS;

    /* Skip empty / zero words */
    while (    (i > 0)
            && (*(--x)) == 0)
    {
        i -= 32;
    }
    /* Run through rest if count is not multiple of bitsize of DTYPE */
    if (i != 0)
    {
        uint32_t u32mask = ((uint32_t)1 << 31);
        while (((*x) & u32mask) == 0)
        {
            u32mask >>= 1;
            i -= 1;
        }
    }
    return i;
}

/* left-shift by 'count' digits */
static void bitvec_lshift(bitvec_t x, const bitvec_t y, int nbits)
{
    int nwords = (nbits / 32);

    /* Shift whole words first if nwords > 0 */
    int i,j;
    for (i = 0; i < nwords; ++i)
    {
        /* Zero-initialize from least-significant word until offset reached */
        x[i] = 0;
    }
    j = 0;
    /* Copy to x output */
    while (i < BITVEC_NWORDS)
    {
        x[i] = y[j];
        i += 1;
        j += 1;
    }

    /* Shift the rest if count was not multiple of bitsize of DTYPE */
    nbits &= 31;
    if (nbits != 0)
    {
        /* Left shift rest */
        int i;
        for (i = (BITVEC_NWORDS - 1); i > 0; --i)
        {
            x[i]  = (x[i] << nbits) | (x[i - 1] >> (32 - nbits));
        }
        x[0] <<= nbits;
    }
}


/*************************************************************************************************/
/*
   Code that does arithmetic on bit-vectors in the Galois Field GF(2^CURVE_DEGREE).
   */
/*************************************************************************************************/


static void gf2field_set_one(gf2elem_t x)
{
    /* Set first word to one */
    x[0] = 1;
    /* .. and the rest to zero */
    int i;
    for (i = 1; i < BITVEC_NWORDS; ++i)
    {
        x[i] = 0;
    }
}

#if defined(CONST_TIME) && (CONST_TIME == 0)
/* fastest check if x == 1 */
static int gf2field_is_one(const gf2elem_t x) 
{
    /* Check if first word == 1 */
    if (x[0] != 1)
    {
        return 0;
    }
    /* ...and if rest of words == 0 */
    int i;
    for (i = 1; i < BITVEC_NWORDS; ++i)
    {
        if (x[i] != 0)
        {
            break;
        }
    }
    return (i == BITVEC_NWORDS);
}
#else
/* constant-time check */
static int gf2field_is_one(const gf2elem_t x)
{
    int ret = 0;
    /* Check if first word == 1 */
    if (x[0] == 1)
    {
        ret = 1;
    }
    /* ...and if rest of words == 0 */
    int i;
    for (i = 1; i < BITVEC_NWORDS; ++i)
    {
        ret &= (x[i] == 0);
    }
    return ret; //(i == BITVEC_NWORDS);
}
#endif


/* galois field(2^m) addition is modulo 2, so XOR is used instead - 'z := a + b' */
static void gf2field_add(gf2elem_t z, const gf2elem_t x, const gf2elem_t y)
{
    int i;
    for (i = 0; i < BITVEC_NWORDS; ++i)
    {
        z[i] = (x[i] ^ y[i]);
    }
}

/* increment element */
static void gf2field_inc(gf2elem_t x)
{
    x[0] ^= 1;
}


/* field multiplication 'z := (x * y)' */
static void gf2field_mul(gf2elem_t z, const gf2elem_t x, const gf2elem_t y)
{
    int i;
    gf2elem_t tmp;
#if defined(CONST_TIME) && (CONST_TIME == 1)
    gf2elem_t blind;
    bitvec_set_zero(blind);
#endif
    assert(z != y);

    bitvec_copy(tmp, x);

    /* LSB set? Then start with x */
    if (bitvec_get_bit(y, 0) != 0)
    {
        bitvec_copy(z, x);
    }
    else /* .. or else start with zero */
    {
        bitvec_set_zero(z);
    }

    /* Then add 2^i * x for the rest */
    for (i = 1; i < CURVE_DEGREE; ++i)
    {
        /* lshift 1 - doubling the value of tmp */
        bitvec_lshift(tmp, tmp, 1);

        /* Modulo reduction polynomial if degree(tmp) > CURVE_DEGREE */
        if (bitvec_get_bit(tmp, CURVE_DEGREE))
        {
            gf2field_add(tmp, tmp, polynomial);
        }
#if defined(CONST_TIME) && (CONST_TIME == 1)
        else /* blinding operation */
        {
            gf2field_add(tmp, tmp, blind);
        }
#endif

        /* Add 2^i * tmp if this factor in y is non-zero */
        if (bitvec_get_bit(y, i))
        {
            gf2field_add(z, z, tmp);
        }
#if defined(CONST_TIME) && (CONST_TIME == 1)
        else /* blinding operation */
        {
            gf2field_add(z, z, blind);
        }
#endif
    }
}

/* field inversion 'z := 1/x' */
static void gf2field_inv(gf2elem_t z, const gf2elem_t x)
{
    gf2elem_t u, v, g, h;
    int i;

    bitvec_copy(u, x);
    bitvec_copy(v, polynomial);
    bitvec_set_zero(g);
    gf2field_set_one(z);

    while (!gf2field_is_one(u))
    {
        i = (bitvec_degree(u) - bitvec_degree(v));

        if (i < 0)
        {
            bitvec_swap(u, v);
            bitvec_swap(g, z);
            i = -i;
        }
#if defined(CONST_TIME) && (CONST_TIME == 1)
        else
        {
            bitvec_swap(u, v);
            bitvec_swap(v, u);
        }
#endif
        bitvec_lshift(h, v, i);
        gf2field_add(u, u, h);
        bitvec_lshift(h, g, i);
        gf2field_add(z, z, h);
    }
}

/*************************************************************************************************/
/*
   The following code takes care of Galois-Field arithmetic. 
   Elliptic curve points are represented  by pairs (x,y) of bitvec_t. 
   It is assumed that curve coefficient 'a' is {0,1}
   This is the case for all NIST binary curves.
   Coefficient 'b' is given in 'coeff_b'.
   '(base_x, base_y)' is a point that generates a large prime order group.
   */
/*************************************************************************************************/


static void gf2point_copy(gf2elem_t x1, gf2elem_t y1, const gf2elem_t x2, const gf2elem_t y2)
{
    bitvec_copy(x1, x2);
    bitvec_copy(y1, y2);
}

static void gf2point_set_zero(gf2elem_t x, gf2elem_t y)
{
    bitvec_set_zero(x);
    bitvec_set_zero(y);
}

static int gf2point_is_zero(const gf2elem_t x, const gf2elem_t y)
{
    return (    bitvec_is_zero(x)
            && bitvec_is_zero(y));
}

/* double the point (x,y) */
static void gf2point_double(gf2elem_t x, gf2elem_t y)
{
    /* iff P = O (zero or infinity): 2 * P = P */
    if (bitvec_is_zero(x))
    {
        bitvec_set_zero(y);
    }
    else
    {
        gf2elem_t l;

        gf2field_inv(l, x);
        gf2field_mul(l, l, y);
        gf2field_add(l, l, x);
        gf2field_mul(y, x, x);
        gf2field_mul(x, l, l);
#if (coeff_a == 1)
        gf2field_inc(l);
#endif
        gf2field_add(x, x, l);
        gf2field_mul(l, l, x);
        gf2field_add(y, y, l);
    }
}


/* add two points together (x1, y1) := (x1, y1) + (x2, y2) */
static void gf2point_add(gf2elem_t x1, gf2elem_t y1, const gf2elem_t x2, const gf2elem_t y2)
{
    if (!gf2point_is_zero(x2, y2))
    {
        if (gf2point_is_zero(x1, y1))
        {
            gf2point_copy(x1, y1, x2, y2);
        }
        else
        {
            if (bitvec_equal(x1, x2))
            {
                if (bitvec_equal(y1, y2))
                {
                    gf2point_double(x1, y1);
                }
                else
                {
                    gf2point_set_zero(x1, y1);
                }
            }
            else
            {
                /* Arithmetic with temporary variables */
                gf2elem_t a, b, c, d;

                gf2field_add(a, y1, y2);
                gf2field_add(b, x1, x2);
                gf2field_inv(c, b);
                gf2field_mul(c, c, a);
                gf2field_mul(d, c, c);
                gf2field_add(d, d, c);
                gf2field_add(d, d, b);
#if (coeff_a == 1)
                gf2field_inc(d);
#endif
                gf2field_add(x1, x1, d);
                gf2field_mul(a, x1, c);
                gf2field_add(a, a, d);
                gf2field_add(y1, y1, a);
                bitvec_copy(x1, d);
            }
        }
    }
}



#if defined(CONST_TIME) && (CONST_TIME == 0)
/* point multiplication via double-and-add algorithm */
static void gf2point_mul(gf2elem_t x, gf2elem_t y, const scalar_t exp)
{
    gf2elem_t tmpx, tmpy;
    int i;
    int nbits = bitvec_degree(exp);

    gf2point_set_zero(tmpx, tmpy);

    for (i = (nbits - 1); i >= 0; --i)
    {
        gf2point_double(tmpx, tmpy);
        if (bitvec_get_bit(exp, i))
        {
            gf2point_add(tmpx, tmpy, x, y);
        }
    }
    gf2point_copy(x, y, tmpx, tmpy);
}
#else
/* point multiplication via double-and-add-always algorithm using scalar blinding */
static void gf2point_mul(gf2elem_t x, gf2elem_t y, const scalar_t exp)
{
    gf2elem_t tmpx, tmpy;
    gf2elem_t dummyx, dummyy;
    int i;
    int nbits = bitvec_degree(exp);

    gf2point_set_zero(tmpx, tmpy);
    gf2point_set_zero(dummyx, dummyy);

    for (i = (nbits - 1); i >= 0; --i)
    {
        gf2point_double(tmpx, tmpy);

        /* Add point if bit(i) is set in exp */
        if (bitvec_get_bit(exp, i))
        {
            gf2point_add(tmpx, tmpy, x, y);
        }
        /* .. or add the neutral element to keep operation constant-time */
        else
        {
            gf2point_add(tmpx, tmpy, dummyx, dummyy);
        }
    }
    gf2point_copy(x, y, tmpx, tmpy);
}
#endif



/* check if y^2 + x*y = x^3 + a*x^2 + coeff_b holds */
static int gf2point_on_curve(const gf2elem_t x, const gf2elem_t y)
{
    gf2elem_t a, b;

    if (gf2point_is_zero(x, y))
    {
        return 1;
    }
    else
    {
        gf2field_mul(a, x, x);
#if (coeff_a == 0)
        gf2field_mul(a, a, x);
#else
        gf2field_mul(b, a, x);
        gf2field_add(a, a, b);
#endif
        gf2field_add(a, a, coeff_b);
        gf2field_mul(b, y, y);
        gf2field_add(a, a, b);
        gf2field_mul(b, x, y);

        return bitvec_equal(a, b);
    }
}

/*************************************************************************************************/
/* Elliptic Curve Diffie-Hellman key exchange protocol. */
/*************************************************************************************************/

/* NOTE: private should contain random data a-priori! */
static int ecdh_generate_keys(uint8_t* public_key, uint8_t* private_key)
{
    /* Get copy of "base" point 'G' */
    gf2point_copy((uint32_t*)public_key, (uint32_t*)(public_key + BITVEC_NBYTES), base_x, base_y);

    /* Abort key generation if random number is too small */
    if (bitvec_degree((uint32_t*)private_key) < (CURVE_DEGREE / 2))
    {
        return 0;
    }
    else
    {
        /* Clear bits > CURVE_DEGREE in highest word to satisfy constraint 1 <= exp < n. */
        int nbits = bitvec_degree(base_order);
        int i;

        for (i = (nbits - 1); i < (BITVEC_NWORDS * 32); ++i)
        {
            bitvec_clr_bit((uint32_t*)private_key, i);
        }

        /* Multiply base-point with scalar (private-key) */
        gf2point_mul((uint32_t*)public_key, (uint32_t*)(public_key + BITVEC_NBYTES), (uint32_t*)private_key);

        return 1;
    }
}

static int ecdh_shared_secret(const uint8_t* private_key, const uint8_t* others_pub, uint8_t* output)
{
    /* Do some basic validation of other party's public key */
    if (    !gf2point_is_zero ((uint32_t*)others_pub, (uint32_t*)(others_pub + BITVEC_NBYTES))
            &&  gf2point_on_curve((uint32_t*)others_pub, (uint32_t*)(others_pub + BITVEC_NBYTES)) )
    {
        /* Copy other side's public key to output */
        unsigned int i;
        for (i = 0; i < (BITVEC_NBYTES * 2); ++i)
        {
            output[i] = others_pub[i];
        }

        /* Multiply other side's public key with own private key */
        gf2point_mul((uint32_t*)output,(uint32_t*)(output + BITVEC_NBYTES), (const uint32_t*)private_key);

        /* Multiply outcome by cofactor if using ECC CDH-variant: */
#if defined(ECDH_COFACTOR_VARIANT) && (ECDH_COFACTOR_VARIANT == 1)
#if   (cofactor == 2)
        gf2point_double((uint32_t*)output, (uint32_t*)(output + BITVEC_NBYTES));
#elif (cofactor == 4)
        gf2point_double((uint32_t*)output, (uint32_t*)(output + BITVEC_NBYTES));
        gf2point_double((uint32_t*)output, (uint32_t*)(output + BITVEC_NBYTES));
#endif
#endif

        return 1;
    }
    else
    {
        return 0;
    }
}

#pragma endregion /* ECDH */

#pragma region /* AES */

/*****************************************************************************/
/* Defines:                                                                  */
/*****************************************************************************/
// The number of columns comprising a state in AES. This is a constant in AES. Value=4
#define Nb 4

#if defined(AES256) && (AES256 == 1)
#define Nk 8
#define Nr 14
#elif defined(AES192) && (AES192 == 1)
#define Nk 6
#define Nr 12
#else
#define Nk 4        // The number of 32 bit words in a key.
#define Nr 10       // The number of rounds in AES Cipher.
#endif

// jcallan@github points out that declaring Multiply as a function 
// reduces code size considerably with the Keil ARM compiler.
// See this link for more information: https://github.com/kokke/tiny-AES-C/pull/3
#ifndef MULTIPLY_AS_A_FUNCTION
#define MULTIPLY_AS_A_FUNCTION 0
#endif

/*****************************************************************************/
/* Private variables:                                                        */
/*****************************************************************************/
// state - array holding the intermediate results during decryption.
typedef uint8_t state_t[4][4];

// The lookup-tables are marked const so they can be placed in read-only storage instead of RAM
// The numbers below can be computed dynamically trading ROM for RAM - 
// This can be useful in (embedded) bootloader applications, where ROM is often limited.
static const uint8_t sbox[256] = {
    //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

static const uint8_t rsbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };

// The round constant word array, Rcon[i], contains the values given by 
// x to the power (i-1) being powers of x (x is denoted as {02}) in the field GF(2^8)
static const uint8_t Rcon[11] = {
    0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

/*
 * Jordan Goulder points out in PR #12 (https://github.com/kokke/tiny-AES-C/pull/12),
 * that you can remove most of the elements in the Rcon array, because they are unused.
 *
 * From Wikipedia's article on the Rijndael key schedule @ https://en.wikipedia.org/wiki/Rijndael_key_schedule#Rcon
 * 
 * "Only the first some of these constants are actually used – up to rcon[10] for AES-128 (as 11 round keys are needed), 
 *  up to rcon[8] for AES-192, up to rcon[7] for AES-256. rcon[0] is not used in AES algorithm."
 */


/*****************************************************************************/
/* Private functions:                                                        */
/*****************************************************************************/
/*
   static uint8_t getSBoxValue(uint8_t num)
   {
   return sbox[num];
   }
   */
#define getSBoxValue(num) (sbox[(num)])
/*
   static uint8_t getSBoxInvert(uint8_t num)
   {
   return rsbox[num];
   }
   */
#define getSBoxInvert(num) (rsbox[(num)])

// This function produces Nb(Nr+1) round keys. The round keys are used in each round to decrypt the states. 
static void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key)
{
    unsigned i, j, k;
    uint8_t tempa[4]; // Used for the column/row operations

    // The first round key is the key itself.
    for (i = 0; i < Nk; ++i)
    {
        RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
        RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
        RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
        RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
    }

    // All other round keys are found from the previous round keys.
    for (i = Nk; i < Nb * (Nr + 1); ++i)
    {
        {
            k = (i - 1) * 4;
            tempa[0]=RoundKey[k + 0];
            tempa[1]=RoundKey[k + 1];
            tempa[2]=RoundKey[k + 2];
            tempa[3]=RoundKey[k + 3];

        }

        if (i % Nk == 0)
        {
            // This function shifts the 4 bytes in a word to the left once.
            // [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

            // Function RotWord()
            {
                const uint8_t u8tmp = tempa[0];
                tempa[0] = tempa[1];
                tempa[1] = tempa[2];
                tempa[2] = tempa[3];
                tempa[3] = u8tmp;
            }

            // SubWord() is a function that takes a four-byte input word and 
            // applies the S-box to each of the four bytes to produce an output word.

            // Function Subword()
            {
                tempa[0] = getSBoxValue(tempa[0]);
                tempa[1] = getSBoxValue(tempa[1]);
                tempa[2] = getSBoxValue(tempa[2]);
                tempa[3] = getSBoxValue(tempa[3]);
            }

            tempa[0] = tempa[0] ^ Rcon[i/Nk];
        }
#if defined(AES256) && (AES256 == 1)
        if (i % Nk == 4)
        {
            // Function Subword()
            {
                tempa[0] = getSBoxValue(tempa[0]);
                tempa[1] = getSBoxValue(tempa[1]);
                tempa[2] = getSBoxValue(tempa[2]);
                tempa[3] = getSBoxValue(tempa[3]);
            }
        }
#endif
        j = i * 4; k=(i - Nk) * 4;
        RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0];
        RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
        RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2];
        RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
    }
}

static void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv)
{
    KeyExpansion(ctx->RoundKey, key);
    memcpy (ctx->Iv, iv, AES_BLOCKLEN);
}

// This function adds the round key to state.
// The round key is added to the state by an XOR function.
static void AddRoundKey(uint8_t round,state_t* state,uint8_t* RoundKey)
{
    uint8_t i,j;
    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            (*state)[i][j] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
        }
    }
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
static void SubBytes(state_t* state)
{
    uint8_t i, j;
    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            (*state)[j][i] = getSBoxValue((*state)[j][i]);
        }
    }
}

// The ShiftRows() function shifts the rows in the state to the left.
// Each row is shifted with different offset.
// Offset = Row number. So the first row is not shifted.
static void ShiftRows(state_t* state)
{
    uint8_t temp;

    // Rotate first row 1 columns to left  
    temp           = (*state)[0][1];
    (*state)[0][1] = (*state)[1][1];
    (*state)[1][1] = (*state)[2][1];
    (*state)[2][1] = (*state)[3][1];
    (*state)[3][1] = temp;

    // Rotate second row 2 columns to left  
    temp           = (*state)[0][2];
    (*state)[0][2] = (*state)[2][2];
    (*state)[2][2] = temp;

    temp           = (*state)[1][2];
    (*state)[1][2] = (*state)[3][2];
    (*state)[3][2] = temp;

    // Rotate third row 3 columns to left
    temp           = (*state)[0][3];
    (*state)[0][3] = (*state)[3][3];
    (*state)[3][3] = (*state)[2][3];
    (*state)[2][3] = (*state)[1][3];
    (*state)[1][3] = temp;
}

static uint8_t xtime(uint8_t x)
{
    return ((x<<1) ^ (((x>>7) & 1) * 0x1b));
}

// MixColumns function mixes the columns of the state matrix
static void MixColumns(state_t* state)
{
    uint8_t i;
    uint8_t Tmp, Tm, t;
    for (i = 0; i < 4; ++i)
    {  
        t   = (*state)[i][0];
        Tmp = (*state)[i][0] ^ (*state)[i][1] ^ (*state)[i][2] ^ (*state)[i][3] ;
        Tm  = (*state)[i][0] ^ (*state)[i][1] ; Tm = xtime(Tm);  (*state)[i][0] ^= Tm ^ Tmp ;
        Tm  = (*state)[i][1] ^ (*state)[i][2] ; Tm = xtime(Tm);  (*state)[i][1] ^= Tm ^ Tmp ;
        Tm  = (*state)[i][2] ^ (*state)[i][3] ; Tm = xtime(Tm);  (*state)[i][2] ^= Tm ^ Tmp ;
        Tm  = (*state)[i][3] ^ t ;              Tm = xtime(Tm);  (*state)[i][3] ^= Tm ^ Tmp ;
    }
}

// Multiply is used to multiply numbers in the field GF(2^8)
// Note: The last call to xtime() is unneeded, but often ends up generating a smaller binary
//       The compiler seems to be able to vectorize the operation better this way.
//       See https://github.com/kokke/tiny-AES-c/pull/34
#if MULTIPLY_AS_A_FUNCTION
static uint8_t Multiply(uint8_t x, uint8_t y)
{
    return (((y & 1) * x) ^
            ((y>>1 & 1) * xtime(x)) ^
            ((y>>2 & 1) * xtime(xtime(x))) ^
            ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^
            ((y>>4 & 1) * xtime(xtime(xtime(xtime(x)))))); /* this last call to xtime() can be omitted */
}
#else
#define Multiply(x, y)                                \
    (  ((y & 1) * x) ^                              \
       ((y>>1 & 1) * xtime(x)) ^                       \
       ((y>>2 & 1) * xtime(xtime(x))) ^                \
       ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^         \
       ((y>>4 & 1) * xtime(xtime(xtime(xtime(x))))))   \

#endif

// MixColumns function mixes the columns of the state matrix.
// The method used to multiply may be difficult to understand for the inexperienced.
// Please use the references to gain more information.
static void InvMixColumns(state_t* state)
{
    int i;
    uint8_t a, b, c, d;
    for (i = 0; i < 4; ++i)
    { 
        a = (*state)[i][0];
        b = (*state)[i][1];
        c = (*state)[i][2];
        d = (*state)[i][3];

        (*state)[i][0] = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
        (*state)[i][1] = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
        (*state)[i][2] = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
        (*state)[i][3] = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
    }
}


// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
static void InvSubBytes(state_t* state)
{
    uint8_t i, j;
    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            (*state)[j][i] = getSBoxInvert((*state)[j][i]);
        }
    }
}

static void InvShiftRows(state_t* state)
{
    uint8_t temp;

    // Rotate first row 1 columns to right  
    temp = (*state)[3][1];
    (*state)[3][1] = (*state)[2][1];
    (*state)[2][1] = (*state)[1][1];
    (*state)[1][1] = (*state)[0][1];
    (*state)[0][1] = temp;

    // Rotate second row 2 columns to right 
    temp = (*state)[0][2];
    (*state)[0][2] = (*state)[2][2];
    (*state)[2][2] = temp;

    temp = (*state)[1][2];
    (*state)[1][2] = (*state)[3][2];
    (*state)[3][2] = temp;

    // Rotate third row 3 columns to right
    temp = (*state)[0][3];
    (*state)[0][3] = (*state)[1][3];
    (*state)[1][3] = (*state)[2][3];
    (*state)[2][3] = (*state)[3][3];
    (*state)[3][3] = temp;
}

// Cipher is the main function that encrypts the PlainText.
static void Cipher(state_t* state, uint8_t* RoundKey)
{
    uint8_t round = 0;

    // Add the First round key to the state before starting the rounds.
    AddRoundKey(0, state, RoundKey); 

    // There will be Nr rounds.
    // The first Nr-1 rounds are identical.
    // These Nr-1 rounds are executed in the loop below.
    for (round = 1; round < Nr; ++round)
    {
        SubBytes(state);
        ShiftRows(state);
        MixColumns(state);
        AddRoundKey(round, state, RoundKey);
    }

    // The last round is given below.
    // The MixColumns function is not here in the last round.
    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(Nr, state, RoundKey);
}

static void InvCipher(state_t* state,uint8_t* RoundKey)
{
    uint8_t round = 0;

    // Add the First round key to the state before starting the rounds.
    AddRoundKey(Nr, state, RoundKey); 

    // There will be Nr rounds.
    // The first Nr-1 rounds are identical.
    // These Nr-1 rounds are executed in the loop below.
    for (round = (Nr - 1); round > 0; --round)
    {
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(round, state, RoundKey);
        InvMixColumns(state);
    }

    // The last round is given below.
    // The MixColumns function is not here in the last round.
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(0, state, RoundKey);
}

/*****************************************************************************/
/* Public functions:                                                         */
/*****************************************************************************/

static void XorWithIv(uint8_t* buf, uint8_t* Iv)
{
    uint8_t i;
    for (i = 0; i < AES_BLOCKLEN; ++i) // The block in AES is always 128bit no matter the key size
    {
        buf[i] ^= Iv[i];
    }
}

static void AES_CBC_encrypt_buffer(struct AES_ctx *ctx,uint8_t* buf, uint32_t length)
{
    uintptr_t i;
    uint8_t *Iv = ctx->Iv;
    for (i = 0; i < length; i += AES_BLOCKLEN)
    {
        XorWithIv(buf, Iv);
        Cipher((state_t*)buf, ctx->RoundKey);
        Iv = buf;
        buf += AES_BLOCKLEN;
        //printf("Step %d - %d", i/16, i);
    }
    /* store Iv in ctx for next call */
    memcpy(ctx->Iv, Iv, AES_BLOCKLEN);
}

static void AES_CBC_decrypt_buffer(struct AES_ctx* ctx, uint8_t* buf,  uint32_t length)
{
    uintptr_t i;
    uint8_t storeNextIv[AES_BLOCKLEN];
    for (i = 0; i < length; i += AES_BLOCKLEN)
    {
        memcpy(storeNextIv, buf, AES_BLOCKLEN);
        InvCipher((state_t*)buf, ctx->RoundKey);
        XorWithIv(buf, ctx->Iv);
        memcpy(ctx->Iv, storeNextIv, AES_BLOCKLEN);
        buf += AES_BLOCKLEN;
    }

}

#pragma endregion /* AES */

#pragma region Poly1305

#define mul32x32_64(a, b) ((uint64_t)(a) * (b))

#define U8TO32_LE(p)                                                           \
  (((uint32_t)((p)[0])) | ((uint32_t)((p)[1]) << 8) |                          \
   ((uint32_t)((p)[2]) << 16) | ((uint32_t)((p)[3]) << 24))

#define U32TO8_LE(p, v)                                                        \
  do {                                                                         \
    (p)[0] = (uint8_t)((v));                                                   \
    (p)[1] = (uint8_t)((v) >> 8);                                              \
    (p)[2] = (uint8_t)((v) >> 16);                                             \
    (p)[3] = (uint8_t)((v) >> 24);                                             \
  } while (0)

void poly1305_auth(unsigned char out[POLY1305_TAGLEN], const unsigned char *m,
        size_t inlen, const unsigned char key[POLY1305_KEYLEN]) {
    uint32_t t0, t1, t2, t3;
    uint32_t h0, h1, h2, h3, h4;
    uint32_t r0, r1, r2, r3, r4;
    uint32_t s1, s2, s3, s4;
    uint32_t b, nb;
    size_t j;
    uint64_t t[5];
    uint64_t f0, f1, f2, f3;
    uint32_t g0, g1, g2, g3, g4;
    uint64_t c;
    unsigned char mp[16];

    /* clamp key */
    t0 = U8TO32_LE(key + 0);
    t1 = U8TO32_LE(key + 4);
    t2 = U8TO32_LE(key + 8);
    t3 = U8TO32_LE(key + 12);

    /* precompute multipliers */
    r0 = t0 & 0x3ffffff;
    t0 >>= 26;
    t0 |= t1 << 6;
    r1 = t0 & 0x3ffff03;
    t1 >>= 20;
    t1 |= t2 << 12;
    r2 = t1 & 0x3ffc0ff;
    t2 >>= 14;
    t2 |= t3 << 18;
    r3 = t2 & 0x3f03fff;
    t3 >>= 8;
    r4 = t3 & 0x00fffff;

    s1 = r1 * 5;
    s2 = r2 * 5;
    s3 = r3 * 5;
    s4 = r4 * 5;

    /* init state */
    h0 = 0;
    h1 = 0;
    h2 = 0;
    h3 = 0;
    h4 = 0;

    /* full blocks */
    if (inlen < 16)
        goto poly1305_donna_atmost15bytes;
poly1305_donna_16bytes:
    m += 16;
    inlen -= 16;

    t0 = U8TO32_LE(m - 16);
    t1 = U8TO32_LE(m - 12);
    t2 = U8TO32_LE(m - 8);
    t3 = U8TO32_LE(m - 4);

    h0 += t0 & 0x3ffffff;
    h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
    h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
    h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
    h4 += (t3 >> 8) | (1 << 24);

poly1305_donna_mul:
    t[0] = mul32x32_64(h0, r0) + mul32x32_64(h1, s4) + mul32x32_64(h2, s3) +
        mul32x32_64(h3, s2) + mul32x32_64(h4, s1);
    t[1] = mul32x32_64(h0, r1) + mul32x32_64(h1, r0) + mul32x32_64(h2, s4) +
        mul32x32_64(h3, s3) + mul32x32_64(h4, s2);
    t[2] = mul32x32_64(h0, r2) + mul32x32_64(h1, r1) + mul32x32_64(h2, r0) +
        mul32x32_64(h3, s4) + mul32x32_64(h4, s3);
    t[3] = mul32x32_64(h0, r3) + mul32x32_64(h1, r2) + mul32x32_64(h2, r1) +
        mul32x32_64(h3, r0) + mul32x32_64(h4, s4);
    t[4] = mul32x32_64(h0, r4) + mul32x32_64(h1, r3) + mul32x32_64(h2, r2) +
        mul32x32_64(h3, r1) + mul32x32_64(h4, r0);

    h0 = (uint32_t)t[0] & 0x3ffffff;
    c = (t[0] >> 26);
    t[1] += c;
    h1 = (uint32_t)t[1] & 0x3ffffff;
    b = (uint32_t)(t[1] >> 26);
    t[2] += b;
    h2 = (uint32_t)t[2] & 0x3ffffff;
    b = (uint32_t)(t[2] >> 26);
    t[3] += b;
    h3 = (uint32_t)t[3] & 0x3ffffff;
    b = (uint32_t)(t[3] >> 26);
    t[4] += b;
    h4 = (uint32_t)t[4] & 0x3ffffff;
    b = (uint32_t)(t[4] >> 26);
    h0 += b * 5;

    if (inlen >= 16)
        goto poly1305_donna_16bytes;

    /* final bytes */
poly1305_donna_atmost15bytes:
    if (!inlen)
        goto poly1305_donna_finish;

    for (j = 0; j < inlen; j++)
        mp[j] = m[j];
    mp[j++] = 1;
    for (; j < 16; j++)
        mp[j] = 0;
    inlen = 0;

    t0 = U8TO32_LE(mp + 0);
    t1 = U8TO32_LE(mp + 4);
    t2 = U8TO32_LE(mp + 8);
    t3 = U8TO32_LE(mp + 12);

    h0 += t0 & 0x3ffffff;
    h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
    h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
    h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
    h4 += (t3 >> 8);

    goto poly1305_donna_mul;

poly1305_donna_finish:
    b = h0 >> 26;
    h0 = h0 & 0x3ffffff;
    h1 += b;
    b = h1 >> 26;
    h1 = h1 & 0x3ffffff;
    h2 += b;
    b = h2 >> 26;
    h2 = h2 & 0x3ffffff;
    h3 += b;
    b = h3 >> 26;
    h3 = h3 & 0x3ffffff;
    h4 += b;
    b = h4 >> 26;
    h4 = h4 & 0x3ffffff;
    h0 += b * 5;
    b = h0 >> 26;
    h0 = h0 & 0x3ffffff;
    h1 += b;

    g0 = h0 + 5;
    b = g0 >> 26;
    g0 &= 0x3ffffff;
    g1 = h1 + b;
    b = g1 >> 26;
    g1 &= 0x3ffffff;
    g2 = h2 + b;
    b = g2 >> 26;
    g2 &= 0x3ffffff;
    g3 = h3 + b;
    b = g3 >> 26;
    g3 &= 0x3ffffff;
    g4 = h4 + b - (1 << 26);

    b = (g4 >> 31) - 1;
    nb = ~b;
    h0 = (h0 & nb) | (g0 & b);
    h1 = (h1 & nb) | (g1 & b);
    h2 = (h2 & nb) | (g2 & b);
    h3 = (h3 & nb) | (g3 & b);
    h4 = (h4 & nb) | (g4 & b);

    f0 = ((h0) | (h1 << 26)) + (uint64_t)U8TO32_LE(&key[16]);
    f1 = ((h1 >> 6) | (h2 << 20)) + (uint64_t)U8TO32_LE(&key[20]);
    f2 = ((h2 >> 12) | (h3 << 14)) + (uint64_t)U8TO32_LE(&key[24]);
    f3 = ((h3 >> 18) | (h4 << 8)) + (uint64_t)U8TO32_LE(&key[28]);

    U32TO8_LE(&out[0], f0);
    f1 += (f0 >> 32);
    U32TO8_LE(&out[4], f1);
    f2 += (f1 >> 32);
    U32TO8_LE(&out[8], f2);
    f3 += (f2 >> 32);
    U32TO8_LE(&out[12], f3);
}

#pragma endregion /* Poly1305 */

#pragma region Pseudo random generator

/*
// Source for the OS Cryptographically-Secure Pseudo-Random Number Generator
// Copyright 2017 Michael Thomas Greer
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file ../LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt )
*/

#ifdef _WIN32

/* ------------------------------------------------------------------------------------------- */
static CSPRNG csprng_create()
{
    CSPRNG_TYPE csprng;
    if (!CryptAcquireContextA( &csprng.hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT ))
        csprng.hCryptProv = 0;
    return csprng.object;
}

/* ------------------------------------------------------------------------------------------- */
static int csprng_get( CSPRNG object, void* dest, unsigned long long size )
{
    // Alas, we have to be pedantic here. csprng_get().size is a 64-bit entity.
    // However, CryptGenRandom().size is only a 32-bit DWORD. So we have to make sure failure
    // isn't from providing less random data than requested, even if absurd.
    unsigned long long n;

    CSPRNG_TYPE csprng;
    csprng.object = object;
    if (!csprng.hCryptProv) return 0;

    n = size >> 30;
    while (n--)
        if (!CryptGenRandom( csprng.hCryptProv, 1UL << 30, (BYTE*)dest )) return 0;

    return !!CryptGenRandom( csprng.hCryptProv, size & ((1ULL << 30) - 1), (BYTE*)dest );
}

/* ------------------------------------------------------------------------------------------- */
static CSPRNG csprng_destroy( CSPRNG object )
{
    CSPRNG_TYPE csprng;
    csprng.object = object;
    if (csprng.hCryptProv) CryptReleaseContext( csprng.hCryptProv, 0 );
    return 0;
}

/* ///////////////////////////////////////////////////////////////////////////////////////////// */
#else  /* Using /dev/urandom                                                                     */
/* ///////////////////////////////////////////////////////////////////////////////////////////// */

/* ------------------------------------------------------------------------------------------- */
static CSPRNG csprng_create()
{
    CSPRNG_TYPE csprng;
    csprng.urandom = fopen( "/dev/urandom", "rb" );
    return csprng.object;
}

/* ------------------------------------------------------------------------------------------- */
static int csprng_get( CSPRNG object, void* dest, unsigned long long size )
{
    CSPRNG_TYPE csprng;
    csprng.object = object;
    return (csprng.urandom) && (fread( (char*)dest, 1, size, csprng.urandom ) == size);
}

/* ------------------------------------------------------------------------------------------- */
static CSPRNG csprng_destroy( CSPRNG object )
{
    CSPRNG_TYPE csprng;
    csprng.object = object;
    if (csprng.urandom) fclose( csprng.urandom );
    return 0;
}

#endif /* _WIN32 */

#pragma endregion /* Pseudo random generator */

#pragma endregion /* Encryption */

#endif /* NBNET_IMPL */

#pragma endregion /* Implementations */
