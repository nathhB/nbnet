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
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#pragma region Declarations

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
void NBN_List_Clear(NBN_List *, bool, NBN_List_FreeItemFunc);
void NBN_List_PushBack(NBN_List *, void *);
void *NBN_List_GetAt(NBN_List *, int);
void *NBN_List_Remove(NBN_List *, void *);
void *NBN_List_RemoveAt(NBN_List *, int);
bool NBN_List_Includes(NBN_List *, void *);

#pragma endregion /* NBN_List */

#pragma region NBN_Timer

typedef struct
{
    time_t elapsed_ms;
    time_t current_ms;
    bool running;
} NBN_Timer;

NBN_Timer *NBN_Timer_Create(void);
void NBN_Timer_Destroy(NBN_Timer *);
void NBN_Timer_Start(NBN_Timer *);
void NBN_Timer_Stop(NBN_Timer *);
void NBN_Timer_Update(NBN_Timer *);

#pragma endregion

#pragma region Serialization

typedef uint32_t Word;

unsigned int number_of_bits_required_for(unsigned int);

#define WORD_BYTES (sizeof(Word))
#define WORD_BITS (WORD_BYTES * 8)
#define BITS_REQUIRED(min, max) (min == max) ? 0 : number_of_bits_required_for(max - min)

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

#define NBN_MAX_MESSAGE_CHANNELS 255 /* Maximum value of uint8_t, see message header */
#define NBN_MAX_MESSAGE_TYPES 255 /* Maximum value of uint8_t, see message header */
#define NBN_MESSAGE_RESEND_DELAY_MS 100 /* Number of ms before a message is resent (reliable messages redundancy) */

/* Chunk max size is the number of bytes of data a packet can hold minus the size of a message header minus 2 bytes
 * to hold the chunk id and total number of chunks.
 */
#define NBN_MESSAGE_CHUNK_SIZE (NBN_PACKET_MAX_DATA_SIZE - sizeof(NBN_MessageHeader) - 2)
#define NBN_MESSAGE_CHUNK_TYPE (NBN_MAX_MESSAGE_TYPES - 1) /* Reserved message type for chunks */

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
    NBN_Channel *channel;
    NBN_MessageDestructor destructor;
    time_t last_send_time;
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
    NBN_Connection *sender;
    void *data;
} NBN_MessageInfo;

NBN_Message *NBN_Message_Create(uint8_t, NBN_Channel *, NBN_MessageDestructor, void *);
void NBN_Message_Destroy(NBN_Message *, bool);
int NBN_Message_SerializeHeader(NBN_MessageHeader *, NBN_Stream *);
int NBN_Message_Measure(NBN_Message *, NBN_MessageSerializer, NBN_MeasureStream *);

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
    bool sealed;
    uint8_t buffer[NBN_PACKET_MAX_SIZE];
    unsigned int size; // in bytes

    // streams
    NBN_WriteStream w_stream;
    NBN_ReadStream r_stream;
    NBN_MeasureStream m_stream;
} NBN_Packet;

void NBN_Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
int NBN_Packet_InitRead(NBN_Packet *, uint8_t[NBN_PACKET_MAX_SIZE], unsigned int);
int NBN_Packet_WriteMessage(NBN_Packet *, NBN_Message *, NBN_MessageSerializer);
int NBN_Packet_Seal(NBN_Packet *);

#pragma endregion /* NBN_Packet */

#pragma region NBN_MessageChunk

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

#pragma region NBN_Channel

#define NBN_CHANNEL_BUFFER_SIZE 1024
#define NBN_CHANNEL_CHUNKS_BUFFER_SIZE 255

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
    unsigned int chunks_count;
    int last_received_chunk_id;
    NBN_MessageChunk *chunks_buffer[NBN_CHANNEL_CHUNKS_BUFFER_SIZE];
    NBN_OutgoingMessagePolicy (*get_outgoing_message_policy)(NBN_Channel *, NBN_Message *);
    bool (*handle_message_reception)(NBN_Channel *, NBN_Message *);
    bool (*process_queued_received_message)(NBN_Channel *, NBN_Message *);
    void (*on_message_acked)(NBN_Channel *, NBN_Message *);
};

bool NBN_Channel_AddChunk(NBN_Channel *, NBN_Message *);
NBN_Message *NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *, NBN_Connection *);

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

#pragma region NBN_Connection

#define NBN_MAX_PACKET_ENTRIES 1024

/* Number of ms before the connection is considered stale */
#ifdef NBN_DEBUG
/* Set it to a ridiculously high value so connections won't be considered stale and get closed when testing
with very bad connection conditions */
#define NBN_CONNECTION_STALE_MS_THRESHOLD 999999
#else
#define NBN_CONNECTION_STALE_MS_THRESHOLD 5000
#endif /* NBN_DEBUG */

typedef struct
{
    bool acked;
    int messages_count;
    uint16_t message_ids[NBN_MAX_MESSAGES_PER_PACKET];
    time_t send_time;
} NBN_PacketEntry;

typedef struct
{
    unsigned int ping;
    float packet_loss; // TODO: compute
    unsigned int sent_packets;
    unsigned int acked_packets;
    unsigned int lost_packets;
} NBN_ConnectionStats;

#ifdef NBN_DEBUG

typedef struct
{
    unsigned int created_messages_count;
    unsigned int destroyed_messages_count;
} NBN_ConnectionDebugInfo;

typedef struct 
{
    float min_packet_loss_ratio;
    float max_packet_loss_ratio;
    float current_packet_loss_ratio;
    float packet_loss_fluctuation_ratio;
    float packet_duplication_ratio;
    unsigned int ping;
    unsigned int jitter;
} NBN_ConnectionDebugSettings;

typedef enum
{
    NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE
} NBN_ConnectionDebugCallback;

#endif /* NBN_DEBUG */

