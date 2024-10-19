/*

   Copyright (C) 2024 BIAGINI Nathan

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
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>

#define NBNET_WINDOWS

#endif

#ifndef NBNET_WINDOWS

#include <sys/time.h>
#include <time.h>

#endif

#ifndef NBN_Allocator
#define NBN_Allocator malloc
#endif

#ifndef NBN_Reallocator
#define NBN_Reallocator realloc
#endif

#ifndef NBN_Deallocator
#define NBN_Deallocator free
#endif

#pragma region Declarations

#ifndef NBN_Abort
#define NBN_Abort abort
#endif

#ifndef NBN_Assert
#define NBN_Assert(cond) \
{ \
    if (!(cond)) { \
        NBN_LogError(#cond); \
        NBN_Abort(); \
    } \
}
#endif

#ifndef NBN_LogError
#define NBN_LogError(...) do {} while (0)
#endif

#ifndef NBN_LogInfo
#define NBN_LogInfo(...) do {} while (0)
#endif

#ifndef NBN_LogDebug
#define NBN_LogDebug(...) do {} while (0)
#endif

#ifndef NBN_LogWarning
#define NBN_LogWarning(...) do {} while (0)
#endif

#ifndef NBN_LogTrace
#define NBN_LogTrace(...) do {} while (0)
#endif

#define NBN_ERROR -1

typedef struct NBN_Endpoint NBN_Endpoint;
typedef struct NBN_Connection NBN_Connection;
typedef struct NBN_Channel NBN_Channel;
typedef struct NBN_Driver NBN_Driver;
typedef uint32_t NBN_ConnectionHandle;

#pragma region NBN_ConnectionVector

typedef struct NBN_ConnectionVector
{
    NBN_Connection **connections;
    unsigned int count;
    unsigned int capacity;
} NBN_ConnectionVector;

#pragma endregion // NBN_ConnectionVector

#pragma region NBN_ConnectionTable

typedef struct NBN_ConnectionTable
{
    NBN_Connection **connections;
    unsigned int capacity;
    unsigned int count;
    float load_factor;
} NBN_ConnectionTable;

#pragma endregion // NBN_ConnectionTable

#pragma region Memory management

enum
{
    NBN_MEM_MESSAGE_CHUNK,
    NBN_MEM_CONNECTION,

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_MEM_PACKET_SIMULATOR_ENTRY
#endif
};

typedef struct NBN_MemPoolFreeBlock
{
    struct NBN_MemPoolFreeBlock *next;
} NBN_MemPoolFreeBlock;

typedef struct NBN_MemPool
{
    uint8_t **blocks;
    size_t block_size;
    unsigned int block_count;
    unsigned int block_idx;
    NBN_MemPoolFreeBlock *free;
} NBN_MemPool;

typedef struct NBN_MemoryManager
{
#ifdef NBN_DISABLE_MEMORY_POOLING
    size_t mem_sizes[16];
#else
    NBN_MemPool mem_pools[16];
#endif /* NBN_DISABLE_MEMORY_POOLING */
} NBN_MemoryManager;

extern NBN_MemoryManager nbn_mem_manager;

#pragma endregion /* Memory management */

#pragma region Serialization

#define B_MASK(n) (1u << (n))
#define B_SET(mask, n) (mask |= B_MASK(n))
#define B_UNSET(mask, n) (mask &= ~B_MASK(n))
#define B_IS_SET(mask, n) ((B_MASK(n) & mask) == B_MASK(n))
#define B_IS_UNSET(mask, n) ((B_MASK(n) & mask) == 0)

typedef struct NBN_Writer
{
    uint8_t *buffer;
    unsigned int length;
    unsigned int position;
} NBN_Writer;

typedef struct NBN_Reader
{
    uint8_t *buffer;
    unsigned int length;
    unsigned int position;
} NBN_Reader;

void NBN_Writer_Init(NBN_Writer *writer, uint8_t *buffer, unsigned int length);
void NBN_Writer_WriteInt32(NBN_Writer *writer, int32_t value);
void NBN_Writer_WriteUInt16(NBN_Writer *writer, uint16_t value);
void NBN_Writer_WriteUInt32(NBN_Writer *writer, uint32_t value);
void NBN_Writer_WriteUInt8(NBN_Writer *writer, uint8_t value);
void NBN_Writer_WriteBytes(NBN_Writer *writer, uint8_t *bytes, unsigned int length);

void NBN_Reader_Init(NBN_Reader *reader, uint8_t *buffer, unsigned int length);
int NBN_Reader_ReadInt32(NBN_Reader *reader, int32_t *value);
int NBN_Reader_ReadUInt16(NBN_Reader *reader, uint16_t *value);
int NBN_Reader_ReadUInt32(NBN_Reader *reader, uint32_t *value);
int NBN_Reader_ReadUInt8(NBN_Reader *reader, uint8_t *value);
int NBN_Reader_ReadBytes(NBN_Reader *reader, uint8_t *bytes, unsigned int length);

#pragma endregion /* Serialization */

#pragma region NBN_Message

#define NBN_MAX_CHANNELS 8
#define NBN_LIBRARY_RESERVED_CHANNELS 3
#define NBN_MAX_MESSAGE_TYPES 255 /* Maximum value of uint8_t, see message header */
#define NBN_MESSAGE_RESEND_DELAY 0.1 /* Number of seconds before a message is resent (reliable messages redundancy) */
#define NBN_MESSAGE_HEADER_SIZE 6 /* See NBN_MessageHeader struct */

// IMPORTANT: make sure you update NBN_MESSAGE_HEADER_SIZE if you modify NBN_MessageHeader struct
typedef struct NBN_MessageHeader
{
    uint16_t id;
    uint16_t length;
    uint8_t type;
    uint8_t channel_id;
} NBN_MessageHeader;

typedef struct NBN_Message
{
    NBN_MessageHeader header;
    NBN_Connection *sender;
    unsigned int ref_count;
    uint8_t *data;
} NBN_Message;

/**
 * Information about a received message.
 */
typedef struct NBN_MessageInfo
{
    /** User defined message type */
    uint8_t type;

    /** Channel the message was received on */
    uint8_t channel_id;

    /** Message data */
    uint8_t *data;

    /** Length of message data in bytes */
    uint16_t length;

    /**
     * The message's sender.
     * 
     * On the client side, it will always be 0 (all received messages come from the game server).
    */
    NBN_ConnectionHandle sender;
} NBN_MessageInfo;

#pragma endregion /* NBN_Message */

#pragma region NBN_Packet

/*  
 * Maximum allowed packet size (including header) in bytes.
 * The 1400 value has been chosen based on this statement:
 * 
 * With the IPv4 header being 20 bytes and the UDP header being 8 bytes, the payload
 * of a UDP packet should be no larger than 1500 - 20 - 8 = 1472 bytes to avoid fragmentation.
 */
#define NBN_PACKET_MAX_SIZE 1400
#define NBN_MAX_MESSAGES_PER_PACKET 255 /* Maximum value of uint8_t, see packet header */

#define NBN_PACKET_HEADER_SIZE 13

/* Maximum size of packet's data (NBN_PACKET_MAX_DATA_SIZE + NBN_PACKET_HEADER_SIZE = NBN_PACKET_MAX_SIZE) */
#define NBN_PACKET_MAX_DATA_SIZE (NBN_PACKET_MAX_SIZE - NBN_PACKET_HEADER_SIZE)

enum
{
    NBN_PACKET_WRITE_ERROR = -1,
    NBN_PACKET_WRITE_OK,
    NBN_PACKET_WRITE_NO_SPACE,
};

typedef enum NBN_PacketMode
{
    NBN_PACKET_MODE_WRITE = 1,
    NBN_PACKET_MODE_READ
} NBN_PacketMode;

// IMPORTANT: don't forget to update NBN_PACKET_HEADER_SIZE after modifying this structure
typedef struct NBN_PacketHeader
{
    uint32_t protocol_id;
    uint32_t ack_bits;
    uint16_t seq_number;
    uint16_t ack;
    uint8_t messages_count; 
} NBN_PacketHeader;

typedef struct NBN_Packet
{
    NBN_PacketHeader header;
    NBN_PacketMode mode;
    struct NBN_Connection *sender; /* not serialized, fill by the network driver upon reception */
    uint8_t buffer[NBN_PACKET_MAX_SIZE];
    unsigned int size; /* in bytes */
    bool sealed;
} NBN_Packet;

void NBN_Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
int NBN_Packet_WriteMessage(NBN_Packet *, NBN_Message *);
int NBN_Packet_Seal(NBN_Packet *, NBN_Connection *);
int NBN_Packet_InitRead(NBN_Packet *, uint32_t, unsigned int);

#pragma endregion /* NBN_Packet */

#pragma region Library reserved messages

#define NBN_MESSAGE_CHUNK_TYPE (NBN_MAX_MESSAGE_TYPES)
#define NBN_CLIENT_CLOSED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 1)
#define NBN_CLIENT_ACCEPTED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 2)
#define NBN_DISCONNECTION_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 3)
#define NBN_CONNECTION_REQUEST_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 4)

/*
 * Chunk max size is the number of bytes of data a packet can hold minus the size of a message header minus 2 bytes
 * to hold the chunk id and total number of chunks.
 */
#define NBN_MESSAGE_CHUNK_SIZE (NBN_PACKET_MAX_DATA_SIZE - NBN_MESSAGE_HEADER_SIZE - 2)

#define NBN_SERVER_DATA_MAX_SIZE 1024
#define NBN_CONNECTION_DATA_MAX_SIZE 512

#pragma endregion /* Library reserved messages */

#pragma region NBN_Channel

#define NBN_CHANNEL_BUFFER_SIZE 1024
#define NBN_CHANNEL_CHUNKS_BUFFER_SIZE 255
#define NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE 2048
#define NBN_CHANNEL_OUTGOING_MESSAGE_POOL_SIZE 512

/* IMPORTANT: if you add a library reserved channel below, make sure to update NBN_LIBRARY_RESERVED_CHANNELS */

/* Library reserved unreliable ordered channel */
#define NBN_CHANNEL_RESERVED_UNRELIABLE (NBN_MAX_CHANNELS - 1)

/* Library reserved reliable ordered channel */
#define NBN_CHANNEL_RESERVED_RELIABLE (NBN_MAX_CHANNELS - 2)

/* Library reserved messages channel */
#define NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES (NBN_MAX_CHANNELS - 3)

typedef NBN_Channel *(*NBN_ChannelBuilder)(void);
typedef void (*NBN_ChannelDestructor)(NBN_Channel *);

typedef struct NBN_MessageSlot
{
    NBN_Message message;
    double last_send_time;
    bool free;
} NBN_MessageSlot;

struct NBN_Channel
{
    uint8_t id;
    // uint8_t *write_chunk_buffer;
    uint16_t next_outgoing_message_id;
    uint16_t next_recv_message_id;
    unsigned int next_outgoing_message_pool_slot;
    unsigned int outgoing_message_count;
    unsigned int chunk_count;
    unsigned int write_chunk_buffer_size;
    unsigned int read_chunk_buffer_size;
    unsigned int next_outgoing_chunked_message;
    int last_received_chunk_id;
    // uint8_t *read_chunk_buffer;
    NBN_ChannelDestructor destructor;
    NBN_Connection *connection;
    NBN_MessageSlot outgoing_message_slot_buffer[NBN_CHANNEL_BUFFER_SIZE];
    NBN_MessageSlot recved_message_slot_buffer[NBN_CHANNEL_BUFFER_SIZE];
    // NBN_MessageChunk *recv_chunk_buffer[NBN_CHANNEL_CHUNKS_BUFFER_SIZE];
    NBN_Message outgoing_message_pool[NBN_CHANNEL_OUTGOING_MESSAGE_POOL_SIZE];

    bool (*AddReceivedMessage)(NBN_Channel *, NBN_Message *);
    bool (*AddOutgoingMessage)(NBN_Channel *, NBN_Message *);
    NBN_Message *(*GetNextRecvedMessage)(NBN_Channel *);
    NBN_Message *(*GetNextOutgoingMessage)(NBN_Channel *, double);
    int (*OnOutgoingMessageAcked)(NBN_Channel *, uint16_t);
    int (*OnOutgoingMessageSent)(NBN_Channel *, NBN_Message *);
};

void NBN_Channel_Destroy(NBN_Channel *);
// bool NBN_Channel_AddChunk(NBN_Channel *, NBN_Message *);
int NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *, NBN_Connection *, NBN_Message *);
/*void NBN_Channel_ResizeWriteChunkBuffer(NBN_Channel *, unsigned int);*/
/*void NBN_Channel_ResizeReadChunkBuffer(NBN_Channel *, unsigned int);*/
void NBN_Channel_UpdateMessageLastSendTime(NBN_Channel *, NBN_Message *, double);

/*
   Unreliable ordered

   Guarantee that messages will be received in order, does not however guarantee that all message will be received when
   packets get lost. This is meant to be used for time critical messages when it does not matter that much if they
   end up getting lost. A good example would be game snaphosts when any newly received snapshot is more up to date
   than the previous one.
   */
typedef struct NBN_UnreliableOrderedChannel
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
typedef struct NBN_ReliableOrderedChannel
{
    NBN_Channel base;
    uint16_t oldest_unacked_message_id;
    uint16_t most_recent_message_id;
    bool ack_buffer[NBN_CHANNEL_BUFFER_SIZE];
} NBN_ReliableOrderedChannel;

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(void);

#pragma endregion /* NBN_Channel */

#pragma region NBN_Connection

#define NBN_MAX_PACKET_ENTRIES 1024

/* Maximum number of packets that can be sent in a single flush
 *
 * IMPORTANT: do not increase this, it will break packet acks
*/
#define NBN_CONNECTION_MAX_SENT_PACKET_COUNT 16

/* Number of seconds before the connection is considered stale and get closed */
#define NBN_CONNECTION_STALE_TIME_THRESHOLD 3

typedef struct NBN_MessageEntry
{
    uint16_t id;
    uint8_t channel_id;
} NBN_MessageEntry;

typedef struct NBN_PacketEntry
{
    bool acked;
    bool flagged_as_lost;
    unsigned int messages_count;
    double send_time;
    NBN_MessageEntry messages[NBN_MAX_MESSAGES_PER_PACKET];
} NBN_PacketEntry;

typedef struct NBN_ConnectionStats
{
    double ping;
    unsigned int total_lost_packets;
    float packet_loss;
    float upload_bandwidth;
    float download_bandwidth;
} NBN_ConnectionStats;

#ifdef NBN_DEBUG

typedef enum NBN_ConnectionDebugCallback
{
    NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE
} NBN_ConnectionDebugCallback;

#endif /* NBN_DEBUG */

struct NBN_Connection
{
    uint32_t id;
    double last_recv_packet_time; /* Used to detect stale connections */
    double last_flush_time; /* Last time the send queue was flushed */
    double last_read_packets_time; /* Last time packets were read from the network driver */
    unsigned int downloaded_bytes; /* Keep track of bytes read from the socket (used for download bandwith calculation) */
    int vector_pos; /* Position of the connection in the connections vector */
    uint8_t is_accepted     : 1;
    uint8_t is_stale        : 1;
    uint8_t is_closed       : 1;
    struct NBN_Endpoint *endpoint;
    NBN_Driver *driver; /* Network driver used for that connection */
    NBN_Channel *channels[NBN_MAX_CHANNELS]; /* Messages channeling (sending & receiving) */
    NBN_ConnectionStats stats;
    void *driver_data; /* Data attached to the connection by the underlying driver */

#ifdef NBN_DEBUG
    /* Debug callbacks */
    void (*OnMessageAddedToRecvQueue)(struct NBN_Connection *, NBN_Message *);
#endif /* NBN_DEBUG */

    /*
     *  Packet sequencing & acking
     */
    uint16_t next_packet_seq_number;
    uint16_t last_received_packet_seq_number;
    uint32_t packet_send_seq_buffer[NBN_MAX_PACKET_ENTRIES];
    NBN_PacketEntry packet_send_buffer[NBN_MAX_PACKET_ENTRIES];
    uint32_t packet_recv_seq_buffer[NBN_MAX_PACKET_ENTRIES];
};

typedef struct NBN_ConnectionListNode NBN_ConnectionListNode;

