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

/*
    --- NBNET ---

    How to use:
        #define NBNET_IMPL
    before you include this file in *one* C or C++ file.
    
    You'll also need to load a network driver:
        #define <NETWORK DRIVER MACRO NAME> (look into the driver header file to get the actual name)
    before you include the driver header in *one* C or C++ file.
    The driver header needs to be included *after* this one.
*/

#ifndef NBNET_H
#define NBNET_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#pragma region Declarations

#define NBN_Abort abort /* TODO: custom abort mechanism */

#pragma region Memory management

#define NBN_Alloc(size) __Alloc(size)
#define NBN_AllocObject(obj_type) __AllocObject(obj_type)
#define NBN_Dealloc(ptr) __Dealloc(ptr)
#define NBN_DeallocObject(obj_type, obj_ptr) __DeallocObject(obj_type, obj_ptr)

typedef struct
{
    unsigned int alloc_count;
    unsigned int dealloc_count;
    unsigned int object_allocs[32];
    unsigned int object_deallocs[32];
    unsigned int created_message_count;
    unsigned int destroyed_message_count;
} NBN_MemoryReport;

typedef struct
{
    NBN_MemoryReport report;
} NBN_MemoryManager;

typedef enum
{
    NBN_OBJ_MESSAGE,
    NBN_OBJ_MESSAGE_CHUNK,
    NBN_OBJ_CONNECTION,
    NBN_OBJ_EVENT
} NBN_ObjectType;

void *NBN_MemoryManager_Alloc(size_t);
void *NBN_MemoryManager_AllocObject(NBN_ObjectType);
void NBN_MemoryManager_Dealloc(void *);
void NBN_MemoryManager_DeallocObject(NBN_ObjectType, void *);
NBN_MemoryReport NBN_MemoryManager_GetReport(void);

#pragma endregion

#pragma region NBN_List

typedef struct NBN_ListNode
{
    void *data;
    struct NBN_ListNode *next;
    struct NBN_ListNode *prev;
} NBN_ListNode;

typedef struct NBN_List
{
    NBN_ListNode *head;
    NBN_ListNode *tail;
    unsigned int count;
} NBN_List;

typedef void (*NBN_List_FreeItemFunc)(void *);

NBN_List *NBN_List_Create();
void NBN_List_Destroy(NBN_List *, bool, NBN_List_FreeItemFunc);
void NBN_List_PushBack(NBN_List *, void *);
void *NBN_List_Remove(NBN_List *, void *);
void *NBN_List_RemoveAt(NBN_List *, int);
bool NBN_List_Includes(NBN_List *, void *);

#pragma endregion /* NBN_List */

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
#define ASSERTED_SERIALIZE(v, min, max, code)     \
{                                                 \
    if (stream->type == NBN_STREAM_WRITE)         \
    ASSERT_VALUE_IN_RANGE(v, min, max);           \
    if (code < 0)                                 \
    return -1;                                    \
    if (stream->type == NBN_STREAM_READ)          \
    ASSERT_VALUE_IN_RANGE(v, min, max);           \
}

#define SERIALIZE_UINT(v, min, max) ASSERTED_SERIALIZE(v, min, max, stream->serialize_uint_func(stream, (unsigned int *)&(v), min, max))
#define SERIALIZE_INT(v, min, max) ASSERTED_SERIALIZE(v, min, max, stream->serialize_int_func(stream, &(v), min, max))
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
int NBN_MeasureStream_SerializeBool(NBN_MeasureStream *, unsigned int *);
int NBN_MeasureStream_SerializePadding(NBN_MeasureStream *);
int NBN_MeasureStream_SerializeBytes(NBN_MeasureStream *, uint8_t *, unsigned int);
void NBN_MeasureStream_Reset(NBN_MeasureStream *);

#pragma endregion /* NBN_MeasureStream */

#pragma endregion /* Serialization */

#pragma region NBN_Message

#define NBN_MAX_CHANNELS 255 /* Maximum value of uint8_t, see message header */
#define NBN_MAX_MESSAGE_TYPES 255 /* Maximum value of uint8_t, see message header */
#define NBN_MESSAGE_RESEND_DELAY 0.1 /* Number of seconds before a message is resent (reliable messages redundancy) */

typedef int (*NBN_MessageSerializer)(void *, NBN_Stream *);
typedef void *(*NBN_MessageBuilder)(void);
typedef void (*NBN_MessageDestructor)(void *);

typedef struct
{
    uint16_t id;
    uint8_t type;
    uint8_t channel_id;
} __attribute__((__packed__)) NBN_MessageHeader;

typedef struct __NBN_Connection NBN_Connection;
typedef struct __NBN_Channel NBN_Channel;

typedef struct
{
    NBN_MessageHeader header;
    NBN_Connection *sender;
    NBN_MessageSerializer serializer;
    NBN_MessageDestructor destructor;
    double last_send_time;
    bool sent;
    bool acked;
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

NBN_Message *NBN_Message_Create(uint8_t, NBN_MessageSerializer, NBN_MessageDestructor, void *);
void NBN_Message_Destroy(NBN_Message *, bool);
int NBN_Message_SerializeHeader(NBN_MessageHeader *, NBN_Stream *);
int NBN_Message_Measure(NBN_Message *, NBN_MeasureStream *);
NBN_Message *NBN_Message_ReadFromStream(NBN_ReadStream *);

#pragma endregion /* NBN_Message */

#pragma region NBN_Packet

/*  
 * Maximum allowed packet size (including header) in bytes.
 * 1024 is an arbitrary value chosen to be below the MTU in order to avoid packet fragmentation.
*/
#define NBN_PACKET_MAX_SIZE 1024
#define NBN_MAX_MESSAGES_PER_PACKET 255 /* Maximum value of uint8_t, see packet header */

#define NBN_PACKET_HEADER_SIZE sizeof(NBN_PacketHeader)
#define NBN_PACKET_MAX_DATA_SIZE (NBN_PACKET_MAX_SIZE - NBN_PACKET_HEADER_SIZE)

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
} __attribute__((__packed__))  NBN_PacketHeader;

typedef struct
{
    NBN_PacketHeader header;
    NBN_PacketMode mode;
    struct __NBN_Connection *sender; /* not serialized, fill by the network driver upon reception */
    uint8_t buffer[NBN_PACKET_MAX_SIZE];
    unsigned int size; // in bytes
    bool sealed;

    // streams
    NBN_WriteStream w_stream;
    NBN_ReadStream r_stream;
    NBN_MeasureStream m_stream;
} NBN_Packet;

void NBN_Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
int NBN_Packet_InitRead(NBN_Packet *, uint8_t[NBN_PACKET_MAX_SIZE], unsigned int);
int NBN_Packet_WriteMessage(NBN_Packet *, NBN_Message *);
NBN_Message *NBN_Packet_ReadNextMessage(NBN_Packet *);
int NBN_Packet_Seal(NBN_Packet *);

#pragma endregion /* NBN_Packet */

#pragma region NBN_MessageChunk

/* Chunk max size is the number of bytes of data a packet can hold minus the size of a message header minus 2 bytes
 * to hold the chunk id and total number of chunks.
 */
#define NBN_MESSAGE_CHUNK_SIZE (NBN_PACKET_MAX_DATA_SIZE - sizeof(NBN_MessageHeader) - 2)
#define NBN_MESSAGE_CHUNK_TYPE (NBN_MAX_MESSAGE_TYPES - 1) /* Reserved message type for chunks */

typedef struct
{
    uint8_t id;
    uint8_t total;
    uint8_t data[NBN_MESSAGE_CHUNK_SIZE];
} NBN_MessageChunk;

NBN_MessageChunk *NBN_MessageChunk_Create(void);
void NBN_MessageChunk_Destroy(NBN_MessageChunk *);
int NBN_MessageChunk_Serialize(NBN_MessageChunk *, NBN_Stream *);

#pragma endregion /* NBN_MessageChunk */

#pragma region NBN_ClientClosedMessage

#define NBN_CLIENT_CLOSED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 2) /* Reserved message type */

typedef struct
{
    uint32_t code;
} NBN_ClientClosedMessage;

NBN_MessageChunk *NBN_ClientClosedMessage_Create(void);
void NBN_ClientClosedMessage_Destroy(NBN_ClientClosedMessage *);
int NBN_ClientClosedMessage_Serialize(NBN_ClientClosedMessage *, NBN_Stream *);

#pragma endregion /* NBN_ClientClosedMessage */

#pragma region NBN_Channel

#define NBN_CHANNEL_BUFFER_SIZE 512
#define NBN_CHANNEL_CHUNKS_BUFFER_SIZE 255

/* Library reserved unreliable ordered channel */
#define NBN_RESERVED_UNRELIABLE_CHANNEL 0

/* Library reserved reliable ordered channel */
#define NBN_RESERVED_RELIABLE_CHANNEL 1

typedef enum
{
    /* Skip the message */
    NBN_SKIP_MESSAGE,

    /* Skip the message and remove it from the send queue */
    NBN_DISCARD_MESSAGE,

    /* Add the message to the outgoing packet and remove it from the send queue */
    NBN_SEND_ONCE,

    /* Add the message to the outgoing packet and leave it in the send queue */
    NBN_SEND_REPEAT
} NBN_OutgoingMessagePolicy;

typedef enum
{
    NBN_CHANNEL_UNDEFINED = -1,
    NBN_CHANNEL_UNRELIABLE_ORDERED,
    NBN_CHANNEL_RELIABLE_ORDERED
} NBN_ChannelType;

struct __NBN_Channel
{
    uint8_t id;
    uint16_t next_message_id;
    unsigned int outgoing_message_count;
    unsigned int chunk_count;
    int last_received_chunk_id;
    double time;
    NBN_MessageChunk *chunks_buffer[NBN_CHANNEL_CHUNKS_BUFFER_SIZE];
    NBN_OutgoingMessagePolicy (*GetOutgoingMessagePolicy)(NBN_Channel *, NBN_Message *);
    bool (*AddReceivedMessage)(NBN_Channel *, NBN_Message *);
    bool (*CanProcessReceivedMessage)(NBN_Channel *, NBN_Message *);
    void (*OnMessageAcked)(NBN_Channel *, NBN_Message *);
};

void NBN_Channel_AddTime(NBN_Channel *, double);
bool NBN_Channel_AddChunk(NBN_Channel *, NBN_Message *);
NBN_Message *NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *, NBN_Connection *);
bool NBN_Channel_HasRoomForMessage(NBN_Channel *, unsigned int message_size);

/*
    Unreliable ordered

    Guarantee that messages will be received in order, does not however guarantee that all message will be received when
    packets get lost. This is meant to be used for time critical messages when it does not matter that much if they
    end up getting lost. A good example would be game snaphosts when any new newly received snapshot is more up to date
    than the previous one.
*/
typedef struct
{
    NBN_Channel base;
    uint16_t last_received_message_id;
} NBN_UnreliableOrderedChannel;

NBN_UnreliableOrderedChannel *NBN_UnreliableOrderedChannel_Create(uint8_t);

/*
    Reliable ordered

    Will guarantee that all messages will be received in order. Use this when you want to make sure a message
    will be received, for example for chat messages or initial game world state.
*/
typedef struct
{
    NBN_Channel base;
    uint16_t last_received_message_id;
    uint16_t oldest_unacked_message_id;
    uint16_t most_recent_message_id;
    uint32_t recv_message_buffer[NBN_CHANNEL_BUFFER_SIZE];
    bool ack_buffer[NBN_CHANNEL_BUFFER_SIZE];
} NBN_ReliableOrderedChannel;

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(uint8_t);

#pragma endregion /* NBN_Channel */

#pragma region NBN_Config

typedef struct
{
    const char *protocol_name;
    const char *ip_address;
    uint16_t port;
} NBN_Config;

#pragma endregion

#pragma region NBN_Connection

#define NBN_MAX_PACKET_ENTRIES 1024

/* Maximum number of packets sent at once */
#define NBN_MAX_SENT_PACKET_COUNT 8

/* Number of seconds before the connection is considered stale and get closed */
#define NBN_CONNECTION_STALE_TIME_THRESHOLD 3