struct __NBN_Connection
{
    int id;
    uint32_t protocol_id;
    bool accepted;
    NBN_MessageBuilder message_builders[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageDestructor message_destructors[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageSerializer message_serializers[NBN_MAX_MESSAGE_TYPES];
    time_t last_recv_packet_time; /* used to detect stale connections */
    NBN_Timer *timer;
    void *user_data;
    NBN_Message *message;
    NBN_ConnectionStats stats;

#ifdef NBN_DEBUG
    NBN_ConnectionDebugInfo debug_info;
    NBN_ConnectionDebugSettings debug_settings;
    void (*on_message_added_to_recv_queue)(struct __NBN_Connection *, NBN_Message *);
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
    NBN_Channel *channels[NBN_MAX_MESSAGE_CHANNELS];
    NBN_List *recv_queue;
    NBN_List *send_queue;
};

NBN_Connection *NBN_Connection_Create(
    int, uint32_t,
    bool,
    NBN_MessageBuilder[NBN_MAX_MESSAGE_TYPES],
    NBN_MessageDestructor[NBN_MAX_MESSAGE_TYPES],
    NBN_MessageSerializer[NBN_MAX_MESSAGE_TYPES]);
void NBN_Connection_Destroy(NBN_Connection *);
int NBN_Connection_ProcessReceivedPacket(NBN_Connection *, NBN_Packet *);
NBN_Message *NBN_Connection_DequeueReceivedMessage(NBN_Connection *);
void *NBN_Connection_CreateOutgoingMessage(NBN_Connection *, uint8_t, uint8_t);
int NBN_Connection_EnqueueOutgoingMessage(NBN_Connection *);
int NBN_Connection_FlushSendQueue(NBN_Connection *);
int NBN_Connection_CreateChannel(NBN_Connection *, NBN_ChannelType, uint8_t);
bool NBN_Connection_IsStale(NBN_Connection *);
void NBN_Connection_UpdateTimer(NBN_Connection *);

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

#pragma region Debug packet simulator

#ifdef NBN_DEBUG

typedef struct
{
    NBN_Packet *packet;
    uint32_t receiver_id;
    time_t delay_ms;
    time_t enqueued_at;
} NBN_PacketSimulatorEntry;

typedef struct
{
    NBN_List *packets_queue;
    pthread_mutex_t packets_queue_mutex;
    pthread_t thread;
    bool running;
    NBN_Timer *timer;
} NBN_PacketSimulator;

NBN_PacketSimulator *NBN_PacketSimulator_Create(void);
void NBN_PacketSimulator_Destroy(NBN_PacketSimulator *);
void NBN_PacketSimulator_EnqueuePacket(NBN_PacketSimulator *, NBN_Packet *, uint32_t, NBN_ConnectionDebugSettings);
void NBN_PacketSimulator_Start(NBN_PacketSimulator *);
void NBN_PacketSimulator_Stop(NBN_PacketSimulator *);
void NBN_PacketSimulator_UpdateTimer(NBN_PacketSimulator *);
NBN_PacketSimulatorEntry *NBN_PacketSimulator_CreateEntry(NBN_PacketSimulator *, NBN_Packet *, uint32_t);
void NBN_PacketSimulator_DestroyEntry(NBN_PacketSimulatorEntry *);

static void *packet_simulator_routine(void *);

#endif /* NBN_DEBUG */

#pragma endregion

#pragma region NBN_Endpoint

#ifdef NBN_GAME_CLIENT

#define NBN_RegisterMessage(type, builder, serializer, destructor) \
{ \
    if (type == NBN_MESSAGE_CHUNK_TYPE) \
    { \
        log_error("The message type %d is reserved by the library", type); \
        exit(1); \
    } \
    NBN_Endpoint_RegisterMessageBuilder(&game_client.endpoint, (NBN_MessageBuilder)builder, type); \
    NBN_Endpoint_RegisterMessageDestructor(&game_client.endpoint, (NBN_MessageDestructor)destructor, type); \
    NBN_Endpoint_RegisterMessageSerializer(&game_client.endpoint, (NBN_MessageSerializer)serializer, type); \
}

#define NBN_RegisterChannel(type, id) NBN_Endpoint_RegisterChannel(&game_client.endpoint, type, id)

#endif /* NBN_GAME_CLIENT */

#ifdef NBN_GAME_SERVER

#define NBN_RegisterMessage(type, builder, serializer, destructor) \
{ \
    if (type == NBN_MESSAGE_CHUNK_TYPE) \
    { \
        log_error("The message type %d is reserved by the library", type); \
        exit(1); \
    } \
    NBN_Endpoint_RegisterMessageBuilder(&game_server.endpoint, (NBN_MessageBuilder)builder, type); \
    NBN_Endpoint_RegisterMessageDestructor(&game_server.endpoint, (NBN_MessageDestructor)destructor, type); \
    NBN_Endpoint_RegisterMessageSerializer(&game_server.endpoint, (NBN_MessageSerializer)serializer, type); \
}

#define NBN_RegisterChannel(type, id) NBN_Endpoint_RegisterChannel(&game_server.endpoint, type, id)

#endif /* NBN_GAME_SERVER */

typedef struct
{
    uint32_t protocol_id;
    NBN_ChannelType channels[NBN_MAX_MESSAGE_CHANNELS];
    NBN_MessageBuilder message_builders[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageDestructor message_destructors[NBN_MAX_MESSAGE_TYPES];
    NBN_MessageSerializer message_serializers[NBN_MAX_MESSAGE_TYPES];
    NBN_EventQueue *events_queue;

#ifdef NBN_DEBUG
    NBN_ConnectionDebugSettings debug_settings;
    void (*on_message_added_to_recv_queue)(NBN_Connection *, NBN_Message *);
    NBN_PacketSimulator *packet_simulator;
#endif /* NBN_DEBUG */
} NBN_Endpoint;

void NBN_Endpoint_Init(NBN_Endpoint *, const char *);
void NBN_Endpoint_Deinit(NBN_Endpoint *);
void NBN_Endpoint_RegisterMessageBuilder(NBN_Endpoint *, NBN_MessageBuilder, uint8_t);
void NBN_Endpoint_RegisterMessageDestructor(NBN_Endpoint *, NBN_MessageDestructor, uint8_t);
void NBN_Endpoint_RegisterMessageSerializer(NBN_Endpoint *, NBN_MessageSerializer, uint8_t);
void NBN_Endpoint_RegisterChannel(NBN_Endpoint *, NBN_ChannelType, uint8_t);
NBN_Connection *NBN_Endpoint_CreateConnection(NBN_Endpoint *, int, bool);

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

#ifdef NBN_GAME_CLIENT

typedef enum
{
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

void NBN_GameClient_Init(const char *);
int NBN_GameClient_Start(const char *, uint16_t);
void NBN_GameClient_Stop(void);
NBN_GameClientEvent NBN_GameClient_Poll(void);
int NBN_GameClient_Flush(void);
void *NBN_GameClient_CreateMessage(uint8_t, uint8_t);
int NBN_GameClient_SendMessage(void);
NBN_Connection *NBN_GameClient_CreateServerConnection(void);
int NBN_GameClient_ReadReceivedMessage(NBN_MessageInfo *);
NBN_ConnectionStats NBN_GameClient_GetStats(void);
void NBN_GameClient_OnConnected(void);
void NBN_GameClient_OnDisconnected(void);
int NBN_GameClient_OnPacketReceived(NBN_Packet *);

#ifdef NBN_DEBUG
void NBN_GameClient_Debug_SetMinPacketLossRatio(float);
void NBN_GameClient_Debug_SetMaxPacketLossRatio(float);
void NBN_GameClient_Debug_SetPacketDuplicationRatio(float);
void NBN_GameClient_Debug_SetPing(unsigned int);
void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);
NBN_ConnectionDebugInfo NBN_GameClient_Debug_GetInfo(void);

#endif /* NBN_DEBUG */

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* NBN_GameClient */

#pragma region NBN_GameServer

#ifdef NBN_GAME_SERVER

typedef enum
{
    /* A client has requested a connection */
    NBN_CLIENT_CONNECTION_REQUESTED = 1,

    /* A new client has connected */
    NBN_CLIENT_CONNECTED,

    /* A client has disconnected */
    NBN_CLIENT_DISCONNECTED,

    /* A message has been received from a client */
    NBN_CLIENT_MESSAGE_RECEIVED
} NBN_GameServerEvent;

typedef struct
{
    NBN_Endpoint endpoint;
    NBN_List* clients;
    bool auto_accept_clients;
    uint8_t auth_message_type;
} NBN_GameServer;

extern NBN_GameServer game_server;

void NBN_GameServer_Init(const char *);
int NBN_GameServer_Start(uint16_t);
void NBN_GameServer_Stop(void);
NBN_GameServerEvent NBN_GameServer_Poll(void);
int NBN_GameServer_Flush(void);
NBN_Connection *NBN_GameServer_CreateClientConnection(uint32_t);
void NBN_GameServer_AcceptClient(NBN_Connection *);
void NBN_GameServer_RejectClient(NBN_Connection *);
void NBN_GameServer_CloseClient(NBN_Connection *);
void *NBN_GameServer_CreateMessage(uint8_t, uint8_t, NBN_Connection *);
int NBN_GameServer_SendMessageTo(NBN_Connection *);
NBN_Connection *NBN_GameServer_GetLastEventClient(void);
void *NBN_GameServer_ReadAuthRequest(void);
int NBN_GameServer_ReadReceivedMessage(NBN_MessageInfo *);
void NBN_GameServer_AttachDataToClient(uint32_t, void *);
NBN_ConnectionStats NBN_GameServer_GetDisconnectedClientStats(void);
NBN_ConnectionStats* NBN_GameServer_GetClientStats(uint32_t);
void NBN_GameServer_OnClientConnected(NBN_Connection *);
int NBN_GameServer_OnPacketReceived(NBN_Packet *, NBN_Connection *);

#define NBN_GameServer_GetConnectedClient NBN_GameServer_GetLastEventClient
#define NBN_GameServer_GetDisconnectedClient NBN_GameServer_GetLastEventClient
#define NBN_GameServer_GetMessageSender NBN_GameServer_GetLastEventClient

#ifdef NBN_DEBUG
NBN_ConnectionDebugInfo NBN_GameServer_Debug_GetDisconnectedClientInfo(void);
NBN_ConnectionDebugInfo* NBN_GameServer_Debug_GetClientInfo(uint32_t);
void NBN_GameServer_Debug_SetMinPacketLossRatio(float);
void NBN_GameServer_Debug_SetMaxPacketLossRatio(float);
void NBN_GameServer_Debug_SetPacketDuplicationRatio(float);
void NBN_GameServer_Debug_SetPing(unsigned int);
void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#endif /* NBN_GAME_SERVER */

#pragma endregion /* NBN_GameServer */

#pragma region Network driver

#ifdef NBN_GAME_SERVER

int NBN_Driver_GServ_Start(uint32_t, uint16_t);
void NBN_Driver_GServ_Stop(void);
int NBN_Driver_GServ_RecvPackets(void);
void NBN_Driver_GServ_CloseClient(uint32_t);
int NBN_Driver_GServ_SendPacketTo(NBN_Packet *, uint32_t);

#endif /* NBN_GAME_SERVER */

#ifdef NBN_GAME_CLIENT

int NBN_Driver_GCli_Start(uint32_t, const char *, uint16_t);
void NBN_Driver_GCli_Stop(void);
int NBN_Driver_GCli_RecvPackets(void);
int NBN_Driver_GCli_SendPacket(NBN_Packet *);

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

#pragma region Debugging

#ifdef NBN_DEBUG

/* Macros used to simulate bad connection conditions */

#define RAND_RATIO_BETWEEN(min, max) (((rand() % (int)((max * 100.f) - (min * 100.f) + 1)) + (min * 100.f)) / 100.f)
#define RAND_RATIO RAND_RATIO_BETWEEN(0, 1)

#endif /* NBN_DEBUG */

#pragma endregion /* Debugging */

#pragma region Logging

/* I did not write this: https://github.com/rxi/log.c */

/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#define LOG_VERSION "0.1.0"

typedef void (*log_LockFn)(void *udata, int lock);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define log_trace(...) log_log(LOG_TRACE, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILENAME__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILENAME__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILENAME__, __LINE__, __VA_ARGS__)

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_set_quiet(int enable);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#pragma endregion /* Logging */

#pragma endregion /* Declarations */

#endif /* NBNET_H */

#pragma region Implementations

#ifdef NBNET_IMPL

#pragma region NBN_List

static NBN_ListNode *create_list_node(void *);
static void *remove_node_from_list(NBN_List *, NBN_ListNode *);

NBN_List *NBN_List_Create()
{
    NBN_List *list = malloc(sizeof(NBN_List));

    if (list == NULL)
        return NULL;

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;

    return list;
}

void NBN_List_Destroy(NBN_List *list, bool free_items, NBN_List_FreeItemFunc free_item_func)
{
    NBN_List_Clear(list, free_items, free_item_func);
    free(list);
}

void NBN_List_Clear(NBN_List *list, bool free_items, NBN_List_FreeItemFunc free_item_func)
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
                free(current_node->data);
        }

        free(current_node);

        current_node = next_node;
    }

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

void NBN_List_PushBack(NBN_List *list, void *data)
{
    NBN_ListNode *node = create_list_node(data);

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

void *NBN_List_GetAt(NBN_List *list, int index)
{
    NBN_ListNode *current_node = list->head;

    for (int i = 0; current_node != NULL && i < index; i++)
    {
        current_node = current_node->next;
    }

    return (current_node != NULL) ? current_node->data : NULL;
}

void *NBN_List_Remove(NBN_List *list, void *data)
{
    NBN_ListNode *current_node = list->head;

    for (int i = 0; current_node != NULL && current_node->data != data; i++)
        current_node = current_node->next;

    if (current_node != NULL)
    {
        return remove_node_from_list(list, current_node);
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
        return remove_node_from_list(list, current_node);
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

static NBN_ListNode *create_list_node(void *data)
{
    NBN_ListNode *node = malloc(sizeof(NBN_ListNode));

    node->data = data;

    return node;
}

static void *remove_node_from_list(NBN_List *list, NBN_ListNode *node)
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

        free(node);
        list->count--;

        return data;
    }

    if (node == list->tail)
    {
        NBN_ListNode *new_tail = node->prev;

        new_tail->next = NULL;
        list->tail = new_tail;

        void *data = node->data;

        free(node);
        list->count--;

        return data;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;

    void *data = node->data;

    free(node);
    list->count--;

    return data;
}

#pragma endregion /* NBN_List */

#pragma region NBN_Timer

NBN_Timer *NBN_Timer_Create(void)
{
    NBN_Timer *timer = malloc(sizeof(NBN_Timer));

    timer->current_ms = 0;
    timer->elapsed_ms = 0;
    timer->running = false;

    return timer;
}

void NBN_Timer_Destroy(NBN_Timer *timer)
{
    free(timer);
}

void NBN_Timer_Start(NBN_Timer *timer)
{
    timer->running = true;
}

void NBN_Timer_Stop(NBN_Timer *timer)
{
    timer->running = false;
}

void NBN_Timer_Update(NBN_Timer *timer)
{
    if (!timer->running)
        return;

    struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC_RAW, &spec);

    time_t ms = spec.tv_nsec / 1.0e6;

    /* handle wrap at 1000 */
    if (ms < timer->current_ms)
    {
        unsigned int wrap_amount = (1000 - timer->current_ms) + ms;

        timer->elapsed_ms += wrap_amount;
    }
    else
    {
        timer->elapsed_ms += (ms - timer->current_ms);
    }

    timer->current_ms = ms;
}

#pragma endregion

#pragma region Serialization

unsigned int number_of_bits_required_for(unsigned int v)
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

static void readFromBuffer(NBN_BitReader *);

void NBN_BitReader_Init(NBN_BitReader *bit_reader, uint8_t *buffer, unsigned int size)
{
    bit_reader->size = size;
    bit_reader->buffer = buffer;
    bit_reader->scratch = 0;
    bit_reader->scratch_bits_count = 0;
    bit_reader->byte_cursor = 0;
}

int NBN_BitReader_Read(NBN_BitReader *bit_reader, Word *word, unsigned int numberOfBits)
{
    *word = 0;

    if (numberOfBits > bit_reader->scratch_bits_count)
    {
        unsigned int needed_bytes = (numberOfBits - bit_reader->scratch_bits_count - 1) / 8 + 1;

        if (bit_reader->byte_cursor + needed_bytes > bit_reader->size)
            return -1;

        readFromBuffer(bit_reader);
    }

    *word |= (bit_reader->scratch & (((uint64_t)1 << numberOfBits) - 1));
    bit_reader->scratch >>= numberOfBits;
    bit_reader->scratch_bits_count -= numberOfBits;

    return 0;
}

static void readFromBuffer(NBN_BitReader *bit_reader)
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

static int flushScratchBitsToBuffer(NBN_BitWriter *, unsigned int);

void NBN_BitWriter_Init(NBN_BitWriter *bit_writer, uint8_t *buffer, unsigned int size)
{
    bit_writer->size = size;
    bit_writer->buffer = buffer;
    bit_writer->scratch = 0;
    bit_writer->scratch_bits_count = 0;
    bit_writer->byte_cursor = 0;
}

int NBN_BitWriter_Write(NBN_BitWriter *bit_writer, Word value, unsigned int numberOfBits)
{
    bit_writer->scratch |= ((uint64_t)value << bit_writer->scratch_bits_count);

    if ((bit_writer->scratch_bits_count += numberOfBits) >= WORD_BITS)
        return flushScratchBitsToBuffer(bit_writer, WORD_BITS);

    return 0;
}

int NBN_BitWriter_Flush(NBN_BitWriter *bit_writer)
{
    return flushScratchBitsToBuffer(bit_writer, bit_writer->scratch_bits_count);
}

static int flushScratchBitsToBuffer(NBN_BitWriter *bit_writer, unsigned int numberOfBits)
{
    if (bit_writer->scratch_bits_count < 1)
        return 0;

    unsigned int bytes_count = (numberOfBits - 1) / 8 + 1;

    assert(bytes_count <= WORD_BYTES);

    if (bit_writer->byte_cursor + bytes_count > bit_writer->size)
        return -1;

    Word word = 0 | (bit_writer->scratch & (((uint64_t)1 << numberOfBits) - 1));

    memcpy(bit_writer->buffer + bit_writer->byte_cursor, &word, bytes_count);

    bit_writer->scratch >>= numberOfBits;
    bit_writer->scratch_bits_count -= numberOfBits;
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

static int serialize_packet_header(NBN_Packet *, NBN_Stream *);

void NBN_Packet_InitWrite(NBN_Packet *packet, uint32_t protocol_id, uint16_t seq_number, uint16_t ack, uint32_t ack_bits)
{
    packet->header.protocol_id = protocol_id;
    packet->header.messages_count = 0;
    packet->header.seq_number = seq_number;
    packet->header.ack = ack;
    packet->header.ack_bits = ack_bits;

    packet->mode = NBN_PACKET_MODE_WRITE;
    packet->sealed = false;
    packet->size = 0;
    packet->m_stream.number_of_bits = 0;

    NBN_WriteStream_Init(&packet->w_stream, packet->buffer + NBN_PACKET_HEADER_SIZE, NBN_PACKET_MAX_DATA_SIZE);
    NBN_MeasureStream_Init(&packet->m_stream);
}

int NBN_Packet_InitRead(NBN_Packet *packet, uint8_t buffer[NBN_PACKET_MAX_SIZE], unsigned int size)
{
    packet->mode = NBN_PACKET_MODE_READ;
    packet->sealed = false;
    packet->size = size;

    memcpy(packet->buffer, buffer, NBN_PACKET_MAX_SIZE);

    NBN_ReadStream header_r_stream;

    NBN_ReadStream_Init(&header_r_stream, buffer, NBN_PACKET_HEADER_SIZE);

    if (serialize_packet_header(packet, (NBN_Stream *)&header_r_stream) < 0)
        return -1;

    NBN_ReadStream_Init(&packet->r_stream, buffer + NBN_PACKET_HEADER_SIZE, NBN_PACKET_MAX_DATA_SIZE);

    return 0;
}

int NBN_Packet_WriteMessage(NBN_Packet *packet, NBN_Message *message, NBN_MessageSerializer message_serializer)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE || packet->sealed)
        return NBN_PACKET_WRITE_ERROR;

    int current_number_of_bits = packet->m_stream.number_of_bits;

    if (NBN_Message_Measure(message, message_serializer, &packet->m_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    if (packet->header.messages_count >= NBN_MAX_MESSAGES_PER_PACKET || packet->m_stream.number_of_bits > NBN_PACKET_MAX_DATA_SIZE * 8)
    {
        packet->m_stream.number_of_bits = current_number_of_bits;

        return NBN_PACKET_WRITE_NO_SPACE;
    }

    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)&packet->w_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    if (message_serializer(message->data, (NBN_Stream *)&packet->w_stream) < 0)
        return NBN_PACKET_WRITE_ERROR;

    packet->size = (packet->m_stream.number_of_bits - 1) / 8 + 1;
    packet->header.messages_count++;

    return NBN_PACKET_WRITE_OK;
}

int NBN_Packet_Seal(NBN_Packet *packet)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE)
        return -1;