/* Linked list of connections */
struct NBN_ConnectionListNode
{
    NBN_Connection *conn;
    NBN_ConnectionListNode *next;
    NBN_ConnectionListNode *prev;
};

NBN_Connection *NBN_Connection_Create(uint32_t, NBN_Endpoint *, NBN_Driver *, void *);
void NBN_Connection_Destroy(NBN_Connection *);
int NBN_Connection_ProcessReceivedPacket(NBN_Connection *, NBN_Packet *, double);
int NBN_Connection_FlushChannels(NBN_Connection *, uint32_t, double);
int NBN_Connection_InitChannel(NBN_Connection *, NBN_Channel *);
bool NBN_Connection_CheckIfStale(NBN_Connection *, double);

#pragma endregion /* NBN_Connection */

#pragma region NBN_EventQueue

#define NBN_NO_EVENT 0 /* No event left in the events queue */
#define NBN_SKIP_EVENT 1 /* Indicates that the event should be skipped */
#define NBN_EVENT_QUEUE_CAPACITY 1024

typedef union NBN_EventData
{
    NBN_MessageInfo message_info;
    NBN_Connection *connection;
    NBN_ConnectionHandle connection_handle;
} NBN_EventData;

typedef struct NBN_Event
{
    int type;
    NBN_EventData data;
} NBN_Event;

typedef struct NBN_EventQueue
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

/**
 * Threading.
 *
 * Windows headers need to be included by the user of the library before
 * the nbnet header because of some winsock2.h / windows.h dependencies.
 */
#ifndef NBNET_WINDOWS
#include <pthread.h> // Threading
#endif /* NBNET_WINDOWS */

#define NBN_GameClient_SetPing(v) { nbn_game_client.endpoint.packet_simulator.ping = v; }
#define NBN_GameClient_SetJitter(v) { nbn_game_client.endpoint.packet_simulator.jitter = v; }
#define NBN_GameClient_SetPacketLoss(v) { nbn_game_client.endpoint.packet_simulator.packet_loss_ratio = v; }
#define NBN_GameClient_SetPacketDuplication(v) { nbn_game_client.endpoint.packet_simulator.packet_duplication_ratio = v; }

#define NBN_GameServer_SetPing(v) { nbn_game_server.endpoint.packet_simulator.ping = v; }
#define NBN_GameServer_SetJitter(v) { nbn_game_server.endpoint.packet_simulator.jitter = v; }
#define NBN_GameServer_SetPacketLoss(v) { nbn_game_server.endpoint.packet_simulator.packet_loss_ratio = v; }
#define NBN_GameServer_SetPacketDuplication(v) { nbn_game_server.endpoint.packet_simulator.packet_duplication_ratio = v; }

typedef struct NBN_PacketSimulatorEntry NBN_PacketSimulatorEntry;

struct NBN_PacketSimulatorEntry
{
    NBN_Packet packet;
    NBN_Connection *receiver;
    double delay;
    double enqueued_at;
    struct NBN_PacketSimulatorEntry *next;
    struct NBN_PacketSimulatorEntry *prev;
};

typedef struct NBN_PacketSimulator
{
    NBN_Endpoint *endpoint;
    NBN_PacketSimulatorEntry *head_packet;
    NBN_PacketSimulatorEntry *tail_packet;
    unsigned int packet_count;

#ifdef NBNET_WINDOWS
    HANDLE queue_mutex;
    HANDLE thread; 
#else
    pthread_mutex_t queue_mutex;
    pthread_t thread;
#endif

    bool running;
    unsigned int total_dropped_packets;

    /* Settings */
    float packet_loss_ratio;
    float current_packet_loss_ratio;
    float packet_duplication_ratio;
    double ping;
    double jitter;
} NBN_PacketSimulator;

void NBN_PacketSimulator_Init(NBN_PacketSimulator *, NBN_Endpoint *);
int NBN_PacketSimulator_EnqueuePacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *);
void NBN_PacketSimulator_Start(NBN_PacketSimulator *);
void NBN_PacketSimulator_Stop(NBN_PacketSimulator *);

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

#define NBN_IsReservedMessage(type) (type == NBN_MESSAGE_CHUNK_TYPE || type == NBN_CLIENT_CLOSED_MESSAGE_TYPE \
|| type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE || type == NBN_BYTE_ARRAY_MESSAGE_TYPE \
|| type == NBN_DISCONNECTION_MESSAGE_TYPE || type == NBN_CONNECTION_REQUEST_MESSAGE_TYPE)

struct NBN_Endpoint
{
    NBN_ChannelBuilder channel_builders[NBN_MAX_CHANNELS];
    NBN_ChannelDestructor channel_destructors[NBN_MAX_CHANNELS];
    NBN_EventQueue event_queue;
    uint32_t protocol_id;
    bool is_server;
    double time;

#ifdef NBN_DEBUG
    /* Debug callbacks */
    void (*OnMessageAddedToRecvQueue)(NBN_Connection *, NBN_Message *);
#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator packet_simulator;
#endif
};

#pragma endregion /* NBN_Endpoint */

#pragma region NBN_GameClient

enum
{
    /* Client is connected to server */
    NBN_CONNECTED = 2,

    /* Client is disconnected from the server */
    NBN_DISCONNECTED,

    /* Client has received a message from the server */
    NBN_MESSAGE_RECEIVED
};

typedef struct NBN_GameClient
{
    NBN_Endpoint endpoint;
    NBN_Connection *server_connection;
    bool is_connected;
    uint8_t server_data[NBN_SERVER_DATA_MAX_SIZE]; /* Data sent by the server when accepting the client's connection */
    unsigned int server_data_len; /* Length of the received server data in bytes */
    NBN_Event last_event;
    int closed_code;
} NBN_GameClient;

extern NBN_GameClient nbn_game_client;

/**
 * Start the game client and send a connection request to the server.
 *
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be able to communicate
 * @param host Host to connect to
 * @param port Port to connect to
 *
 * @return 0 when successully started, -1 otherwise
 */
int NBN_GameClient_Start(const char *protocol_name, const char *host, uint16_t port);

/**
 * Same as NBN_GameClient_Start but with additional parameters. 
 *
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be able to communicate
 * @param host Host to connect to
 * @param port Port to connect to
 * @param data Array of bytes to send to the server upon connection (cannot exceed NBN_CONNECTION_DATA_MAX_SIZE)
 * @param length length of the array of bytes
 *
 * @return 0 when successully started, -1 otherwise
 */
int NBN_GameClient_StartEx(const char *protocol_name, const char *host, uint16_t port, uint8_t *data, unsigned int length);

/**
 * Disconnect from the server. The client can be restarted by calling NBN_GameClient_Start or NBN_GameClient_StartWithData again.
 */
void NBN_GameClient_Stop(void);

/**
 * Read the server data that was received upon connection into a preallocated buffer (the buffer must have a size of at least NBN_SERVER_DATA_MAX_SIZE bytes).
 *
 * @param data The target buffer to copy the server data to
 *
 * @return the length of the server data in bytes
 */
unsigned int NBN_GameClient_ReadServerData(uint8_t *data);

/**
 * Poll game client events.
 * 
 * This function should be called in a loop until it returns NBN_NO_EVENT.
 * 
 * @return The code of the polled event or NBN_NO_EVENT when there is no more events.
 */
int NBN_GameClient_Poll(void);

/**
 * Pack all enqueued messages into packets and send them.
 * 
 * This should be called at a relatively high frequency, probably at the end of
 * every game tick.
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameClient_SendPackets(void);

/**
 * Send a message to the server on a given channel.
 * 
 * It's recommended to use NBN_GameClient_SendUnreliableMessage or NBN_GameClient_SendReliableMessage
 * unless you really want to use a specific channel.
 * 
 * @param type The user-defined type of message to send
 * @param channel_id The ID of the channel to send the message on
 * @param data A pointer to the message data buffer (managed by user code)
 * @param length The length of the message data buffer in bytes
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameClient_SendMessage(uint8_t type, uint8_t channel_id, uint8_t *data, uint16_t length);

/**
 * Send a message to the server, unreliably.
 * 
 * @param type The type of message to send
 * @param data A pointer to the message data buffer (managed by user code)
 * @param length The length of the message data buffer in bytes
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameClient_SendUnreliableMessage(uint8_t type, uint8_t *data, uint16_t length);

/**
 * Send a message to the server, reliably.
 *
 * @param type The type of message to send
 * @param data A pointer to the message data buffer (managed by user code)
 * @param length The length of the message data buffer in bytes
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameClient_SendReliableMessage(uint8_t type, uint8_t *data, uint16_t length);

/**
 * For drivers only! NOT MEANT TO BE USED BY USER CODE.
 */
NBN_Connection *NBN_GameClient_CreateServerConnection(int driver_id, void *driver_data);

/**
 * Retrieve the info about the last received message.
 * 
 * Call this function when receiveing a NBN_MESSAGE_RECEIVED event to access
 * information about the message.
 * 
 * @return A structure containing information about the received message
 */
NBN_MessageInfo NBN_GameClient_GetMessageInfo(void);

/**
 * Retrieve network stats about the game client.
 * 
 * @return A structure containing network related stats about the game client
 */
NBN_ConnectionStats NBN_GameClient_GetStats(void);

/**
 * Retrieve the code sent by the server when closing the connection.
 * 
 * Call this function when receiving a NBN_DISCONNECTED event.
 * 
 * @return The code used by the server when closing the connection or -1 (the default code)
 */
int NBN_GameClient_GetServerCloseCode(void);

/**
 * @return true if connected, false otherwise
 */
bool NBN_GameClient_IsConnected(void);

#ifdef NBN_DEBUG

void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#pragma endregion /* NBN_GameClient */

#pragma region NBN_GameServer

#define NBN_MAX_CLIENTS 1024

enum
{
    /* A new client has connected */
    NBN_NEW_CONNECTION = 2,

    /* A client has disconnected */
    NBN_CLIENT_DISCONNECTED,

    /* A message has been received from a client */
    NBN_CLIENT_MESSAGE_RECEIVED
};

typedef struct NBN_GameServerStats
{
    float upload_bandwidth; /* Total upload bandwith of the game server */
    float download_bandwidth; /* Total download bandwith of the game server */
} NBN_GameServerStats;

typedef struct NBN_GameServer
{
    NBN_Endpoint endpoint;
    NBN_ConnectionVector *clients; /* Vector of clients connections */
    NBN_ConnectionTable *clients_table; /* Hash table of clients connections */
    NBN_ConnectionListNode *closed_clients_head;
    NBN_GameServerStats stats;
    NBN_Event last_event;
    uint8_t last_connection_data[NBN_CONNECTION_DATA_MAX_SIZE];
    unsigned int last_connection_data_len;
} NBN_GameServer;

extern NBN_GameServer nbn_game_server;

/**
 * Start the game server.
 * 
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be able to communicate
 * @param port The server's port
 *
 * @return 0 when successfully started, -1 otherwise
 */
int NBN_GameServer_Start(const char *protocol_name, uint16_t port);

/**
 * Same as NBN_GameServer_Start but with additional parameters.
 *
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be able to communicate
 * @param port The server's port
 *
 * @return 0 when successfully started, -1 otherwise
 */
int NBN_GameServer_StartEx(const char *protocol_name, uint16_t port);

/**
 * Stop the game server and clean everything up.
 */
void NBN_GameServer_Stop(void);

/**
 * Poll game server events.
 *
 * This function should be called in a loop until it returns NBN_NO_EVENT.
 *
 * @return The code of the polled event or NBN_NO_EVENT when there is no more events.
 */
int NBN_GameServer_Poll(void);

/**
 * Pack all enqueued messages into packets and send them.
 * 
 * This should be called at a relatively high frequency, probably at the end of
 * every game tick.
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_SendPackets(void);

/**
 * For drivers only! NOT MEANT TO BE USED BY USER CODE.
 */
NBN_Connection *NBN_GameServer_CreateClientConnection(int, void *, uint32_t);

/**
 * Close a client's connection without a specific code (default code is -1)
 *
 * @param connection_handle The connection to close
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_CloseClient(NBN_ConnectionHandle connection_handle);

/**
 * Close a client's connection with a specific code.
 * 
 * The code is an arbitrary integer to let the client knows
 * why his connection was closed.
 *
 * @param connection_handle The connection to close
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_CloseClientWithCode(NBN_ConnectionHandle connection_handle, int code);

/**
 * Send a message to a client on a given channel.
 *
 * It's recommended to use NBN_GameServer_SendUnreliableMessageTo or NBN_GameServer_SendReliableMessageTo
 * unless you really want to use a specific channel.
 *
 * @param connection_handle The connection to send the message to
 * @param type The type of message to send
 * @param channel_id The ID of the channel to send the message on
 * @param data A pointer to the message data buffer (managed by user code)
 * @param length The length of the message data buffer in bytes
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_SendMessageTo(NBN_ConnectionHandle connection_handle, uint8_t type, uint8_t channel_id, uint8_t *data, uint16_t length);

/**
 * Send a message to a client, unreliably.
 *
 * @param connection_handle The connection to send the message to
 * @param type The type of message to send
 * @param data A pointer to the message data buffer (managed by user code)
 * @param length The length of the message data buffer in bytes
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_SendUnreliableMessageTo(NBN_ConnectionHandle connection_handle, uint8_t type, uint8_t *data, uint16_t length);

/**
 * Send a message to a client, reliably.
 *
 * @param connection_handle The connection to send the message to
 * @param type The type of message to send
 * @param data A pointer to the message data buffer (managed by user code)
 * @param length The length of the message data buffer in bytes
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_SendReliableMessageTo(NBN_ConnectionHandle connection_handle, uint8_t type, uint8_t *data, uint16_t length);

/**
 * Accept the last client connection request and send a blob of data to the client.
 * The client can read that data using the NBN_GameClient_ReadServerData function.
 * If you do not wish to send any data to the client upon accepting his connection, use the NBN_GameServer_AcceptIncomingConnection function instead.
 * 
 * Call this function after receiving a NBN_NEW_CONNECTION event.
 *
 * @param data Data to send
 * @param length Data length in bytes
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_AcceptIncomingConnectionWithData(uint8_t *data, unsigned int length);

/**
 * Accept the last client connection request.
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_AcceptIncomingConnection(void);

/**
 * Reject the last client connection request with a specific code.
 * 
 * The code is an arbitrary integer to let the client knows why his connection
 * was rejected.
 * 
 * Call this function after receiving a NBN_NEW_CONNECTION event.
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_RejectIncomingConnectionWithCode(int code);

/**
 * Reject the last client connection request without any specific code (default code is -1)
 * 
 * Call this function after receiving a NBN_NEW_CONNECTION event.
 * 
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_RejectIncomingConnection(void);

/**
 * Retrieve the last connection to the game server.
 * 
 * Call this function after receiving a NBN_NEW_CONNECTION event.
 * 
 * @return A NBN_ConnectionHandle for the new connection
 */
NBN_ConnectionHandle NBN_GameServer_GetIncomingConnection(void);

/**
 * Read the last connection data into a preallocated buffer. The target buffer must have a length of at least NBN_CONNECTION_DATA_MAX_SIZE bytes.
 *
 * @param data the buffer to copy the connection data to
 *
 * @return the length in bytes of the connection data
 */
unsigned int NBN_GameServer_ReadIncomingConnectionData(uint8_t *data);

/**
 * Return the last disconnected client.
 * 
 * Call this function after receiving a NBN_CLIENT_DISCONNECTED event.
 * 
 * @return The last disconnected connection
 */
NBN_ConnectionHandle NBN_GameServer_GetDisconnectedClient(void);

/**
 * Retrieve the info about the last received message.
 * 
 * Call this function when receiving a NBN_CLIENT_MESSAGE_RECEIVED event to access
 * information about the message.
 * 
 * @return A structure containing information about the received message
 */
NBN_MessageInfo NBN_GameServer_GetMessageInfo(void);

/**
 * Retrieve network stats about the game server.
 * 
 * @return A structure containing network related stats about the game server
 */
NBN_GameServerStats NBN_GameServer_GetStats(void);

#ifdef NBN_DEBUG

void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback, void *);

#endif /* NBN_DEBUG */

#pragma endregion /* NBN_GameServer */

#pragma region Network driver

#define NBN_MAX_DRIVERS 4