typedef struct
{
    bool acked;
    int messages_count;
    uint16_t message_ids[NBN_MAX_MESSAGES_PER_PACKET];
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

struct __NBN_Connection
{
    uint32_t id;
    uint32_t protocol_id;
    double last_recv_packet_time; /* Used to detect stale connections */
    double last_flush_time; /* Last time the send queue was flushed */
    double last_read_packets_time; /* Last time packets were read from the socket */
    double time; /* Current time */
    unsigned int downloaded_bytes; /* Keep track of bytes read from the socket (used for download bandwith calculation) */
    bool accepted;
    bool is_stale;
    bool closed;
    NBN_Config config;
    NBN_ConnectionStats stats;

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
    NBN_List *recv_queue;
    NBN_List *send_queue;
};

NBN_Connection *NBN_Connection_Create(uint32_t, uint32_t, NBN_Config);
void NBN_Connection_Destroy(NBN_Connection *);
int NBN_Connection_ProcessReceivedPacket(NBN_Connection *, NBN_Packet *);
NBN_Message *NBN_Connection_DequeueReceivedMessage(NBN_Connection *);
void *NBN_Connection_CreateOutgoingMessage(uint8_t);
int NBN_Connection_EnqueueOutgoingMessage(NBN_Connection *, uint8_t);
int NBN_Connection_FlushSendQueue(NBN_Connection *);
int NBN_Connection_CreateChannel(NBN_Connection *, NBN_ChannelType, uint8_t);
bool NBN_Connection_CheckIfStale(NBN_Connection *);
void NBN_Connection_AddTime(NBN_Connection *, double);

#pragma endregion /* NBN_Connection */

#define NBN_NO_EVENT 0 /* No event left in the events queue */

#pragma region NBN_EventQueue

typedef struct
{
    NBN_List *queue;
    void *last_event_data;
} NBN_EventQueue;

typedef struct
{
    int type;
    void *data;
} NBN_Event;

NBN_EventQueue *NBN_EventQueue_Create(void);
void NBN_EventQueue_Destroy(NBN_EventQueue *);
void NBN_EventQueue_Enqueue(NBN_EventQueue *, int, void *);
int NBN_EventQueue_Dequeue(NBN_EventQueue *);
bool NBN_EventQueue_IsEmpty(NBN_EventQueue *);

#pragma endregion /* NBN_EventQueue */

#pragma region Packet simulator

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#if defined(_WIN32) || defined(_WIN64)
#define NBNET_WINDOWS
#endif

/*
    Threading.

    Windows headers are a pain... they need to be included by the nbnet user before
    the nbnet header.
*/
#ifndef NBNET_WINDOWS
#include <pthread.h>
#endif /* NBNET_WINDOWS */

#ifdef NBN_GAME_CLIENT

#define NBN_Debug_SetPing(v) { game_client.endpoint.packet_simulator->ping = v; }
#define NBN_Debug_SetJitter(v) { game_client.endpoint.packet_simulator->jitter = v; }
#define NBN_Debug_SetPacketLoss(v) { game_client.endpoint.packet_simulator->packet_loss_ratio = v; }
#define NBN_Debug_SetPacketDuplication(v) { game_client.endpoint.packet_simulator->packet_duplication_ratio = v; }

#endif /* NBN_GAME_CLIENT */

#ifdef NBN_GAME_SERVER

#define NBN_Debug_SetPing(v) { game_server.endpoint.packet_simulator->ping = v; }
#define NBN_Debug_SetJitter(v) { game_server.endpoint.packet_simulator->jitter = v; }
#define NBN_Debug_SetPacketLoss(v) { game_server.endpoint.packet_simulator->packet_loss_ratio = v; }
#define NBN_Debug_SetPacketDuplication(v) { game_server.endpoint.packet_simulator->packet_duplication_ratio = v; }

#endif /* NBN_GAME_SERVER */

typedef struct
{
    NBN_Packet *packet;
    uint32_t receiver_id;
    double delay;
    double enqueued_at;
} NBN_PacketSimulatorEntry;

typedef struct
{
    NBN_List *packets_queue;
    double time;
#ifdef NBNET_WINDOWS
    HANDLE packets_queue_mutex;
    HANDLE thread; 
#else
    pthread_mutex_t packets_queue_mutex;
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

NBN_PacketSimulator *NBN_PacketSimulator_Create(void);
void NBN_PacketSimulator_Destroy(NBN_PacketSimulator *);
void NBN_PacketSimulator_EnqueuePacket(NBN_PacketSimulator *, NBN_Packet *, uint32_t);
void NBN_PacketSimulator_Start(NBN_PacketSimulator *);
void NBN_PacketSimulator_Stop(NBN_PacketSimulator *);
void NBN_PacketSimulator_AddTime(NBN_PacketSimulator *, double);

#else

#define NBN_Debug_SetPing(v) { NBN_LogInfo("NBN_Debug_SetPing: packet simulator is not enabled, ignore"); }
#define NBN_Debug_SetJitter(v) { NBN_LogInfo("NBN_Debug_SetJitter: packet simulator is not enabled, ignore"); }
#define NBN_Debug_SetPacketLoss(v) { NBN_LogInfo("NBN_Debug_SetPacketLoss: packet simulator is not enabled, ignore"); }
#define NBN_Debug_SetPacketDuplication(v) { NBN_LogInfo("NBN_Debug_SetPacketDuplication: packet simulator is not enabled, ignore"); }

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */

#pragma region NBN_Endpoint

#ifdef NBN_GAME_CLIENT

#define NBN_RegisterMessage(type, builder, serializer, destructor) \
{ \
    if (type == NBN_MESSAGE_CHUNK_TYPE || type == NBN_CLIENT_CLOSED_MESSAGE_TYPE) \
    { \
        NBN_LogError("Message type %d is reserved by the library", type); \
        NBN_Abort(); \
    } \
    NBN_Endpoint_RegisterMessageBuilder(&game_client.endpoint, (NBN_MessageBuilder)builder, type); \
    NBN_Endpoint_RegisterMessageDestructor(&game_client.endpoint, (NBN_MessageDestructor)destructor, type); \
    NBN_Endpoint_RegisterMessageSerializer(&game_client.endpoint, (NBN_MessageSerializer)serializer, type); \
}

#define NBN_RegisterChannel(type, id) \
{ \
    if (id == NBN_RESERVED_UNRELIABLE_CHANNEL || id == NBN_RESERVED_RELIABLE_CHANNEL) \
    { \
        NBN_LogError("Channel id %d is reserved by the library", type); \
        NBN_Abort(); \
    } \
    NBN_Endpoint_RegisterChannel(&game_client.endpoint, type, id); \
}

#endif /* NBN_GAME_CLIENT */

#ifdef NBN_GAME_SERVER

#define NBN_RegisterMessage(type, builder, serializer, destructor) \
{ \
    if (type == NBN_MESSAGE_CHUNK_TYPE || type == NBN_CLIENT_CLOSED_MESSAGE_TYPE) \
    { \
        NBN_LogError("Message type %d is reserved by the library", type); \
        NBN_Abort(); \
    } \
    NBN_Endpoint_RegisterMessageBuilder(&game_server.endpoint, (NBN_MessageBuilder)builder, type); \
    NBN_Endpoint_RegisterMessageDestructor(&game_server.endpoint, (NBN_MessageDestructor)destructor, type); \
    NBN_Endpoint_RegisterMessageSerializer(&game_server.endpoint, (NBN_MessageSerializer)serializer, type); \
}

#define NBN_RegisterChannel(type, id) \
{ \
    if (id == NBN_RESERVED_UNRELIABLE_CHANNEL || id == NBN_RESERVED_RELIABLE_CHANNEL) \
    { \
        NBN_LogError("Channel id %d is reserved by the library", type); \
        NBN_Abort(); \
    } \
    NBN_Endpoint_RegisterChannel(&game_server.endpoint, type, id); \
}

#endif /* NBN_GAME_SERVER */

typedef struct
{
    NBN_Config config;
    NBN_ChannelType channels[NBN_MAX_CHANNELS];
    NBN_MessageBuilder message_builders[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageDestructor message_destructors[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageSerializer message_serializers[NBN_MAX_MESSAGE_TYPES];
    NBN_EventQueue *events_queue;

#ifdef NBN_DEBUG
    /* Debug callbacks */
    void (*OnMessageAddedToRecvQueue)(NBN_Connection *, NBN_Message *);
#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator *packet_simulator;
#endif
} NBN_Endpoint;

void NBN_Endpoint_Init(NBN_Endpoint *, NBN_Config);
void NBN_Endpoint_Deinit(NBN_Endpoint *);
void NBN_Endpoint_RegisterMessageBuilder(NBN_Endpoint *, NBN_MessageBuilder, uint8_t);
void NBN_Endpoint_RegisterMessageDestructor(NBN_Endpoint *, NBN_MessageDestructor, uint8_t);
void NBN_Endpoint_RegisterMessageSerializer(NBN_Endpoint *, NBN_MessageSerializer, uint8_t);
void NBN_Endpoint_RegisterChannel(NBN_Endpoint *, NBN_ChannelType, uint8_t);
NBN_Connection *NBN_Endpoint_CreateConnection(NBN_Endpoint *, int);

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

#ifdef NBN_GAME_CLIENT

typedef enum
{
    NBN_ERROR = -1,

    /* Client is connected to server */
    NBN_CONNECTED = 1,

    /* Client is disconnected from the server */
    NBN_DISCONNECTED,

    /* Client has received a message from the server */
    NBN_MESSAGE_RECEIVED
} NBN_GameClientEvent;

typedef struct
{
    NBN_Endpoint endpoint;
    NBN_Connection *server_connection;
} NBN_GameClient;

extern NBN_GameClient game_client;

void NBN_GameClient_Init(const char *, const char *, uint16_t);
int NBN_GameClient_Start(void);
void NBN_GameClient_Stop(void);
void NBN_GameClient_AddTime(double);
NBN_GameClientEvent NBN_GameClient_Poll(void);
int NBN_GameClient_SendPackets(void);
void *NBN_GameClient_CreateMessage(uint8_t);
int NBN_GameClient_EnqueueMessage(uint8_t);
int NBN_GameClient_EnqueueUnreliableMessage(void);
int NBN_GameClient_EnqueueReliableMessage(void);
bool NBN_GameClient_CanEnqueueMessage(uint8_t);
NBN_Connection *NBN_GameClient_CreateServerConnection(void);
NBN_MessageInfo NBN_GameClient_GetReceivedMessageInfo(void);
NBN_ConnectionStats NBN_GameClient_GetStats(void);
NBN_Message *NBN_GameClient_GetOutgoingMessage(void);
int NBN_GameClient_GetClientClosedCode(void);

#ifdef NBN_DEBUG
void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* NBN_GameClient */

#pragma region NBN_GameServer

#ifdef NBN_GAME_SERVER

typedef enum
{
    NBN_ERROR = -1,

    /* A new client has connected */
    NBN_NEW_CONNECTION = 1,

    /* A client has disconnected */
    NBN_CLIENT_DISCONNECTED,

    /* A message has been received from a client */
    NBN_CLIENT_MESSAGE_RECEIVED
} NBN_GameServerEvent;

typedef struct
{
    float upload_bandwidth; /* Total upload bandwith of the game server */
    float download_bandwidth; /* Total download bandwith of the game server */
} NBN_GameServerStats;

typedef struct
{
    NBN_Endpoint endpoint;
    NBN_List* clients;
    NBN_GameServerStats stats;
} NBN_GameServer;

extern NBN_GameServer game_server;

void NBN_GameServer_Init(const char *, uint16_t);
int NBN_GameServer_Start(void);
void NBN_GameServer_Stop(void);
void NBN_GameServer_AddTime(double);
NBN_GameServerEvent NBN_GameServer_Poll(void);
int NBN_GameServer_SendPackets(void);
NBN_Connection *NBN_GameServer_CreateClientConnection(uint32_t);
void NBN_GameServer_CloseClient(NBN_Connection *, int);
void *NBN_GameServer_CreateMessage(uint8_t);
int NBN_GameServer_EnqueueMessageFor(NBN_Connection *, uint8_t);
int NBN_GameServer_EnqueueUnreliableMessageFor(NBN_Connection *);
int NBN_GameServer_EnqueueReliableMessageFor(NBN_Connection *);
NBN_Connection *NBN_GameServer_AcceptConnection(void);
void NBN_GameServer_RejectConnection(int);
bool NBN_GameServer_CanEnqueueMessageFor(NBN_Connection *, uint8_t);
uint32_t NBN_GameServer_GetDisconnectedClientId(void);
NBN_MessageInfo NBN_GameServer_GetReceivedMessageInfo(void);
NBN_Message *NBN_GameServer_GetOutgoingMessage(void);
NBN_GameServerStats NBN_GameServer_GetStats(void);

#ifdef NBN_DEBUG
void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#endif /* NBN_GAME_SERVER */

#pragma endregion /* NBN_GameServer */

#pragma region Network driver

#ifdef NBN_GAME_SERVER

typedef enum
{
    NBN_DRIVER_GSERV_CLIENT_CONNECTED,
    NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED
} NBN_Driver_GServ_EventType;

int NBN_Driver_GServ_Start(uint32_t, uint16_t);
void NBN_Driver_GServ_Stop(void);
int NBN_Driver_GServ_RecvPackets(void);
void NBN_Driver_GServ_DestroyClientConnection(uint32_t);
int NBN_Driver_GServ_SendPacketTo(NBN_Packet *, uint32_t);
void NBN_Driver_GServ_RaiseEvent(NBN_Driver_GServ_EventType, void *);

#endif /* NBN_GAME_SERVER */

#ifdef NBN_GAME_CLIENT

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

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* Network driver */

#pragma region Utils

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ABS(v) (((v) > 0) ? (v) : -(v))

#define SEQUENCE_NUMBER_GT(seq1, seq2) ((seq1 > seq2 && (seq1 - seq2) <= 32767) || (seq1 < seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_GTE(seq1, seq2) ((seq1 >= seq2 && (seq1 - seq2) <= 32767) || (seq1 <= seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_LT(seq1, seq2) ((seq1 < seq2 && (seq2 - seq1) <= 32767) || (seq1 > seq2 && (seq1 - seq2) >= 32767))

#pragma endregion /* Utils */

#pragma endregion /* Declarations */

#endif /* NBNET_H */

#pragma region Implementations

#ifdef NBNET_IMPL

static NBN_Message *outgoing_message = NULL;

#pragma region Memory management

static NBN_MemoryManager mem_manager = {
    .report = {
        .alloc_count = 0,
        .dealloc_count = 0,
        .object_allocs = {0},
        .object_deallocs = {0}}};

void *NBN_MemoryManager_Alloc(size_t size)
{
    mem_manager.report.alloc_count++;

    return malloc(size);
}

void *NBN_MemoryManager_AllocObject(NBN_ObjectType obj_type)
{
    void *obj_ptr;

    switch (obj_type)
    {
        case NBN_OBJ_MESSAGE:
            obj_ptr = NBN_MemoryManager_Alloc(sizeof(NBN_Message));
            break;

        case NBN_OBJ_MESSAGE_CHUNK:
            obj_ptr = NBN_MemoryManager_Alloc(sizeof(NBN_MessageChunk));
            break;

        case NBN_OBJ_CONNECTION:
            obj_ptr = NBN_MemoryManager_Alloc(sizeof(NBN_Connection));
            break;
        
        case NBN_OBJ_EVENT:
            obj_ptr = NBN_MemoryManager_Alloc(sizeof(NBN_Event));
            break;

        default:
            return NULL;
    }

#ifdef NBN_DEBUG
    mem_manager.report.object_allocs[obj_type]++;
#endif

    return obj_ptr;
}

void NBN_MemoryManager_Dealloc(void *ptr)
{
#ifdef NBN_DEBUG
    mem_manager.report.dealloc_count++;
#endif

    free(ptr);
}

void NBN_MemoryManager_DeallocObject(NBN_ObjectType obj_type, void *obj_ptr)
{
#ifdef NBN_DEBUG
    assert(mem_manager.report.object_allocs[obj_type] > 0);
#endif

    // TODO: implement object pooling here

    NBN_MemoryManager_Dealloc(obj_ptr);

#ifdef NBN_DEBUG
    mem_manager.report.object_deallocs[obj_type]++;
#endif
}

NBN_MemoryReport NBN_MemoryManager_GetReport(void)
{
    return mem_manager.report;
}

static inline void *__Alloc(size_t size)
{
    void *ptr = NBN_MemoryManager_Alloc(size);

    if (ptr == NULL)
    {
        NBN_LogError("Failed to allocate memory");
        NBN_Abort();
    }

    return ptr;
}

static inline void *__AllocObject(NBN_ObjectType obj_type)
{
    void *obj_ptr = NBN_MemoryManager_AllocObject(obj_type);

    if (obj_ptr == NULL)
    {
        NBN_LogError("Failed to allocate memory for object %d", obj_type);
        NBN_Abort();
    }

    return obj_ptr;
}

static inline void __Dealloc(void *ptr)
{
    NBN_MemoryManager_Dealloc(ptr);
}

static inline void __DeallocObject(NBN_ObjectType obj_type, void *obj_ptr)
{
    NBN_MemoryManager_DeallocObject(obj_type, obj_ptr);
}

#pragma endregion

#pragma region NBN_List

static NBN_ListNode *NBN_List_CreateNode(void *);
static void *NBN_List_RemoveNodeFromList(NBN_List *, NBN_ListNode *);

NBN_List *NBN_List_Create()
{
    NBN_List *list = NBN_Alloc(sizeof(NBN_List));

    if (list == NULL)
        return NULL;

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;

    return list;
}

void NBN_List_Destroy(NBN_List *list, bool free_items, NBN_List_FreeItemFunc free_item_func)
{
    NBN_ListNode *current_node = list->head;

    while (current_node != NULL)
    {
        NBN_ListNode *next_node = current_node->next;

        if (free_items)
        {
            if (free_item_func)
                free_item_func(current_node->data);
            else
                NBN_Dealloc(current_node->data);
        }

        NBN_Dealloc(current_node);

        current_node = next_node;
    }

    NBN_Dealloc(list);
}

void NBN_List_PushBack(NBN_List *list, void *data)
{
    NBN_ListNode *node = NBN_List_CreateNode(data);

    if (list->count == 0)
    {
        node->next = NULL;
        node->prev = NULL;

        list->head = node;
        list->tail = node;
    }
    else
    {
        node->next = NULL;
        node->prev = list->tail;

        list->tail->next = node;
        list->tail = node;
    }

    list->count++;
}

void *NBN_List_Remove(NBN_List *list, void *data)
{
    NBN_ListNode *current_node = list->head;

    for (int i = 0; current_node != NULL && current_node->data != data; i++)
        current_node = current_node->next;

    if (current_node != NULL)
    {
        return NBN_List_RemoveNodeFromList(list, current_node);
    }

    return NULL;
}

void *NBN_List_RemoveAt(NBN_List *list, int index)
{
    NBN_ListNode *current_node = list->head;

    for (int i = 0; current_node != NULL && i < index; i++)
        current_node = current_node->next;

    if (current_node != NULL)
    {
        return NBN_List_RemoveNodeFromList(list, current_node);
    }

    return NULL;
}

bool NBN_List_Includes(NBN_List *list, void *data)
{
    NBN_ListNode *current_node = list->head;

    for (int i = 0; current_node != NULL && current_node->data != data; i++)
        current_node = current_node->next;

    return current_node != NULL;
}

static NBN_ListNode *NBN_List_CreateNode(void *data)
{
    NBN_ListNode *node = NBN_Alloc(sizeof(NBN_ListNode));

    node->data = data;

    return node;
}

static void *NBN_List_RemoveNodeFromList(NBN_List *list, NBN_ListNode *node)
{
    if (node == list->head)
    {
        NBN_ListNode *new_head = node->next;

        if (new_head != NULL)
            new_head->prev = NULL;
        else
            list->tail = NULL;

        list->head = new_head;

        void *data = node->data;

        NBN_Dealloc(node);
        list->count--;

        return data;
    }

    if (node == list->tail)
    {
        NBN_ListNode *new_tail = node->prev;

        new_tail->next = NULL;
        list->tail = new_tail;

        void *data = node->data;

        NBN_Dealloc(node);
        list->count--;

        return data;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;

    void *data = node->data;

    NBN_Dealloc(node);
    list->count--;

    return data;
}

#pragma endregion /* NBN_List */

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
            return -1;

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
        return -1;

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
    read_stream->base.serialize_bool_func = (NBN_Stream_SerializeBool)NBN_ReadStream_SerializeBool;
    read_stream->base.serialize_padding_func = (NBN_Stream_SerializePadding)NBN_ReadStream_SerializePadding;
    read_stream->base.serialize_bytes_func = (NBN_Stream_SerializeBytes)NBN_ReadStream_SerializeBytes;

    NBN_BitReader_Init(&read_stream->bit_reader, buffer, size);
}

int NBN_ReadStream_SerializeUint(NBN_ReadStream *read_stream, unsigned int *value, unsigned int min, unsigned int max)
{
    assert(min <= max);

    if (NBN_BitReader_Read(&read_stream->bit_reader, value, BITS_REQUIRED(min, max)) < 0)
        return -1;

    *value += min;

#ifdef NBN_DEBUG
    assert(*value >= min && *value <= max);
#else
    if (*value < min || *value > max)
        return -1;
#endif

    return 0;
}

int NBN_ReadStream_SerializeInt(NBN_ReadStream *read_stream, int *value, int min, int max)
{
    assert(min <= max);

    unsigned int isNegative = 0;
    unsigned int abs_min = MIN(abs(min), abs(max));
    unsigned int abs_max = MAX(abs(min), abs(max));

    isNegative = *value < 0;
    *value = abs(*value);

    if (NBN_ReadStream_SerializeBool(read_stream, &isNegative) < 0)
        return -1;

    if (NBN_ReadStream_SerializeUint(read_stream, (unsigned int *)value, (min < 0 && max > 0) ? 0 : abs_min, abs_max) < 0)
        return -1;

    if (isNegative)
        *value *= -1;

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
        return -1;
#endif

    return ret;
}

int NBN_ReadStream_SerializeBytes(NBN_ReadStream *read_stream, uint8_t *bytes, unsigned int length)
{
    if (length == 0)
        return -1;

    if (NBN_ReadStream_SerializePadding(read_stream) < 0)
        return -1;

    NBN_BitReader *bit_reader = &read_stream->bit_reader;

    // make sure we are at the start of a new byte after applying padding
    assert(bit_reader->scratch_bits_count % 8 == 0);

    if (length * 8 <= bit_reader->scratch_bits_count)
    {
        // the byte array is fully contained inside the read word

        Word word;

        if (NBN_BitReader_Read(bit_reader, &word, length * 8) < 0)
            return -1;

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
    write_stream->base.serialize_bool_func = (NBN_Stream_SerializeBool)NBN_WriteStream_SerializeBool;
    write_stream->base.serialize_padding_func = (NBN_Stream_SerializePadding)NBN_WriteStream_SerializePadding;
    write_stream->base.serialize_bytes_func = (NBN_Stream_SerializeBytes)NBN_WriteStream_SerializeBytes;

    NBN_BitWriter_Init(&write_stream->bit_writer, buffer, size);
}

int NBN_WriteStream_SerializeUint(NBN_WriteStream *write_stream, unsigned int *value, unsigned int min, unsigned int max)
{
    assert(min <= max);
    assert(*value >= min && *value <= max);

    if (NBN_BitWriter_Write(&write_stream->bit_writer, *value - min, BITS_REQUIRED(min, max)) < 0)
        return -1;

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
        return -1;

    if (NBN_WriteStream_SerializeUint(write_stream, (unsigned int *)value, (min < 0 && max > 0) ? 0 : abs_min, abs_max) < 0)
        return -1;

    if (isNegative)
        *value *= -1;

    return 0;
}

int NBN_WriteStream_SerializeBool(NBN_WriteStream *write_stream, unsigned int *value)
{
    return NBN_WriteStream_SerializeUint(write_stream, value, 0, 1);
}

int NBN_WriteStream_SerializePadding(NBN_WriteStream *write_stream)
{
    // if we are at the beginning of a new byte, no need to pad, no need to pad
    if (write_stream->bit_writer.scratch_bits_count % 8 == 0) 
        return 0;

    unsigned int padding = 8 - (write_stream->bit_writer.scratch_bits_count % 8);

    return NBN_BitWriter_Write(&write_stream->bit_writer, 0, padding);
}

int NBN_WriteStream_SerializeBytes(NBN_WriteStream *write_stream, uint8_t *bytes, unsigned int length)
{
    if (length == 0)
        return -1;

    if (NBN_WriteStream_SerializePadding(write_stream) < 0)
        return -1;

    NBN_BitWriter *bit_writer = &write_stream->bit_writer;

    // make sure we are at the start of a new byte after applying padding
    assert(bit_writer->scratch_bits_count % 8 == 0);

    if (NBN_WriteStream_Flush(write_stream) < 0)
        return -1;

    // make sure everything has been flushed to the buffer before writing the byte array
    assert(bit_writer->scratch_bits_count == 0);

    if (bit_writer->byte_cursor + length > bit_writer->size)
        return -1;

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
    measure_stream->base.serialize_bool_func = (NBN_Stream_SerializeBool)NBN_MeasureStream_SerializeBool;
    measure_stream->base.serialize_padding_func = (NBN_Stream_SerializePadding)NBN_MeasureStream_SerializePadding;
    measure_stream->base.serialize_bytes_func = (NBN_Stream_SerializeBytes)NBN_MeasureStream_SerializeBytes;

    measure_stream->number_of_bits = 0;
}

int NBN_MeasureStream_SerializeUint(NBN_MeasureStream *measure_stream, unsigned int *value, unsigned int min, unsigned int max)
{
    assert(min <= max);
    assert(*value >= min && *value <= max);

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
    unsigned number_of_bits = NBN_MeasureStream_SerializeUint(measure_stream, &abs_value, (min < 0 && max > 0) ? 0 : abs_min, abs_max);

    measure_stream->number_of_bits++; // +1 for int sign

    return number_of_bits + 1;
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

static int NBN_Packet_SerializeHeader(NBN_Packet *, NBN_Stream *);

void NBN_Packet_InitWrite(NBN_Packet *packet, uint32_t protocol_id, uint16_t seq_number, uint16_t ack, uint32_t ack_bits)
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

    NBN_WriteStream_Init(&packet->w_stream, packet->buffer + NBN_PACKET_HEADER_SIZE, NBN_PACKET_MAX_DATA_SIZE);
    NBN_MeasureStream_Init(&packet->m_stream);
}

int NBN_Packet_InitRead(NBN_Packet *packet, uint8_t buffer[NBN_PACKET_MAX_SIZE], unsigned int size)
{
    packet->mode = NBN_PACKET_MODE_READ;
    packet->sender = NULL;
    packet->size = size;
    packet->sealed = false;

    memcpy(packet->buffer, buffer, NBN_PACKET_MAX_SIZE);

    NBN_ReadStream header_r_stream;

    NBN_ReadStream_Init(&header_r_stream, buffer, NBN_PACKET_HEADER_SIZE);

    if (NBN_Packet_SerializeHeader(packet, (NBN_Stream *)&header_r_stream) < 0)
        return -1;

    NBN_ReadStream_Init(&packet->r_stream, buffer + NBN_PACKET_HEADER_SIZE, NBN_PACKET_MAX_DATA_SIZE);

    return 0;
}

int NBN_Packet_WriteMessage(NBN_Packet *packet, NBN_Message *message)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE || packet->sealed)
        return NBN_PACKET_WRITE_ERROR;

    int current_number_of_bits = packet->m_stream.number_of_bits;

    if (NBN_Message_Measure(message, &packet->m_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    if (packet->header.messages_count >= NBN_MAX_MESSAGES_PER_PACKET || packet->m_stream.number_of_bits > NBN_PACKET_MAX_DATA_SIZE * 8)
    {
        packet->m_stream.number_of_bits = current_number_of_bits;

        return NBN_PACKET_WRITE_NO_SPACE;
    }

    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)&packet->w_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    if (message->serializer(message->data, (NBN_Stream *)&packet->w_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    packet->size = (packet->m_stream.number_of_bits - 1) / 8 + 1;
    packet->header.messages_count++;

    return NBN_PACKET_WRITE_OK;
}

NBN_Message *NBN_Packet_ReadNextMessage(NBN_Packet *packet)
{
    return NBN_Message_ReadFromStream(&packet->r_stream);
}

int NBN_Packet_Seal(NBN_Packet *packet)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE)
        return -1;

    if (NBN_WriteStream_Flush(&packet->w_stream) < 0)
        return -1;

    NBN_WriteStream header_w_stream;

    NBN_WriteStream_Init(&header_w_stream, packet->buffer, NBN_PACKET_HEADER_SIZE);

    if (NBN_Packet_SerializeHeader(packet, (NBN_Stream *)&header_w_stream) < 0)
        return -1;

    if (NBN_WriteStream_Flush(&header_w_stream) < 0)
        return -1;

    packet->sealed = true;
    packet->size += NBN_PACKET_HEADER_SIZE;

    return 0;
}

static int NBN_Packet_SerializeHeader(NBN_Packet *packet, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&packet->header.protocol_id, sizeof(packet->header.protocol_id));
    SERIALIZE_BYTES(&packet->header.seq_number, sizeof(packet->header.seq_number));
    SERIALIZE_BYTES(&packet->header.ack, sizeof(packet->header.ack));
    SERIALIZE_BYTES(&packet->header.ack_bits, sizeof(packet->header.ack_bits));
    SERIALIZE_BYTES(&packet->header.messages_count, sizeof(packet->header.messages_count));

    return 0;
}

#pragma endregion /* NBN_Packet */

#pragma region NBN_Message

NBN_Message *NBN_Message_Create(
    uint8_t type, NBN_MessageSerializer serializer, NBN_MessageDestructor destructor, void *data)
{
    NBN_Message *message = NBN_AllocObject(NBN_OBJ_MESSAGE);

    message->header.id = 0;
    message->header.type = type;

    message->serializer = serializer;
    message->destructor = destructor;
    message->sender = NULL;
    message->last_send_time = -1;
    message->sent = false;
    message->acked = false;
    message->data = data;

#ifdef NBN_DEBUG
    mem_manager.report.created_message_count++;
#endif

    return message;
}

void NBN_Message_Destroy(NBN_Message *message, bool free_data)
{
    if (free_data)
    {
        message->destructor(message->data);

#ifdef NBN_DEBUG
        mem_manager.report.destroyed_message_count++;
#endif
    }

    NBN_DeallocObject(NBN_OBJ_MESSAGE, message);
}

int NBN_Message_SerializeHeader(NBN_MessageHeader *message_header, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&message_header->id, sizeof(message_header->id));
    SERIALIZE_BYTES(&message_header->type, sizeof(message_header->type));
    SERIALIZE_BYTES(&message_header->channel_id, sizeof(message_header->channel_id));

    return 0;
}

int NBN_Message_Measure(NBN_Message *message, NBN_MeasureStream *m_stream)
{
    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)m_stream) < 0)
        return -1;

    if (message->serializer(message->data, (NBN_Stream *)m_stream) < 0)
        return -1;

    return m_stream->number_of_bits;
}