    if (NBN_WriteStream_Flush(&packet->w_stream) < 0)
        return -1;

    NBN_WriteStream header_w_stream;

    NBN_WriteStream_Init(&header_w_stream, packet->buffer, NBN_PACKET_HEADER_SIZE);

    if (serialize_packet_header(packet, (NBN_Stream *)&header_w_stream) < 0)
        return -1;

    if (NBN_WriteStream_Flush(&header_w_stream) < 0)
        return -1;

    packet->sealed = true;
    packet->size += NBN_PACKET_HEADER_SIZE;

    return 0;
}

static int serialize_packet_header(NBN_Packet *packet, NBN_Stream *stream)
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

NBN_Message *NBN_Message_Create(uint8_t type, NBN_Channel *channel, NBN_MessageDestructor destructor, void *data)
{
    NBN_Message *message = malloc(sizeof(NBN_Message));

    message->header.id = 0;
    message->header.type = type;
    message->header.channel_id = channel->id;

    message->destructor = destructor;
    message->sender = NULL;
    message->channel = channel;
    message->last_send_time = -1;
    message->sent = false;
    message->acked = false;
    message->data = data;

    return message;
}

void NBN_Message_Destroy(NBN_Message *message, bool free_data)
{
    if (free_data)
    {
        if (message->destructor)
            message->destructor(message->data);
        else
            free(message->data);
    }

    free(message);
}

int NBN_Message_SerializeHeader(NBN_MessageHeader *message_header, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&message_header->id, sizeof(message_header->id));
    SERIALIZE_BYTES(&message_header->type, sizeof(message_header->type));
    SERIALIZE_BYTES(&message_header->channel_id, sizeof(message_header->channel_id));

    return 0;
}

int NBN_Message_Measure(NBN_Message *message, NBN_MessageSerializer serializer, NBN_MeasureStream *m_stream)
{
    if (NBN_Message_SerializeHeader(&message->header, (NBN_Stream *)m_stream) < 0)
        return -1;

    if (serializer(message->data, (NBN_Stream *)m_stream) < 0)
        return -1;

    return m_stream->number_of_bits;
}

#pragma endregion /* NBN_Message */

#pragma region NBN_MessageChunk

NBN_MessageChunk *NBN_MessageChunk_Create(void)
{
    return malloc(sizeof(NBN_MessageChunk));
}

void NBN_MessageChunk_Destroy(NBN_MessageChunk *chunk)
{
    free(chunk);
}

int NBN_MessageChunk_Serialize(NBN_MessageChunk *chunk, NBN_Stream *stream)
{
    SERIALIZE_BYTES(&chunk->id, 1);
    SERIALIZE_BYTES(&chunk->total, 1);
    SERIALIZE_BYTES(chunk->data, NBN_MESSAGE_CHUNK_SIZE);

    return 0;
}

#pragma endregion /* NBN_MessageChunk */

#pragma region NBN_Connection

typedef enum
{
    NBN_MESSAGE_ERROR = -1,
    NBN_MESSAGE_ADDED_TO_PACKET,
    NBN_MESSAGE_NOT_ADDED_TO_PACKET
} OutgoingMessageProcessResult;

static uint32_t build_packet_ack_bits(NBN_Connection *);
static void decode_incoming_packet_header(NBN_Connection *, NBN_Packet *);
static void ack_packet(NBN_Connection *, uint16_t);
static NBN_Message *read_next_packet_message(NBN_Connection *, NBN_Packet *);
static int handle_message_reception(NBN_Message *, NBN_Connection *);
static OutgoingMessageProcessResult process_outgoing_message(NBN_Message *, NBN_Packet *, NBN_Connection *);
static void remove_message_from_send_queue(NBN_Connection *, NBN_Message *);
static NBN_PacketEntry *insert_send_packet_entry(NBN_Connection *, uint16_t);
static bool insert_recved_packet_entry(NBN_Connection *, uint16_t);
static NBN_PacketEntry *find_send_packet_entry(NBN_Connection *, uint16_t);
static bool is_packet_received(NBN_Connection *, uint16_t);
static int send_packet(NBN_Connection *, NBN_Packet *);
static NBN_Message *find_message(NBN_Connection *, uint16_t);
void update_connection_average_ping(NBN_Connection *, unsigned int);
static NBN_Message *read_message_from_stream(NBN_Connection *, NBN_ReadStream *);
static void destroy_message(void *);