typedef enum NBN_DriverEvent
{
    // Client events
    NBN_DRIVER_CLI_PACKET_RECEIVED,

    // Server events
    NBN_DRIVER_SERV_CLIENT_CONNECTED,
    NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED,
} NBN_DriverEvent;

typedef void (*NBN_Driver_StopFunc)(void);
typedef int (*NBN_Driver_RecvPacketsFunc)(void);

typedef int (*NBN_Driver_ClientStartFunc)(uint32_t, const char *, uint16_t);
typedef int (*NBN_Driver_ClientSendPacketFunc)(NBN_Packet *);

typedef int (*NBN_Driver_ServerStartFunc)(uint32_t, uint16_t);
typedef int (*NBN_Driver_ServerSendPacketToFunc)(NBN_Packet *, NBN_Connection *);
typedef void (*NBN_Driver_ServerRemoveConnection)(NBN_Connection *);

typedef struct NBN_DriverImplementation
{
    /* Client functions */
    NBN_Driver_ClientStartFunc cli_start;
    NBN_Driver_StopFunc cli_stop;
    NBN_Driver_RecvPacketsFunc cli_recv_packets;
    NBN_Driver_ClientSendPacketFunc cli_send_packet;

    /* Server functions */
    NBN_Driver_ServerStartFunc serv_start;
    NBN_Driver_StopFunc serv_stop;
    NBN_Driver_RecvPacketsFunc serv_recv_packets;
    NBN_Driver_ServerSendPacketToFunc serv_send_packet_to;
    NBN_Driver_ServerRemoveConnection serv_remove_connection;
} NBN_DriverImplementation;

struct NBN_Driver
{
    int id;
    const char *name;
    NBN_DriverImplementation impl;
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
static NBN_Driver nbn_drivers[NBN_MAX_DRIVERS] = {
    {-1, NULL},
    {-1, NULL},
    {-1, NULL},
    {-1, NULL}
};
#pragma clang diagnostic pop

static unsigned int nbn_driver_count = 0;

/**
 * Register a new network driver, at least one network driver has to be registered.
 *
 * @param id ID of the driver, must be unique and within 0 and NBN_MAX_DRIVERS
 * @param name Name of the driver
 * @param signature Driver implementation (structure containing all driver implementation function pointers)
 */
void NBN_Driver_Register(int id, const char *name, NBN_DriverImplementation implementation);

/**
 * Let nbnet know about specific network events happening within a network driver.
 *
 * @param ev Event type
 * @param data Arbitrary data about the event
*/
int NBN_Driver_RaiseEvent(NBN_DriverEvent ev, void *data);

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

#pragma region NBN_ConnectionVector

#define NBN_CONNECTION_VECTOR_INITIAL_CAPACITY 32

static void NBN_ConnectionVector_Grow(NBN_ConnectionVector *vector, unsigned int new_capacity);

static NBN_ConnectionVector *NBN_ConnectionVector_Create(void)
{
    NBN_ConnectionVector *vector = (NBN_ConnectionVector *)NBN_Allocator(sizeof(NBN_ConnectionVector));

    vector->connections = NULL;
    vector->capacity = 0;
    vector->count = 0;

    NBN_ConnectionVector_Grow(vector, NBN_CONNECTION_VECTOR_INITIAL_CAPACITY);
    return vector;
}

static void NBN_ConnectionVector_Destroy(NBN_ConnectionVector *vector)
{
    NBN_Deallocator(vector->connections);
    NBN_Deallocator(vector);
}

static void NBN_ConnectionVector_Add(NBN_ConnectionVector *vector, NBN_Connection *conn)
{
    NBN_Assert(conn->vector_pos == -1);

    if (vector->count >= vector->capacity)
    {
        NBN_ConnectionVector_Grow(vector, vector->capacity * 2);
    }

    unsigned int position = vector->count;

    if (vector->connections[position])
    {
        NBN_LogError("Failed to add connection (id: %d) to vector: position %d is not empty", conn->id, position);
        NBN_Abort();
    }

    conn->vector_pos = position;
    vector->connections[position] = conn;
    vector->count++;
}

static uint32_t NBN_ConnectionVector_RemoveAt(NBN_ConnectionVector *vector, unsigned int position)
{
    NBN_Connection *conn = vector->connections[position];

    if (conn == NULL) return 0;

    // Make sure that connections are stored contiguously in memory

    NBN_Connection *last_conn = vector->connections[vector->count - 1];

    vector->connections[position] = last_conn;
    vector->connections[vector->count - 1] = NULL;
    last_conn->vector_pos = position; // last connection in the vector is moved to the position of the removed one
    vector->count--;

    return conn->id;
}

static void NBN_ConnectionVector_Grow(NBN_ConnectionVector *vector, unsigned int new_capacity)
{
    vector->connections = (NBN_Connection **)NBN_Reallocator(vector->connections, sizeof(NBN_Connection *) * new_capacity);

    if (vector->connections == NULL)
    {
        NBN_LogError("Failed to allocate memory to grow the connection vector");
        NBN_Abort();
    }

    for (unsigned int i = 0; i < new_capacity - vector->capacity; i++)
        vector->connections[vector->capacity + i] = NULL;

    vector->capacity = new_capacity;
}

#pragma endregion // NBN_ConnectionVector

#pragma region NBN_ConnectionTable

#define NBN_CONNECTION_TABLE_INITIAL_CAPACITY 32
#define NBN_CONNECTION_TABLE_LOAD_FACTOR 0.75f

static void NBN_ConnectionTable_InsertEntry(NBN_ConnectionTable *table, unsigned int slot, NBN_Connection *conn);
static void NBN_ConnectionTable_RemoveEntry(NBN_ConnectionTable *table, unsigned int slot);
static unsigned int NBN_ConnectionTable_Hash(int hash);
static void NBN_ConnectionTable_Grow(NBN_ConnectionTable *table, unsigned int new_capacity);

static NBN_ConnectionTable *NBN_ConnectionTable_Create(void)
{
    NBN_ConnectionTable *table = (NBN_ConnectionTable *)NBN_Allocator(sizeof(NBN_ConnectionTable));

    table->connections = NULL;
    table->capacity = 0;

    NBN_ConnectionTable_Grow(table, NBN_CONNECTION_TABLE_INITIAL_CAPACITY);

    for (unsigned int i = 0; i < table->capacity; i++)
        table->connections[i] = NULL;

    return table;
}

static void NBN_ConnectionTable_Destroy(NBN_ConnectionTable *table)
{
    // Make sure we don't destroy the actual connections as they will be destroyed with the connection vector
    NBN_Deallocator(table->connections);
    NBN_Deallocator(table);
}

static void NBN_ConnectionTable_Add(NBN_ConnectionTable *table, NBN_Connection *conn)
{
    unsigned int hash = NBN_ConnectionTable_Hash(conn->id);
    unsigned int slot = hash % table->capacity;

    NBN_Connection *entry = table->connections[slot];

    if (entry == NULL)
    {
        // no collision, just insert in slot
        NBN_ConnectionTable_InsertEntry(table, slot, conn);
        return;
    }

    // collision, do quadratic probing until we find a free slot

    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % table->capacity;
        entry = table->connections[slot];

        i++;
    } while (entry != NULL);

    NBN_ConnectionTable_InsertEntry(table, slot, conn);
}

static bool NBN_ConnectionTable_Remove(NBN_ConnectionTable *table, uint32_t id)
{
    unsigned int hash = NBN_ConnectionTable_Hash(id);
    unsigned int slot = hash % table->capacity;
    NBN_Connection *conn = table->connections[slot];

    if (conn->id == id)
    {
        NBN_ConnectionTable_RemoveEntry(table, slot);
        return true;
    }

    // quadratic probing

    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % table->capacity;
        conn = table->connections[slot];

        if (conn != NULL && conn->id == id)
        {
            NBN_ConnectionTable_RemoveEntry(table, slot);
            return true;
        }

        i++;
    } while (i < table->capacity);

    return false;
}

static void NBN_ConnectionTable_InsertEntry(NBN_ConnectionTable *table, unsigned int slot, NBN_Connection *conn)
{
    table->connections[slot] = conn;
    table->count++;
    table->load_factor = (float)table->count / table->capacity;

    if (table->load_factor > NBN_CONNECTION_TABLE_LOAD_FACTOR)
    {
        NBN_ConnectionTable_Grow(table, table->capacity * 2);
    }
}

static void NBN_ConnectionTable_RemoveEntry(NBN_ConnectionTable *table, unsigned int slot)
{
    table->connections[slot] = NULL;
    table->count--;
    table->load_factor = (float)table->count / table->capacity;
}

static NBN_Connection *NBN_ConnectionTable_Get(NBN_ConnectionTable *table, uint32_t id)
{
    unsigned int hash = NBN_ConnectionTable_Hash(id);
    unsigned int slot = hash % table->capacity;
    NBN_Connection *conn = table->connections[slot];

    if (conn && conn->id == id)
        return conn;

    // quadratic probing

    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % table->capacity;
        conn = table->connections[slot];

        if (conn != NULL && conn->id == id)
        {
            return conn;
        }

        i++;
    } while (i < table->capacity);

    return NULL;
}

static unsigned int NBN_ConnectionTable_Hash(int hash)
{
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = (hash >> 16) ^ hash;

    return hash;
}

static void NBN_ConnectionTable_Grow(NBN_ConnectionTable *table, unsigned int new_capacity)
{
    unsigned int old_capacity = table->capacity;
    NBN_Connection** old_connections_array = table->connections;
    NBN_Connection** new_connections_array = (NBN_Connection **)NBN_Allocator(sizeof(NBN_Connection *) * new_capacity);

    for (unsigned int i = 0; i < new_capacity; i++)
    {
        new_connections_array[i] = NULL;
    }

    table->connections = new_connections_array;
    table->capacity = new_capacity;
    table->count = 0;
    table->load_factor = 0;

    // rehash

    NBN_Assert(old_connections_array || old_capacity == 0);

    for (unsigned int i = 0; i < old_capacity; i++)
    {
        if (old_connections_array[i])
        {
            NBN_ConnectionTable_Add(table, old_connections_array[i]);
        }
    }

    if (old_connections_array) NBN_Deallocator(old_connections_array);
}

#pragma region // NBN_ConnectionTable

#pragma region Memory management

NBN_MemoryManager nbn_mem_manager;

static void MemoryManager_Init(void);
static void MemoryManager_Deinit(void);
static void *MemoryManager_Alloc(unsigned int);
static void MemoryManager_Dealloc(void *, unsigned int);

#if !defined(NBN_DISABLE_MEMORY_POOLING)

static void MemPool_Init(NBN_MemPool *, size_t, unsigned int);
static void MemPool_Deinit(NBN_MemPool *);
static void MemPool_Grow(NBN_MemPool *, unsigned int);
static void *MemPool_Alloc(NBN_MemPool *);
static void MemPool_Dealloc(NBN_MemPool *, void *);

#endif /* NBN_DISABLE_MEMORY_POOLING */

static void MemoryManager_Init(void)
{
#ifdef NBN_DISABLE_MEMORY_POOLING
    NBN_LogDebug("MemoryManager_Init without pooling!");

    nbn_mem_manager.mem_sizes[NBN_MEM_MESSAGE_CHUNK] = sizeof(NBN_MessageChunk);
    nbn_mem_manager.mem_sizes[NBN_MEM_CONNECTION] = sizeof(NBN_Connection);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    nbn_mem_manager.mem_sizes[NBN_MEM_PACKET_SIMULATOR_ENTRY] = sizeof(NBN_PacketSimulatorEntry);
#endif
#else
    NBN_LogDebug("MemoryManager_Init with pooling!");

    // MemPool_Init(&nbn_mem_manager.mem_pools[NBN_MEM_MESSAGE_CHUNK], sizeof(NBN_MessageChunk), 256);
    MemPool_Init(&nbn_mem_manager.mem_pools[NBN_MEM_CONNECTION], sizeof(NBN_Connection), 16);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    MemPool_Init(&nbn_mem_manager.mem_pools[NBN_MEM_PACKET_SIMULATOR_ENTRY], sizeof(NBN_PacketSimulatorEntry), 32);
#endif
#endif /* NBN_DISABLE_MEMORY_POOLING */
}

static void MemoryManager_Deinit(void)
{
#if !defined(NBN_DISABLE_MEMORY_POOLING)
    MemPool_Deinit(&nbn_mem_manager.mem_pools[NBN_MEM_MESSAGE_CHUNK]);
    MemPool_Deinit(&nbn_mem_manager.mem_pools[NBN_MEM_CONNECTION]);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    MemPool_Deinit(&nbn_mem_manager.mem_pools[NBN_MEM_PACKET_SIMULATOR_ENTRY]);
#endif
#endif /* NBN_DISABLE_MEMORY_POOLING */
}

static void *MemoryManager_Alloc(unsigned int mem_tag)
{
#ifdef NBN_DISABLE_MEMORY_POOLING
    return NBN_Allocator(nbn_mem_manager.mem_sizes[mem_tag]);
#else
    return MemPool_Alloc(&nbn_mem_manager.mem_pools[mem_tag]);
#endif /* NBN_DISABLE_MEMORY_POOLING */
}

static void MemoryManager_Dealloc(void *ptr, unsigned int mem_tag)
{
#ifdef NBN_DISABLE_MEMORY_POOLING
    (void)mem_tag;

    NBN_Deallocator(ptr);
#else
    MemPool_Dealloc(&nbn_mem_manager.mem_pools[mem_tag], ptr);
#endif /* NBN_DISABLE_MEMORY_POOLING */
}

#if !defined(NBN_DISABLE_MEMORY_POOLING)

static void MemPool_Init(NBN_MemPool *pool, size_t block_size, unsigned int initial_block_count)
{
    pool->block_size = MAX(block_size, sizeof(NBN_MemPoolFreeBlock));
    pool->block_idx = 0;
    pool->block_count = 0;
    pool->free = NULL;
    pool->blocks = NULL;

    MemPool_Grow(pool, initial_block_count);
}

static void MemPool_Deinit(NBN_MemPool *pool)
{
    for (unsigned int i = 0; i < pool->block_count; i++)
        NBN_Deallocator(pool->blocks[i]);

    NBN_Deallocator(pool->blocks);
}

static void *MemPool_Alloc(NBN_MemPool *pool)
{
    if (pool->free)
    {
        void *block = pool->free;

        pool->free = pool->free->next;

        return block;
    }

    if (pool->block_idx == pool->block_count)
        MemPool_Grow(pool, pool->block_count * 2);

    void *block = pool->blocks[pool->block_idx];

    pool->block_idx++;

    return block;
}

static void MemPool_Dealloc(NBN_MemPool *pool, void *ptr)
{
    NBN_MemPoolFreeBlock *free = pool->free;

    pool->free = (NBN_MemPoolFreeBlock*)ptr;
    pool->free->next = free;
}

static void MemPool_Grow(NBN_MemPool *pool, unsigned int block_count)
{
    pool->blocks = (uint8_t**)NBN_Reallocator(pool->blocks, sizeof(uint8_t *) * block_count);

    for (unsigned int i = 0; i < block_count - pool->block_count; i++)
        pool->blocks[pool->block_idx + i] = (uint8_t*)NBN_Allocator(pool->block_size);

    pool->block_count = block_count;
}

#endif /* NBN_DISABLE_MEMORY_POOLING */

#pragma endregion /* Memory management */

#pragma region Serialization

void NBN_Writer_Init(NBN_Writer *writer, uint8_t *buffer, unsigned int length)
{
    writer->buffer = buffer;
    writer->length = length;
    writer->position = 0;
}

void NBN_Writer_WriteInt32(NBN_Writer *writer, int32_t value)
{
    NBN_Writer_WriteUInt32(writer, value);
}

void NBN_Writer_WriteUInt16(NBN_Writer *writer, uint16_t value)
{
    NBN_Assert(writer->position + 2 <= writer->length);

    *((uint16_t *)(writer->buffer + writer->position)) = htons(value);
    writer->position += 2;
}

void NBN_Writer_WriteUInt32(NBN_Writer *writer, uint32_t value)
{
    NBN_Assert(writer->position + 4 <= writer->length);

    *((uint32_t *)(writer->buffer + writer->position)) = htonl(value);
    writer->position += 4;
}

