/*

   Copyright (C) 2026 BIAGINI Nathan

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

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#if defined(_WIN32) || defined(_WIN64)

#define NBN_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
// prevent inclusion of winnt.h in windows.h
#define _WINNT_

#include <winsock2.h>
#include <windows.h>

#elif (defined(__APPLE__) && defined(__MACH__))

#define NBN_PLATFORM_MAC

#else

#define NBN_PLATFORM_UNIX

#endif

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#endif

#ifndef NBN_PLATFORM_WINDOWS

#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

#endif

#define NBN_ERROR -1

typedef struct NBN_Endpoint NBN_Endpoint;
typedef struct NBN_Connection NBN_Connection;
typedef struct NBN_Channel NBN_Channel;
typedef struct NBN_Driver NBN_Driver;
typedef enum NBN_Driver_ID NBN_Driver_ID;

typedef enum NBN_LogLevel { NBN_LOG_ERROR, NBN_LOG_INFO, NBN_LOG_WARNING, NBN_LOG_DEBUG } NBN_LogLevel;

typedef void (*NBN_LogFunc)(NBN_LogLevel level, const char *filename, int line, const char *msg, ...);

void NBN_SetLogFunction(NBN_LogFunc);
void NBN_SetLogLevel(NBN_LogLevel);

#pragma region Serialization

#define B_MASK(n) (1u << (n))
#define B_SET(mask, n) (mask |= B_MASK(n))
#define B_UNSET(mask, n) (mask &= ~B_MASK(n))
#define B_IS_SET(mask, n) ((B_MASK(n) & mask) == B_MASK(n))
#define B_IS_UNSET(mask, n) ((B_MASK(n) & mask) == 0)

typedef struct NBN_Writer {
    uint8_t *buffer;
    unsigned int length;
    unsigned int position;
} NBN_Writer;

typedef struct NBN_Reader {
    uint8_t *buffer;
    unsigned int length;
    unsigned int position;
} NBN_Reader;

void NBN_Writer_Init(NBN_Writer *writer, uint8_t *buffer, unsigned int length);
void NBN_Writer_WriteInt32(NBN_Writer *writer, int32_t value);
void NBN_Writer_WriteUInt16(NBN_Writer *writer, uint16_t value);
void NBN_Writer_WriteUInt32(NBN_Writer *writer, uint32_t value);
void NBN_Writer_WriteUInt8(NBN_Writer *writer, uint8_t value);
void NBN_Writer_WriteFloat(NBN_Writer *writer, float value);
void NBN_Writer_WriteBytes(NBN_Writer *writer, uint8_t *bytes, unsigned int length);
void NBN_Writer_WriteString(NBN_Writer *writer, const char *str, unsigned int max_len);

void NBN_Reader_Init(NBN_Reader *reader, uint8_t *buffer, unsigned int length);
int NBN_Reader_ReadInt32(NBN_Reader *reader, int32_t *value);
int NBN_Reader_ReadUInt16(NBN_Reader *reader, uint16_t *value);
int NBN_Reader_ReadUInt32(NBN_Reader *reader, uint32_t *value);
int NBN_Reader_ReadUInt8(NBN_Reader *reader, uint8_t *value);
int NBN_Reader_ReadFloat(NBN_Reader *reader, float *value);
int NBN_Reader_ReadBytes(NBN_Reader *reader, uint8_t *bytes, unsigned int length);
int NBN_Reader_ReadString(NBN_Reader *reader, char *str, unsigned int max_len);

#pragma endregion /* Serialization */

#pragma region NBN_Message

#define NBN_MAX_MESSAGE_TYPES UINT8_MAX
#define NBN_RESERVED_MESSAGE_TYPES 4 /* Number of message types reserved for the library */
#define NBN_MESSAGE_RESEND_DELAY 0.1 /* Number of seconds before a message is resent (reliable messages redundancy) */
#define NBN_MESSAGE_HEADER_SIZE 6    /* See NBN_MessageHeader struct */
#define NBN_RESERVED_MESSAGE_BUFFER_LEN 32 /* Fixed size used for all library reserved messages' buffer */
#define NBN_MESSAGE_MAX_SIZE 256