#ifdef NBN_DEBUG
static void update_packet_loss_ratio(NBN_Connection *);
#endif

NBN_Connection *NBN_Connection_Create(int id,
                                uint32_t protocol_id,
                                bool accepted,
                                NBN_MessageBuilder message_builders[NBN_MAX_MESSAGE_TYPES],
                                NBN_MessageDestructor message_destructors[NBN_MAX_MESSAGE_TYPES],
                                NBN_MessageSerializer message_serializers[NBN_MAX_MESSAGE_TYPES])
{
    NBN_Connection *connection = malloc(sizeof(NBN_Connection));

    connection->id = id;
    connection->protocol_id = protocol_id;
    connection->accepted = accepted;
    connection->last_recv_packet_time = 0;

    memcpy(connection->message_builders, message_builders, sizeof(NBN_MessageBuilder[NBN_MAX_MESSAGE_TYPES]));
    memcpy(connection->message_destructors, message_destructors, sizeof(NBN_MessageDestructor[NBN_MAX_MESSAGE_TYPES]));
    memcpy(connection->message_serializers, message_serializers, sizeof(NBN_MessageSerializer[NBN_MAX_MESSAGE_TYPES]));

    connection->next_packet_seq_number = 1;
    connection->last_received_packet_seq_number = 0;
    connection->recv_queue = NBN_List_Create();
    connection->send_queue = NBN_List_Create();
    connection->timer = NBN_Timer_Create();
    connection->user_data = NULL;

    for (int i = 0; i < NBN_MAX_MESSAGE_CHANNELS; i++)
        connection->channels[i] = NULL;

    for (int i = 0; i < NBN_MAX_PACKET_ENTRIES; i++)
    {
        connection->packet_send_seq_buffer[i] = 0xFFFFFFFF;
        connection->packet_recv_seq_buffer[i] = 0xFFFFFFFF;
    }

#ifdef NBN_DEBUG
    connection->debug_settings = (NBN_ConnectionDebugSettings){0};
#endif

    NBN_Timer_Start(connection->timer);

    return connection;
}

void NBN_Connection_Destroy(NBN_Connection *connection)
{
    NBN_List_Destroy(connection->recv_queue, true, destroy_message);
    NBN_List_Destroy(connection->send_queue, true, destroy_message);

    NBN_Timer_Destroy(connection->timer);

    for (int i = 0; i < NBN_MAX_MESSAGE_CHANNELS; i++)
    {
        if (connection->channels[i])
            free(connection->channels[i]);
    }

    free(connection);
}

int NBN_Connection_ProcessReceivedPacket(NBN_Connection *connection, NBN_Packet *packet)
{
    decode_incoming_packet_header(connection, packet);

    if (!insert_recved_packet_entry(connection, packet->header.seq_number))
        return 0;

    if (SEQUENCE_NUMBER_GT(packet->header.seq_number, connection->last_received_packet_seq_number))
        connection->last_received_packet_seq_number = packet->header.seq_number;

    for (int i = 0; i < packet->header.messages_count; i++)
    {
        NBN_Message *message = read_next_packet_message(connection, packet);

        if (message == NULL)
        {
            log_error("Failed to read packet messages");

            return -1;
        }

        if (handle_message_reception(message, connection) < 0)
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

        dequeue_current_node = dequeue_current_node->next;

        if (message->channel->process_queued_received_message(message->channel, message))
        {
            NBN_List_Remove(connection->recv_queue, message);

            message->sender = connection;

            return message;
        }
    }

    return NULL;
}

void *NBN_Connection_CreateOutgoingMessage(NBN_Connection *connection, uint8_t msg_type, uint8_t chann_id)
{
    NBN_MessageBuilder msg_builder = connection->message_builders[msg_type];

    if (msg_builder == NULL)
    {
        log_error("No message builder is registered for messages of type %d", msg_type);

        return NULL;
    }

    if (connection->message_serializers[msg_type] == NULL)
    {
        log_error("No message serializer attached to messages of type %d", msg_type);

        return NULL;
    } 

    NBN_Channel *channel = connection->channels[chann_id];

    if (channel == NULL)
    {
        log_error("Channel %d does not exist", chann_id);

        return NULL;
    }

    NBN_Message *msg = NBN_Message_Create(msg_type, channel, connection->message_destructors[msg_type], msg_builder());

    connection->message = msg;

#ifdef NBN_DEBUG
    connection->debug_info.created_messages_count++;
#endif

    return msg->data;
}

int NBN_Connection_EnqueueOutgoingMessage(NBN_Connection *connection)
{
    assert(connection->message != NULL);

    NBN_MeasureStream measure_stream;
    NBN_MessageSerializer msg_serializer = connection->message_serializers[connection->message->header.type];

    NBN_MeasureStream_Init(&measure_stream);

    unsigned int message_size = (NBN_Message_Measure(connection->message, msg_serializer, &measure_stream) - 1) / 8 + 1;

    log_trace("Enqueue message of type %d for channel %d (%d bytes)",
            connection->message->header.type, connection->message->header.channel_id, message_size);

    if (message_size > NBN_PACKET_MAX_DATA_SIZE)
    {
        uint8_t chann_id = connection->message->header.channel_id;
        uint8_t *message_bytes = malloc(message_size);
        NBN_WriteStream write_stream;

        NBN_WriteStream_Init(&write_stream, message_bytes, message_size);

        if (NBN_Message_SerializeHeader(&connection->message->header, (NBN_Stream *)&write_stream) < 0)
            return -1;

        if (msg_serializer(connection->message->data, (NBN_Stream *)&write_stream) < 0)
            return -1;

        NBN_Message_Destroy(connection->message, true);

        connection->message = NULL;

        unsigned int chunks_count = ((message_size - 1) / NBN_MESSAGE_CHUNK_SIZE) + 1;

        log_trace("Split message into %d chunks", chunks_count);

        if (chunks_count > NBN_CHANNEL_CHUNKS_BUFFER_SIZE)
        {
            log_error("The maximum number of chunks is 255");

            return -1;
        }

        for (unsigned int i = 0; i < chunks_count; i++)
        {
            void *chunk_start = message_bytes + (i * NBN_MESSAGE_CHUNK_SIZE);
            unsigned int chunk_size = MIN(NBN_MESSAGE_CHUNK_SIZE, message_size - (i * NBN_MESSAGE_CHUNK_SIZE));

            log_trace("Enqueue chunk %d (size: %d)", i, chunk_size);

            NBN_MessageChunk *chunk = NBN_Connection_CreateOutgoingMessage(
                    connection, NBN_MESSAGE_CHUNK_TYPE, chann_id);

            assert(chunk != NULL);

            chunk->id = i;
            chunk->total = chunks_count;

            memcpy(chunk->data, chunk_start, chunk_size);

            NBN_Connection_EnqueueOutgoingMessage(connection);
        }
    }
    else
    {
        NBN_List_PushBack(connection->send_queue, connection->message);
    }

    connection->message = NULL;

    return 0;
}

int NBN_Connection_FlushSendQueue(NBN_Connection *connection)
{
    NBN_Packet outgoing_packet;

    NBN_Packet_InitWrite(
        &outgoing_packet,
        connection->protocol_id,
        connection->next_packet_seq_number++,
        connection->last_received_packet_seq_number,
        build_packet_ack_bits(connection));

    NBN_PacketEntry *packet_entry = insert_send_packet_entry(connection, outgoing_packet.header.seq_number);
    NBN_ListNode *current_node = connection->send_queue->head;

    log_trace("Flushing send queue (messages in queue: %d)", connection->send_queue->count);

    while (current_node)
    {
        NBN_Message *message = current_node->data;

        current_node = current_node->next;

        if (message->last_send_time >= 0 &&
                connection->timer->elapsed_ms - message->last_send_time < NBN_MESSAGE_RESEND_DELAY_MS)
            continue;

        OutgoingMessageProcessResult ret = process_outgoing_message(message, &outgoing_packet, connection);

        if (ret == NBN_MESSAGE_ERROR)
            return -1;

        if (ret == NBN_MESSAGE_ADDED_TO_PACKET)
        {
            message->last_send_time = connection->timer->elapsed_ms;
            packet_entry->message_ids[packet_entry->messages_count++] = message->header.id;
        }
    }

    assert(packet_entry->messages_count == outgoing_packet.header.messages_count);

    if (NBN_Packet_Seal(&outgoing_packet) < 0)
        return -1;

    if (send_packet(connection, &outgoing_packet) < 0)
        return -1;

    packet_entry->send_time = connection->timer->elapsed_ms;

    return 0;
}

int NBN_Connection_CreateChannel(NBN_Connection *connection, NBN_ChannelType type, uint8_t id)
{
    if (id > NBN_MAX_MESSAGE_CHANNELS - 1)
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
        log_error("Unsupported message channel type: %d", type);

        return -1;
    }

    channel->chunks_count = 0;
    channel->last_received_chunk_id = -1;

    for (int i = 0; i < NBN_CHANNEL_CHUNKS_BUFFER_SIZE; i++)
        channel->chunks_buffer[i] = NULL;

    connection->channels[id] = channel;

    return 0;
}

bool NBN_Connection_IsStale(NBN_Connection *connection)
{
    return connection->timer->elapsed_ms - connection->last_recv_packet_time > NBN_CONNECTION_STALE_MS_THRESHOLD;
}

void NBN_Connection_UpdateTimer(NBN_Connection *connection)
{
    assert(connection->timer->running);

    NBN_Timer_Update(connection->timer);
}

static void decode_incoming_packet_header(NBN_Connection *connection, NBN_Packet *packet)
{
    ack_packet(connection, packet->header.ack);

    for (int i = 0; i < 32; i++)
    {
        if (B_IS_UNSET(packet->header.ack_bits, i))
            continue;

        ack_packet(connection, packet->header.ack - (i + 1));
    }
}

static uint32_t build_packet_ack_bits(NBN_Connection *connection)
{
    uint32_t ack_bits = 0;

    for (int i = 0; i < 32; i++)
    {
        /* 
            when last_received_packet_seq_number is lower than 32, the value of acked_packet_seq_number will eventually
            wrap around, which means the packets from before the wrap around will naturally be acked
        */

        uint16_t acked_packet_seq_number = connection->last_received_packet_seq_number - (i + 1);

        if (is_packet_received(connection, acked_packet_seq_number))
            B_SET(ack_bits, i);
    }

    return ack_bits;
}