void NBN_Writer_WriteUInt8(NBN_Writer *writer, uint8_t value)
{
    NBN_Assert(writer->position + 1 <= writer->length);

    writer->buffer[writer->position] = value;
    writer->position++;
}

void NBN_Writer_WriteBytes(NBN_Writer *writer, uint8_t *bytes, unsigned int length)
{
    NBN_Assert(writer->position + length <= writer->length);

    memcpy(writer->buffer + writer->position, bytes, length);
    writer->position += length;
}

void NBN_Reader_Init(NBN_Reader *reader, uint8_t *buffer, unsigned int length)
{
    reader->buffer = buffer;
    reader->length = length;
    reader->position = 0;
}

int NBN_Reader_ReadInt32(NBN_Reader *reader, int32_t *value)
{
    return NBN_Reader_ReadUInt32(reader, (uint32_t *)value);
}

int NBN_Reader_ReadUInt16(NBN_Reader *reader, uint16_t *value)
{
    if (reader->position + 2 > reader->length)
    {
        return NBN_ERROR;
    }

    *value = *((uint16_t *)(reader->buffer + reader->position));
    reader->position += 2;

    return 0;
}

int NBN_Reader_ReadUInt32(NBN_Reader *reader, uint32_t *value)
{
    if (reader->position + 4 > reader->length)
    {
        return NBN_ERROR;
    }

    *value = *((uint32_t *)(reader->buffer + reader->position));
    reader->position += 4;

    return 0;
}

int NBN_Reader_ReadUInt8(NBN_Reader *reader, uint8_t *value)
{
    if (reader->position + 1 > reader->length)
    {
        return NBN_ERROR;
    }

    *value = reader->buffer[reader->position];
    reader->position++;

    return 0;
}

int NBN_Reader_ReadBytes(NBN_Reader *reader, uint8_t *bytes, unsigned int length)
{
    if (reader->position + length > reader->length)
    {
        return NBN_ERROR;
    }

    memcpy(bytes, reader->buffer + reader->position, length);
    reader->position += length;

    return 0;
}

#pragma endregion /* Serialization */

#pragma region NBN_Packet

void NBN_Packet_InitWrite(NBN_Packet *packet, uint32_t protocol_id, uint16_t seq_number, uint16_t ack, uint32_t ack_bits)
{
    packet->header.protocol_id = protocol_id;
    packet->header.messages_count = 0;
    packet->header.seq_number = seq_number;
    packet->header.ack = ack;
    packet->header.ack_bits = ack_bits;

    packet->mode = NBN_PACKET_MODE_WRITE;
    packet->sender = NULL;
    packet->size = NBN_PACKET_HEADER_SIZE;
    packet->sealed = false;

    memset(packet->buffer, 0, sizeof(packet->buffer));
}

int NBN_Packet_WriteMessage(NBN_Packet *packet, NBN_Message *message)
{
    NBN_Assert(
            (message->data != NULL && message->header.length > 0) ||
            (message->data == NULL && message->header.length == 0));

    if (packet->mode != NBN_PACKET_MODE_WRITE || packet->sealed) return NBN_PACKET_WRITE_ERROR;

    unsigned int message_size = NBN_MESSAGE_HEADER_SIZE + message->header.length;

    if (
            packet->header.messages_count >= NBN_MAX_MESSAGES_PER_PACKET ||
            packet->size + message_size > NBN_PACKET_MAX_SIZE)
    {
        return NBN_PACKET_WRITE_NO_SPACE;
    }

    NBN_Writer writer;

    NBN_Writer_Init(&writer, packet->buffer + packet->size, sizeof(packet->buffer) - packet->size);

    NBN_Writer_WriteUInt16(&writer, message->header.id);
    NBN_Writer_WriteUInt16(&writer, message->header.length);
    NBN_Writer_WriteUInt8(&writer, message->header.type);
    NBN_Writer_WriteUInt8(&writer, message->header.channel_id);

    NBN_Assert(writer.position == NBN_MESSAGE_HEADER_SIZE);

    if (message->data)
    {
        NBN_Writer_WriteBytes(&writer, message->data, message->header.length);
    }

    NBN_Assert(writer.position == message_size);

    packet->size += writer.position;
    packet->header.messages_count++;

    return NBN_PACKET_WRITE_OK;
}

int NBN_Packet_Seal(NBN_Packet *packet, NBN_Connection *connection)
{
    if (packet->mode != NBN_PACKET_MODE_WRITE)
        return NBN_ERROR;

    NBN_Writer writer;

    NBN_Writer_Init(&writer, packet->buffer, NBN_PACKET_HEADER_SIZE);

    NBN_Writer_WriteUInt32(&writer, packet->header.protocol_id);
    NBN_Writer_WriteUInt32(&writer, packet->header.ack_bits);
    NBN_Writer_WriteUInt16(&writer, packet->header.seq_number);
    NBN_Writer_WriteUInt16(&writer, packet->header.ack);
    NBN_Writer_WriteUInt8(&writer, packet->header.messages_count);

    NBN_Assert(writer.position == NBN_PACKET_HEADER_SIZE);

    packet->sealed = true;

    return 0;
}

int NBN_Packet_InitRead(NBN_Packet *packet, uint32_t protocol_id, unsigned int size)
{
    NBN_Assert(size >= NBN_PACKET_HEADER_SIZE);

    packet->mode = NBN_PACKET_MODE_READ;
    packet->size = size;
    packet->sender = NULL; // IMPORTANT: must be set by the drivers
    packet->sealed = false;

    NBN_Reader reader;

    NBN_Reader_Init(&reader, packet->buffer, NBN_PACKET_HEADER_SIZE);

    if (NBN_Reader_ReadUInt32(&reader, &packet->header.protocol_id) < 0) return NBN_ERROR;
    if (packet->header.protocol_id != protocol_id) return NBN_ERROR;

    if (NBN_Reader_ReadUInt32(&reader, &packet->header.ack_bits) < 0) return NBN_ERROR;
    if (NBN_Reader_ReadUInt16(&reader, &packet->header.seq_number) < 0) return NBN_ERROR;
    if (NBN_Reader_ReadUInt16(&reader, &packet->header.ack) < 0) return NBN_ERROR;
    if (NBN_Reader_ReadUInt8(&reader, &packet->header.messages_count) < 0) return NBN_ERROR;

    return 0;
}

#pragma endregion /* NBN_Packet */

#pragma region NBN_Connection

static NBN_Message *Endpoint_CreateOutgoingMessage(NBN_Endpoint *, NBN_Channel*, uint8_t, uint8_t *, uint16_t);

static uint32_t Connection_BuildPacketAckBits(NBN_Connection *);
static int Connection_DecodePacketHeader(NBN_Connection *, NBN_Packet *, double);
static int Connection_AckPacket(NBN_Connection *, uint16_t, double time);
static void Connection_InitOutgoingPacket(NBN_Connection *, uint32_t, NBN_Packet *, NBN_PacketEntry **);
static NBN_PacketEntry *Connection_InsertOutgoingPacketEntry(NBN_Connection *, uint16_t);
static bool Connection_InsertReceivedPacketEntry(NBN_Connection *, uint16_t);
static NBN_PacketEntry *Connection_FindSendPacketEntry(NBN_Connection *, uint16_t);
static bool Connection_IsPacketReceived(NBN_Connection *, uint16_t);
static int Connection_SendPacket(NBN_Connection *, NBN_Packet *, NBN_PacketEntry *, double);
static int Connection_ReadNextMessageFromBuffer(NBN_Reader *, NBN_Message *);
static void Connection_RecycleMessage(NBN_Channel *, NBN_Message *);
static void Connection_UpdateAveragePing(NBN_Connection *, double);
static void Connection_UpdateAveragePacketLoss(NBN_Connection *, uint16_t);
static void Connection_UpdateAverageUploadBandwidth(NBN_Connection *, float);
static void Connection_UpdateAverageDownloadBandwidth(NBN_Connection *, double);

NBN_Connection *NBN_Connection_Create(uint32_t id, NBN_Endpoint *endpoint, NBN_Driver *driver, void *driver_data)
{
    NBN_Connection *connection = (NBN_Connection*)MemoryManager_Alloc(NBN_MEM_CONNECTION);

    connection->id = id;
    connection->endpoint = endpoint;
    connection->last_recv_packet_time = endpoint->time;
    connection->next_packet_seq_number = 1;
    connection->last_received_packet_seq_number = 0;
    connection->last_flush_time = endpoint->time;
    connection->last_read_packets_time = endpoint->time;
    connection->downloaded_bytes = 0;
    connection->is_accepted = false;
    connection->is_stale = false;
    connection->is_closed = false;
    connection->vector_pos = -1;

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
        connection->channels[i] = NULL;

    for (int i = 0; i < NBN_MAX_PACKET_ENTRIES; i++)
    {
        connection->packet_send_seq_buffer[i] = 0xFFFFFFFF;
        connection->packet_recv_seq_buffer[i] = 0xFFFFFFFF;
    }

    NBN_ConnectionStats stats = { 0 };

    connection->stats = stats;
    connection->driver = driver;
    connection->driver_data = driver_data;

    return connection;
}

void NBN_Connection_Destroy(NBN_Connection *connection)
{
    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        NBN_Channel *channel = connection->channels[i];

        if (channel)
        {
            assert(channel->destructor);
            channel->destructor(channel);
        }
    }

    MemoryManager_Dealloc(connection, NBN_MEM_CONNECTION);
}

int NBN_Connection_ProcessReceivedPacket(NBN_Connection *connection, NBN_Packet *packet, double time)
{
    if (Connection_DecodePacketHeader(connection, packet, time) < 0)
    {
        NBN_LogError("Failed to decode packet %d header", packet->header.seq_number);

        return NBN_ERROR;
    }

    Connection_UpdateAveragePacketLoss(connection, packet->header.ack);

    if (!Connection_InsertReceivedPacketEntry(connection, packet->header.seq_number))
        return 0;

    if (SEQUENCE_NUMBER_GT(packet->header.seq_number, connection->last_received_packet_seq_number))
        connection->last_received_packet_seq_number = packet->header.seq_number;

    NBN_Reader msg_reader;

    NBN_Reader_Init(&msg_reader, packet->buffer + NBN_PACKET_HEADER_SIZE, packet->size - NBN_PACKET_HEADER_SIZE);

    for (int i = 0; i < packet->header.messages_count; i++)
    {
        NBN_Message message = {0};
        int msg_len = Connection_ReadNextMessageFromBuffer(&msg_reader, &message);

        if (msg_len < 0)
        {
            NBN_LogError("Failed to read packet, invalid data");

            return NBN_ERROR;
        }

        uint8_t channel_id = message.header.channel_id;

        if (channel_id > NBN_MAX_CHANNELS - 1 || connection->channels[channel_id] == NULL)
        {
            NBN_LogError("Failed to read packet, messages on invalid channels");

            return NBN_ERROR;
        }

        NBN_Channel *channel = connection->channels[channel_id];

        if (channel->AddReceivedMessage(channel, &message))
        {
            NBN_LogTrace("Received message %d on channel %d : added to recv queue", message.header.id, channel->id);

#ifdef NBN_DEBUG
            if (connection->OnMessageAddedToRecvQueue) connection->OnMessageAddedToRecvQueue(connection, &message);
#endif
        }
        else
        {
            NBN_LogTrace("Received message %d : discarded", message.header.id);

            Connection_RecycleMessage(channel, &message);
        }
    }

    return 0;
}

int NBN_Connection_FlushChannels(NBN_Connection *connection, uint32_t protocol_id, double time)
{
    NBN_LogTrace("Flushing the send queue");

    NBN_Packet packet = {0};
    NBN_PacketEntry *packet_entry;
    unsigned int sent_packet_count = 0;
    unsigned int sent_bytes = 0;

    Connection_InitOutgoingPacket(connection, protocol_id, &packet, &packet_entry);

    for (unsigned int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        NBN_Channel *channel = connection->channels[i];

        if (channel == NULL)
            continue;

        NBN_LogTrace("Flushing channel %d (message count: %d)", channel->id, channel->outgoing_message_count);

        NBN_Message *message;
        unsigned int j = 0;

        while (
                j < channel->outgoing_message_count &&
                sent_packet_count < NBN_CONNECTION_MAX_SENT_PACKET_COUNT &&
                (message = channel->GetNextOutgoingMessage(channel, time)) != NULL
              )
        {
            bool message_sent = false;
            int ret = NBN_Packet_WriteMessage(&packet, message);

            if (ret == NBN_PACKET_WRITE_OK)
            {
                message_sent = true;
            }
            else if (ret == NBN_PACKET_WRITE_NO_SPACE)
            {
                if (Connection_SendPacket(connection, &packet, packet_entry, time) < 0)
                {
                    NBN_LogError("Failed to send packet %d", packet.header.seq_number);

                    return NBN_ERROR;
                }

                sent_packet_count++;
                sent_bytes += packet.size;

                Connection_InitOutgoingPacket(connection, protocol_id, &packet, &packet_entry);

                int ret = NBN_Packet_WriteMessage(&packet, message);

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

                NBN_Channel_UpdateMessageLastSendTime(channel, message, time);

                NBN_MessageEntry e = { message->header.id, channel->id };

                packet_entry->messages[packet_entry->messages_count++] = e;

                if (channel->OnOutgoingMessageSent)
                {
                    channel->OnOutgoingMessageSent(channel, message);
                }
            }

            j++;
        }
    }

    if (Connection_SendPacket(connection, &packet, packet_entry, time) < 0)
    {
        NBN_LogError("Failed to send packet %d to connection %d", packet.header.seq_number, connection->id);

        return NBN_ERROR;
    }

    sent_bytes += packet.size;
    sent_packet_count++;

    double t = time - connection->last_flush_time;

    if (t > 0) Connection_UpdateAverageUploadBandwidth(connection, sent_bytes / t);

    connection->last_flush_time = time;

    return 0;
}

int NBN_Connection_InitChannel(NBN_Connection *connection, NBN_Channel *channel)
{
    channel->connection = connection;
    // channel->read_chunk_buffer = (uint8_t*)NBN_Allocator(NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE);
    // channel->write_chunk_buffer = (uint8_t*)NBN_Allocator(NBN_CHANNEL_RW_CHUNK_BUFFER_INITIAL_SIZE);
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

    // TODO
    /*for (int i = 0; i < NBN_CHANNEL_CHUNKS_BUFFER_SIZE; i++)*/
    /*    channel->recv_chunk_buffer[i] = NULL;*/
    /**/
    NBN_LogDebug("Initialized channel %d for connection %d", channel->id, connection->id);
    return 0;
}

bool NBN_Connection_CheckIfStale(NBN_Connection *connection, double time)
{
#if defined(NBN_DEBUG) && defined(NBN_DISABLE_STALE_CONNECTION_DETECTION)
    /* When testing under bad network conditions (in soak test for instance), we don't want to deal
       with stale connections */
    return false;
#else
    return time - connection->last_recv_packet_time > NBN_CONNECTION_STALE_TIME_THRESHOLD;
#endif
}

static int Connection_DecodePacketHeader(NBN_Connection *connection, NBN_Packet *packet, double time)
{
    if (Connection_AckPacket(connection, packet->header.ack, time) < 0)
    {
        NBN_LogError("Failed to ack packet %d", packet->header.seq_number);

        return NBN_ERROR;
    }

    for (unsigned int i = 0; i < 32; i++)
    {
        if (B_IS_UNSET(packet->header.ack_bits, i))
            continue;

        if (Connection_AckPacket(connection, packet->header.ack - (i + 1), time) < 0)
        {
            NBN_LogError("Failed to ack packet %d", packet->header.seq_number);

            return NBN_ERROR;
        }
    }

    return 0;
}

static uint32_t Connection_BuildPacketAckBits(NBN_Connection *connection)
{
    uint32_t ack_bits = 0;

    for (int i = 0; i < 32; i++)
    {
        /* 
           when last_received_packet_seq_number is lower than 32, the value of acked_packet_seq_number will eventually
           wrap around, which means the packets from before the wrap around will naturally be acked
           */

        uint16_t acked_packet_seq_number = connection->last_received_packet_seq_number - (i + 1);

        if (Connection_IsPacketReceived(connection, acked_packet_seq_number))
            B_SET(ack_bits, i);
    }

    return ack_bits;
}