NBN_Message *NBN_Message_ReadFromStream(NBN_ReadStream *r_stream)
{
    NBN_MessageHeader msg_header;

    if (NBN_Message_SerializeHeader(&msg_header, (NBN_Stream *)r_stream) < 0)
    {
        NBN_LogError("Failed to read net message header");

        return NULL;
    }

    uint8_t msg_type = msg_header.type;

#if defined(NBN_GAME_CLIENT)
    NBN_MessageBuilder msg_builder = game_client.endpoint.message_builders[msg_type];
    NBN_MessageSerializer msg_serializer = game_client.endpoint.message_serializers[msg_type];
    NBN_MessageDestructor msg_destructor = game_client.endpoint.message_destructors[msg_type];
#elif defined(NBN_GAME_SERVER)
    NBN_MessageBuilder msg_builder = game_server.endpoint.message_builders[msg_type];
    NBN_MessageSerializer msg_serializer = game_server.endpoint.message_serializers[msg_type];
    NBN_MessageDestructor msg_destructor = game_server.endpoint.message_destructors[msg_type];
#endif

    if (msg_builder == NULL)
    {
        NBN_LogError("No message builder is registered for messages of type %d", msg_type);

        return NULL;
    }

    if (msg_serializer == NULL)
    {
        NBN_LogError("No message serializer attached to message of type %d", msg_type);

        return NULL;
    }

    if (msg_destructor == NULL)
    {
        NBN_LogError("No message destructor attached to message of type %d", msg_type);

        return NULL;
    }

    NBN_Message *message = NBN_Message_Create(msg_header.type, msg_serializer, msg_destructor, msg_builder());

    message->header.id = msg_header.id;
    message->header.channel_id = msg_header.channel_id;

    if (message->serializer(message->data, (NBN_Stream *)r_stream) < 0)
    {
        NBN_LogError("Failed to read message body");

        return NULL;
    }

    return message;
}