static void ack_packet(NBN_Connection *connection, uint16_t ack_packet_seq_number)
{
    NBN_PacketEntry *entry = find_send_packet_entry(connection, ack_packet_seq_number);

    if (entry && !entry->acked)
    {
        log_trace("Ack packet: %d", ack_packet_seq_number);

        unsigned int ping = connection->timer->elapsed_ms - entry->send_time;

        entry->acked = true;

        update_connection_average_ping(connection, ping);

        for (int i = 0; i < entry->messages_count; i++)
        {
            /* TODO: optimize this */
            NBN_Message *message = find_message(connection, entry->message_ids[i]);

            if (message && !message->acked)
            {
                if (message->channel->on_message_acked)
                    message->channel->on_message_acked(message->channel, message);

                message->acked = true;

                log_trace("Message acked: %d", message->header.id);
            }
        }

        connection->stats.acked_packets++;
    }
}

static NBN_Message *read_next_packet_message(NBN_Connection *connection, NBN_Packet *packet)
{
    return read_message_from_stream(connection, &packet->r_stream);
}

static int handle_message_reception(NBN_Message *message, NBN_Connection *connection)
{
    if (message->channel->handle_message_reception(message->channel, message))
    {
        log_trace("Received message %d on channel %d : added to recv queue", message->header.id, message->channel->id);

        NBN_List_PushBack(connection->recv_queue, message);

#ifdef NBN_DEBUG
        if (connection->on_message_added_to_recv_queue)
            connection->on_message_added_to_recv_queue(connection, message);
#endif
    }
    else
    {
        log_trace("Received message %d : discarded", message->header.id);

        NBN_Message_Destroy(message, true);
    }

    return 0;
}

static int process_outgoing_message(NBN_Message *message, NBN_Packet *outgoing_packet, NBN_Connection *connection)
{
    NBN_OutgoingMessagePolicy policy = message->channel->get_outgoing_message_policy(message->channel, message);

    if (policy == NBN_SKIP_MESSAGE)
    {
        return NBN_MESSAGE_NOT_ADDED_TO_PACKET;
    }
    else if (policy == NBN_DISCARD_MESSAGE)
    {
        remove_message_from_send_queue(connection, message);

        return NBN_MESSAGE_NOT_ADDED_TO_PACKET;
    }

    int ret = NBN_Packet_WriteMessage(outgoing_packet, message, connection->message_serializers[message->header.type]);

    if (ret < 0)
        return NBN_MESSAGE_ERROR;

    if (ret == NBN_PACKET_WRITE_OK)
    {
        log_trace("Message %d added to packet", message->header.id);

        if (policy == NBN_SEND_ONCE)
            remove_message_from_send_queue(connection, message);

        return NBN_MESSAGE_ADDED_TO_PACKET;
    }

    return NBN_MESSAGE_NOT_ADDED_TO_PACKET;
}

static void remove_message_from_send_queue(NBN_Connection *connection, NBN_Message *message)
{
    NBN_Message_Destroy(NBN_List_Remove(connection->send_queue, message), true);

#ifdef NBN_DEBUG
    connection->debug_info.destroyed_messages_count++;
#endif
}

static NBN_PacketEntry *insert_send_packet_entry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    connection->packet_send_seq_buffer[index] = seq_number;
    connection->packet_send_buffer[index] = (NBN_PacketEntry){.acked = false, .messages_count = 0};

    return &connection->packet_send_buffer[index];
}

static bool insert_recved_packet_entry(NBN_Connection *connection, uint16_t seq_number)
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
        for (uint16_t seq = connection->last_received_packet_seq_number; SEQUENCE_NUMBER_LT(seq, seq_number); seq++)
            connection->packet_recv_seq_buffer[seq % NBN_MAX_PACKET_ENTRIES] = 0xFFFFFFFF;
    }

    connection->packet_recv_seq_buffer[index] = seq_number;

    return true;
}

static NBN_PacketEntry *find_send_packet_entry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    if (connection->packet_send_seq_buffer[index] == seq_number)
        return &connection->packet_send_buffer[index];

    return NULL;
}

static bool is_packet_received(NBN_Connection *connection, uint16_t packet_seq_number)
{
    uint16_t index = packet_seq_number % NBN_MAX_PACKET_ENTRIES;

    return connection->packet_recv_seq_buffer[index] == packet_seq_number;
}

static int send_packet(NBN_Connection *connection, NBN_Packet *packet)
{
    connection->stats.sent_packets++;

#ifdef NBN_DEBUG
    update_packet_loss_ratio(connection);

    if (RAND_RATIO < connection->debug_settings.current_packet_loss_ratio)
    {
        log_trace("Send packet %d (DEBUG DROPPED)", packet->header.seq_number);

        return 0;
    }

    NBN_Packet *dup_packet = malloc(sizeof(NBN_Packet));

    memcpy(dup_packet, packet, sizeof(NBN_Packet));
#endif /* NBN_DEBUG */

    log_trace("Send packet %d (messages count: %d)", packet->header.seq_number, packet->header.messages_count);

#ifdef NBN_GAME_SERVER

#ifdef NBN_DEBUG
    NBN_PacketSimulator_EnqueuePacket(
        game_server.endpoint.packet_simulator, dup_packet, connection->id, connection->debug_settings);
#else
    return NBN_Driver_GServ_SendPacketTo(packet, connection->id);
#endif /* NBN_DEBUG */

#endif /* NBN_GAME_SERVER */

#ifdef NBN_GAME_CLIENT

#ifdef NBN_DEBUG
    NBN_PacketSimulator_EnqueuePacket(game_client.endpoint.packet_simulator, dup_packet, 0, connection->debug_settings);
#else
    return NBN_Driver_GCli_SendPacket(packet);
#endif /* NBN_DEBUG */

#endif /* NBN_GAME_CLIENT */

#ifdef NBN_DEBUG
    return 0;
#else
    return -1;
#endif
}

static NBN_Message *find_message(NBN_Connection *connection, uint16_t id)
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

void update_connection_average_ping(NBN_Connection *connection, unsigned int ping)
{
    /* exponential smoothing with a factor of 0.5 */
    connection->stats.ping = connection->stats.ping + .5f * (ping - connection->stats.ping);
}

static NBN_Message *read_message_from_stream(NBN_Connection *connection, NBN_ReadStream *r_stream)
{
    NBN_MessageHeader msg_header;

    if (NBN_Message_SerializeHeader(&msg_header, (NBN_Stream *)r_stream) < 0)
    {
        log_error("Failed to read net message header");

        return NULL;
    }

    NBN_Channel *channel = connection->channels[msg_header.channel_id];

    if (channel == NULL)
    {
        log_error("Channel %d does not exist", channel);

        return NULL;
    }

    uint8_t msg_type = msg_header.type;
    NBN_MessageBuilder msg_builder = connection->message_builders[msg_type];

    if (msg_builder == NULL)
    {
        log_error("No message builder is registered for messages of type %d", msg_type);

        return NULL;
    }

    NBN_MessageSerializer msg_serializer = connection->message_serializers[msg_type];

    if (msg_serializer == NULL)
    {
        log_error("No message serializer attached to message of type %d", msg_type);

        return NULL;
    }

    NBN_Message *message = NBN_Message_Create(
        msg_header.type, channel, connection->message_destructors[msg_type], msg_builder());

    message->header.id = msg_header.id;

    if (msg_serializer(message->data, (NBN_Stream *)r_stream) < 0)
    {
        log_error("Failed to read message body");

        return NULL;
    }

    return message;
}

static void destroy_message(void *msg_ptr)
{
    NBN_Message_Destroy((NBN_Message *)msg_ptr, true);
}

#ifdef NBN_DEBUG
static void update_packet_loss_ratio(NBN_Connection *connection)
{
    if (RAND_RATIO > connection->debug_settings.packet_loss_fluctuation_ratio)
        return;

    connection->debug_settings.current_packet_loss_ratio = RAND_RATIO_BETWEEN(
        connection->debug_settings.min_packet_loss_ratio,
        connection->debug_settings.max_packet_loss_ratio);
}
#endif /* NBN_DEBUG */

#pragma endregion /* NBN_Connection */

#pragma region NBN_Channel

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

        if (++channel->chunks_count == chunk->total)
        {
            channel->last_received_chunk_id = -1;

            return true;
        }

        return false;
    }

    /* Clear the chunks buffer */
    for (unsigned int i = 0; i < channel->chunks_count; i++)
    {
        assert(channel->chunks_buffer[i] != NULL);

        NBN_MessageChunk_Destroy(channel->chunks_buffer[i]);

        channel->chunks_buffer[i] = NULL;
    }

    channel->chunks_count = 0;
    channel->last_received_chunk_id = -1;

    if (chunk->id == 0)
        return NBN_Channel_AddChunk(channel, chunk_msg);

    NBN_Message_Destroy(chunk_msg, true);

    return false;
}

NBN_Message *NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *channel, NBN_Connection *connection)
{
    unsigned int size = channel->chunks_count * NBN_MESSAGE_CHUNK_SIZE;
    uint8_t *data = malloc(size);

    for (unsigned int i = 0; i < channel->chunks_count; i++)
    {
        NBN_MessageChunk *chunk = channel->chunks_buffer[i];

        memcpy(data + i * NBN_MESSAGE_CHUNK_SIZE, chunk->data, NBN_MESSAGE_CHUNK_SIZE);

        NBN_MessageChunk_Destroy(chunk);

        channel->chunks_buffer[i] = NULL;
    }

    channel->chunks_count = 0;

    NBN_ReadStream r_stream;

    NBN_ReadStream_Init(&r_stream, data, size);

    NBN_Message *message = read_message_from_stream(connection, &r_stream);

    message->sender = connection;

    free(data);

    return message;
}

/* Unreliable ordered */

static NBN_OutgoingMessagePolicy get_unreliable_ordered_outgoing_message_policy(NBN_Channel *, NBN_Message *);
static bool handle_unreliable_ordered_message_reception(NBN_Channel *, NBN_Message *);
static bool process_received_unreliable_ordered_message(NBN_Channel *, NBN_Message *);

NBN_UnreliableOrderedChannel *NBN_UnreliableOrderedChannel_Create(uint8_t id)
{
    NBN_UnreliableOrderedChannel *channel = malloc(sizeof(NBN_UnreliableOrderedChannel));

    channel->base.id = id;
    channel->base.get_outgoing_message_policy = get_unreliable_ordered_outgoing_message_policy;
    channel->base.handle_message_reception = handle_unreliable_ordered_message_reception;
    channel->base.process_queued_received_message = process_received_unreliable_ordered_message;

    channel->last_received_message_id = 0;

    return channel;
}