static int Connection_AckPacket(NBN_Connection *connection, uint16_t ack_packet_seq_number, double time)
{
    NBN_PacketEntry *packet_entry = Connection_FindSendPacketEntry(connection, ack_packet_seq_number);

    if (packet_entry && !packet_entry->acked)
    {
        NBN_LogTrace("Packet %d acked (connection: %d)", ack_packet_seq_number, connection->id);

        packet_entry->acked = true;

        Connection_UpdateAveragePing(connection, time - packet_entry->send_time);

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

static void Connection_InitOutgoingPacket(
        NBN_Connection *connection, uint32_t protocol_id, NBN_Packet *outgoing_packet, NBN_PacketEntry **packet_entry)
{
    NBN_Packet_InitWrite(
            outgoing_packet,
            protocol_id,
            connection->next_packet_seq_number++,
            connection->last_received_packet_seq_number,
            Connection_BuildPacketAckBits(connection));

    *packet_entry = Connection_InsertOutgoingPacketEntry(connection, outgoing_packet->header.seq_number);
}

static NBN_PacketEntry *Connection_InsertOutgoingPacketEntry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;
    NBN_PacketEntry entry = {
            .acked = false,
            .flagged_as_lost = false,
            .messages_count = 0,
            .send_time = 0,
            .messages = { {0, 0} }
    };

    connection->packet_send_seq_buffer[index] = seq_number;
    connection->packet_send_buffer[index] = entry;

    return &connection->packet_send_buffer[index];
}

static bool Connection_InsertReceivedPacketEntry(NBN_Connection *connection, uint16_t seq_number)
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

static NBN_PacketEntry *Connection_FindSendPacketEntry(NBN_Connection *connection, uint16_t seq_number)
{
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    if (connection->packet_send_seq_buffer[index] == seq_number)
        return &connection->packet_send_buffer[index];

    return NULL;
}

static bool Connection_IsPacketReceived(NBN_Connection *connection, uint16_t packet_seq_number)
{
    uint16_t index = packet_seq_number % NBN_MAX_PACKET_ENTRIES;

    return connection->packet_recv_seq_buffer[index] == packet_seq_number;
}

static int Connection_SendPacket(NBN_Connection *connection, NBN_Packet *packet, NBN_PacketEntry *packet_entry, double time)
{
    NBN_LogTrace("Send packet %d to connection %d (messages count: %d)",
            packet->header.seq_number, connection->id, packet->header.messages_count);

    assert(packet_entry->messages_count == packet->header.messages_count);

    if (NBN_Packet_Seal(packet, connection) < 0)
    {
        NBN_LogError("Failed to seal packet");

        return NBN_ERROR;
    }

    packet_entry->send_time = time;

    if (connection->endpoint->is_server)
    {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return NBN_PacketSimulator_EnqueuePacket(&nbn_game_server.endpoint.packet_simulator, packet, connection);
#else
        if (connection->is_stale)
            return 0;

        return connection->driver->impl.serv_send_packet_to(packet, connection);
#endif
    }
    else
    {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return NBN_PacketSimulator_EnqueuePacket(&nbn_game_client.endpoint.packet_simulator, packet, connection);
#else
        NBN_Driver *driver = nbn_game_client.server_connection->driver;

        return driver->impl.cli_send_packet(packet);
#endif
    }
}

static int Connection_ReadNextMessageFromBuffer(NBN_Reader *reader, NBN_Message *message)
{
    if (NBN_Reader_ReadUInt16(reader, &message->header.id) < 0)
    {
        NBN_LogError("Failed to read message id");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt16(reader, &message->header.length) < 0)
    {
        NBN_LogError("Failed to read message length");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(reader, &message->header.type) < 0)
    {
        NBN_LogError("Failed to read message type");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(reader, &message->header.channel_id) < 0)
    {
        NBN_LogError("Failed to read message channel");

        return NBN_ERROR;
    }

    if (message->header.length > 0)
    {
        // TODO: user defined message allocation strategy
        message->data = NBN_Allocator(message->header.length);

        if (NBN_Reader_ReadBytes(reader, message->data, message->header.length) < 0)
        {
            NBN_LogError("Failed to read message data");

            return NBN_ERROR;
        }
    }

    return 0;
}

static void Connection_RecycleMessage(NBN_Channel *channel, NBN_Message *message)
{
    // for incoming messages : message->outgoing_msg == NULL
    assert(message->ref_count > 0);

    message->ref_count--;

    if (message->ref_count == 0)
    {
        // TODO

        /*if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
        {
            NBN_MessageChunk *chunk = (NBN_MessageChunk*)message->data;

            if (chunk->outgoing_msg)
            {
                assert(chunk->outgoing_msg->ref_count > 0);

                if (--chunk->outgoing_msg->ref_count == 0)
                {
                    // TODO
                }
            }
        }*/

        // TODO: user defined message allocation strategy
        NBN_Deallocator(message->data);
    }
}

static void Connection_UpdateAveragePing(NBN_Connection *connection, double ping)
{
    /* exponential smoothing with a factor of 0.05 */
    connection->stats.ping = connection->stats.ping + .05f * (ping - connection->stats.ping);
}

static void Connection_UpdateAveragePacketLoss(NBN_Connection *connection, uint16_t seq)
{
    unsigned int lost_packet_count = 0;
    uint16_t start_seq = seq - 64;

    for (int i = 0; i < 100; i++)
    {
        uint16_t s = start_seq - i;
        NBN_PacketEntry *entry = Connection_FindSendPacketEntry(connection, s);

        if (entry && !entry->acked)
        {
            lost_packet_count++;

            if (!entry->flagged_as_lost)
            {
                entry->flagged_as_lost = true;
                connection->stats.total_lost_packets++;
            }
        }
    }

    float packet_loss = lost_packet_count / 100.f;

    /* exponential smoothing with a factor of 0.1 */
    connection->stats.packet_loss = connection->stats.packet_loss + .1f * (packet_loss - connection->stats.packet_loss);
}

static void Connection_UpdateAverageUploadBandwidth(NBN_Connection *connection, float bytes_per_sec)
{
    /* exponential smoothing with a factor of 0.1 */
    connection->stats.upload_bandwidth =
        connection->stats.upload_bandwidth + .1f * (bytes_per_sec - connection->stats.upload_bandwidth);
}

static void Connection_UpdateAverageDownloadBandwidth(NBN_Connection *connection, double time)
{
    double t = time - connection->last_read_packets_time;

    if (t == 0)
        return;

    float bytes_per_sec = connection->downloaded_bytes / t;

    /* exponential smoothing with a factor of 0.1 */
    connection->stats.download_bandwidth =
        connection->stats.download_bandwidth + .1f * (bytes_per_sec - connection->stats.download_bandwidth);

    connection->downloaded_bytes = 0;
}

#pragma endregion /* NBN_Connection */

#pragma region NBN_Channel

void NBN_Channel_Destroy(NBN_Channel *channel)
{
    for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
    {
        NBN_MessageSlot *slot = &channel->recved_message_slot_buffer[i];

        if (!slot->free)
        {
            Connection_RecycleMessage(channel, &slot->message);
        }

        slot = &channel->outgoing_message_slot_buffer[i];

        if (!slot->free)
        {
            Connection_RecycleMessage(channel, &slot->message);
        }
    }

    /*NBN_Deallocator(channel->read_chunk_buffer);*/
    /*NBN_Deallocator(channel->write_chunk_buffer);*/

    NBN_Deallocator(channel);
}

/*bool NBN_Channel_AddChunk(NBN_Channel *channel, NBN_Message *chunk_msg)*/
/*{*/
/*    assert(chunk_msg->header.type == NBN_MESSAGE_CHUNK_TYPE);*/
/**/
/*    NBN_MessageChunk *chunk = (NBN_MessageChunk *)chunk_msg->data;*/
/**/
/*    NBN_LogTrace("Add chunk %d to channel %d (current chunk count: %d, last recved chunk id: %d)",*/
/*                 chunk->id, channel->id, channel->chunk_count, channel->last_received_chunk_id);*/
/**/
/*    if (chunk->id == channel->last_received_chunk_id + 1)*/
/*    {*/
/*        assert(channel->recv_chunk_buffer[chunk->id] == NULL);*/
/**/
/*        channel->recv_chunk_buffer[chunk->id] = chunk;*/
/*        channel->last_received_chunk_id++;*/
/*        channel->chunk_count++;*/
/**/
/*        NBN_LogTrace("Chunk added (%d/%d)", channel->chunk_count, chunk->total);*/
/**/
/*        if (channel->chunk_count == chunk->total)*/
/*        {*/
/*            channel->last_received_chunk_id = -1;*/
/**/
/*            return true;*/
/*        }*/
/**/
/*        return false;*/
/*    }*/
/*    else*/
/*    {*/
/*        NBN_LogTrace("Chunk ignored");*/
/*    }*/
/**/
/*    for (unsigned int i = 0; i < channel->chunk_count; i++)*/
/*    {*/
/*        assert(channel->recv_chunk_buffer[i] != NULL);*/
/**/
/*        NBN_MessageChunk_Destroy(channel->recv_chunk_buffer[i]);*/
/**/
/*        channel->recv_chunk_buffer[i] = NULL;*/
/*    }*/
/**/
/*    channel->chunk_count = 0;*/
/*    channel->last_received_chunk_id = -1;*/
/**/
/*    if (chunk->id == 0)*/
/*        return NBN_Channel_AddChunk(channel, chunk_msg);*/
/**/
/*    return false;*/
/*}*/

int NBN_Channel_ReconstructMessageFromChunks(NBN_Channel *channel, NBN_Connection *connection, NBN_Message *message)
{
    // TODO

    /*unsigned int message_size = channel->chunk_count * NBN_MESSAGE_CHUNK_SIZE;

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

    if (Connection_ReadNextMessageFromStream(connection, &r_stream, message) < 0)
        return NBN_ERROR;

    NBN_LogTrace("Reconstructed message %d of type %d", message->header.id, message->header.type);*/

    return 0;
}

/*void NBN_Channel_ResizeWriteChunkBuffer(NBN_Channel *channel, unsigned int size)*/
/*{*/
/*    channel->write_chunk_buffer = (uint8_t *)NBN_Reallocator(channel->write_chunk_buffer, size);*/
/**/
/*    channel->write_chunk_buffer_size = size;*/
/*}*/
/**/
/*void NBN_Channel_ResizeReadChunkBuffer(NBN_Channel *channel, unsigned int size)*/
/*{*/
/*    channel->read_chunk_buffer = (uint8_t *)NBN_Reallocator(channel->read_chunk_buffer, size);*/
/**/
/*    channel->read_chunk_buffer_size = size;*/
/*}*/

void NBN_Channel_UpdateMessageLastSendTime(NBN_Channel *channel, NBN_Message *message, double time)
{
    NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[message->header.id % NBN_CHANNEL_BUFFER_SIZE];

    assert(slot->message.header.id == message->header.id);

    slot->last_send_time = time;
}

/* Unreliable ordered */

static bool UnreliableOrderedChannel_AddReceivedMessage(NBN_Channel *, NBN_Message *);
static bool UnreliableOrderedChannel_AddOutgoingMessage(NBN_Channel *, NBN_Message *);
static NBN_Message *UnreliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *);
static NBN_Message *UnreliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *, double);
static int UnreliableOrderedChannel_OnMessageSent(NBN_Channel *, NBN_Message *);

NBN_UnreliableOrderedChannel *NBN_UnreliableOrderedChannel_Create(void)
{
    NBN_UnreliableOrderedChannel *channel = (NBN_UnreliableOrderedChannel *)NBN_Allocator(sizeof(NBN_UnreliableOrderedChannel));

    channel->base.next_outgoing_message_pool_slot = 0;
    channel->base.AddReceivedMessage = UnreliableOrderedChannel_AddReceivedMessage;
    channel->base.AddOutgoingMessage = UnreliableOrderedChannel_AddOutgoingMessage;
    channel->base.GetNextRecvedMessage = UnreliableOrderedChannel_GetNextRecvedMessage;
    channel->base.GetNextOutgoingMessage = UnreliableOrderedChannel_GetNextOutgoingMessage;
    channel->base.OnOutgoingMessageAcked = NULL;
    channel->base.OnOutgoingMessageSent = UnreliableOrderedChannel_OnMessageSent;

    memset(channel->base.outgoing_message_pool, 0, sizeof(channel->base.outgoing_message_pool));

    channel->last_received_message_id = 0;
    channel->next_outgoing_message_slot = 0;

    return channel;
}

static bool UnreliableOrderedChannel_AddReceivedMessage(NBN_Channel *channel, NBN_Message *message)
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

static bool UnreliableOrderedChannel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *message)
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

static NBN_Message *UnreliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *channel)
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

static NBN_Message *UnreliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *channel, double time)
{
    (void)time;

    NBN_UnreliableOrderedChannel *unreliable_ordered_channel = (NBN_UnreliableOrderedChannel *)channel;

    NBN_MessageSlot *slot = &channel->outgoing_message_slot_buffer[unreliable_ordered_channel->next_outgoing_message_slot];

    if (slot->free)
        return NULL;

    slot->free = true;

    unreliable_ordered_channel->next_outgoing_message_slot =
        (unreliable_ordered_channel->next_outgoing_message_slot + 1) % NBN_CHANNEL_BUFFER_SIZE;

    return &slot->message;
}

static int UnreliableOrderedChannel_OnMessageSent(NBN_Channel *channel, NBN_Message *message)
{
    Connection_RecycleMessage(channel, message);

    return 0;
}

/* Reliable ordered */

static bool ReliableOrderedChannel_AddReceivedMessage(NBN_Channel *, NBN_Message *);
static bool ReliableOrderedChannel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *);
static NBN_Message *ReliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *);
static NBN_Message *ReliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *, double);
static int ReliableOrderedChannel_OnOutgoingMessageAcked(NBN_Channel *, uint16_t);

NBN_ReliableOrderedChannel *NBN_ReliableOrderedChannel_Create(void)
{
    NBN_ReliableOrderedChannel *channel = (NBN_ReliableOrderedChannel *)NBN_Allocator(sizeof(NBN_ReliableOrderedChannel));

    channel->base.next_outgoing_message_pool_slot = 0;
    channel->base.AddReceivedMessage = ReliableOrderedChannel_AddReceivedMessage;
    channel->base.AddOutgoingMessage = ReliableOrderedChannel_AddOutgoingMessage;
    channel->base.GetNextRecvedMessage = ReliableOrderedChannel_GetNextRecvedMessage;
    channel->base.GetNextOutgoingMessage = ReliableOrderedChannel_GetNextOutgoingMessage;
    channel->base.OnOutgoingMessageAcked = ReliableOrderedChannel_OnOutgoingMessageAcked;
    channel->base.OnOutgoingMessageSent = NULL;

    memset(channel->base.recved_message_slot_buffer, 0, sizeof(channel->base.recved_message_slot_buffer));
    memset(channel->base.outgoing_message_pool, 0, sizeof(channel->base.outgoing_message_pool));

    for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
        channel->ack_buffer[i] = false;

    channel->oldest_unacked_message_id = 0;
    channel->most_recent_message_id = 0;

    return channel;
}

static unsigned int ReliableOrderedChannel_ComputeMessageIdDelta(uint16_t id1, uint16_t id2)
{
    if (SEQUENCE_NUMBER_GT(id1, id2))
        return (id1 >= id2) ? id1 - id2 : ((0xFFFF + 1) - id2) + id1;
    else
        return (id2 >= id1) ? id2 - id1 : (((0xFFFF + 1) - id1) + id2) % 0xFFFF;
}

static bool ReliableOrderedChannel_AddReceivedMessage(NBN_Channel *channel, NBN_Message *message)
{
    NBN_ReliableOrderedChannel *reliable_ordered_channel = (NBN_ReliableOrderedChannel *)channel;
    unsigned int dt = ReliableOrderedChannel_ComputeMessageIdDelta(
        message->header.id, reliable_ordered_channel->most_recent_message_id);

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
    {
        Connection_RecycleMessage(channel, &slot->message);
    }

    memcpy(&slot->message, message, sizeof(NBN_Message));

    slot->free = false;

    return true;
}

static bool ReliableOrderedChannel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *message)
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