#pragma endregion /* NBN_Message */

#pragma region NBN_MessageChunk

NBN_MessageChunk *NBN_MessageChunk_Create(void)
{
    return NBN_AllocObject(NBN_OBJ_MESSAGE_CHUNK);
}

void NBN_MessageChunk_Destroy(NBN_MessageChunk *chunk)
{
    NBN_DeallocObject(NBN_OBJ_MESSAGE_CHUNK, chunk);
}

int NBN_MessageChunk_Serialize(NBN_MessageChunk *chunk, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&chunk->id, 1);
    SERIALIZE_BYTES(&chunk->total, 1);
    SERIALIZE_BYTES(chunk->data, NBN_MESSAGE_CHUNK_SIZE);

    return 0;
}

#pragma endregion /* NBN_MessageChunk */

#pragma region NBN_ClientClosedMessage

NBN_MessageChunk *NBN_ClientClosedMessage_Create(void)
{
    return NBN_Alloc(sizeof(NBN_ClientClosedMessage));
}

void NBN_ClientClosedMessage_Destroy(NBN_ClientClosedMessage *msg)
{
    NBN_Dealloc(msg);
}

int NBN_ClientClosedMessage_Serialize(NBN_ClientClosedMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->code, 0, UINT32_MAX);

    return 0;
}

#pragma endregion /* NBN_ClientClosedMessage */

#pragma region NBN_Connection

typedef enum
{
    NBN_MESSAGE_ERROR = -1,
    NBN_MESSAGE_ADDED_TO_PACKET,
    NBN_MESSAGE_NOT_ADDED_TO_PACKET,
    NBN_PACKET_FULL
} NBN_OutgoingMessageProcessResult;

static uint32_t NBN_Connection_BuildPacketAckBits(NBN_Connection *);
static void NBN_Connection_DecodePacketHeader(NBN_Connection *, NBN_Packet *);
static void NBN_Connection_AckPacket(NBN_Connection *, uint16_t);
static int NBN_Connection_AddReceivedMessage(NBN_Connection *, NBN_Message *, NBN_Channel *);
static NBN_OutgoingMessageProcessResult NBN_Connection_ProcessOutgoingMessage(NBN_Message *, NBN_Packet *, NBN_Connection *);
static void NBN_Connection_InitOutgoingPacket(NBN_Connection *, NBN_Packet *, NBN_PacketEntry **);
static int NBN_Connection_SendPackets(NBN_Connection *, NBN_Packet[], NBN_PacketEntry *[], unsigned int);
static void NBN_Connection_AddOutgoingMessageToSendQueue(NBN_Connection *, NBN_Channel *);
static void NBN_Connection_RemoveMessageFromSendQueue(NBN_Connection *, NBN_Message *, NBN_Channel *);
static NBN_PacketEntry *NBN_Connection_InsertOutgoingPacketEntry(NBN_Connection *, uint16_t);
static bool NBN_Connection_InsertReceivedPacketEntry(NBN_Connection *, uint16_t);
static NBN_PacketEntry *NBN_Connection_FindSendPacketEntry(NBN_Connection *, uint16_t);
static bool NBN_Connection_IsPacketReceived(NBN_Connection *, uint16_t);
static int NBN_Connection_SendPacket(NBN_Connection *, NBN_Packet *);
static NBN_Message *NBN_Connection_FindOutgoingMessage(NBN_Connection *, uint16_t);
static void NBN_Connection_UpdateAveragePing(NBN_Connection *, double);
static void NBN_Connection_UpdateAveragePacketLoss(NBN_Connection *, uint16_t);
static void NBN_Connection_UpdateAverageUploadBandwidth(NBN_Connection *, float);
static void NBN_Connection_UpdateAverageDownloadBandwidth(NBN_Connection *);
static void NBN_Connection_ClearMessageQueue(NBN_List *);

NBN_Connection *NBN_Connection_Create(uint32_t id, uint32_t protocol_id, NBN_Config config)
{
    NBN_Connection *connection = NBN_AllocObject(NBN_OBJ_CONNECTION);

    connection->id = id;
    connection->protocol_id = protocol_id;
    connection->config = config;
    connection->last_recv_packet_time = 0;
    connection->next_packet_seq_number = 1;
    connection->last_received_packet_seq_number = 0;
    connection->last_flush_time = 0;
    connection->last_read_packets_time = 0;
    connection->downloaded_bytes = 0;
    connection->time = 0;
    connection->accepted = false;
    connection->is_stale = false;
    connection->closed = false;
    connection->recv_queue = NBN_List_Create();
    connection->send_queue = NBN_List_Create();

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
        connection->channels[i] = NULL;

    for (int i = 0; i < NBN_MAX_PACKET_ENTRIES; i++)
    {
        connection->packet_send_seq_buffer[i] = 0xFFFFFFFF;
        connection->packet_recv_seq_buffer[i] = 0xFFFFFFFF;
    }
    
    connection->stats = (NBN_ConnectionStats){0};

    return connection;
}

void NBN_Connection_Destroy(NBN_Connection *connection)
{
    NBN_Connection_ClearMessageQueue(connection->recv_queue);
    NBN_Connection_ClearMessageQueue(connection->send_queue);

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        if (connection->channels[i])
            NBN_Dealloc(connection->channels[i]);
    }

    NBN_DeallocObject(NBN_OBJ_CONNECTION, connection);
}

int NBN_Connection_ProcessReceivedPacket(NBN_Connection *connection, NBN_Packet *packet)
{
    NBN_Connection_DecodePacketHeader(connection, packet);
    NBN_Connection_UpdateAveragePacketLoss(connection, packet->header.ack);

    if (!NBN_Connection_InsertReceivedPacketEntry(connection, packet->header.seq_number))
        return 0;

    if (SEQUENCE_NUMBER_GT(packet->header.seq_number, connection->last_received_packet_seq_number))
        connection->last_received_packet_seq_number = packet->header.seq_number;

    for (int i = 0; i < packet->header.messages_count; i++)
    {
        NBN_Message *message = NBN_Packet_ReadNextMessage(packet);

        if (message == NULL)
        {
            NBN_LogError("Failed to read message from packet");

            return -1;
        }

        NBN_Channel *channel = connection->channels[message->header.channel_id];

        if (channel == NULL)
        {
            NBN_LogError("Channel %d does not exist", message->header.channel_id);

            return -1;
        }

        if (NBN_Connection_AddReceivedMessage(connection, message, channel) < 0)
            return -1;
    }

    return 0;
}

static NBN_ListNode *dequeue_current_node = NULL;

NBN_Message *NBN_Connection_DequeueReceivedMessage(NBN_Connection *connection)
{
    if (dequeue_current_node == NULL)
        dequeue_current_node = connection->recv_queue->head;

    while (dequeue_current_node)
    {
        NBN_Message *message = dequeue_current_node->data;
        NBN_Channel *channel = connection->channels[message->header.channel_id];

        assert(channel != NULL);

        dequeue_current_node = dequeue_current_node->next;

        if (channel->CanProcessReceivedMessage(channel, message))
        {
            NBN_List_Remove(connection->recv_queue, message);

            message->sender = connection;

            return message;
        }
    }

    return NULL;
}

void *NBN_Connection_CreateOutgoingMessage(uint8_t msg_type)
{
#if defined(NBN_GAME_CLIENT)
    NBN_MessageBuilder msg_builder = game_client.endpoint.message_builders[msg_type];
#elif defined(NBN_GAME_SERVER)
    NBN_MessageBuilder msg_builder = game_server.endpoint.message_builders[msg_type];
#endif

    if (msg_builder == NULL)
    {
        NBN_LogError("No message builder is registered for messages of type %d", msg_type);

        return NULL;
    }

#if defined(NBN_GAME_CLIENT)
    NBN_MessageSerializer msg_serializer = game_client.endpoint.message_serializers[msg_type];
#elif defined(NBN_GAME_SERVER)
    NBN_MessageSerializer msg_serializer = game_server.endpoint.message_serializers[msg_type];
#endif

    if (msg_serializer == NULL)
    {
        NBN_LogError("No message serializer attached to messages of type %d", msg_type);

        return NULL;
    } 

#if defined(NBN_GAME_CLIENT)
    NBN_MessageDestructor msg_destructor = game_client.endpoint.message_destructors[msg_type];
#elif defined(NBN_GAME_SERVER)
    NBN_MessageDestructor msg_destructor = game_server.endpoint.message_destructors[msg_type];
#endif

    if (msg_destructor == NULL)
    {
        NBN_LogError("No message destructor is registered for messages of type %d", msg_type);

        return NULL;
    }

    void *msg_data = msg_builder();
    outgoing_message = NBN_Message_Create(msg_type, msg_serializer, msg_destructor, msg_data);

    return msg_data;
}