static NBN_OutgoingMessagePolicy get_unreliable_ordered_outgoing_message_policy(NBN_Channel *channel, NBN_Message *message)
{
    message->header.id = channel->next_message_id++;

    return NBN_SEND_ONCE;
}

static bool handle_unreliable_ordered_message_reception(NBN_Channel *channel, NBN_Message *message)
{
    NBN_UnreliableOrderedChannel *unreliable_ordered_channel = (NBN_UnreliableOrderedChannel *)channel;

    if (SEQUENCE_NUMBER_GT(message->header.id, unreliable_ordered_channel->last_received_message_id))
    {
        unreliable_ordered_channel->last_received_message_id = message->header.id;

        return true;
    }

    return false;
}

static bool process_received_unreliable_ordered_message(NBN_Channel *channel, NBN_Message *message)
{
    (void)channel;
    (void)message;

    return true;
}

/* Reliable ordered */

static NBN_OutgoingMessagePolicy get_reliable_ordered_outgoing_message_policy(NBN_Channel *, NBN_Message *);
static bool handle_reliable_ordered_message_reception(NBN_Channel *, NBN_Message *);
static bool process_received_reliable_ordered_message(NBN_Channel *, NBN_Message *);
static void on_message_acked(NBN_Channel *, NBN_Message *);

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(uint8_t id)
{
    NBN_ReliableOrderedChannel *channel = malloc(sizeof(NBN_ReliableOrderedChannel));

    channel->base.id = id;
    channel->base.get_outgoing_message_policy = get_reliable_ordered_outgoing_message_policy;
    channel->base.handle_message_reception = handle_reliable_ordered_message_reception;
    channel->base.process_queued_received_message = process_received_reliable_ordered_message;
    channel->base.on_message_acked = on_message_acked;
    channel->base.next_message_id = 0;

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

static NBN_OutgoingMessagePolicy get_reliable_ordered_outgoing_message_policy(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;

    if (message->acked)
        return NBN_DISCARD_MESSAGE;

    if (!message->sent)
    {
        int max_message_id = (reliable_ordered_channel->oldest_unacked_message_id + (NBN_CHANNEL_BUFFER_SIZE - 1)) % 0xFFFF;

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
        return (id1 > id2) ? id1 - id2 : (0xFFFF - id2) + id1;
    else
        return (id2 > id1) ? id2 - id1 : ((0xFFFF - id1) + id2) % 0xFFFF;
}

static bool handle_reliable_ordered_message_reception(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    unsigned int dt = compute_message_id_delta(message->header.id,
            reliable_ordered_channel->most_recent_message_id);

    if (SEQUENCE_NUMBER_GT(message->header.id, reliable_ordered_channel->most_recent_message_id))
    {
        assert(dt < NBN_CHANNEL_BUFFER_SIZE);

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

static bool process_received_reliable_ordered_message(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel; 

    if (message->header.id == reliable_ordered_channel->last_received_message_id)
    {
        reliable_ordered_channel->last_received_message_id++;

        return true;
    }

    return false;
}

static void on_message_acked(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    uint16_t msg_id = message->header.id;

    reliable_ordered_channel->ack_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE] = true;

    /*
        TODO: possible optimization would be to keep track of the most recent acked message id
        and instead of looping over the whole buffer, only loop between the oldest unacked message id and
        the most recent acked one.
    */

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
    NBN_EventQueue *events_queue = malloc(sizeof(NBN_EventQueue));

    events_queue->queue = NBN_List_Create();
    events_queue->last_event_data = NULL;

    return events_queue;
}

void NBN_EventQueue_Destroy(NBN_EventQueue *events_queue)
{
    NBN_List_Destroy(events_queue->queue, true, NULL);
    free(events_queue);
}

void NBN_EventQueue_Enqueue(NBN_EventQueue *events_queue, int type, void *data)
{
    NBN_Event *ev = malloc(sizeof(NBN_Event));

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

    free(ev);

    return ev_type;
}

bool NBN_EventQueue_IsEmpty(NBN_EventQueue *events_queue)
{
    return events_queue->queue->count == 0;
}

#pragma endregion /* NBN_EventQueue */

#pragma region Debug packet simulator

#ifdef NBN_DEBUG

NBN_PacketSimulator *NBN_PacketSimulator_Create(void)
{
    NBN_PacketSimulator *packet_simulator = malloc(sizeof(NBN_PacketSimulator));

    packet_simulator->packets_queue = NBN_List_Create();
    packet_simulator->packets_queue_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    packet_simulator->timer = NBN_Timer_Create();
    packet_simulator->running = false;

    return packet_simulator;
}

void NBN_PacketSimulator_Destroy(NBN_PacketSimulator *packet_simulator)
{
    packet_simulator->running = false;

    NBN_List_Destroy(packet_simulator->packets_queue, true, NULL);
    NBN_Timer_Destroy(packet_simulator->timer);
    free(packet_simulator);
}

void NBN_PacketSimulator_EnqueuePacket(
    NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, uint32_t receiver_id, NBN_ConnectionDebugSettings debug_settings)
{
    pthread_mutex_lock(&packet_simulator->packets_queue_mutex);

    NBN_PacketSimulatorEntry *entry = NBN_PacketSimulator_CreateEntry(packet_simulator, packet, receiver_id);

    /* compute jitter in range [ -jitter, +jitter ] */
    int jitter = (debug_settings.jitter > 0) ? (rand() % (debug_settings.jitter * 2)) - debug_settings.jitter : 0;

    entry->delay_ms = debug_settings.ping + jitter;

    NBN_List_PushBack(packet_simulator->packets_queue, entry);

    pthread_mutex_unlock(&packet_simulator->packets_queue_mutex);
}

void NBN_PacketSimulator_Start(NBN_PacketSimulator *packet_simulator)
{
    NBN_Timer_Start(packet_simulator->timer);
    pthread_create(&packet_simulator->thread, NULL, packet_simulator_routine, packet_simulator);

    packet_simulator->running = true;
}

void NBN_PacketSimulator_Stop(NBN_PacketSimulator *packet_simulator)
{
    packet_simulator->running = false;
    
    pthread_join(packet_simulator->thread, NULL);
}

void NBN_PacketSimulator_UpdateTimer(NBN_PacketSimulator *packet_simulator)
{
    NBN_Timer_Update(packet_simulator->timer);
}

NBN_PacketSimulatorEntry *NBN_PacketSimulator_CreateEntry(NBN_PacketSimulator *packet_simulator,
                                                          NBN_Packet *packet,
                                                          uint32_t receiver_id)
{
    NBN_PacketSimulatorEntry *entry = malloc(sizeof(NBN_PacketSimulatorEntry));

    entry->packet = packet;
    entry->receiver_id = receiver_id;
    entry->delay_ms = 0;
    entry->enqueued_at = packet_simulator->timer->elapsed_ms;

    return entry;
}

void NBN_PacketSimulator_DestroyEntry(NBN_PacketSimulatorEntry *entry)
{
    free(entry->packet);
    free(entry);
}

static void *packet_simulator_routine(void *arg)
{
    NBN_PacketSimulator *packet_simulator = arg;

    while (packet_simulator->running)
    {
        pthread_mutex_lock(&packet_simulator->packets_queue_mutex);

        NBN_PacketSimulator_UpdateTimer(packet_simulator);

        NBN_ListNode *current_node = packet_simulator->packets_queue->head;

        while (current_node)
        {
            NBN_PacketSimulatorEntry *entry = current_node->data;

            current_node = current_node->next;

            if (packet_simulator->timer->elapsed_ms - entry->enqueued_at < entry->delay_ms)
                continue;

            NBN_List_Remove(packet_simulator->packets_queue, entry);

#ifdef NBN_GAME_CLIENT
            NBN_Driver_GCli_SendPacket(entry->packet);
#endif

#ifdef NBN_GAME_SERVER
            NBN_Driver_GServ_SendPacketTo(entry->packet, entry->receiver_id);
#endif

            NBN_PacketSimulator_DestroyEntry(entry);
        }

        pthread_mutex_unlock(&packet_simulator->packets_queue_mutex);
    }

    return NULL;
}

#endif /* NBN_DEBUG */

#pragma endregion /* Debug packet simulator */

#pragma region NBN_Endpoint

static uint32_t build_protocol_id(const char *);
static int process_received_packet(NBN_Endpoint *, NBN_Packet *, NBN_Connection *);

void NBN_Endpoint_Init(NBN_Endpoint *endpoint, const char *protocol_name)
{
    endpoint->protocol_id = build_protocol_id(protocol_name);

    for (int i = 0; i < NBN_MAX_MESSAGE_CHANNELS; i++)
        endpoint->channels[i] = NBN_CHANNEL_UNDEFINED;

    for (int i = 0; i < NBN_MAX_MESSAGE_TYPES; i++)
        endpoint->message_destructors[i] = NULL;

    for (int i = 0; i < NBN_MAX_MESSAGE_TYPES; i++)
        endpoint->message_serializers[i] = NULL;

    endpoint->events_queue = NBN_EventQueue_Create();

    NBN_Endpoint_RegisterMessageBuilder(
        endpoint, (NBN_MessageBuilder)NBN_MessageChunk_Create, NBN_MESSAGE_CHUNK_TYPE);
    NBN_Endpoint_RegisterMessageDestructor(
        endpoint, (NBN_MessageDestructor)NBN_MessageChunk_Destroy, NBN_MESSAGE_CHUNK_TYPE);
    NBN_Endpoint_RegisterMessageSerializer(
        endpoint, (NBN_MessageSerializer)NBN_MessageChunk_Serialize, NBN_MESSAGE_CHUNK_TYPE);

#ifdef NBN_DEBUG
    endpoint->debug_settings = (NBN_ConnectionDebugSettings){0};
    endpoint->debug_settings.packet_loss_fluctuation_ratio = .1f;
    endpoint->packet_simulator = NBN_PacketSimulator_Create();

    NBN_PacketSimulator_Start(endpoint->packet_simulator);
#endif
}

void NBN_Endpoint_Deinit(NBN_Endpoint *endpoint)
{
    NBN_EventQueue_Destroy(endpoint->events_queue);

#ifdef NBN_DEBUG
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

NBN_Connection *NBN_Endpoint_CreateConnection(NBN_Endpoint *endpoint, int id, bool accepted)
{
    NBN_Connection *connection = NBN_Connection_Create(
        id,
        endpoint->protocol_id,
        accepted,
        endpoint->message_builders,
        endpoint->message_destructors,
        endpoint->message_serializers);

    for (int chan_id = 0; chan_id < NBN_MAX_MESSAGE_CHANNELS; chan_id++)
    {
        NBN_ChannelType channel_type = endpoint->channels[chan_id];

        if (channel_type != NBN_CHANNEL_UNDEFINED)
            NBN_Connection_CreateChannel(connection, channel_type, chan_id);
    }

    return connection;
}

static uint32_t build_protocol_id(const char *protocol_name)
{
    uint32_t protocol_id = 2166136261;

    for (unsigned int i = 0; i < strlen(protocol_name); i++)
    {
        protocol_id *= 16777619;
        protocol_id ^= protocol_name[i];
    }

    return protocol_id;
}

static int process_received_packet(NBN_Endpoint *endpoint, NBN_Packet *packet, NBN_Connection *connection)
{
    (void)endpoint;

    log_trace("Received packet %d (conn id: %d, ack: %d, messages count: %d)", packet->header.seq_number,
        connection->id, packet->header.ack, packet->header.messages_count);

#ifdef NBN_DEBUG
    NBN_Packet dup_packets[10];
    bool dup = RAND_RATIO < connection->debug_settings.packet_duplication_ratio;
    int dup_count;

    if (dup)
    {
        dup_count = rand() % 10 + 1;

        for (int i = 0; i < dup_count; i++)
            memcpy(&dup_packets[i], packet, sizeof(NBN_Packet));
    }
#endif /* NBN_DEBUG */

    if (NBN_Connection_ProcessReceivedPacket(connection, packet) < 0)
        return -1;

#ifdef NBN_DEBUG
    if (dup)
    {
        for (int i = 0; i < dup_count; i++)
        {
            log_trace("Received packet %d (DEBUG DUPLICATE %d)", dup_packets[i].header.seq_number, i + 1);

            if (NBN_Connection_ProcessReceivedPacket(connection, &dup_packets[i]) < 0)
                return -1;
        }
    }
#endif /* NBN_DEBUG */

    connection->last_recv_packet_time = connection->timer->elapsed_ms;

    return 0;
}

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

#ifdef NBN_GAME_CLIENT

NBN_GameClient game_client;

static int last_event_type = NBN_NO_EVENT;
static uint8_t last_received_message_type;
static void *last_received_message_data = NULL;

static void on_server_message_received(NBN_Message *, NBN_Connection *);
static void handle_dequeued_event_data(int);
static void handle_server_message_received_event_data(void);
static void release_last_event_data(void);
static void release_message_received_event_data(void);

void NBN_GameClient_Init(const char *protocol_name)
{
    NBN_Endpoint_Init(&game_client.endpoint, protocol_name);
}

int NBN_GameClient_Start(const char *host, uint16_t port)
{
    if (NBN_Driver_GCli_Start(game_client.endpoint.protocol_id, host, port) < 0)
        return -1;

    log_info("Started");

    return 0;
}

void NBN_GameClient_Stop(void)
{
    NBN_Driver_GCli_Stop();
    NBN_Endpoint_Deinit(&game_client.endpoint);
    NBN_Connection_Destroy(game_client.server_connection);

    log_info("Stopped");
}

NBN_GameClientEvent NBN_GameClient_Poll(void)
{
    release_last_event_data();

    if (NBN_EventQueue_IsEmpty(game_client.endpoint.events_queue)) 
    {
        NBN_Connection_UpdateTimer(game_client.server_connection);

        if (NBN_Connection_IsStale(game_client.server_connection))
        {
            log_trace("Server connection is stale. Disconnected.");

            NBN_EventQueue_Enqueue(game_client.endpoint.events_queue, NBN_DISCONNECTED, NULL);
        }
        else
        {
            if (NBN_Driver_GCli_RecvPackets() < 0)
                return -1;

            NBN_Message *msg;

            assert(dequeue_current_node == NULL);

            while ((msg = NBN_Connection_DequeueReceivedMessage(game_client.server_connection)))
                on_server_message_received(msg, game_client.server_connection);
        }
    }

    int ev_type = NBN_EventQueue_Dequeue(game_client.endpoint.events_queue);
    last_event_type = ev_type;

    handle_dequeued_event_data(ev_type);

    return ev_type;
}

int NBN_GameClient_Flush(void)
{
    return NBN_Connection_FlushSendQueue(game_client.server_connection);
}

void *NBN_GameClient_CreateMessage(uint8_t type, uint8_t channel)
{
    return NBN_Connection_CreateOutgoingMessage(game_client.server_connection, type, channel);
}

int NBN_GameClient_SendMessage(void)
{
    return NBN_Connection_EnqueueOutgoingMessage(game_client.server_connection);
}

NBN_Connection *NBN_GameClient_CreateServerConnection(void)
{
    NBN_Connection *server_connection = NBN_Endpoint_CreateConnection(&game_client.endpoint, 0, true);

#ifdef NBN_DEBUG
    server_connection->debug_settings = game_client.endpoint.debug_settings;
    server_connection->on_message_added_to_recv_queue = game_client.endpoint.on_message_added_to_recv_queue;
#endif

    game_client.server_connection = server_connection;

    return server_connection;
}

int NBN_GameClient_ReadReceivedMessage(NBN_MessageInfo *message_info)
{
    if (last_received_message_data == NULL)
    {
        log_error("No message message to read, this should not happen. \
        Are you sure you are properly listening for 'NBN_MESSAGE_RECEIVED' events ?");

        return -1;
    }

    message_info->type = last_received_message_type;
    message_info->data = last_received_message_data;
    message_info->sender = NULL;

    return 0;
}

NBN_ConnectionStats NBN_GameClient_GetStats(void)
{
    return game_client.server_connection->stats;
}

void NBN_GameClient_OnConnected(void)
{
    NBN_EventQueue_Enqueue(game_client.endpoint.events_queue, NBN_CONNECTED, NULL);
}

void NBN_GameClient_OnDisconnected(void)
{
    NBN_EventQueue_Enqueue(game_client.endpoint.events_queue, NBN_DISCONNECTED, NULL);
}

int NBN_GameClient_OnPacketReceived(NBN_Packet *packet)
{
    return process_received_packet(&game_client.endpoint, packet, game_client.server_connection);
}

#ifdef NBN_DEBUG
void NBN_GameClient_Debug_SetMinPacketLossRatio(float ratio)
{
    game_client.endpoint.debug_settings.min_packet_loss_ratio = ratio;
}

void NBN_GameClient_Debug_SetMaxPacketLossRatio(float ratio)
{
    game_client.endpoint.debug_settings.max_packet_loss_ratio = ratio;
}

void NBN_GameClient_Debug_SetPacketDuplicationRatio(float ratio)
{
    game_client.endpoint.debug_settings.packet_duplication_ratio = ratio;
}

void NBN_GameClient_Debug_SetPing(unsigned int ping)
{
    game_client.endpoint.debug_settings.ping = ping;
}

void NBN_GameClient_Debug_SetJitter(unsigned int jitter)
{
    game_client.endpoint.debug_settings.jitter = jitter;
}

void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
    case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
        game_client.endpoint.on_message_added_to_recv_queue = cb;
        break;
    }
}

NBN_ConnectionDebugInfo NBN_GameClient_Debug_GetInfo(void)
{
    return game_client.server_connection->debug_info;
}

#endif /* NBN_DEBUG */

static void on_server_message_received(NBN_Message *message, NBN_Connection *server_connection)
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

static void handle_dequeued_event_data(int event_type)
{
    switch (event_type)
    {
    case NBN_MESSAGE_RECEIVED:
        handle_server_message_received_event_data();
        break;
    
    default:
        break;
    }
}

static void handle_server_message_received_event_data(void)
{
    NBN_Message *message = game_client.endpoint.events_queue->last_event_data;

    last_received_message_type = message->header.type;
    last_received_message_data = message->data;

    NBN_Message_Destroy(message, false);
}

static void release_last_event_data(void)
{
    switch (last_event_type)
    {
    case NBN_MESSAGE_RECEIVED:
        release_message_received_event_data();
        break;

    default:
        break;
    }
}

static void release_message_received_event_data(void)
{
    assert(last_received_message_data != NULL);

    NBN_MessageDestructor destructor = game_client.endpoint.message_destructors[last_received_message_type];

    if (destructor)
        destructor(last_received_message_data);
    else
        free(last_received_message_data);

    last_received_message_data = NULL;
}

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* NBN_GameClient */

#pragma region NBN_GameServer

#ifdef NBN_GAME_SERVER

NBN_GameServer game_server;

static int last_event_type = NBN_NO_EVENT;
static NBN_Connection *last_event_client;
static uint8_t last_received_message_type;
static void *last_received_message_data = NULL;

static void on_client_message_received(NBN_Message *, NBN_Connection *);
static void update_client_connections_timers(void);
static void close_stale_client_connections(void);
static NBN_Connection *find_client_by_id(uint32_t);
static void handle_dequeued_event_data(int);
static void handle_client_connected_event_data(void);
static void handle_client_disconnected_event_data(void);
static void handle_client_connection_request_event_data(void);
static void handle_client_message_received_event_data(void);
static void release_last_event_data(void);
static void release_client_disconnected_event_data(void);
static void release_client_message_received_event_data(void);

void NBN_GameServer_Init(const char *protocol_name)
{
    NBN_Endpoint_Init(&game_server.endpoint, protocol_name);

    game_server.clients = NBN_List_Create();
    game_server.auto_accept_clients = true;
}

int NBN_GameServer_Start(uint16_t port)
{
    if (NBN_Driver_GServ_Start(game_server.endpoint.protocol_id, port) < 0)
        return -1;

    log_info("Started");

    return 0;
}

void NBN_GameServer_Stop(void)
{
    NBN_Driver_GServ_Stop();
    NBN_Endpoint_Deinit(&game_server.endpoint);
    NBN_List_Destroy(game_server.clients, true, (NBN_List_FreeItemFunc)NBN_Connection_Destroy);

    log_info("Stopped");
}

NBN_GameServerEvent NBN_GameServer_Poll(void)
{
    release_last_event_data();

    if (NBN_EventQueue_IsEmpty(game_server.endpoint.events_queue)) 
    {
        update_client_connections_timers();
        close_stale_client_connections();

        if (NBN_Driver_GServ_RecvPackets() < 0)
            return -1;

        NBN_ListNode *current_node = game_server.clients->head;

        while (current_node)
        {
            NBN_Connection *client = current_node->data;
            NBN_Message *msg;

            assert(dequeue_current_node == NULL);

            while ((msg = NBN_Connection_DequeueReceivedMessage(client)))
                on_client_message_received(msg, client);

            current_node = current_node->next;
        }
    }

    int ev_type = NBN_EventQueue_Dequeue(game_server.endpoint.events_queue);
    last_event_type = ev_type;

    handle_dequeued_event_data(ev_type);

    return ev_type;
}

int NBN_GameServer_Flush(void)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection *client = current_node->data;

        if (NBN_Connection_FlushSendQueue(client) < 0)
            return -1;

        current_node = current_node->next;
    }

    return 0;
}