static NBN_Message *ReliableOrderedChannel_GetNextRecvedMessage(NBN_Channel *channel)
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

static NBN_Message *ReliableOrderedChannel_GetNextOutgoingMessage(NBN_Channel *channel, double time)
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
            (slot->last_send_time < 0 || time - slot->last_send_time >= NBN_MESSAGE_RESEND_DELAY))
        {
            return &slot->message;
        }

        msg_id++;
    }

    return NULL;
}

static int ReliableOrderedChannel_OnOutgoingMessageAcked(NBN_Channel *channel, uint16_t msg_id)
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

    Connection_RecycleMessage(channel, &slot->message);

    return 0;
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

static void Endpoint_Init(NBN_Endpoint *, uint32_t, bool);
static void Endpoint_Deinit(NBN_Endpoint *);
static NBN_Connection *Endpoint_CreateConnection(NBN_Endpoint *, uint32_t, int, void *);
static uint32_t Endpoint_BuildProtocolId(const char *);
static void Endpoint_RegisterChannel(NBN_Endpoint *, uint8_t, NBN_ChannelBuilder, NBN_ChannelDestructor);
static int Endpoint_ProcessReceivedPacket(NBN_Endpoint *, NBN_Packet *, NBN_Connection *);
static int Endpoint_EnqueueOutgoingMessage(NBN_Endpoint *, NBN_Connection *, NBN_Message *);
// static int Endpoint_SplitMessageIntoChunks(NBN_Message *, NBN_OutgoingMessage *, NBN_Channel *, NBN_MessageSerializer, unsigned int, NBN_MessageChunk **);
static void Endpoint_UpdateTime(NBN_Endpoint *);

static void Endpoint_Init(NBN_Endpoint *endpoint, uint32_t protocol_id, bool is_server)
{
    MemoryManager_Init();

    endpoint->is_server = is_server;
    endpoint->protocol_id = protocol_id;

    for (int i = 0; i < NBN_MAX_CHANNELS; i++)
    {
        endpoint->channel_builders[i] = NULL;
        endpoint->channel_destructors[i] = NULL;
    }

    NBN_EventQueue_Init(&endpoint->event_queue);

    /* Register library reserved reliable channel */
    Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_RESERVED_UNRELIABLE, (NBN_ChannelBuilder)NBN_UnreliableOrderedChannel_Create, (NBN_ChannelDestructor)NBN_Channel_Destroy);
    Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_RESERVED_RELIABLE, (NBN_ChannelBuilder)NBN_ReliableOrderedChannel_Create, (NBN_ChannelDestructor)NBN_Channel_Destroy);
    Endpoint_RegisterChannel(endpoint, NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES, (NBN_ChannelBuilder)NBN_ReliableOrderedChannel_Create, (NBN_ChannelDestructor)NBN_Channel_Destroy);

    // TODO: don't need to create that many channels by default
    for (int i = 0; i < NBN_MAX_CHANNELS - NBN_LIBRARY_RESERVED_CHANNELS; i++)
    {
        Endpoint_RegisterChannel(endpoint, i, (NBN_ChannelBuilder)NBN_ReliableOrderedChannel_Create, (NBN_ChannelDestructor)NBN_Channel_Destroy);
    }

#ifdef NBN_DEBUG
    endpoint->OnMessageAddedToRecvQueue = NULL;
#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_Init(&endpoint->packet_simulator, endpoint);
    NBN_PacketSimulator_Start(&endpoint->packet_simulator);
#endif

    Endpoint_UpdateTime(endpoint);
}

static void Endpoint_Deinit(NBN_Endpoint *endpoint)
{
    (void)endpoint;

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator_Stop(&endpoint->packet_simulator);
#endif

    MemoryManager_Deinit();
}

static NBN_Connection *Endpoint_CreateConnection(NBN_Endpoint *endpoint, uint32_t id, int driver_id, void *driver_data)
{
    NBN_Driver *driver = &nbn_drivers[driver_id];

    assert(driver->id >= 0);

    NBN_Connection *connection = NBN_Connection_Create(id, endpoint, driver, driver_data);

    for (unsigned int chan_id = 0; chan_id < NBN_MAX_CHANNELS; chan_id++)
    {
        NBN_ChannelBuilder builder = endpoint->channel_builders[chan_id]; 

        if (!builder) continue; 

        NBN_ChannelDestructor destructor = endpoint->channel_destructors[chan_id]; 

        assert(destructor);

        NBN_Channel *channel = builder();

        assert(channel);

        channel->id = chan_id;
        channel->destructor = destructor;

        connection->channels[chan_id] = channel;

        NBN_Connection_InitChannel(connection, channel);
    }

    return connection;
}

static uint32_t Endpoint_BuildProtocolId(const char *protocol_name)
{
    uint32_t protocol_id = 2166136261;

    for (unsigned int i = 0; i < strlen(protocol_name); i++)
    {
        protocol_id *= 16777619;
        protocol_id ^= protocol_name[i];
    }

    return protocol_id;
}

static void Endpoint_RegisterChannel(NBN_Endpoint *endpoint, uint8_t id, NBN_ChannelBuilder builder, NBN_ChannelDestructor destructor)
{
    if (endpoint->channel_builders[id] || endpoint->channel_destructors[id])
    {
        NBN_LogError("A channel with id %d already exists", id);
        NBN_Abort();
    }

    endpoint->channel_builders[id] = builder;
    endpoint->channel_destructors[id] = destructor;
}

static int Endpoint_ProcessReceivedPacket(NBN_Endpoint *endpoint, NBN_Packet *packet, NBN_Connection *connection)
{
    (void)endpoint;

    NBN_LogTrace("Received packet %d (conn id: %d, ack: %d, messages count: %d)", packet->header.seq_number,
                 connection->id, packet->header.ack, packet->header.messages_count);

    if (NBN_Connection_ProcessReceivedPacket(connection, packet, endpoint->time) < 0)
        return NBN_ERROR;

    connection->last_recv_packet_time = endpoint->time;
    connection->downloaded_bytes += packet->size;

    return 0;
}

static NBN_Message *Endpoint_CreateOutgoingMessage(
        NBN_Endpoint *endpoint, NBN_Channel *channel, uint8_t type, uint8_t *data, uint16_t length)
{
    NBN_Assert(channel);

    NBN_Message *message = &channel->outgoing_message_pool[channel->next_outgoing_message_pool_slot];

    if (message->ref_count != 0)
    {
        NBN_LogError("Outgoing message pool has run out of space");

        return NULL;
    }

    message->header = (NBN_MessageHeader){-1, length, type, channel->id};
    message->sender = NULL;
    message->data = data;
    message->ref_count = 0;

    channel->next_outgoing_message_pool_slot = (channel->next_outgoing_message_pool_slot + 1) % NBN_CHANNEL_OUTGOING_MESSAGE_POOL_SIZE;

    return message;
}

static int Endpoint_EnqueueOutgoingMessage(NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_Message *message)
{
    NBN_Assert(!connection->is_closed || message->header.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Assert(!connection->is_stale);

    NBN_Channel *channel = connection->channels[message->header.channel_id];

    NBN_Assert(channel);

    if (message->header.length > NBN_PACKET_MAX_DATA_SIZE)
    {
        /*NBN_MessageChunk *chunks[NBN_CHANNEL_CHUNKS_BUFFER_SIZE];
        int chunk_count = Endpoint_SplitMessageIntoChunks(&message, outgoing_msg, channel, msg_serializer, message_size, chunks);

        assert(chunk_count <= NBN_CHANNEL_CHUNKS_BUFFER_SIZE);

        if (chunk_count < 0)
        {
            NBN_LogError("Failed to split message into chunks");

            return NBN_ERROR;
        }

        assert(outgoing_msg->ref_count == 0);
        outgoing_msg->ref_count = chunk_count;

        NBN_Channel *channel = connection->channels[channel_id];

        for (int i = 0; i < chunk_count; i++)
        {
            NBN_OutgoingMessage *chunk_outgoing_msg = Endpoint_CreateOutgoingMessage(endpoint, channel, NBN_MESSAGE_CHUNK_TYPE, chunks[i]);

            if (chunk_outgoing_msg == NULL)
                return NBN_ERROR;

            if (Endpoint_EnqueueOutgoingMessage(endpoint, connection, chunk_outgoing_msg, channel_id) < 0)
            {
                NBN_LogError("Failed to enqueue message chunk");

                return NBN_ERROR;
            }
        }*/

        // TODO: fix chunks
        assert(false);
    }
    else
    {
        assert(message->ref_count == 0);

        message->ref_count = 1;

        NBN_LogTrace("Enqueue message of type %d on channel %d", message->header.type, channel->id);

        if (!channel->AddOutgoingMessage(channel, message))
        {
            NBN_LogError("Failed to enqueue outgoing message of type %d on channel %d", message->header.type, message->header.channel_id);

            return NBN_ERROR;
        }
    }

    return 0;
}

// TODO
/*static int Endpoint_SplitMessageIntoChunks(
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
    return 0;
}*/

static void Endpoint_UpdateTime(NBN_Endpoint *endpoint)
{
#ifdef NBNET_WINDOWS
    endpoint->time = GetTickCount64() / 1000.0;
#else
    static struct timespec tp;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &tp) < 0)
    {
        NBN_LogError("gettimeofday() failed");
        NBN_Abort();
    }

    endpoint->time = tp.tv_sec + (tp.tv_nsec / (double)1e9);
#endif // NBNET_WINDOWS
}

#pragma endregion /* NBN_Endpoint */

#pragma region Network driver

static void ClientDriver_OnPacketReceived(NBN_Packet *packet);
static int ServerDriver_OnClientConnected(NBN_Connection *);
static int ServerDriver_OnClientPacketReceived(NBN_Packet *);

void NBN_Driver_Register(int id, const char *name, NBN_DriverImplementation implementation)
{
    // driver id must be valid
    assert(id >= 0 && id < NBN_MAX_DRIVERS);

    NBN_Driver *driver = &nbn_drivers[id];

    // driver id must be unique
    assert(driver->id == -1);

    driver->id = id;
    driver->name = name;
    driver->impl = implementation;

    NBN_LogInfo("Registered driver (ID: %d, Name: %s)", id, name);

    nbn_driver_count++;
}

int NBN_Driver_RaiseEvent(NBN_DriverEvent ev, void *data)
{
    switch (ev)
    {
    case NBN_DRIVER_CLI_PACKET_RECEIVED:
        ClientDriver_OnPacketReceived((NBN_Packet *)data);
        break;

    case NBN_DRIVER_SERV_CLIENT_CONNECTED:
        return ServerDriver_OnClientConnected((NBN_Connection *)data);

    case NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED:
        return ServerDriver_OnClientPacketReceived((NBN_Packet *)data);
    }

    return 0;
}

#pragma endregion /* Network driver */

#pragma region NBN_GameClient

NBN_GameClient nbn_game_client;

static int GameClient_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static int GameClient_HandleEvent(void);
static int GameClient_HandleMessageReceivedEvent(void);

int NBN_GameClient_Start(const char *protocol_name, const char *host, uint16_t port)
{
    return NBN_GameClient_StartEx(protocol_name, host, port, NULL, 0);
}

int NBN_GameClient_StartEx(const char *protocol_name, const char *host, uint16_t port, uint8_t *data, unsigned int length)
{
    if (nbn_driver_count < 1)
    {
        NBN_LogError("At least one network driver has to be registered");
        NBN_Abort();
    }

    uint32_t protocol_id = Endpoint_BuildProtocolId(protocol_name);

    Endpoint_Init(&nbn_game_client.endpoint, protocol_id, false);

    nbn_game_client.server_connection = NULL;
    nbn_game_client.is_connected = false;
    nbn_game_client.closed_code = -1;

    for (unsigned int i = 0; i < NBN_MAX_DRIVERS; i++)
    {
        NBN_Driver *driver = &nbn_drivers[i];

        if (driver->id < 0) continue;

        if (driver->impl.cli_start(protocol_id, host, port) < 0)
        {
            NBN_LogError("Failed to start driver %s", driver->name);
            return NBN_ERROR;
        }
    }

    uint8_t *msg = NULL;
    unsigned int msg_length = 0;

    if (data)
    {
        NBN_Assert(length > 0 && length <= NBN_CONNECTION_DATA_MAX_SIZE);

        msg_length = length + 4;
        msg = NBN_Allocator(msg_length); // TODO: pooling

        NBN_Writer writer;

        NBN_Writer_Init(&writer, msg, msg_length);
        NBN_Writer_WriteUInt32(&writer, msg_length);
        NBN_Writer_WriteBytes(&writer, data, length);
    }

    if (NBN_GameClient_SendMessage(
                NBN_CONNECTION_REQUEST_MESSAGE_TYPE,
                NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES,
                msg,
                msg_length) < 0)
        return NBN_ERROR;

    NBN_LogInfo("Started");

    return 0;
}

void NBN_GameClient_Stop(void)
{
    // Poll remaining events to clear the event queue
    while (NBN_GameClient_Poll() != NBN_NO_EVENT) {}

    if (nbn_game_client.server_connection)
    {
        if (!nbn_game_client.server_connection->is_closed && !nbn_game_client.server_connection->is_stale)
        {
            NBN_LogInfo("Disconnecting...");

            if (NBN_GameClient_SendMessage(NBN_DISCONNECTION_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES, NULL, 0) < 0)
            {
                NBN_LogError("Failed to send disconnection message");
            }

            if (NBN_GameClient_SendPackets() < 0)
            {
                NBN_LogError("Failed to send packets");
            }

            nbn_game_client.server_connection->is_closed = true;

            NBN_LogInfo("Disconnected");
        }

        NBN_Connection_Destroy(nbn_game_client.server_connection);
        nbn_game_client.server_connection = NULL;
    } 

    NBN_LogInfo("Stopping all drivers...");

    for (unsigned int i = 0; i < NBN_MAX_DRIVERS; i++)
    {
        NBN_Driver *driver = &nbn_drivers[i];

        if (driver->id < 0) continue;

        driver->impl.cli_stop();
    }

    nbn_game_client.is_connected = false;
    nbn_game_client.closed_code = -1;
    nbn_game_client.server_data_len = 0;

    memset(nbn_game_client.server_data, 0, sizeof(nbn_game_client.server_data));
    Endpoint_Deinit(&nbn_game_client.endpoint);

    NBN_LogInfo("Stopped");
}

unsigned int NBN_GameClient_ReadServerData(uint8_t *data)
{
    memcpy(data, nbn_game_client.server_data, nbn_game_client.server_data_len);

    return nbn_game_client.server_data_len;
}

int NBN_GameClient_Poll(void)
{
    Endpoint_UpdateTime(&nbn_game_client.endpoint);

    if (nbn_game_client.server_connection->is_stale)
        return NBN_NO_EVENT;

    if (NBN_EventQueue_IsEmpty(&nbn_game_client.endpoint.event_queue))
    {
        if (NBN_Connection_CheckIfStale(nbn_game_client.server_connection, nbn_game_client.endpoint.time))
        {
            nbn_game_client.server_connection->is_stale = true;
            nbn_game_client.is_connected = false;

            NBN_LogInfo("Server connection is stale. Disconnected.");

            NBN_Event e;

            e.type = NBN_DISCONNECTED;
            e.data.connection = (NBN_Connection *)NULL;

            if (!NBN_EventQueue_Enqueue(&nbn_game_client.endpoint.event_queue, e))
                return NBN_ERROR;
        }
        else
        {
            for (unsigned int i = 0; i < NBN_MAX_DRIVERS; i++)
            {
                NBN_Driver *driver = &nbn_drivers[i];

                if (driver->id < 0) continue;

                if (driver->impl.cli_recv_packets() < 0)
                {
                    NBN_LogError("Failed to read packets from driver %s", driver->name);
                    return NBN_ERROR;
                }
            }

            for (unsigned int i = 0; i < NBN_MAX_CHANNELS; i++)
            {
                NBN_Channel *channel = nbn_game_client.server_connection->channels[i];

                if (channel)
                {
                    NBN_Message *msg;

                    while ((msg = channel->GetNextRecvedMessage(channel)) != NULL)
                    {
                        NBN_LogTrace("Got message %d of type %d from the recv queue", msg->header.id, msg->header.type);

                        if (GameClient_ProcessReceivedMessage(msg, nbn_game_client.server_connection) < 0)
                        {
                            NBN_LogError("Failed to process received message");

                            return NBN_ERROR;
                        }
                    }
                }
            }

            Connection_UpdateAverageDownloadBandwidth(nbn_game_client.server_connection, nbn_game_client.endpoint.time);

            nbn_game_client.server_connection->last_read_packets_time = nbn_game_client.endpoint.time;
        }
    }

    bool ret = NBN_EventQueue_Dequeue(&nbn_game_client.endpoint.event_queue, &nbn_game_client.last_event);

    return ret ? GameClient_HandleEvent() : NBN_NO_EVENT;
}