int NBN_Connection_EnqueueOutgoingMessage(NBN_Connection *connection, uint8_t channel_id)
{
    assert(!connection->closed);
    assert(outgoing_message != NULL);

    NBN_Channel *channel = connection->channels[channel_id];
    
    if (channel == NULL)
    {
        NBN_LogError("Channel %d does not exist", channel_id);

        return -1;
    }

    outgoing_message->header.channel_id = channel_id;

    if (outgoing_message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        NBN_Connection_AddOutgoingMessageToSendQueue(connection, channel);
    }
    else
    {
        NBN_MeasureStream measure_stream;

        NBN_MeasureStream_Init(&measure_stream);

        unsigned int message_size = (NBN_Message_Measure(outgoing_message, &measure_stream) - 1) / 8 + 1;

        if (!NBN_Channel_HasRoomForMessage(channel, message_size))
        {
            NBN_LogError("Failed to enqueue outgoing message of type %d on channel %d, the send queue is full",
                         outgoing_message->header.type, outgoing_message->header.channel_id);

            return -1;
        }

        if (message_size > NBN_PACKET_MAX_DATA_SIZE)
        {
            uint8_t *message_bytes = NBN_Alloc(message_size);
            NBN_WriteStream write_stream;

            NBN_WriteStream_Init(&write_stream, message_bytes, message_size);

            if (NBN_Message_SerializeHeader(&outgoing_message->header, (NBN_Stream *)&write_stream) < 0)
                return -1;

            if (outgoing_message->serializer(outgoing_message->data, (NBN_Stream *)&write_stream) < 0)
                return -1;

            NBN_Message_Destroy(outgoing_message, true);

            outgoing_message = NULL;

            unsigned int chunk_count = ((message_size - 1) / NBN_MESSAGE_CHUNK_SIZE) + 1;

            NBN_LogTrace("Split message into %d chunks", chunk_count);

            if (chunk_count > NBN_CHANNEL_CHUNKS_BUFFER_SIZE)
            {
                NBN_LogError("The maximum number of chunks is 255");

                return -1;
            }

            for (unsigned int i = 0; i < chunk_count; i++)
            {
                void *chunk_start = message_bytes + (i * NBN_MESSAGE_CHUNK_SIZE);
                unsigned int chunk_size = MIN(NBN_MESSAGE_CHUNK_SIZE, message_size - (i * NBN_MESSAGE_CHUNK_SIZE));

                NBN_LogTrace("Enqueue chunk %d (size: %d)", i, chunk_size);

                NBN_MessageChunk *chunk = NBN_Connection_CreateOutgoingMessage(NBN_MESSAGE_CHUNK_TYPE);

                assert(chunk != NULL);

                chunk->id = i;
                chunk->total = chunk_count;

                memcpy(chunk->data, chunk_start, chunk_size);

                if (NBN_Connection_EnqueueOutgoingMessage(connection, channel_id) < 0)
                {
                    NBN_Dealloc(message_bytes);

                    return -1;
                }
            }

            NBN_Dealloc(message_bytes);
        }
        else
        {
            NBN_Connection_AddOutgoingMessageToSendQueue(connection, channel);
        }
    }    

    outgoing_message = NULL;

    return 0;
}

int NBN_Connection_FlushSendQueue(NBN_Connection *connection)
{
    NBN_LogTrace("Flushing the send queue (messages in queue: %d)", connection->send_queue->count);

    NBN_Packet outgoing_packets[NBN_MAX_SENT_PACKET_COUNT];
    NBN_PacketEntry *packet_entries[NBN_MAX_SENT_PACKET_COUNT];
    unsigned int packet_count = 1;

    NBN_Connection_InitOutgoingPacket(connection, &outgoing_packets[0], &packet_entries[0]);

    if (connection->send_queue->count > 0)
    {
        bool new_packet = false;
        NBN_ListNode *current_node = connection->send_queue->head;

        while (true)
        {
            NBN_Message *message = current_node->data;
            NBN_Packet *outgoing_packet = &outgoing_packets[packet_count - 1];

            if (new_packet)
            {
                NBN_Connection_InitOutgoingPacket(connection, outgoing_packet, &packet_entries[packet_count - 1]);

                new_packet = false;
            }

            NBN_ListNode *prev_node = current_node;
            current_node = current_node->next;

            NBN_OutgoingMessageProcessResult ret = NBN_Connection_ProcessOutgoingMessage(message, outgoing_packet, connection);

            if (ret == NBN_MESSAGE_ERROR)
            {
                return NBN_ERROR;
            }
            else if (ret == NBN_MESSAGE_ADDED_TO_PACKET)
            {
                NBN_PacketEntry *packet_entry = packet_entries[packet_count - 1];

                message->last_send_time = connection->time;
                packet_entry->message_ids[packet_entry->messages_count++] = message->header.id;
            }
            else if (ret == NBN_PACKET_FULL)
            {
                current_node = prev_node;
                new_packet = true;

                if (packet_count + 1 > NBN_MAX_SENT_PACKET_COUNT)
                    break;

                packet_count++;
            }

            if (current_node == NULL)
                break;
        }
    }

    if (NBN_Connection_SendPackets(connection, outgoing_packets, packet_entries, packet_count) < 0)
        return NBN_ERROR;

    connection->last_flush_time = connection->time;

    return 0;
}

int NBN_Connection_CreateChannel(NBN_Connection *connection, NBN_ChannelType type, uint8_t id)
{
    if (id > NBN_MAX_CHANNELS - 1)
        return -1;

    NBN_Channel *channel;

    switch (type)
    {
    case NBN_CHANNEL_UNRELIABLE_ORDERED:
        channel = (NBN_Channel *)NBN_UnreliableOrderedChannel_Create(id);
        break;

    case NBN_CHANNEL_RELIABLE_ORDERED:
        channel = (NBN_Channel *)NBN_ReliableOrderedChannel_Create(id);
        break;
    
    default:
        NBN_LogError("Unsupported message channel type: %d", type);

        return -1;
    }

    channel->chunk_count = 0;
    channel->last_received_chunk_id = -1;

    for (int i = 0; i < NBN_CHANNEL_CHUNKS_BUFFER_SIZE; i++)
        channel->chunks_buffer[i] = NULL;

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

static void NBN_Connection_DecodePacketHeader(NBN_Connection *connection, NBN_Packet *packet)
{
    NBN_Connection_AckPacket(connection, packet->header.ack);

    for (int i = 0; i < 32; i++)
    {
        if (B_IS_UNSET(packet->header.ack_bits, i))
            continue;

        NBN_Connection_AckPacket(connection, packet->header.ack - (i + 1));
    }
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

static void NBN_Connection_AckPacket(NBN_Connection *connection, uint16_t ack_packet_seq_number)
{
    NBN_PacketEntry *entry = NBN_Connection_FindSendPacketEntry(connection, ack_packet_seq_number);

    if (entry && !entry->acked)
    {
        NBN_LogTrace("Ack packet: %d", ack_packet_seq_number);

        entry->acked = true;

        NBN_Connection_UpdateAveragePing(connection, connection->time - entry->send_time);

        for (int i = 0; i < entry->messages_count; i++)
        {
            NBN_Message *message = NBN_Connection_FindOutgoingMessage(connection, entry->message_ids[i]);

            if (message && !message->acked)
            {
                NBN_Channel *channel = connection->channels[message->header.channel_id];

                assert(channel != NULL);

                if (channel->OnMessageAcked)
                    channel->OnMessageAcked(channel, message);

                message->acked = true;

                NBN_LogTrace("Message acked: %d", message->header.id);
            }
        }
    }
}

static int NBN_Connection_AddReceivedMessage(NBN_Connection *connection, NBN_Message *message, NBN_Channel *channel)
{
    if (channel->AddReceivedMessage(channel, message))
    {
        NBN_LogTrace("Received message %d on channel %d : added to recv queue", message->header.id, channel->id);

        NBN_List_PushBack(connection->recv_queue, message);

#ifdef NBN_DEBUG
        if (connection->OnMessageAddedToRecvQueue)
            connection->OnMessageAddedToRecvQueue(connection, message);
#endif
    }
    else
    {
        NBN_LogTrace("Received message %d : discarded", message->header.id);

        NBN_Message_Destroy(message, true);
    }

    return 0;
}

static NBN_OutgoingMessageProcessResult NBN_Connection_ProcessOutgoingMessage(NBN_Message *message, NBN_Packet *outgoing_packet, NBN_Connection *connection)
{
    NBN_Channel *channel = connection->channels[message->header.channel_id];

    assert(channel != NULL);

    NBN_OutgoingMessagePolicy policy = channel->GetOutgoingMessagePolicy(channel, message);

    if (policy == NBN_SKIP_MESSAGE)
    {
        return NBN_MESSAGE_NOT_ADDED_TO_PACKET;
    }
    else if (policy == NBN_DISCARD_MESSAGE)
    {
        NBN_Connection_RemoveMessageFromSendQueue(connection, message, channel);

        return NBN_MESSAGE_NOT_ADDED_TO_PACKET;
    }

    int ret = NBN_Packet_WriteMessage(outgoing_packet, message);

    if (ret < 0)
    {
        NBN_LogError("Failed to write message to packet");

        return NBN_MESSAGE_ERROR;
    }

    if (ret == NBN_PACKET_WRITE_OK)
    {
        NBN_LogTrace("Message %d added to packet", message->header.id);

        if (policy == NBN_SEND_ONCE)
            NBN_Connection_RemoveMessageFromSendQueue(connection, message, channel);

        return NBN_MESSAGE_ADDED_TO_PACKET;
    }

    return NBN_PACKET_FULL;
}

static void NBN_Connection_InitOutgoingPacket(NBN_Connection *connection, NBN_Packet *outgoing_packet, NBN_PacketEntry **packet_entry)
{
    NBN_Packet_InitWrite(
        outgoing_packet,
        connection->protocol_id,
        connection->next_packet_seq_number++,
        connection->last_received_packet_seq_number,
        NBN_Connection_BuildPacketAckBits(connection));

    *packet_entry = NBN_Connection_InsertOutgoingPacketEntry(connection, outgoing_packet->header.seq_number);
}

static int NBN_Connection_SendPackets(
    NBN_Connection *connection, NBN_Packet outgoing_packets[], NBN_PacketEntry *packet_entries[], unsigned int packet_count)
{
    unsigned int sent_bytes = 0;

    for (unsigned int i = 0; i < packet_count; i++)
    {
        NBN_Packet *outgoing_packet = &outgoing_packets[i];
        NBN_PacketEntry *packet_entry = packet_entries[i];

        assert(packet_entry->messages_count == outgoing_packet->header.messages_count);

        if (NBN_Packet_Seal(outgoing_packet) < 0)
        {
            NBN_LogError("Failed to seal packet");

            return NBN_ERROR;
        }

        if (NBN_Connection_SendPacket(connection, outgoing_packet) < 0)
        {
            NBN_LogError("Failed to send packet");

            return NBN_ERROR;
        }

        packet_entry->send_time = connection->time;
        sent_bytes += outgoing_packet->size;
    }

    NBN_LogTrace("Sent %d packet(s) (%d bytes)", packet_count, sent_bytes);

    double t = connection->time - connection->last_flush_time;

    if (t > 0)
        NBN_Connection_UpdateAverageUploadBandwidth(connection, sent_bytes / t);

    return 0;
}

static void NBN_Connection_AddOutgoingMessageToSendQueue(NBN_Connection *connection, NBN_Channel *channel)
{
    assert(channel->outgoing_message_count + 1 <= NBN_CHANNEL_BUFFER_SIZE);

    NBN_LogTrace("Enqueue message of type %d for channel %d", outgoing_message->header.type, channel->id);
    NBN_List_PushBack(connection->send_queue, outgoing_message);

    channel->outgoing_message_count++;
}

static void NBN_Connection_RemoveMessageFromSendQueue(NBN_Connection *connection, NBN_Message *message, NBN_Channel *channel)
{
    assert(channel->outgoing_message_count > 0);

    channel->outgoing_message_count--;

    NBN_Message_Destroy(NBN_List_Remove(connection->send_queue, message), true);
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

static int NBN_Connection_SendPacket(NBN_Connection *connection, NBN_Packet *packet)
{
    NBN_LogTrace("Send packet %d (messages count: %d)", packet->header.seq_number, packet->header.messages_count);

#ifdef NBN_GAME_SERVER

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_EnqueuePacket(game_server.endpoint.packet_simulator, packet, connection->id);

    return 0;
#else
    return NBN_Driver_GServ_SendPacketTo(packet, connection->id);
#endif

#endif /* NBN_GAME_SERVER */

#ifdef NBN_GAME_CLIENT

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_EnqueuePacket(game_client.endpoint.packet_simulator, packet, connection->id);

    return 0;
#else
    (void)connection;

    return NBN_Driver_GCli_SendPacket(packet);
#endif

#endif /* NBN_GAME_CLIENT */

    return -1;
}

static NBN_Message *NBN_Connection_FindOutgoingMessage(NBN_Connection *connection, uint16_t id)
{
    NBN_ListNode *current_node = connection->send_queue->head;

    while (current_node)
    {
        NBN_Message *message = current_node->data;

        if (message->sent && message->header.id == id)
            return message;

        current_node = current_node->next;
    }

    return NULL;
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

static void NBN_Connection_ClearMessageQueue(NBN_List *queue)
{
    NBN_ListNode *current_node = queue->head;

    while (current_node != NULL)
    {
        NBN_ListNode *next_node = current_node->next;
        
        NBN_Message_Destroy(current_node->data, true);
        NBN_Dealloc(current_node);

        current_node = next_node;
    }
}

#pragma endregion /* NBN_Connection */

#pragma region NBN_Channel

void NBN_Channel_AddTime(NBN_Channel *channel, double time)
{
    channel->time += time;
}

bool NBN_Channel_AddChunk(NBN_Channel *channel, NBN_Message *chunk_msg)
{
    assert(chunk_msg->header.type == NBN_MESSAGE_CHUNK_TYPE);

    NBN_MessageChunk *chunk = chunk_msg->data;

    if (chunk->id == channel->last_received_chunk_id + 1)
    {
        assert(channel->chunks_buffer[chunk->id] == NULL);

        channel->chunks_buffer[chunk->id] = chunk;
        channel->last_received_chunk_id++;

        NBN_Message_Destroy(chunk_msg, false);

        if (++channel->chunk_count == chunk->total)
        {
            channel->last_received_chunk_id = -1;

            return true;
        }

        return false;
    }

    /* Clear the chunks buffer */
    for (unsigned int i = 0; i < channel->chunk_count; i++)
    {
        assert(channel->chunks_buffer[i] != NULL);

        NBN_MessageChunk_Destroy(channel->chunks_buffer[i]);

        channel->chunks_buffer[i] = NULL;
    }

    channel->chunk_count = 0;
    channel->last_received_chunk_id = -1;

    if (chunk->id == 0)
        return NBN_Channel_AddChunk(channel, chunk_msg);

    NBN_Message_Destroy(chunk_msg, true);

    return false;
}

NBN_Message *NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *channel, NBN_Connection *connection)
{
    unsigned int size = channel->chunk_count * NBN_MESSAGE_CHUNK_SIZE;
    uint8_t *data = NBN_Alloc(size);

    for (unsigned int i = 0; i < channel->chunk_count; i++)
    {
        NBN_MessageChunk *chunk = channel->chunks_buffer[i];

        memcpy(data + i * NBN_MESSAGE_CHUNK_SIZE, chunk->data, NBN_MESSAGE_CHUNK_SIZE);

        NBN_MessageChunk_Destroy(chunk);

        channel->chunks_buffer[i] = NULL;
    }

#ifdef NBN_DEBUG
    mem_manager.report.destroyed_message_count += channel->chunk_count;
#endif

    channel->chunk_count = 0;

    NBN_ReadStream r_stream;

    NBN_ReadStream_Init(&r_stream, data, size);

    NBN_Message *message = NBN_Message_ReadFromStream(&r_stream);

    assert(message != NULL);

    message->sender = connection;

    NBN_Dealloc(data);

    return message;
}

bool NBN_Channel_HasRoomForMessage(NBN_Channel *channel, unsigned int message_size)
{
    unsigned int chunk_count = ((message_size - 1) / NBN_MESSAGE_CHUNK_SIZE) + 1;

    return channel->outgoing_message_count + chunk_count <= NBN_CHANNEL_BUFFER_SIZE;
}

/* Unreliable ordered */

static NBN_OutgoingMessagePolicy NBN_UnreliableOrderedChannel_GetOutgoingMessagePolicy(NBN_Channel *, NBN_Message *);
static bool NBN_UnreliableOrderedChannel_AddReceivedMessage(NBN_Channel *, NBN_Message *);
static bool NBN_UnreliableOrderedChannel_CanProcessReceivedMessage(NBN_Channel *, NBN_Message *);

NBN_UnreliableOrderedChannel *NBN_UnreliableOrderedChannel_Create(uint8_t id)
{
    NBN_UnreliableOrderedChannel *channel = NBN_Alloc(sizeof(NBN_UnreliableOrderedChannel));

    channel->base.id = id;
    channel->base.GetOutgoingMessagePolicy = NBN_UnreliableOrderedChannel_GetOutgoingMessagePolicy;
    channel->base.AddReceivedMessage = NBN_UnreliableOrderedChannel_AddReceivedMessage;
    channel->base.CanProcessReceivedMessage = NBN_UnreliableOrderedChannel_CanProcessReceivedMessage;
    channel->base.next_message_id = 0;
    channel->base.outgoing_message_count = 0;

    channel->last_received_message_id = 0;

    return channel;
}

static NBN_OutgoingMessagePolicy NBN_UnreliableOrderedChannel_GetOutgoingMessagePolicy(NBN_Channel *channel, NBN_Message *message)
{
    message->header.id = channel->next_message_id++;

    return NBN_SEND_ONCE;
}

static bool NBN_UnreliableOrderedChannel_AddReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    NBN_UnreliableOrderedChannel *unreliable_ordered_channel = (NBN_UnreliableOrderedChannel *)channel;

    if (SEQUENCE_NUMBER_GT(message->header.id, unreliable_ordered_channel->last_received_message_id))
    {
        unreliable_ordered_channel->last_received_message_id = message->header.id;

        return true;
    }

    return false;
}

static bool NBN_UnreliableOrderedChannel_CanProcessReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    (void)channel;
    (void)message;

    return true;
}

/* Reliable ordered */

static NBN_OutgoingMessagePolicy NBN_ReliableOrderedChannel_GetOugoingMessagePolicy(NBN_Channel *, NBN_Message *);
static bool NBN_ReliableOrderedChannel_AddReceivedMessage(NBN_Channel *, NBN_Message *);
static bool NBN_ReliableOrderedChannel_CanProcessReceivedMessage(NBN_Channel *, NBN_Message *);
static void NBN_ReliableOrderedChannel_OnMessageAcked(NBN_Channel *, NBN_Message *);

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(uint8_t id)
{
    NBN_ReliableOrderedChannel *channel = NBN_Alloc(sizeof(NBN_ReliableOrderedChannel));

    channel->base.id = id;
    channel->base.GetOutgoingMessagePolicy = NBN_ReliableOrderedChannel_GetOugoingMessagePolicy;
    channel->base.AddReceivedMessage = NBN_ReliableOrderedChannel_AddReceivedMessage;
    channel->base.CanProcessReceivedMessage = NBN_ReliableOrderedChannel_CanProcessReceivedMessage;
    channel->base.OnMessageAcked = NBN_ReliableOrderedChannel_OnMessageAcked;
    channel->base.next_message_id = 0;
    channel->base.outgoing_message_count = 0;

    channel->last_received_message_id = 0;
    channel->oldest_unacked_message_id = 0;
    channel->most_recent_message_id = 0;

    for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
    {
        channel->recv_message_buffer[i] = 0xFFFFFFFF;
        channel->ack_buffer[i] = false;
    }

    return channel;
}

static NBN_OutgoingMessagePolicy NBN_ReliableOrderedChannel_GetOugoingMessagePolicy(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;

    if (message->acked)
        return NBN_DISCARD_MESSAGE;

    if (message->last_send_time >= 0 && channel->time - message->last_send_time < NBN_MESSAGE_RESEND_DELAY)
        return NBN_SKIP_MESSAGE;

    if (!message->sent)
    {
        int max_message_id =
            (reliable_ordered_channel->oldest_unacked_message_id + (NBN_CHANNEL_BUFFER_SIZE - 1)) % (0xFFFF + 1);

        if (SEQUENCE_NUMBER_GTE(channel->next_message_id, max_message_id))
            return NBN_SKIP_MESSAGE;

        message->header.id = channel->next_message_id++;
        message->sent = true;
    }

    return NBN_SEND_REPEAT;
}

static unsigned int compute_message_id_delta(uint16_t id1, uint16_t id2)
{
    if (SEQUENCE_NUMBER_GT(id1, id2))
        return (id1 >= id2) ? id1 - id2 : ((0xFFFF + 1) - id2) + id1;
    else
        return (id2 >= id1) ? id2 - id1 : (((0xFFFF + 1) - id1) + id2) % 0xFFFF;
}

static bool NBN_ReliableOrderedChannel_AddReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    unsigned int dt = compute_message_id_delta(message->header.id,
            reliable_ordered_channel->most_recent_message_id);

    NBN_LogDebug("Add message to channel (id: %d, most recent msg id: %d, dt: %d)",
            message->header.id, reliable_ordered_channel->most_recent_message_id, dt);

    if (SEQUENCE_NUMBER_GT(message->header.id, reliable_ordered_channel->most_recent_message_id))
    {
#ifdef NBN_DEBUG 
        assert(dt < NBN_CHANNEL_BUFFER_SIZE);
#endif

        reliable_ordered_channel->most_recent_message_id = message->header.id;
    }
    else
    {
        /*
            This is an old message that has already been received, probably coming from
            an out of order late packet.
        */
        if (dt >= NBN_CHANNEL_BUFFER_SIZE)
            return false;
    }

    uint16_t index = message->header.id % NBN_CHANNEL_BUFFER_SIZE;

    if (reliable_ordered_channel->recv_message_buffer[index] != message->header.id)
    {
        reliable_ordered_channel->recv_message_buffer[index] = message->header.id;

        return true;
    }

    return false;
}