NBN_Connection *NBN_GameServer_CreateClientConnection(uint32_t id)
{
    NBN_Connection *client = NBN_Endpoint_CreateConnection(&game_server.endpoint, id, game_server.auto_accept_clients);

#ifdef NBN_DEBUG
    client->debug_settings = game_server.endpoint.debug_settings;
    client->on_message_added_to_recv_queue = game_server.endpoint.on_message_added_to_recv_queue;
#endif

    NBN_List_PushBack(game_server.clients, client);

    return client;
}

void NBN_GameServer_AcceptClient(NBN_Connection *client)
{
    assert(NBN_List_Includes(game_server.clients, client));

    client->accepted = true;

    NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_CLIENT_CONNECTED, client);
}

void NBN_GameServer_RejectClient(NBN_Connection *client)
{
    assert(NBN_List_Includes(game_server.clients, client));

    // TODO:
}

void NBN_GameServer_CloseClient(NBN_Connection *client)
{
    assert(NBN_List_Includes(game_server.clients, client));

    NBN_Driver_GServ_CloseClient(client->id);
    NBN_List_Remove(game_server.clients, client);
    NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_CLIENT_DISCONNECTED, client);
}

void *NBN_GameServer_CreateMessage(uint8_t type, uint8_t channel, NBN_Connection *receiver)
{
    return NBN_Connection_CreateOutgoingMessage(receiver, type, channel);
}