int NBN_GameClient_SendPackets(void)
{
    return NBN_Connection_FlushChannels(
            nbn_game_client.server_connection,
            nbn_game_client.endpoint.protocol_id,
            nbn_game_client.endpoint.time);
}

int NBN_GameClient_SendMessage(uint8_t type, uint8_t channel_id, uint8_t *data, uint16_t length)
{
    NBN_Message *message = Endpoint_CreateOutgoingMessage(
            &nbn_game_client.endpoint,
            nbn_game_client.server_connection->channels[channel_id],
            type,
            data,
            length);

    NBN_Assert(message);

    if (Endpoint_EnqueueOutgoingMessage(&nbn_game_client.endpoint, nbn_game_client.server_connection, message) < 0)
    {
        NBN_LogError("Failed to create outgoing message");

        return NBN_ERROR;
    }

    return 0;
}


int NBN_GameClient_SendUnreliableMessage(uint8_t type, uint8_t *data, uint16_t length)
{
    return NBN_GameClient_SendMessage(type, NBN_CHANNEL_RESERVED_UNRELIABLE, data, length);
}

int NBN_GameClient_SendReliableMessage(uint8_t type, uint8_t *data, uint16_t length)
{
    return NBN_GameClient_SendMessage(type, NBN_CHANNEL_RESERVED_RELIABLE, data, length);
}

NBN_Connection *NBN_GameClient_CreateServerConnection(int driver_id, void *driver_data)
{
    NBN_Connection *server_connection = Endpoint_CreateConnection(&nbn_game_client.endpoint, 0, driver_id, driver_data);

#ifdef NBN_DEBUG
    server_connection->OnMessageAddedToRecvQueue = nbn_game_client.endpoint.OnMessageAddedToRecvQueue;
#endif

    nbn_game_client.server_connection = server_connection;

    return server_connection;
}

NBN_MessageInfo NBN_GameClient_GetMessageInfo(void)
{
    assert(nbn_game_client.last_event.type == NBN_MESSAGE_RECEIVED);

    return nbn_game_client.last_event.data.message_info;
}

NBN_ConnectionStats NBN_GameClient_GetStats(void)
{
    return nbn_game_client.server_connection->stats;
}

int NBN_GameClient_GetServerCloseCode(void)
{
    return nbn_game_client.closed_code;
}

bool NBN_GameClient_IsConnected(void)
{
    return nbn_game_client.is_connected;
}

#ifdef NBN_DEBUG

void NBN_GameClient_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
    case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
        nbn_game_client.endpoint.OnMessageAddedToRecvQueue = (void (*)(NBN_Connection *, NBN_Message *))cb;
        break;
    }
}

#endif /* NBN_DEBUG */