static bool NBN_ReliableOrderedChannel_CanProcessReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel; 

    if (message->header.id == reliable_ordered_channel->last_received_message_id)
    {
        reliable_ordered_channel->last_received_message_id++;

        return true;
    }

    return false;
}

static void NBN_ReliableOrderedChannel_OnMessageAcked(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    uint16_t msg_id = message->header.id;

    reliable_ordered_channel->ack_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE] = true;

    if (msg_id == reliable_ordered_channel->oldest_unacked_message_id)
    {
        for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
        {
            int index = (msg_id + i) % NBN_CHANNEL_BUFFER_SIZE;

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
    }
}

#pragma endregion /* NBN_MessageChannel */

#pragma region NBN_EventQueue

NBN_EventQueue *NBN_EventQueue_Create(void)
{
    NBN_EventQueue *events_queue = NBN_Alloc(sizeof(NBN_EventQueue));

    events_queue->queue = NBN_List_Create();
    events_queue->last_event_data = NULL;

    return events_queue;
}

void NBN_EventQueue_Destroy(NBN_EventQueue *events_queue)
{
    NBN_List_Destroy(events_queue->queue, true, NULL);
    NBN_Dealloc(events_queue);
}

void NBN_EventQueue_Enqueue(NBN_EventQueue *events_queue, int type, void *data)
{
    NBN_Event *ev = NBN_AllocObject(NBN_OBJ_EVENT);

    ev->type = type;
    ev->data = data;

    NBN_List_PushBack(events_queue->queue, ev);
}

int NBN_EventQueue_Dequeue(NBN_EventQueue *events_queue)
{
    if (events_queue->queue->count == 0)
    {
        return NBN_NO_EVENT;
    }

    NBN_Event *ev = NBN_List_RemoveAt(events_queue->queue, 0);
    int ev_type = ev->type;

    events_queue->last_event_data = ev->data;

    NBN_DeallocObject(NBN_OBJ_EVENT, ev);

    return ev_type;
}

bool NBN_EventQueue_IsEmpty(NBN_EventQueue *events_queue)
{
    return events_queue->queue->count == 0;
}

#pragma endregion /* NBN_EventQueue */

#pragma region NBN_Endpoint

static uint32_t NBN_Endpoint_BuildProtocolId(const char *);
static int NBN_Endpoint_ProcessReceivedPacket(NBN_Endpoint *, NBN_Packet *, NBN_Connection *);

void NBN_Endpoint_Init(NBN_Endpoint *endpoint, NBN_Config config)
{
    endpoint->config = config;

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
        endpoint->channels[i] = NBN_CHANNEL_UNDEFINED;

    for (int i = 0; i < NBN_MAX_MESSAGE_TYPES; i++)
        endpoint->message_destructors[i] = NULL;

    for (int i = 0; i < NBN_MAX_MESSAGE_TYPES; i++)
        endpoint->message_serializers[i] = NULL;

    endpoint->events_queue = NBN_EventQueue_Create();

    /* Register library reserved channels */
    NBN_Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_UNRELIABLE_ORDERED, NBN_RESERVED_UNRELIABLE_CHANNEL);
    NBN_Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_RELIABLE_ORDERED, NBN_RESERVED_RELIABLE_CHANNEL);

    /* Register NBN_MessageChunk library message */
    NBN_Endpoint_RegisterMessageBuilder(
        endpoint, (NBN_MessageBuilder)NBN_MessageChunk_Create, NBN_MESSAGE_CHUNK_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
        endpoint, (NBN_MessageDestructor)NBN_MessageChunk_Destroy, NBN_MESSAGE_CHUNK_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
        endpoint, (NBN_MessageSerializer)NBN_MessageChunk_Serialize, NBN_MESSAGE_CHUNK_TYPE);

    /* Register NBN_ClientClosedMessage library message */
    NBN_Endpoint_RegisterMessageBuilder(
        endpoint, (NBN_MessageBuilder)NBN_ClientClosedMessage_Create, NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
        endpoint, (NBN_MessageDestructor)NBN_ClientClosedMessage_Destroy, NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
        endpoint, (NBN_MessageSerializer)NBN_ClientClosedMessage_Serialize, NBN_CLIENT_CLOSED_MESSAGE_TYPE);

#ifdef NBN_DEBUG
    endpoint->OnMessageAddedToRecvQueue = NULL;
#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    endpoint->packet_simulator = NBN_PacketSimulator_Create();

    NBN_PacketSimulator_Start(endpoint->packet_simulator);
#endif
}

void NBN_Endpoint_Deinit(NBN_Endpoint *endpoint)
{
    NBN_EventQueue_Destroy(endpoint->events_queue);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_Stop(endpoint->packet_simulator);
    NBN_PacketSimulator_Destroy(endpoint->packet_simulator);
#endif
}

void NBN_Endpoint_RegisterMessageBuilder(NBN_Endpoint *endpoint, NBN_MessageBuilder msg_builder, uint8_t msg_type)
{
    endpoint->message_builders[msg_type] = msg_builder;
}

void NBN_Endpoint_RegisterMessageDestructor(NBN_Endpoint *endpoint, NBN_MessageDestructor msg_destructor, uint8_t msg_type)
{
    endpoint->message_destructors[msg_type] = msg_destructor;
}

void NBN_Endpoint_RegisterMessageSerializer(NBN_Endpoint *endpoint, NBN_MessageSerializer msg_serializer, uint8_t msg_type)
{
    endpoint->message_serializers[msg_type] = msg_serializer;
}