int NBN_GameServer_SendMessageTo(NBN_Connection *client)
{
    return NBN_Connection_EnqueueOutgoingMessage(client);
}

NBN_Connection *NBN_GameServer_GetLastEventClient(void)
{
    return last_event_client;
}

void *NBN_GameServer_ReadAuthRequest(void)
{
    // TODO:
    assert(false);
    return NULL;
}

int NBN_GameServer_ReadReceivedMessage(NBN_MessageInfo *message_info)
{
    if (last_received_message_data == NULL)
    {
        log_error("No message message to read, this should not happen. \
        Are you sure you are properly listening for 'NBN_CLIENT_MESSAGE_RECEIVED' events ?");

        return -1;
    }

    message_info->type = last_received_message_type;
    message_info->data = last_received_message_data;
    message_info->sender = last_event_client;

    return 0;
}

void NBN_GameServer_AttachDataToClientConnection(uint32_t client_id, void *data)
{
    NBN_Connection *client = find_client_by_id(client_id);

    assert(client != NULL);

    client->user_data = data;
}

NBN_ConnectionStats NBN_GameServer_GetDisconnectedClientStats(void)
{
    return last_event_client->stats;
}

NBN_ConnectionStats *NBN_GameServer_GetClientStats(uint32_t client_id)
{
    NBN_Connection *client = find_client_by_id(client_id);

    if (client == NULL)
        return NULL;

    return &client->stats;
}

void NBN_GameServer_OnClientConnected(NBN_Connection *client)
{
    assert(NBN_List_Includes(game_server.clients, client));

    if (client->accepted)
        NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_CLIENT_CONNECTED, client);
}

int NBN_GameServer_OnPacketReceived(NBN_Packet *packet, NBN_Connection *sender_client)
{
    assert(NBN_List_Includes(game_server.clients, sender_client));

    return process_received_packet(&game_server.endpoint, packet, sender_client);
}

#ifdef NBN_DEBUG
NBN_ConnectionDebugInfo NBN_GameServer_Debug_GetDisconnectedClientInfo(void)
{
    return last_event_client->debug_info;
}

void NBN_GameServer_Debug_SetMinPacketLossRatio(float ratio)
{
    game_server.endpoint.debug_settings.min_packet_loss_ratio = ratio;
}

void NBN_GameServer_Debug_SetMaxPacketLossRatio(float ratio)
{
    game_server.endpoint.debug_settings.max_packet_loss_ratio = ratio;
}

void NBN_GameServer_Debug_SetPacketDuplicationRatio(float ratio)
{
    game_server.endpoint.debug_settings.packet_duplication_ratio = ratio;
}

void NBN_GameServer_Debug_SetPing(unsigned int ping)
{
    game_server.endpoint.debug_settings.ping = ping;
}

void NBN_GameServer_Debug_SetJitter(unsigned int jitter)
{
    game_server.endpoint.debug_settings.jitter = jitter;
}

void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
    case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
        game_server.endpoint.on_message_added_to_recv_queue = cb;
        break;
    }
}

NBN_ConnectionDebugInfo *NBN_GameServer_Debug_GetClientInfo(uint32_t client_id)
{
    NBN_Connection *client = find_client_by_id(client_id);

    if (client == NULL)
        return NULL;

    return &client->debug_info;
}

#endif /* NBN_DEBUG */

static void on_client_message_received(NBN_Message *message, NBN_Connection *client)
{
    assert(NBN_List_Includes(game_server.clients, client));

    if (client->accepted)
    {
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
    else
    {
        if (message->header.type == game_server.auth_message_type)
            NBN_EventQueue_Enqueue(game_server.endpoint.events_queue, NBN_CLIENT_CONNECTION_REQUESTED, message);
        else
            NBN_Message_Destroy(message, true);
    }
}

static void update_client_connections_timers(void)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection *client_connection = current_node->data;

        NBN_Connection_UpdateTimer(client_connection);

        current_node = current_node->next;
    }
}

static void close_stale_client_connections(void)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection *client = current_node->data;

        current_node = current_node->next;

        if (NBN_Connection_IsStale(client))
        {
            log_trace("Client %d connection is stale. Closing connection.", client->id);

            NBN_GameServer_CloseClient(client);
        }
    }
}

NBN_Connection *find_client_by_id(uint32_t id)
{
    NBN_ListNode *current_node = game_server.clients->head;

    while (current_node)
    {
        NBN_Connection *cli = current_node->data;

        if (cli->id == id)
            return cli;

        current_node = current_node->next;
    }

    return NULL;
}

static void handle_dequeued_event_data(int event_type)
{
    switch (event_type)
    {
    case NBN_CLIENT_CONNECTED:
        handle_client_connected_event_data();
        break;

    case NBN_CLIENT_DISCONNECTED:
        handle_client_disconnected_event_data();
        break;

    case NBN_CLIENT_CONNECTION_REQUESTED:
        handle_client_connection_request_event_data();
        break;

    case NBN_CLIENT_MESSAGE_RECEIVED:
        handle_client_message_received_event_data();
        break;

    default:
        break;
    }
}

static void handle_client_connected_event_data(void)
{
    last_event_client = game_server.endpoint.events_queue->last_event_data;
}

static void handle_client_disconnected_event_data(void)
{
    last_event_client = game_server.endpoint.events_queue->last_event_data;
}

static void handle_client_connection_request_event_data(void)
{
    NBN_Message *message = game_server.endpoint.events_queue->last_event_data;

    last_event_client = message->sender;
    last_received_message_data = message->data;

    NBN_Message_Destroy(message, false);
}

static void handle_client_message_received_event_data(void)
{
    NBN_Message *message = game_server.endpoint.events_queue->last_event_data;

    last_event_client = message->sender;
    last_received_message_type = message->header.type;
    last_received_message_data = message->data;

    NBN_Message_Destroy(message, false);
}

static void release_last_event_data(void)
{
    switch (last_event_type)
    {
    case NBN_CLIENT_DISCONNECTED:
        release_client_disconnected_event_data();
        break;

    case NBN_CLIENT_MESSAGE_RECEIVED:
        release_client_message_received_event_data();
        break;

    default:
        break;
    }
}

static void release_client_disconnected_event_data(void)
{
    assert(last_event_client != NULL);

    NBN_Connection_Destroy(last_event_client);

    last_event_client = NULL;
}

static void release_client_message_received_event_data(void)
{
    assert(last_received_message_data != NULL);

    NBN_MessageDestructor destructor = game_server.endpoint.message_destructors[last_received_message_type];

    if (destructor)
        destructor(last_received_message_data);
    else
        free(last_received_message_data);

    last_received_message_data = NULL;
}

#endif /* NBN_GAME_SERVER */

#pragma endregion /* NBN_GameServer */

#pragma region Logging

static struct
{
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
} L;

static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

static void lock(void)
{
    if (L.lock)
    {
        L.lock(L.udata, 1);
    }
}

static void unlock(void)
{
    if (L.lock)
    {
        L.lock(L.udata, 0);
    }
}

void log_set_udata(void *udata)
{
    L.udata = udata;
}

void log_set_lock(log_LockFn fn)
{
    L.lock = fn;
}

void log_set_fp(FILE *fp)
{
    L.fp = fp;
}

void log_set_level(int level)
{
    L.level = level;
}

void log_set_quiet(int enable)
{
    L.quiet = enable ? 1 : 0;
}

void log_log(int level, const char *file, int line, const char *fmt, ...)
{
    if (level < L.level)
    {
        return;
    }

    /* Acquire lock */
    lock();

    /* Get current time */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    /* Log to stderr */
    if (!L.quiet)
    {
        va_list args;
        char buf[16];
        buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
#ifdef LOG_USE_COLOR
        fprintf(
            stderr, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
            buf, level_colors[level], level_names[level], file, line);
#else
        fprintf(stderr, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
#endif
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    /* Log to file */
    if (L.fp)
    {
        va_list args;
        char buf[32];
        buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
        fprintf(L.fp, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
        va_start(args, fmt);
        vfprintf(L.fp, fmt, args);
        va_end(args);
        fprintf(L.fp, "\n");
        fflush(L.fp);
    }

    /* Release lock */
    unlock();
}

#pragma endregion /* Logging */

#endif /* NBNET_IMPL */

#pragma endregion /* Implementations */