static int GameClient_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *server_connection)
{
    assert(nbn_game_client.server_connection == server_connection);

    NBN_Event ev;

    ev.type = NBN_MESSAGE_RECEIVED;

    if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        /*NBN_Channel *channel = server_connection->channels[message->header.channel_id];*/
        /**/
        /*if (!NBN_Channel_AddChunk(channel, message))*/
        /*    return 0;*/
        /**/
        /*NBN_Message complete_message;*/
        /**/
        /*if (NBN_Channel_ReconstructMessageFromChunks(channel, server_connection, &complete_message) < 0)*/
        /*{*/
        /*    NBN_LogError("Failed to reconstruct message from chunks");*/
        /**/
        /*    return NBN_ERROR;*/
        /*}*/
        /**/
        /*NBN_MessageInfo msg_info = {complete_message.header.type, complete_message.header.channel_id, complete_message.data, 0};*/
        /**/
        /*ev.data.message_info = msg_info;*/
    }
    else
    {
        NBN_MessageInfo msg_info = {message->header.type, message->header.channel_id, message->data, message->header.length, 0};

        ev.data.message_info = msg_info;
    }

    if (!NBN_EventQueue_Enqueue(&nbn_game_client.endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static int GameClient_HandleEvent(void)
{
    switch (nbn_game_client.last_event.type)
    {
    case NBN_MESSAGE_RECEIVED:
        return GameClient_HandleMessageReceivedEvent();

    default:
        return nbn_game_client.last_event.type;
    }
}

static int GameClient_HandleMessageReceivedEvent(void)
{
    NBN_MessageInfo message_info = nbn_game_client.last_event.data.message_info;

    int ret = NBN_NO_EVENT;

    if (message_info.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE)
    {
        nbn_game_client.is_connected = false;

        NBN_Reader reader;

        NBN_Reader_Init(&reader, message_info.data, message_info.length);

        if (NBN_Reader_ReadInt32(&reader, &nbn_game_client.closed_code) < 0)
        {
            NBN_LogError("Failed to read code from client closed message");

            return NBN_ERROR;
        }

        ret = NBN_DISCONNECTED;
    }
    else if (message_info.type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE)
    {
        NBN_Reader reader;

        NBN_Reader_Init(&reader, message_info.data, message_info.length);

        unsigned int length;

        if (NBN_Reader_ReadUInt32(&reader, &length) < 0)
        {
            return NBN_ERROR;
        }

        if (length > NBN_SERVER_DATA_MAX_SIZE)
        {
            NBN_LogError("Received an invalid client accepted message");

            return NBN_ERROR;
        }

        if (NBN_Reader_ReadBytes(&reader, nbn_game_client.server_data, length) < 0)
        {
            return NBN_ERROR;
        }

        nbn_game_client.server_data_len = length;
        nbn_game_client.is_connected = true;
        ret = NBN_CONNECTED;
    } 
    else
    {
        ret = NBN_MESSAGE_RECEIVED;
    }

    return ret;
}

#pragma endregion /* NBN_GameClient */

#pragma region Game client driver

static void ClientDriver_OnPacketReceived(NBN_Packet *packet)
{
    // packets from server should always be valid
    if (Endpoint_ProcessReceivedPacket(&nbn_game_client.endpoint, packet, nbn_game_client.server_connection) < 0)
    {
        NBN_LogError("Received invalid packet from server");
        NBN_Abort();
    }
}

#pragma endregion /* Game Client driver */

#pragma region NBN_GameServer

NBN_GameServer nbn_game_server;

static int GameServer_SendMessageTo(NBN_Connection *client, uint8_t msg_type, uint8_t channel_id, uint8_t *data, uint16_t length);
static int GameServer_AddClient(NBN_Connection *);
static int GameServer_CloseClientWithCode(NBN_Connection *client, int code, bool disconnection);
static void GameServer_AddClientToClosedList(NBN_Connection *client);
static unsigned int GameServer_GetClientCount(void);
static int GameServer_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static int GameServer_CloseStaleClientConnections(void);
static void GameServer_RemoveClosedClientConnections(void);
static int GameServer_HandleEvent(void);
static int GameServer_HandleMessageReceivedEvent(void);

int NBN_GameServer_Start(const char *protocol_name, uint16_t port)
{
    return NBN_GameServer_StartEx(protocol_name, port);
}

int NBN_GameServer_StartEx(const char *protocol_name, uint16_t port)
{
    if (nbn_driver_count < 1)
    {
        NBN_LogError("At least one network driver has to be registered");
        NBN_Abort();
    }

    uint32_t protocol_id = Endpoint_BuildProtocolId(protocol_name);

    Endpoint_Init(&nbn_game_server.endpoint, protocol_id, true);

    if ((nbn_game_server.clients = NBN_ConnectionVector_Create()) == NULL)
    {
        NBN_LogError("Failed to create connections vector");
        NBN_Abort();
    }

    if ((nbn_game_server.clients_table = NBN_ConnectionTable_Create()) == NULL)
    {
        NBN_LogError("Failed to create connections table");
        NBN_Abort();
    }

    nbn_game_server.closed_clients_head = NULL;

    for (unsigned int i = 0; i < NBN_MAX_DRIVERS; i++)
    {
        NBN_Driver *driver = &nbn_drivers[i];

        if (driver->id < 0) continue;

        if (driver->impl.serv_start(protocol_id, port) < 0)
        {
            NBN_LogError("Failed to start driver %s", driver->name);
            return NBN_ERROR;
        }
    }

    NBN_LogInfo("Started");

    return 0;
}

void NBN_GameServer_Stop(void)
{
    // Poll remaning events to clear the event queue
    while (NBN_GameServer_Poll() != NBN_NO_EVENT) {}

    for (unsigned int i = 0; i < nbn_game_server.clients->count; i++)
    {
        NBN_Connection_Destroy(nbn_game_server.clients->connections[i]);
    }

    NBN_ConnectionVector_Destroy(nbn_game_server.clients);
    NBN_ConnectionTable_Destroy(nbn_game_server.clients_table);

    for (unsigned int i = 0; i < NBN_MAX_DRIVERS; i++)
    {
        NBN_Driver *driver = &nbn_drivers[i];

        if (driver->id < 0) continue;

        driver->impl.serv_stop();
    }

    // Free closed clients list
    NBN_ConnectionListNode *current = nbn_game_server.closed_clients_head;

    while (current)
    {
        NBN_ConnectionListNode *next = current->next;

        NBN_Deallocator(current);

        current = next;
    }

    nbn_game_server.closed_clients_head = NULL;
    Endpoint_Deinit(&nbn_game_server.endpoint);

    NBN_LogInfo("Stopped");
}

int NBN_GameServer_Poll(void)
{
    Endpoint_UpdateTime(&nbn_game_server.endpoint);

    if (NBN_EventQueue_IsEmpty(&nbn_game_server.endpoint.event_queue))
    {
        if (GameServer_CloseStaleClientConnections() < 0)
            return NBN_ERROR;

        for (unsigned int i = 0; i < NBN_MAX_DRIVERS; i++)
        {
            NBN_Driver *driver = &nbn_drivers[i];

            if (driver->id < 0) continue;

            if (driver->impl.serv_recv_packets() < 0)
            {
                NBN_LogError("Failed to read packets from driver %s", driver->name);
                return NBN_ERROR;
            }
        }

        nbn_game_server.stats.download_bandwidth = 0;

        for (unsigned int i = 0; i < nbn_game_server.clients->count; i++)
        {
            NBN_Connection *client = nbn_game_server.clients->connections[i];

            for (unsigned int i = 0; i < NBN_MAX_CHANNELS; i++)
            {
                NBN_Channel *channel = client->channels[i];

                if (channel)
                {
                    NBN_Message *msg;

                    while ((msg = channel->GetNextRecvedMessage(channel)) != NULL)
                    {
                        if (GameServer_ProcessReceivedMessage(msg, client) < 0)
                        {
                            NBN_LogError("Failed to process received message");

                            return NBN_ERROR;
                        }
                    }
                }
            }

            if (!client->is_closed)
                Connection_UpdateAverageDownloadBandwidth(client, nbn_game_server.endpoint.time);

            nbn_game_server.stats.download_bandwidth += client->stats.download_bandwidth;
            client->last_read_packets_time = nbn_game_server.endpoint.time;
        }

        GameServer_RemoveClosedClientConnections();
    }

    while (NBN_EventQueue_Dequeue(&nbn_game_server.endpoint.event_queue, &nbn_game_server.last_event))
    {
        int ev = GameServer_HandleEvent();

        if (ev != NBN_SKIP_EVENT) return ev;
    }

    return NBN_NO_EVENT;
}

int NBN_GameServer_SendPackets(void)
{
    nbn_game_server.stats.upload_bandwidth = 0;

    GameServer_RemoveClosedClientConnections();

    for (unsigned int i = 0; i < nbn_game_server.clients->count; i++)
    {
        NBN_Connection *client = nbn_game_server.clients->connections[i];

        assert(!(client->is_closed && client->is_stale));

        if (
                !client->is_stale &&
                NBN_Connection_FlushChannels(
                    client,
                    nbn_game_server.endpoint.protocol_id,
                    nbn_game_server.endpoint.time) < 0
           ) {
            return NBN_ERROR;
        }

        nbn_game_server.stats.upload_bandwidth += client->stats.upload_bandwidth;
    }

    return 0;
}

NBN_Connection *NBN_GameServer_CreateClientConnection(int driver_id, void *driver_data, uint32_t conn_id)
{
    assert(conn_id > 0); // Connection IDs start at 1

    NBN_Connection *client = Endpoint_CreateConnection(&nbn_game_server.endpoint, conn_id, driver_id, driver_data);

#ifdef NBN_DEBUG
    client->OnMessageAddedToRecvQueue = nbn_game_server.endpoint.OnMessageAddedToRecvQueue;
#endif

    return client;
}

int NBN_GameServer_CloseClientWithCode(NBN_ConnectionHandle connection_handle, int code)
{
    NBN_Connection *client = NBN_ConnectionTable_Get(nbn_game_server.clients_table, connection_handle);

    if (client == NULL)
    {
        NBN_LogError("Client %d does not exist", connection_handle);
        return NBN_ERROR;
    }

    return GameServer_CloseClientWithCode(client, code, false);
}

int NBN_GameServer_CloseClient(NBN_ConnectionHandle connection_handle)
{
    NBN_Connection *client = NBN_ConnectionTable_Get(nbn_game_server.clients_table, connection_handle);

    if (client == NULL)
    {
        NBN_LogError("Client %d does not exist", connection_handle);
        return NBN_ERROR;
    }

    return GameServer_CloseClientWithCode(client, -1, false);
}

int NBN_GameServer_SendMessageTo(NBN_ConnectionHandle connection_handle, uint8_t msg_type, uint8_t channel_id, uint8_t *data, uint16_t length)
{
    NBN_Connection *client = NBN_ConnectionTable_Get(nbn_game_server.clients_table, connection_handle);

    if (client == NULL)
    {
        NBN_LogWarning("Cannot send message to client %d (does not exist)", connection_handle);
        return 0;
    }

    return GameServer_SendMessageTo(client, msg_type, channel_id, data, length);
}

int NBN_GameServer_SendUnreliableMessageTo(NBN_ConnectionHandle connection_handle, uint8_t msg_type, uint8_t *data, uint16_t length)
{
    return NBN_GameServer_SendMessageTo(connection_handle, msg_type, NBN_CHANNEL_RESERVED_UNRELIABLE, data, length);
}

int NBN_GameServer_SendReliableMessageTo(NBN_ConnectionHandle connection_handle, uint8_t msg_type, uint8_t *data, uint16_t length)
{
    return NBN_GameServer_SendMessageTo(connection_handle, msg_type, NBN_CHANNEL_RESERVED_RELIABLE, data, length);
}

int NBN_GameServer_AcceptIncomingConnection(void)
{
    return NBN_GameServer_AcceptIncomingConnectionWithData(NULL, 0);
}

int NBN_GameServer_AcceptIncomingConnectionWithData(uint8_t *data, unsigned int length)
{
    NBN_Assert(nbn_game_server.last_event.type == NBN_NEW_CONNECTION);
    NBN_Assert(nbn_game_server.last_event.data.connection != NULL);
    NBN_Assert(data != NULL); 

    NBN_Connection *client = nbn_game_server.last_event.data.connection;
    uint8_t *msg = NULL;
    unsigned int msg_length = 0;

    if (data)
    {
        NBN_Assert(length > 0);
        NBN_Assert(length <= NBN_SERVER_DATA_MAX_SIZE);

        msg = NBN_Allocator(length); // TODO: pooling

        NBN_Writer writer;

        NBN_Writer_Init(&writer, msg, length);
        NBN_Writer_WriteUInt32(&writer, length);
        NBN_Writer_WriteBytes(&writer, data, length);

        msg_length = writer.position;
    }

    if (GameServer_SendMessageTo(client, NBN_CLIENT_ACCEPTED_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES, msg, msg_length) < 0)
        return NBN_ERROR;

    client->is_accepted = true;

    NBN_LogTrace("Client %d has been accepted", client->id);

    return 0;
}

int NBN_GameServer_RejectIncomingConnectionWithCode(int code)
{
    assert(nbn_game_server.last_event.type == NBN_NEW_CONNECTION);
    assert(nbn_game_server.last_event.data.connection != NULL);

    return GameServer_CloseClientWithCode(nbn_game_server.last_event.data.connection, code, false);
}

int NBN_GameServer_RejectIncomingConnection(void)
{
    return NBN_GameServer_RejectIncomingConnectionWithCode(-1);
}

NBN_ConnectionHandle NBN_GameServer_GetIncomingConnection(void)
{
    assert(nbn_game_server.last_event.type == NBN_NEW_CONNECTION);
    assert(nbn_game_server.last_event.data.connection != NULL);

    return nbn_game_server.last_event.data.connection->id;
}

unsigned int NBN_GameServer_ReadIncomingConnectionData(uint8_t *data)
{
    assert(nbn_game_server.last_event.type == NBN_NEW_CONNECTION);

    memcpy(data, nbn_game_server.last_connection_data, nbn_game_server.last_connection_data_len);

    return nbn_game_server.last_connection_data_len;
}

NBN_ConnectionHandle NBN_GameServer_GetDisconnectedClient(void)
{
    assert(nbn_game_server.last_event.type == NBN_CLIENT_DISCONNECTED);

    return nbn_game_server.last_event.data.connection_handle;
}

NBN_MessageInfo NBN_GameServer_GetMessageInfo(void)
{
    assert(nbn_game_server.last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    return nbn_game_server.last_event.data.message_info;
}

NBN_GameServerStats NBN_GameServer_GetStats(void)
{
    return nbn_game_server.stats;
}

#ifdef NBN_DEBUG

void NBN_GameServer_Debug_RegisterCallback(NBN_ConnectionDebugCallback cb_type, void *cb)
{
    switch (cb_type)
    {
    case NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE:
        nbn_game_server.endpoint.OnMessageAddedToRecvQueue = (void (*)(NBN_Connection *, NBN_Message *))cb;
        break;
    }
}

#endif /* NBN_DEBUG */

static int GameServer_SendMessageTo(NBN_Connection *client, uint8_t type, uint8_t channel_id, uint8_t *data, uint16_t length)
{
    NBN_Channel *channel = client->channels[channel_id];
    NBN_Message *message = Endpoint_CreateOutgoingMessage(&nbn_game_server.endpoint, channel, type, data, length);

    if (message == NULL)
    {
        NBN_LogError("Failed to create outgoing message");

        return NBN_ERROR;
    }

    /* The only message type we can send to an unaccepted client is a NBN_ClientAcceptedMessage message */
    NBN_Assert(client->is_accepted || message->header.type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE);

    if (Endpoint_EnqueueOutgoingMessage(&nbn_game_server.endpoint, client, message) < 0)
    {
        NBN_LogError("Failed to create outgoing message for client %d", client->id);

        /* Do not close the client if we failed to send the close client message to avoid infinite loops */
        if (message->header.type != NBN_CLIENT_CLOSED_MESSAGE_TYPE)
        {
            GameServer_CloseClientWithCode(client, -1, false);

            return NBN_ERROR;
        }
    }

    return 0;
}

static int GameServer_AddClient(NBN_Connection *client)
{
    if (nbn_game_server.clients->count >= NBN_MAX_CLIENTS)
    {
        NBN_LogError("Cannot accept new client: too many clients");

        return NBN_ERROR;
    }

    NBN_ConnectionVector_Add(nbn_game_server.clients, client);
    NBN_ConnectionTable_Add(nbn_game_server.clients_table, client);

    return 0;
}

static int GameServer_CloseClientWithCode(NBN_Connection *client, int code, bool disconnection)
{
    if (!client->is_closed && client->is_accepted)
    {
        if (!disconnection)
        {
            NBN_Event e;

            e.type = NBN_CLIENT_DISCONNECTED;
            e.data.connection_handle = client->id;

            if (!NBN_EventQueue_Enqueue(&nbn_game_server.endpoint.event_queue, e))
                return NBN_ERROR;
        }
    }

    if (client->is_stale)
    {
        NBN_LogDebug("Closing stale connection %d", client->id);

        GameServer_AddClientToClosedList(client);
        client->is_closed = true;

        return 0;
    }

    NBN_LogDebug("Closing active connection %d (will send a disconnection message)", client->id);

    GameServer_AddClientToClosedList(client);
    client->is_closed = true;

    if (!disconnection)
    {
        NBN_LogDebug("Send close message for client %d (code: %d)", client->id, code);

        unsigned int msg_length = 4;
        uint8_t *msg = NBN_Allocator(msg_length); // TODO: pooling

        NBN_Writer writer;

        NBN_Writer_Init(&writer, msg, msg_length);
        NBN_Writer_WriteInt32(&writer, code);
        GameServer_SendMessageTo(client, NBN_CLIENT_CLOSED_MESSAGE_TYPE, NBN_CHANNEL_RESERVED_LIBRARY_MESSAGES, msg, msg_length);
    }

    return 0;
}

static void GameServer_AddClientToClosedList(NBN_Connection *client)
{
    if (client->is_closed) return;

    NBN_ConnectionListNode *node = (NBN_ConnectionListNode *)NBN_Allocator(sizeof(NBN_ConnectionListNode));

    node->conn = client;
    node->next = NULL;

    if (nbn_game_server.closed_clients_head == NULL)
    {
        // list is empty
        nbn_game_server.closed_clients_head = node;
        node->prev = NULL;
    }
    else
    {
        // list is not empty, add node at the end
        NBN_ConnectionListNode *tail = nbn_game_server.closed_clients_head;

        while (tail->next != NULL) tail = tail->next;

        node->prev = tail;
        tail->next = node;
    }
}

static unsigned int GameServer_GetClientCount(void)
{
    return nbn_game_server.clients->count;
}

static int GameServer_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *client)
{
    NBN_Event ev;

    ev.type = NBN_CLIENT_MESSAGE_RECEIVED;

    if (message->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        /*NBN_Channel *channel = client->channels[message->header.channel_id];*/
        /**/
        /*if (!NBN_Channel_AddChunk(channel, message))*/
        /*    return 0;*/
        /**/
        /*NBN_Message complete_message;*/
        /**/
        /*if (NBN_Channel_ReconstructMessageFromChunks(channel, client, &complete_message) < 0)*/
        /*{*/
        /*    NBN_LogError("Failed to reconstruct message from chunks");*/
        /**/
        /*    return NBN_ERROR;*/
        /*}*/
        /**/
        /*NBN_MessageInfo msg_info = {complete_message.header.type, complete_message.header.channel_id, complete_message.data, client->id};*/
        /**/
        /*ev.data.message_info = msg_info;*/
    }
    else
    {
        NBN_MessageInfo msg_info = {message->header.type, message->header.channel_id, message->data, client->id};

        ev.data.message_info = msg_info;
    }

    if (!NBN_EventQueue_Enqueue(&nbn_game_server.endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static int GameServer_CloseStaleClientConnections(void)
{
    for (unsigned int i = 0; i < nbn_game_server.clients->count; i++)
    {
        NBN_Connection *client = nbn_game_server.clients->connections[i];

        if (!client->is_stale && NBN_Connection_CheckIfStale(client, nbn_game_server.endpoint.time))
        {
            NBN_LogInfo("Client %d connection is stale, closing it.", client->id);

            client->is_stale = true;

            if (GameServer_CloseClientWithCode(client, -1, false) < 0)
                return NBN_ERROR;
        }
    }

    return 0;
}

static void GameServer_RemoveClosedClientConnections(void)
{
    NBN_ConnectionListNode *current = nbn_game_server.closed_clients_head;

    while (current)
    {
        NBN_ConnectionListNode *prev = current->prev;
        NBN_ConnectionListNode *next = current->next;
        NBN_Connection *client = current->conn;

        assert(client->id > 0);

        if (client->is_stale)
        {
            NBN_LogDebug("Remove closed client connection (ID: %d)", client->id);

            client->driver->impl.serv_remove_connection(client); // Notify the driver to remove the connection

            // Remove the connection from the connections vector and table

            uint32_t rm_conn_id = NBN_ConnectionVector_RemoveAt(nbn_game_server.clients, client->vector_pos);

            if (rm_conn_id != client->id)
            {
                NBN_LogError("Failed to remove client connection from connections vector");
                NBN_Abort();
            }

            bool table_rm = NBN_ConnectionTable_Remove(nbn_game_server.clients_table, client->id);

            if (!table_rm)
            {
                NBN_LogError("Failed to remove client connection from connections table");
                NBN_Abort();
            }

            // Destroy the connection

            NBN_Connection_Destroy(client);

            // Remove the connection from the closed clients list

            NBN_Deallocator(current);

            if (current == nbn_game_server.closed_clients_head)
            {
                // delete the head of the list
                NBN_ConnectionListNode *new_head = next;

                if (new_head)
                {
                    new_head->prev = NULL;
                }

                nbn_game_server.closed_clients_head = new_head;
            }
            else
            {
                // delete a node in the middle of the list
                prev->next = next;

                if (next) next->prev = prev;
            }
        }

        current = next;
    }
}

static int GameServer_HandleEvent(void)
{
    return nbn_game_server.last_event.type == NBN_CLIENT_MESSAGE_RECEIVED ?
        GameServer_HandleMessageReceivedEvent() :
        nbn_game_server.last_event.type;
}

static int GameServer_HandleMessageReceivedEvent(void)
{
    NBN_MessageInfo message_info = nbn_game_server.last_event.data.message_info;
    NBN_Connection *sender = NBN_ConnectionTable_Get(nbn_game_server.clients_table, message_info.sender);

    if (sender == NULL)
    {
        NBN_LogTrace("Received message from unknown client (ID: %d)", message_info.sender);

        return NBN_SKIP_EVENT;
    }

    if (sender->is_closed || sender->is_stale)
        return NBN_SKIP_EVENT;

    if (message_info.type == NBN_DISCONNECTION_MESSAGE_TYPE)
    {
        NBN_LogInfo("Received disconnection message from client %d", sender->id);

        if (GameServer_CloseClientWithCode(sender, -1, true) < 0)
            return NBN_ERROR;

        sender->is_stale = true;

        nbn_game_server.last_event.type = NBN_CLIENT_DISCONNECTED;
        nbn_game_server.last_event.data.connection_handle = sender->id;

        GameServer_RemoveClosedClientConnections();

        return NBN_CLIENT_DISCONNECTED;
    }

    if (message_info.type != NBN_CONNECTION_REQUEST_MESSAGE_TYPE)
    {
        return NBN_CLIENT_MESSAGE_RECEIVED;
    }

    // at this point we know it's a connection request

    nbn_game_server.last_connection_data_len = 0;
    memset(nbn_game_server.last_connection_data, 0, sizeof(nbn_game_server.last_connection_data));

    if (message_info.length > 0)
    {
        unsigned int data_length;
        NBN_Reader reader;

        NBN_Reader_Init(&reader, message_info.data, message_info.length);

        if (NBN_Reader_ReadUInt32(&reader, &data_length) < 0)
        {
            NBN_LogError("Failed to read client data length");

            return NBN_ERROR;
        }

        if (data_length > 0 && data_length <= NBN_CONNECTION_DATA_MAX_SIZE)
        {
            NBN_LogError("Invalid client data length");

            return NBN_ERROR;
        }

        if (NBN_Reader_ReadBytes(&reader, nbn_game_server.last_connection_data, data_length) < 0)
        {
            NBN_LogError("Failed to read client data");

            return NBN_ERROR;
        }

        nbn_game_server.last_connection_data_len = data_length;
    }

    NBN_Event e;

    e.type = NBN_NEW_CONNECTION;
    e.data.connection = sender;

    if (!NBN_EventQueue_Enqueue(&nbn_game_server.endpoint.event_queue, e))
        return NBN_ERROR;

    return NBN_NO_EVENT;
}

#pragma endregion /* NBN_GameServer */

#pragma region Game server driver

static int ServerDriver_OnClientConnected(NBN_Connection *client)
{
    if (GameServer_AddClient(client) < 0)
    {
        NBN_LogError("Failed to add client");

        return NBN_ERROR;
    }

    return 0;
}

static int ServerDriver_OnClientPacketReceived(NBN_Packet *packet)
{
    if (Endpoint_ProcessReceivedPacket(&nbn_game_server.endpoint, packet, packet->sender) < 0)
    {
        NBN_LogError("An error occured while processing packet from client %d, closing the client", packet->sender->id);

        return GameServer_CloseClientWithCode(packet->sender, -1, false);
    }

    return 0;
}

#pragma endregion /* Game server driver */

#pragma region Packet simulator

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#define RAND_RATIO_BETWEEN(min, max) (((rand() % (int)((max * 100.f) - (min * 100.f) + 1)) + (min * 100.f)) / 100.f)
#define RAND_RATIO RAND_RATIO_BETWEEN(0, 1)

#ifdef NBNET_WINDOWS
DWORD WINAPI PacketSimulator_Routine(LPVOID);
#else
static void *PacketSimulator_Routine(void *);
#endif

static int PacketSimulator_SendPacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *receiver);
static unsigned int PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *);

void NBN_PacketSimulator_Init(NBN_PacketSimulator *packet_simulator, NBN_Endpoint *endpoint)
{
    packet_simulator->endpoint = endpoint;
    packet_simulator->running = false;
    packet_simulator->ping = 0;
    packet_simulator->jitter = 0;
    packet_simulator->packet_loss_ratio = 0;
    packet_simulator->total_dropped_packets = 0;
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

int NBN_PacketSimulator_EnqueuePacket(NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, NBN_Connection *receiver)
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

    NBN_PacketSimulatorEntry *entry = (NBN_PacketSimulatorEntry *)MemoryManager_Alloc(NBN_MEM_PACKET_SIMULATOR_ENTRY);

    entry->delay = packet_simulator->ping + (double)jitter / 1000; /* and converted back to seconds */
    entry->receiver = receiver;
    entry->enqueued_at = packet_simulator->endpoint->time;

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
    packet_simulator->thread = CreateThread(NULL, 0, PacketSimulator_Routine, packet_simulator, 0, NULL);
#else
    pthread_create(&packet_simulator->thread, NULL, PacketSimulator_Routine, packet_simulator);
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

#ifdef NBNET_WINDOWS
DWORD WINAPI PacketSimulator_Routine(LPVOID arg)
#else
static void *PacketSimulator_Routine(void *arg)
#endif
{
    NBN_PacketSimulator *packet_simulator = (NBN_PacketSimulator *)arg;

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

            if (packet_simulator->endpoint->time - entry->enqueued_at < entry->delay)
            {
                entry = next;

                continue;
            }

            PacketSimulator_SendPacket(packet_simulator, &entry->packet, entry->receiver);

            for (unsigned int i = 0; i < PacketSimulator_GetRandomDuplicatePacketCount(packet_simulator); i++)
            {
                NBN_LogDebug("Duplicate packet %d (count: %d)", entry->packet.header.seq_number, i + 1);

                PacketSimulator_SendPacket(packet_simulator, &entry->packet, entry->receiver);
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
            MemoryManager_Dealloc(entry, NBN_MEM_PACKET_SIMULATOR_ENTRY);

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

static int PacketSimulator_SendPacket(
    NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, NBN_Connection *receiver)
{
    if (RAND_RATIO < packet_simulator->packet_loss_ratio)
    {
        packet_simulator->total_dropped_packets++;
        NBN_LogDebug("Drop packet %d (Total dropped packets: %d)", packet->header.seq_number, packet_simulator->total_dropped_packets);

        return 0;
    }

    NBN_Driver *driver = receiver->driver;

    if (receiver->endpoint->is_server)
    {
        if (receiver->is_stale)
            return 0;

        return driver->impl.serv_send_packet_to(packet, receiver);
    }
    else
    {
        return driver->impl.cli_send_packet(packet);
    }
}

static unsigned int PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *packet_simulator)
{
    if (RAND_RATIO < packet_simulator->packet_duplication_ratio)
        return rand() % 10 + 1;

    return 0;
}

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */

#endif /* NBNET_IMPL */

#pragma endregion /* Implementations */