void NBN_Endpoint_RegisterChannel(NBN_Endpoint *endpoint, NBN_ChannelType type, uint8_t id)
{
    assert(endpoint->channels[id] == NBN_CHANNEL_UNDEFINED);

    endpoint->channels[id] = type;
}

NBN_Connection *NBN_Endpoint_CreateConnection(NBN_Endpoint *endpoint, int id)
{
    NBN_Connection *connection = NBN_Connection_Create(id, NBN_Endpoint_BuildProtocolId(endpoint->config.protocol_name), endpoint->config);

    for (int chan_id = 0; chan_id < NBN_MAX_CHANNELS; chan_id++)
    {
        NBN_ChannelType channel_type = endpoint->channels[chan_id];

        if (channel_type != NBN_CHANNEL_UNDEFINED)
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
        return -1;

    connection->last_recv_packet_time = connection->time;
    connection->downloaded_bytes += packet->size;

    return 0;
}

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

#ifdef NBN_GAME_CLIENT

NBN_GameClient game_client;

static int last_event_type = NBN_NO_EVENT;
static uint8_t last_received_message_type;
static void *last_received_message_data = NULL;
static int last_client_closed_code = -1;

static void NBN_GameClient_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static int NBN_GameClient_HandleEvent(int);
static int NBN_GameClient_HandleMessageReceivedEvent(void);
static void NBN_GameClient_ReleaseLastEvent(void);
static void NBN_GameClient_ReleaseMessageReceivedEvent(void);

void NBN_GameClient_Init(const char *protocol_name, const char *ip_address, uint16_t port)
{
    NBN_Endpoint_Init(&game_client.endpoint, (NBN_Config){.protocol_name = protocol_name, .ip_address = ip_address, .port = port});
}

int NBN_GameClient_Start(void)
{
    NBN_Config config = game_client.endpoint.config;

    if (NBN_Driver_GCli_Start(NBN_Endpoint_BuildProtocolId(config.protocol_name), config.ip_address, config.port) < 0)
        return -1;

    NBN_LogInfo("Started");

    return 0;
}

void NBN_GameClient_Stop(void)
{
    NBN_Driver_GCli_Stop();
    NBN_GameClient_Poll(); /* Poll one last time to clear remaining events */
    NBN_Endpoint_Deinit(&game_client.endpoint);
    NBN_Connection_Destroy(game_client.server_connection);

    NBN_LogInfo("Stopped");
}

void NBN_GameClient_AddTime(double time)
{
    NBN_Connection_AddTime(game_client.server_connection, time);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_AddTime(game_client.endpoint.packet_simulator, time);
#endif
}

NBN_GameClientEvent NBN_GameClient_Poll(void)
{
    if (game_client.server_connection->is_stale)
        return NBN_NO_EVENT;

    NBN_GameClient_ReleaseLastEvent();

    if (NBN_EventQueue_IsEmpty(game_client.endpoint.events_queue)) 
    {
        if (NBN_Connection_CheckIfStale(game_client.server_connection))
        {
            game_client.server_connection->is_stale = true;

            NBN_LogInfo("Server connection is stale. Disconnected.");
            NBN_EventQueue_Enqueue(game_client.endpoint.events_queue, NBN_DISCONNECTED, NULL);
        }
        else
        {
            if (NBN_Driver_GCli_RecvPackets() < 0)
                return NBN_ERROR;

            NBN_Message *msg;

            assert(dequeue_current_node == NULL);

            while ((msg = NBN_Connection_DequeueReceivedMessage(game_client.server_connection)))
                NBN_GameClient_ProcessReceivedMessage(msg, game_client.server_connection);
            
            NBN_Connection_UpdateAverageDownloadBandwidth(game_client.server_connection);

            game_client.server_connection->last_read_packets_time = game_client.server_connection->time;
        }
    }

    int ev_type = NBN_EventQueue_Dequeue(game_client.endpoint.events_queue);
    last_event_type = ev_type;

    return NBN_GameClient_HandleEvent(ev_type);
}

int NBN_GameClient_SendPackets(void)
{
    return NBN_Connection_FlushSendQueue(game_client.server_connection);
}

void *NBN_GameClient_CreateMessage(uint8_t type)
{
    return NBN_Connection_CreateOutgoingMessage(type);
}

int NBN_GameClient_EnqueueMessage(uint8_t channel_id)
{
    return NBN_Connection_EnqueueOutgoingMessage(game_client.server_connection, channel_id);
}

int NBN_GameClient_EnqueueUnreliableMessage(void)
{
    return NBN_GameClient_EnqueueMessage(NBN_RESERVED_UNRELIABLE_CHANNEL);
}

int NBN_GameClient_EnqueueReliableMessage(void)
{
    return NBN_GameClient_EnqueueMessage(NBN_RESERVED_RELIABLE_CHANNEL);
}

bool NBN_GameClient_CanEnqueueMessage(uint8_t channel_id)
{
    NBN_Channel *channel = game_client.server_connection->channels[channel_id];

    assert(channel != NULL);

    NBN_MeasureStream measure_stream;

    NBN_MeasureStream_Init(&measure_stream);

    unsigned int message_size = (NBN_Message_Measure(outgoing_message, &measure_stream) - 1) / 8 + 1;

    return NBN_Channel_HasRoomForMessage(channel, message_size);
}

NBN_Connection *NBN_GameClient_CreateServerConnection(void)
{
    NBN_Connection *server_connection = NBN_Endpoint_CreateConnection(&game_client.endpoint, 0);

#ifdef NBN_DEBUG
    server_connection->OnMessageAddedToRecvQueue = game_client.endpoint.OnMessageAddedToRecvQueue;
#endif

    game_client.server_connection = server_connection;

    return server_connection;
}

NBN_MessageInfo NBN_GameClient_GetReceivedMessageInfo(void)
{
    assert(last_event_type == NBN_MESSAGE_RECEIVED);
    assert(last_received_message_data != NULL);

    return (NBN_MessageInfo){
        .type = last_received_message_type,
        .data = last_received_message_data,
        .sender = NULL};
}

NBN_ConnectionStats NBN_GameClient_GetStats(void)
{
    return game_client.server_connection->stats;
}

NBN_Message *NBN_GameClient_GetOutgoingMessage(void)
{
    return outgoing_message;
}

int NBN_GameClient_GetClientClosedCode(void)
{
    return last_client_closed_code;
}

#ifdef NBN_DEBUG

void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
    case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
        game_client.endpoint.OnMessageAddedToRecvQueue = cb;
        break;
    }
}

#endif /* NBN_DEBUG */

static void NBN_GameClient_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *server_connection)
{
    assert(game_client.server_connection == server_connection);

    if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        NBN_Channel *channel = server_connection->channels[message->header.channel_id];

        if (NBN_Channel_AddChunk(channel, message))
            NBN_EventQueue_Enqueue(
                    game_client.endpoint.events_queue,
                    NBN_MESSAGE_RECEIVED,
                    NBN_Channel_ReconstructMessageFromChunks(channel, server_connection)
                    );
    }
    else
    {
        NBN_EventQueue_Enqueue(game_client.endpoint.events_queue, NBN_MESSAGE_RECEIVED, message);
    }
}

static int NBN_GameClient_HandleEvent(int event_type)
{
    switch (event_type)
    {
    case NBN_MESSAGE_RECEIVED:
        return NBN_GameClient_HandleMessageReceivedEvent();

    default:
        return event_type;
    }
}

static int NBN_GameClient_HandleMessageReceivedEvent(void)
{
    NBN_Message *message = game_client.endpoint.events_queue->last_event_data;

    last_received_message_type = message->header.type;
    last_received_message_data = message->data;

    NBN_Message_Destroy(message, false);

    if (message->header.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE)
    {
        last_client_closed_code = ((NBN_ClientClosedMessage *)message->data)->code;

        return NBN_DISCONNECTED;
    }
    else
    {
        return NBN_MESSAGE_RECEIVED;
    }
}

static void NBN_GameClient_ReleaseLastEvent(void)
{
    switch (last_event_type)
    {
    case NBN_MESSAGE_RECEIVED:
        NBN_GameClient_ReleaseMessageReceivedEvent();
        break;

    default:
        break;
    }

    last_received_message_data = NULL;
}

static void NBN_GameClient_ReleaseMessageReceivedEvent(void)
{
    assert(last_received_message_data != NULL);

    game_client.endpoint.message_destructors[last_received_message_type](last_received_message_data);

#ifdef NBN_DEBUG
    mem_manager.report.destroyed_message_count++;
#endif
}

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* NBN_GameClient */

#pragma region Game client driver

#ifdef NBN_GAME_CLIENT

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
    NBN_EventQueue_Enqueue(game_client.endpoint.events_queue, NBN_CONNECTED, NULL);
}

static void NBN_Driver_GCli_OnPacketReceived(NBN_Packet *packet)
{
    int ret = NBN_Endpoint_ProcessReceivedPacket(&game_client.endpoint, packet, game_client.server_connection);

    /* packets from server should always be valid */
    assert(ret == 0);
}

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* Game Client driver */

#pragma region NBN_GameServer

#ifdef NBN_GAME_SERVER

NBN_GameServer game_server;

static int last_event_type = NBN_NO_EVENT;
static NBN_Connection *last_event_client = NULL;
static void *last_received_message_data = NULL;
static uint8_t last_received_message_type;

static void NBN_GameServer_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static void NBN_GameServer_CloseStaleClientConnections(void);
static void NBN_GameServer_RemoveClosedClientConnections(void);
static void NBN_GameServer_HandleEvent(int);
static void NBN_GameServer_HandleClientConnectionEvent(void);
static void NBN_GameServer_HandleClientDisconnectedEvent(void);
static void NBN_GameServer_HandleMessageReceivedEvent(void);
static void NBN_GameServer_ReleaseLastEvent(void);
static void NBN_GameServer_ReleaseMessageReceivedEvent(void);
static void NBN_GameServer_ReleaseClientDisconnectedEvent(void);

void NBN_GameServer_Init(const char *protocol_name, uint16_t port)
{
    NBN_Endpoint_Init(&game_server.endpoint, (NBN_Config){.protocol_name = protocol_name, .port = port});

    game_server.clients = NBN_List_Create();
}

int NBN_GameServer_Start(void)
{
    NBN_Config config = game_server.endpoint.config;

    if (NBN_Driver_GServ_Start(NBN_Endpoint_BuildProtocolId(config.protocol_name), config.port) < 0)
    {
        NBN_LogError("Failed to start network driver");

        return -1;
    }

    NBN_LogInfo("Started");

    return 0;
}

void NBN_GameServer_Stop(void)
{
    NBN_Driver_GServ_Stop();
    NBN_GameServer_Poll(); /* Poll one last time to clear remaining events */
    NBN_Endpoint_Deinit(&game_server.endpoint);
    NBN_List_Destroy(game_server.clients, true, (NBN_List_FreeItemFunc)NBN_Connection_Destroy);

    NBN_LogInfo("Stopped");
}

void NBN_GameServer_AddTime(double time)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection_AddTime(current_node->data, time);

        current_node = current_node->next;
    }

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_AddTime(game_server.endpoint.packet_simulator, time);
#endif
}

NBN_GameServerEvent NBN_GameServer_Poll(void)
{
    NBN_GameServer_ReleaseLastEvent();

    if (NBN_EventQueue_IsEmpty(game_server.endpoint.events_queue)) 
    {
        NBN_GameServer_CloseStaleClientConnections();

        if (NBN_Driver_GServ_RecvPackets() < 0)
            return NBN_ERROR;

        NBN_ListNode *current_node = game_server.clients->head;

        game_server.stats.download_bandwidth = 0;

        while (current_node)
        {
            NBN_Connection *client = current_node->data;
            NBN_Message *msg;

            assert(dequeue_current_node == NULL);

            while ((msg = NBN_Connection_DequeueReceivedMessage(client)))
                NBN_GameServer_ProcessReceivedMessage(msg, client);

            if (!client->closed)
                NBN_Connection_UpdateAverageDownloadBandwidth(client);

            game_server.stats.download_bandwidth += client->stats.download_bandwidth;
            client->last_read_packets_time = client->time;
            current_node = current_node->next;
        }

        NBN_GameServer_RemoveClosedClientConnections();
    }

    int ev_type = NBN_EventQueue_Dequeue(game_server.endpoint.events_queue);
    last_event_type = ev_type;

    NBN_GameServer_HandleEvent(ev_type);

    return ev_type;
}

int NBN_GameServer_SendPackets(void)
{
    NBN_ListNode *current_node = game_server.clients->head;

    game_server.stats.upload_bandwidth = 0;

    while (current_node)
    {
        NBN_Connection *client = current_node->data;

        if (!client->is_stale && NBN_Connection_FlushSendQueue(client) < 0)
            return -1;

        game_server.stats.upload_bandwidth += client->stats.upload_bandwidth;
        current_node = current_node->next;
    }

    return 0;
}

NBN_Connection *NBN_GameServer_CreateClientConnection(uint32_t id)
{
    NBN_Connection *client = NBN_Endpoint_CreateConnection(&game_server.endpoint, id);

#ifdef NBN_DEBUG
    client->OnMessageAddedToRecvQueue = game_server.endpoint.OnMessageAddedToRecvQueue;
#endif

    return client;
}

void NBN_GameServer_CloseClient(NBN_Connection *client, int code)
{
    assert(NBN_List_Includes(game_server.clients, client));

    if (!client->closed && client->accepted)
        NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_CLIENT_DISCONNECTED, client);

    if (client->is_stale)
    {
        client->closed = true;

        return;
    }

    NBN_Connection_ClearMessageQueue(client->send_queue);

    if (code != -1)
    {
        NBN_ClientClosedMessage *msg = NBN_GameServer_CreateMessage(NBN_CLIENT_CLOSED_MESSAGE_TYPE);

        msg->code = code;

        NBN_GameServer_EnqueueMessageFor(client, NBN_RESERVED_RELIABLE_CHANNEL);
    }

    client->closed = true;
}