// IMPORTANT: make sure you update NBN_MESSAGE_HEADER_SIZE if you modify NBN_MessageHeader struct
typedef struct NBN_MessageHeader {
    uint16_t id;
    uint16_t length;
    uint8_t type;
    uint8_t channel_id;
} NBN_MessageHeader;

typedef enum { NBN_OUTGOING_MESSAGE, NBN_INCOMING_MESSAGE } NBN_MessageType;

typedef struct NBN_Message {
    NBN_MessageHeader header;
    NBN_MessageType type;
    NBN_Connection *sender;
    uint8_t data[NBN_MESSAGE_MAX_SIZE];
} NBN_Message;

typedef struct NBN_OutgoingMessage {
    NBN_Message message;
    uint16_t id; // TODO: needed?
    double last_send_time;
    bool free;
} NBN_OutgoingMessage;

typedef struct NBN_IncomingMessage {
    NBN_Message message;
    bool free;
} NBN_IncomingMessage;

/**
 * Information about a received message.
 */
typedef struct NBN_MessageInfo {
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
     * On the client side, it will always be NULL  as all received messages come from the game server
     */
    NBN_Connection *sender;
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
#define NBN_MAX_MESSAGES_PER_PACKET UINT8_MAX

#define NBN_PACKET_HEADER_SIZE 13

/* Maximum size of packet's data (NBN_PACKET_MAX_DATA_SIZE + NBN_PACKET_HEADER_SIZE = NBN_PACKET_MAX_SIZE) */
#define NBN_PACKET_MAX_DATA_SIZE (NBN_PACKET_MAX_SIZE - NBN_PACKET_HEADER_SIZE)

enum {
    NBN_PACKET_WRITE_ERROR = -1,
    NBN_PACKET_WRITE_OK,
    NBN_PACKET_WRITE_NO_SPACE,
};

typedef enum NBN_PacketMode { NBN_PACKET_MODE_WRITE = 1, NBN_PACKET_MODE_READ } NBN_PacketMode;

// IMPORTANT: don't forget to update NBN_PACKET_HEADER_SIZE after modifying this structure
typedef struct NBN_PacketHeader {
    uint32_t protocol_id;
    uint32_t ack_bits;
    uint16_t seq_number;
    uint16_t ack;
    uint8_t messages_count;
} NBN_PacketHeader;

typedef struct NBN_Packet {
    NBN_PacketHeader header;
    NBN_PacketMode mode;
    struct NBN_Connection *sender; /* not serialized, filled by the network driver upon reception */
    uint8_t buffer[NBN_PACKET_MAX_SIZE];
    unsigned int size; /* in bytes */
    bool sealed;
} NBN_Packet;

void NBN_Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
int NBN_Packet_WriteMessage(NBN_Packet *, NBN_OutgoingMessage *);
int NBN_Packet_Seal(NBN_Packet *);
int NBN_Packet_InitRead(NBN_Packet *, uint32_t, unsigned int);

#pragma endregion /* NBN_Packet */

#pragma region Library reserved messages

// IMPORTANT: update NBN_RESERVED_MESSAGE_TYPES if you add or remove library messages
#define NBN_CLIENT_CLOSED_MESSAGE_TYPE NBN_MAX_MESSAGE_TYPES
#define NBN_CLIENT_ACCEPTED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 1)
#define NBN_DISCONNECTION_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 2)
#define NBN_CONNECTION_REQUEST_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 3)

#define NBN_SERVER_INITIAL_DATA_MAX_SIZE 256
#define NBN_CONNECTION_REQUEST_DATA_MAX_SIZE 256

#pragma endregion /* Library reserved messages */

#pragma region NBN_Channel

#ifndef NBN_CHANNEL_COUNT
/**
 * Number of channels per connection.
 */
#define NBN_CHANNEL_COUNT 2
#endif

#define NBN_CHANNEL_BUFFER_SIZE 256

/* Library reserved unreliable ordered channel */
#define NBN_CHANNEL_RESERVED_UNRELIABLE 0

/* Library reserved reliable ordered channel */
#define NBN_CHANNEL_RESERVED_RELIABLE 1

typedef enum NBN_ChannelType { NBN_CHANNEL_UNRELIABLE, NBN_CHANNEL_RELIABLE } NBN_ChannelType;

struct NBN_Channel {
    uint8_t id;
    NBN_ChannelType type;
    uint16_t next_outgoing_message_id;
    uint16_t next_recv_message_id;
    uint16_t oldest_unacked_message_id;
    uint16_t most_recent_message_id;
    uint16_t last_received_message_id;
    unsigned int next_outgoing_message_slot;
    unsigned int outgoing_message_count;
    NBN_OutgoingMessage outgoing_messages_buffer[NBN_CHANNEL_BUFFER_SIZE];
    NBN_IncomingMessage incoming_messages_buffer[NBN_CHANNEL_BUFFER_SIZE];
    bool ack_buffer[NBN_CHANNEL_BUFFER_SIZE];
};

void NBN_Channel_Destroy(NBN_Endpoint *, NBN_Channel *);

typedef struct NBN_UnreliableOrderedChannel {
    NBN_Channel base;
} NBN_UnreliableOrderedChannel;

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

typedef struct NBN_MessageEntry {
    uint16_t id;
    uint8_t channel_id;
} NBN_MessageEntry;

typedef struct NBN_PacketEntry {
    bool acked;
    bool flagged_as_lost;
    unsigned int messages_count;
    double send_time;
    NBN_MessageEntry messages[NBN_MAX_MESSAGES_PER_PACKET];
} NBN_PacketEntry;

typedef struct NBN_ConnectionStats {
    double ping;
    unsigned int total_lost_packets;
    float packet_loss;
    float upload_bandwidth;
    float download_bandwidth;
} NBN_ConnectionStats;

#ifdef NBN_DEBUG

typedef enum NBN_ConnectionDebugCallback { NBN_DEBUG_CB_MSG_ADDED_TO_RECV_QUEUE } NBN_ConnectionDebugCallback;

#endif /* NBN_DEBUG */

typedef struct NBN_IPAddress {
    uint32_t host;
    uint16_t port;
} NBN_IPAddress;

typedef uint64_t NBN_Connection_ID;

struct NBN_Connection {
    NBN_Connection_ID id;
    double last_recv_packet_time;  /* Used to detect stale connections */
    double last_flush_time;        /* Last time the send queue was flushed */
    double last_read_packets_time; /* Last time packets were read from the network driver */
    /* Keep track of bytes read from the socket (used for download bandwith calculation) */
    unsigned int downloaded_bytes;
    uint8_t is_accepted : 1;
    uint8_t is_stale : 1;
    uint8_t is_closed : 1;
    struct NBN_Endpoint *endpoint;
    NBN_Driver *driver;                      /* Network driver used for that connection */
    NBN_Channel channels[NBN_CHANNEL_COUNT]; /* Message channels (sending & receiving) */
    NBN_ConnectionStats stats;
    void *user_data; /* Pointer to user-defined data */

    /* Driver-related data attached to the connection */
    union {
        struct {
            NBN_IPAddress ip_address;
        } udp;
    } driver_data;

#ifdef NBN_DEBUG
    /* Debug callbacks */
    void (*OnMessageAddedToRecvQueue)(struct NBN_Connection *, NBN_Message *); // TODO: rename this function pointer
#endif                                                                         /* NBN_DEBUG */

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
struct NBN_ConnectionListNode {
    NBN_Connection *conn;
    NBN_ConnectionListNode *next;
    NBN_ConnectionListNode *prev;
};

typedef uint8_t *(*NBN_AllocMessageFunc)(NBN_MessageHeader *);
typedef uint8_t *(*NBN_DeallocMessageFunc)(NBN_MessageHeader *, uint8_t *);

int NBN_Connection_ProcessReceivedPacket(NBN_Endpoint *, NBN_Connection *, NBN_Packet *, double);
int NBN_Connection_FlushChannels(NBN_Endpoint *, NBN_Connection *, uint32_t, double);
bool NBN_Connection_CheckIfStale(NBN_Connection *, double);

#pragma endregion /* NBN_Connection */

#pragma region NBN_EventQueue

#define NBN_NO_EVENT 0   /* No event left in the events queue */
#define NBN_SKIP_EVENT 1 /* Indicates that the event should be skipped */
#define NBN_EVENT_QUEUE_CAPACITY 1024

typedef struct NBN_DisconnectionInfo {
    uint32_t conn_id; /* ID if the disconnected connection */
    void *user_data;  /* User-defined data associated with this connection */
} NBN_DisconnectionInfo;

typedef union NBN_EventData {
    NBN_MessageInfo message_info;
    NBN_Connection *connection;
    NBN_DisconnectionInfo disconnection;
} NBN_EventData;

typedef struct NBN_Event {
    int type;
    NBN_EventData data;
} NBN_Event;

typedef struct NBN_EventQueue {
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

#ifndef NBN_PLATFORM_WINDOWS
#include <pthread.h>
#endif /* NBN_PLATFORM_WINDOWS */

typedef struct NBN_PacketSimulatorEntry NBN_PacketSimulatorEntry;

struct NBN_PacketSimulatorEntry {
    NBN_Packet packet;
    NBN_Connection *receiver;
    double delay;
    double enqueued_at;
    struct NBN_PacketSimulatorEntry *next;
    struct NBN_PacketSimulatorEntry *prev;
};

typedef struct NBN_PacketSimulator {
    NBN_Endpoint *endpoint;
    NBN_PacketSimulatorEntry *head_packet;
    NBN_PacketSimulatorEntry *tail_packet;
    unsigned int packet_count;

#ifdef NBN_PLATFORM_WINDOWS
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

#define NBN_GameClient_SetPing(v)                                                                                      \
    { nbn_game_client.endpoint.packet_simulator.ping = v; }
#define NBN_GameClient_SetJitter(v)                                                                                    \
    { nbn_game_client.endpoint.packet_simulator.jitter = v; }
#define NBN_GameClient_SetPacketLoss(v)                                                                                \
    { nbn_game_client.endpoint.packet_simulator.packet_loss_ratio = v; }
#define NBN_GameClient_SetPacketDuplication(v)                                                                         \
    { nbn_game_client.endpoint.packet_simulator.packet_duplication_ratio = v; }

#define NBN_GameServer_SetPing(v)                                                                                      \
    { nbn_game_server.endpoint.packet_simulator.ping = v; }
#define NBN_GameServer_SetJitter(v)                                                                                    \
    { nbn_game_server.endpoint.packet_simulator.jitter = v; }
#define NBN_GameServer_SetPacketLoss(v)                                                                                \
    { nbn_game_server.endpoint.packet_simulator.packet_loss_ratio = v; }
#define NBN_GameServer_SetPacketDuplication(v)                                                                         \
    { nbn_game_server.endpoint.packet_simulator.packet_duplication_ratio = v; }

#else

#define NBN_PacketSimulator_Disabled                                                                                   \
    do {                                                                                                               \
    } while (0);

#define NBN_GameClient_SetPing(v) NBN_PacketSimulator_Disabled
#define NBN_GameClient_SetJitter(v) NBN_PacketSimulator_Disabled
#define NBN_GameClient_SetPacketLoss(v) NBN_PacketSimulator_Disabled
#define NBN_GameClient_SetPacketDuplication(v) NBN_PacketSimulator_Disabled

#define NBN_GameServer_SetPing(v) NBN_PacketSimulator_Disabled
#define NBN_GameServer_SetJitter(v) NBN_PacketSimulator_Disabled
#define NBN_GameServer_SetPacketLoss(v) NBN_PacketSimulator_Disabled
#define NBN_GameServer_SetPacketDuplication(v) NBN_PacketSimulator_Disabled

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */

#pragma region NBN_Endpoint

struct NBN_Endpoint {
    NBN_EventQueue event_queue;
    uint32_t protocol_id;
    bool is_server;
    double time;
    NBN_Message write_message;
    NBN_Writer message_writer;
    NBN_Reader message_reader;
    uint8_t server_initial_data_buffer[NBN_SERVER_INITIAL_DATA_MAX_SIZE];
    uint8_t connection_request_data_buffer[NBN_CONNECTION_REQUEST_DATA_MAX_SIZE];
    unsigned int client_connection_request_data_len;
    unsigned int server_initial_data_len;

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

enum {
    /* Client is connected to server */
    NBN_CONNECTED = 2,

    /* Client is disconnected from the server */
    NBN_DISCONNECTED,

    /* Client has received a message from the server */
    NBN_MESSAGE_RECEIVED
};

typedef struct NBN_GameClient_Config {
    const char *protocol_name;
    const char *host;
    uint16_t port;
} NBN_GameClient_Config;

typedef struct NBN_GameClient {
    NBN_Endpoint endpoint;
    NBN_GameClient_Config config;
    NBN_Connection *server_connection;
    bool is_connected;
    NBN_Event last_event;
    int closed_code;
    NBN_Writer client_data_writer;
    NBN_Reader server_data_reader;
} NBN_GameClient;

/**
 * Initialize the game client with minimal configuration.
 *
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be able
 * to communicate
 * @param host Host to connect to
 * @param port Port to connect to
 */
void NBN_GameClient_Init(const char *protocol_name, const char *host, uint16_t port);

// TODO: doc
NBN_Writer *NBN_GameClient_GetConnectionRequestDataWriter(void);

/**
 * Start the game client.
 *
 * @return 0 when successully started, -1 otherwise
 */
int NBN_GameClient_Start(void);

/**
 * Disconnect from the server. The client can be restarted by calling NBN_GameClient_Start or
 * NBN_GameClient_StartWithData again.
 */
void NBN_GameClient_Stop(void);

// TODO: doc
NBN_Reader *NBN_GameClient_GetServerDataReader(void);

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
int NBN_GameClient_Flush(void);

// TODO: doc
NBN_Writer *NBN_GameClient_CreateMessage(uint8_t type, uint8_t channel_id);
// TODO: doc
NBN_Writer *NBN_GameClient_CreateUnreliableMessage(uint8_t type);
// TODO: doc
NBN_Writer *NBN_GameClient_CreateReliableMessage(uint8_t type);

// TODO: doc
int NBN_GameClient_EnqueueMessage(void);

// TODO: doc
NBN_Writer *NBN_GameClient_GetMessageWriter(void);

// TODO: doc
NBN_Reader *NBN_GameClient_GetMessageReader(void);

/**
 * For drivers only! NOT MEANT TO BE USED BY USER CODE.
 */
NBN_Connection *NBN_GameClient_CreateServerConnection(NBN_Driver_ID driver_id);

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

enum {
    /* A new client has connected */
    NBN_NEW_CONNECTION = 2,

    /* A client has disconnected */
    NBN_CLIENT_DISCONNECTED,

    /* A message has been received from a client */
    NBN_CLIENT_MESSAGE_RECEIVED
};

typedef struct NBN_GameServerStats {
    float upload_bandwidth;   /* Total upload bandwith of the game server */
    float download_bandwidth; /* Total download bandwith of the game server */
} NBN_GameServerStats;

typedef struct NBN_GameServer_Config {
    const char *protocol_name;
    uint16_t port;
} NBN_GameServer_Config;

typedef struct NBN_GameServer {
    NBN_Endpoint endpoint;
    NBN_GameServer_Config config;
    struct {
        NBN_Connection_ID key;
        NBN_Connection *value;
    } *clients;
    NBN_ConnectionListNode *closed_clients_head;
    NBN_GameServerStats stats;
    NBN_Event last_event;
    NBN_Writer server_data_writer;
    NBN_Reader client_data_reader;
} NBN_GameServer;

/**
 * Initialize the game server with minimal configuration.
 *
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be
 * able to communicate
 * @param port The port clients will connect to
 */
void NBN_GameServer_Init(const char *protocol_name, uint16_t port);

/**
 * Start the game server with the provided configuration.
 *
 * @return 0 when successfully started, -1 otherwise
 */
int NBN_GameServer_Start(void);

/**
 * Stop the game server and clean everything up.
 */
void NBN_GameServer_Stop(void);

// TODO: doc
NBN_Connection *NBN_GameServer_FindConnection(NBN_Connection_ID);

// TODO: doc
unsigned int NBN_GameServer_GetClientCount(void);

// TODO: doc
NBN_Connection *NBN_GameServer_GetClientByIndex(unsigned int index);

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
int NBN_GameServer_Flush(void);

/**
 * For drivers only! NOT MEANT TO BE USED BY USER CODE.
 */
NBN_Connection *NBN_GameServer_CreateClientConnection(NBN_Driver_ID, NBN_Connection_ID);

/**
 * Close a client's connection without a specific code (default code is -1)
 *
 * @param conn The connection to close
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_CloseClient(NBN_Connection *conn);

/**
 * Close a client's connection with a specific code.
 *
 * The code is an arbitrary integer to let the client knows
 * why his connection was closed.
 *
 * @param conn The connection to close
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_CloseClientWithCode(NBN_Connection *conn, int code);

// TODO: doc
NBN_Writer *NBN_GameServer_CreateMessage(uint8_t type, uint8_t channel_id);
// TODO: doc
NBN_Writer *NBN_GameServer_CreateUnreliableMessage(uint8_t type);
// TODO: doc
NBN_Writer *NBN_GameServer_CreateReliableMessage(uint8_t type);
// TODO: doc
int NBN_GameServer_EnqueueMessageFor(NBN_Connection *conn);
// TODO: doc
int NBN_GameServer_EnqueueBroadcastMessage(void);

// TODO: doc
NBN_Reader *NBN_GameServer_GetMessageReader(void);

// TODO: doc
NBN_Writer *NBN_GameServer_GetConnectionDataWriter(void);

// TODO: doc
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
 * @return A pointer to a NBN_Connection representing the new connection
 */
NBN_Connection *NBN_GameServer_GetIncomingConnection(void);

// TODO: doc
NBN_Reader *NBN_GameServer_GetConnectionRequestDataReader(void);

/**
 * Return the information about the last disconnected client.
 *
 * Call this function after receiving a NBN_CLIENT_DISCONNECTED event.
 * See NBN_DisconnectionInfo struct.
 *
 * @return information about the last disconnected client
 */
NBN_DisconnectionInfo NBN_GameServer_GetDisconnectionInfo(void);

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

typedef enum NBN_DriverEvent {
    // Client events
    NBN_DRIVER_CLI_PACKET_RECEIVED,

    // Server events
    NBN_DRIVER_SERV_CLIENT_CONNECTED,
    NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED,
} NBN_DriverEvent;

typedef int (*NBN_Driver_Func_ClientStart)(NBN_GameClient *, const char *, uint16_t);
typedef void (*NBN_Driver_Func_ClientStop)(NBN_GameClient *);
typedef int (*NBN_Driver_Func_ClientSendPacket)(NBN_GameClient *, NBN_Packet *);
typedef int (*NBN_Driver_Func_ClientRecvPackets)(NBN_GameClient *);

typedef int (*NBN_Driver_Func_ServerStart)(NBN_GameServer *, uint16_t);
typedef void (*NBN_Driver_Func_ServerStop)(NBN_GameServer *);
typedef int (*NBN_Driver_Func_ServerSendPacketTo)(NBN_GameServer *, NBN_Packet *, NBN_Connection *);
typedef void (*NBN_Driver_Func_ServerCleanupConnection)(NBN_GameServer *, NBN_Connection *);
typedef int (*NBN_Driver_Func_ServerRecvPackets)(NBN_GameServer *);

typedef struct NBN_Driver_Implementation {
    /* Client functions */
    NBN_Driver_Func_ClientStart cli_start;
    NBN_Driver_Func_ClientStop cli_stop;
    NBN_Driver_Func_ClientRecvPackets cli_recv_packets;
    NBN_Driver_Func_ClientSendPacket cli_send_packet;

    /* Server functions */
    NBN_Driver_Func_ServerStart serv_start;
    NBN_Driver_Func_ServerStop serv_stop;
    NBN_Driver_Func_ServerRecvPackets serv_recv_packets;
    NBN_Driver_Func_ServerSendPacketTo serv_send_packet_to;
    NBN_Driver_Func_ServerCleanupConnection serv_cleanup_connection;
} NBN_Driver_Implementation;

enum NBN_Driver_ID { NBN_DRIVER_UDP = 0x01 };

struct NBN_Driver {
    int id;
    const char *name;
    NBN_Driver_Implementation impl;
};

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

#define SEQUENCE_NUMBER_GT(seq1, seq2)                                                                                 \
    ((seq1 > seq2 && (seq1 - seq2) <= 32767) || (seq1 < seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_GTE(seq1, seq2)                                                                                \
    ((seq1 >= seq2 && (seq1 - seq2) <= 32767) || (seq1 <= seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_LT(seq1, seq2)                                                                                 \
    ((seq1 < seq2 && (seq2 - seq1) <= 32767) || (seq1 > seq2 && (seq1 - seq2) >= 32767))

#pragma endregion /* Utils */

#endif /* NBNET_H */