void *NBN_GameServer_CreateMessage(uint8_t type)
{
    return NBN_Connection_CreateOutgoingMessage(type);
}

int NBN_GameServer_EnqueueMessageFor(NBN_Connection *client, uint8_t channel_id)
{
    return NBN_Connection_EnqueueOutgoingMessage(client, channel_id);
}

int NBN_GameServer_EnqueueUnreliableMessageFor(NBN_Connection *client)
{
    return NBN_GameServer_EnqueueMessageFor(client, NBN_RESERVED_UNRELIABLE_CHANNEL);
}

int NBN_GameServer_EnqueueReliableMessageFor(NBN_Connection *client)
{
    return NBN_GameServer_EnqueueMessageFor(client, NBN_RESERVED_RELIABLE_CHANNEL);
}

NBN_Connection *NBN_GameServer_AcceptConnection(void)
{
    assert(last_event_type == NBN_NEW_CONNECTION);
    assert(last_event_client != NULL);

    last_event_client->accepted = true;

    return last_event_client;
}

void NBN_GameServer_RejectConnection(int code)
{
    assert(last_event_type == NBN_NEW_CONNECTION);
    assert(last_event_client != NULL);

    NBN_GameServer_CloseClient(last_event_client, code);
}

bool NBN_GameServer_CanEnqueueMessageFor(NBN_Connection *client, uint8_t channel_id)
{
    NBN_Channel *channel = client->channels[channel_id];

    assert(channel != NULL);

    NBN_MeasureStream measure_stream;

    NBN_MeasureStream_Init(&measure_stream);

    unsigned int message_size = (NBN_Message_Measure(outgoing_message, &measure_stream) - 1) / 8 + 1;

    return NBN_Channel_HasRoomForMessage(channel, message_size);
}

uint32_t NBN_GameServer_GetDisconnectedClientId(void)
{
    assert(last_event_type == NBN_CLIENT_DISCONNECTED);

    return last_event_client->id;
}

NBN_MessageInfo NBN_GameServer_GetReceivedMessageInfo(void)
{
    assert(last_event_type == NBN_CLIENT_MESSAGE_RECEIVED);
    assert(last_received_message_data != NULL);
    assert(last_event_client != NULL);

    return (NBN_MessageInfo){
        .type = last_received_message_type,
        .data = last_received_message_data,
        .sender = last_event_client};
}

NBN_Message *NBN_GameServer_GetOutgoingMessage(void)
{
    return outgoing_message;
}

NBN_GameServerStats NBN_GameServer_GetStats(void)
{
    return game_server.stats;
}

#ifdef NBN_DEBUG

void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
    case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
        game_server.endpoint.OnMessageAddedToRecvQueue = cb;
        break;
    }
}

#endif /* NBN_DEBUG */

static void NBN_GameServer_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *client)
{
    assert(NBN_List_Includes(game_server.clients, client));

    if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        NBN_Channel *channel = client->channels[message->header.channel_id];

        if (NBN_Channel_AddChunk(channel, message))
            NBN_EventQueue_Enqueue(
                game_server.endpoint.events_queue,
                NBN_CLIENT_MESSAGE_RECEIVED,
                NBN_Channel_ReconstructMessageFromChunks(channel, client)
            );
    }
    else
    {
        NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_CLIENT_MESSAGE_RECEIVED, message);
    }
}

static void NBN_GameServer_CloseStaleClientConnections(void)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection *client = current_node->data;

        current_node = current_node->next;

        if (!client->is_stale && NBN_Connection_CheckIfStale(client))
        {
            NBN_LogInfo("Client %d connection is stale, closing it.", client->id);

            client->is_stale = true;

            NBN_GameServer_CloseClient(client, -1);
        }
    }
}

static void NBN_GameServer_RemoveClosedClientConnections(void)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection *client = current_node->data;

        current_node = current_node->next;

        if (client->closed && (client->send_queue->count == 0 || client->is_stale))
        {
            NBN_LogDebug("Destroy closed client connection (ID: %d)", client->id);

            NBN_Driver_GServ_DestroyClientConnection(client->id);
            NBN_List_Remove(game_server.clients, client);
        }
    }
}

static void NBN_GameServer_HandleEvent(int event_type)
{
    switch (event_type)
    {
    case NBN_NEW_CONNECTION:
        NBN_GameServer_HandleClientConnectionEvent();
        break;

    case NBN_CLIENT_DISCONNECTED:
        NBN_GameServer_HandleClientDisconnectedEvent();
        break;

    case NBN_CLIENT_MESSAGE_RECEIVED:
        NBN_GameServer_HandleMessageReceivedEvent();
        break;

    default:
        break;
    }
}

static void NBN_GameServer_HandleClientConnectionEvent(void)
{
    last_event_client = game_server.endpoint.events_queue->last_event_data;
}

static void NBN_GameServer_HandleClientDisconnectedEvent(void)
{
    last_event_client = game_server.endpoint.events_queue->last_event_data;
}

static void NBN_GameServer_HandleMessageReceivedEvent(void)
{
    NBN_Message *message = game_server.endpoint.events_queue->last_event_data;

    last_event_client = message->sender;
    last_received_message_type = message->header.type;
    last_received_message_data = message->data;

    NBN_Message_Destroy(message, false);
}

static void NBN_GameServer_ReleaseLastEvent(void)
{
    switch (last_event_type)
    {
    case NBN_CLIENT_DISCONNECTED:
        NBN_GameServer_ReleaseClientDisconnectedEvent();
        break;

    case NBN_CLIENT_MESSAGE_RECEIVED:
        NBN_GameServer_ReleaseMessageReceivedEvent();
        break;

    default:
        break;
    }

    last_event_client = NULL;
    last_received_message_data = NULL;
}

static void NBN_GameServer_ReleaseClientDisconnectedEvent(void)
{
    NBN_Connection_Destroy(last_event_client);
}

static void NBN_GameServer_ReleaseMessageReceivedEvent(void)
{
    assert(last_received_message_data != NULL);

    game_server.endpoint.message_destructors[last_received_message_type](last_received_message_data);

#ifdef NBN_DEBUG
    mem_manager.report.destroyed_message_count++;
#endif
}

#endif /* NBN_GAME_SERVER */

#pragma endregion /* NBN_GameServer */

#pragma region Game server driver

#ifdef NBN_GAME_SERVER

static void NBN_GServ_Driver_OnClientConnected(NBN_Connection *);
static void NBN_GServ_Driver_OnClientPacketReceived(NBN_Packet *);

void NBN_Driver_GServ_RaiseEvent(NBN_Driver_GServ_EventType ev, void *data)
{
    switch (ev)
    {
    case NBN_DRIVER_GSERV_CLIENT_CONNECTED:
        NBN_GServ_Driver_OnClientConnected(data);
        break;

    case NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED:
        NBN_GServ_Driver_OnClientPacketReceived(data);
        break;
    }
}

static void NBN_GServ_Driver_OnClientConnected(NBN_Connection *client)
{
    assert(!NBN_List_Includes(game_server.clients, client));

    NBN_List_PushBack(game_server.clients, client);
    NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_NEW_CONNECTION, client);
}

static void NBN_GServ_Driver_OnClientPacketReceived(NBN_Packet *packet)
{
    assert(NBN_List_Includes(game_server.clients, packet->sender));

    if (NBN_Endpoint_ProcessReceivedPacket(&game_server.endpoint, packet, packet->sender) < 0)
    {
        NBN_LogError("An error occured while processing packet from client %d, closing the client", packet->sender->id);
        NBN_GameServer_CloseClient(packet->sender, -1);
    }
}

#endif /* NBN_GAME_SERVER */

#pragma endregion /* Game server driver */

#pragma region Packet simulator

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#define RAND_RATIO_BETWEEN(min, max) (((rand() % (int)((max * 100.f) - (min * 100.f) + 1)) + (min * 100.f)) / 100.f) 
#define RAND_RATIO RAND_RATIO_BETWEEN(0, 1)

static NBN_PacketSimulatorEntry *NBN_PacketSimulator_CreateEntry(NBN_PacketSimulator *, NBN_Packet *, uint32_t);
static void NBN_PacketSimulator_DestroyEntry(NBN_PacketSimulatorEntry *);

#ifdef NBNET_WINDOWS
DWORD WINAPI NBN_PacketSimulator_Routine(LPVOID);
#else
static void *NBN_PacketSimulator_Routine(void *);
#endif

static int NBN_PacketSimulator_SendPacket(NBN_PacketSimulator *, NBN_Packet *, uint32_t);
static unsigned int NBN_PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *);

NBN_PacketSimulator *NBN_PacketSimulator_Create(void)
{
    NBN_PacketSimulator *packet_simulator = NBN_Alloc(sizeof(NBN_PacketSimulator));

    packet_simulator->packets_queue = NBN_List_Create();

#ifdef NBNET_WINDOWS
    packet_simulator->packets_queue_mutex = CreateMutex(NULL, FALSE, NULL);
#else
    packet_simulator->packets_queue_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif

    packet_simulator->time = 0;
    packet_simulator->running = false;
    packet_simulator->ping = 0;
    packet_simulator->jitter = 0;
    packet_simulator->packet_loss_ratio = 0;
    packet_simulator->packet_duplication_ratio = 0;

    return packet_simulator;
}

void NBN_PacketSimulator_Destroy(NBN_PacketSimulator *packet_simulator)
{
    packet_simulator->running = false;

    NBN_List_Destroy(packet_simulator->packets_queue, true, NULL);
    NBN_Dealloc(packet_simulator);
}

void NBN_PacketSimulator_EnqueuePacket(
    NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, uint32_t receiver_id)
{
#ifdef NBNET_WINDOWS
    WaitForSingleObject(packet_simulator->packets_queue_mutex, INFINITE);
#else
    pthread_mutex_lock(&packet_simulator->packets_queue_mutex);
#endif

    NBN_Packet *dup_packet = NBN_Alloc(sizeof(NBN_Packet));

    memcpy(dup_packet, packet, sizeof(NBN_Packet));

    NBN_PacketSimulatorEntry *entry = NBN_PacketSimulator_CreateEntry(packet_simulator, dup_packet, receiver_id);

    /*
        Compute jitter in range [ -jitter, +jitter ].

        Jitter is converted from seconds to milliseconds for the random operation below.
    */
    int jitter = packet_simulator->jitter * 1000;

    jitter = (jitter > 0) ? (rand() % (jitter * 2)) - jitter : 0;

    entry->delay = packet_simulator->ping + jitter / 1000; /* and converted back to seconds */

    NBN_List_PushBack(packet_simulator->packets_queue, entry);

#ifdef NBNET_WINDOWS
    ReleaseMutex(packet_simulator->packets_queue_mutex);
#else
    pthread_mutex_unlock(&packet_simulator->packets_queue_mutex);
#endif
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

static NBN_PacketSimulatorEntry *NBN_PacketSimulator_CreateEntry(NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, uint32_t receiver_id)
{
    NBN_PacketSimulatorEntry *entry = NBN_Alloc(sizeof(NBN_PacketSimulatorEntry));

    entry->packet = packet;
    entry->receiver_id = receiver_id;
    entry->delay = 0;
    entry->enqueued_at = packet_simulator->time;

    return entry;
}

static void NBN_PacketSimulator_DestroyEntry(NBN_PacketSimulatorEntry *entry)
{
    NBN_Dealloc(entry->packet);
    NBN_Dealloc(entry);
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
        WaitForSingleObject(packet_simulator->packets_queue_mutex, INFINITE);
#else
        pthread_mutex_lock(&packet_simulator->packets_queue_mutex);
#endif

        NBN_ListNode *current_node = packet_simulator->packets_queue->head;

        while (current_node)
        {
            NBN_PacketSimulatorEntry *entry = current_node->data;

            current_node = current_node->next;

            if (packet_simulator->time - entry->enqueued_at < entry->delay)
                continue;

            NBN_List_Remove(packet_simulator->packets_queue, entry);
            NBN_PacketSimulator_SendPacket(packet_simulator, entry->packet, entry->receiver_id);

            for (unsigned int i = 0; i < NBN_PacketSimulator_GetRandomDuplicatePacketCount(packet_simulator); i++)
            {
                NBN_LogDebug("Duplicate packet %d (count: %d)", entry->packet->header.seq_number, i + 1);

                NBN_PacketSimulator_SendPacket(packet_simulator, entry->packet, entry->receiver_id);
            }

            NBN_PacketSimulator_DestroyEntry(entry);
        }

#ifdef NBNET_WINDOWS
        ReleaseMutex(packet_simulator->packets_queue_mutex);
#else
        pthread_mutex_unlock(&packet_simulator->packets_queue_mutex);
#endif
    }

#ifdef NBNET_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

static int NBN_PacketSimulator_SendPacket(NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, uint32_t receiver_id)
{
    if (RAND_RATIO < packet_simulator->packet_loss_ratio)
    {
        NBN_LogDebug("Drop packet %d", packet->header.seq_number);

        return 0;
    }

#ifdef NBN_GAME_CLIENT
    (void)receiver_id;

    return NBN_Driver_GCli_SendPacket(packet);
#endif

#ifdef NBN_GAME_SERVER
    return NBN_Driver_GServ_SendPacketTo(packet, receiver_id);
#endif

    return -1;
}

static unsigned int NBN_PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *packet_simulator)
{
    if (RAND_RATIO < packet_simulator->packet_duplication_ratio)
        return rand() % 10 + 1;

    return 0;
}

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */

#endif /* NBNET_IMPL */

#pragma endregion /* Implementations */
