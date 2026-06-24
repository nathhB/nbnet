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

#include <stdint.h>
#include "nbnet.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <arpa/inet.h>

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

#ifndef NBN_PLATFORM_WINDOWS

#include <sys/time.h>

#if _POSIX_C_SOURCE >= 199309L

#include <time.h>

#else

#include <unistd.h>

#endif // _POSIX_C_SOURCE >= 199309L

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

#endif // NBN_PLATFORM_WINDOWS

#ifdef NBN_UDP

#if defined(NBN_PLATFORM_WINDOWS)

#include <winsock2.h>

typedef int socklen_t;

#elif defined(NBN_PLATFORM_UNIX) || defined(NBN_PLATFORM_MAC)

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#endif // NBN_PLATFORM_WINDOWS

#endif // NBN_UDP

typedef struct NBN_Connection NBN_Connection;
typedef struct NBN_Endpoint NBN_Endpoint;
typedef struct NBN_Channel NBN_Channel;
typedef struct NBN_Driver NBN_Driver;

typedef enum NBN_Driver_ID NBN_Driver_ID;

#define NBN_Abort abort
#define NBN_Assert(cond) assert(cond)

#define SEQUENCE_NUMBER_GT(seq1, seq2)                                                                                 \
    ((seq1 > seq2 && (seq1 - seq2) <= 32767) || (seq1 < seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_GTE(seq1, seq2)                                                                                \
    ((seq1 >= seq2 && (seq1 - seq2) <= 32767) || (seq1 <= seq2 && (seq2 - seq1) >= 32767))
#define SEQUENCE_NUMBER_LT(seq1, seq2)                                                                                 \
    ((seq1 < seq2 && (seq2 - seq1) <= 32767) || (seq1 > seq2 && (seq1 - seq2) >= 32767))
#define SEQUENCE_NUMBER_LTE(seq1, seq2)                                                                                \
    ((seq1 <= seq2 && (seq2 - seq1) <= 32767) || (seq1 >= seq2 && (seq1 - seq2) >= 32767))

#define B_MASK(n) (1u << (n))
#define B_SET(mask, n) (mask |= B_MASK(n))
#define B_UNSET(mask, n) (mask &= ~B_MASK(n))
#define B_IS_SET(mask, n) ((B_MASK(n) & mask) == B_MASK(n))
#define B_IS_UNSET(mask, n) ((B_MASK(n) & mask) == 0)

#define HANDLE_TO_CONN(h) ((NBN_Connection *)(void *)h)

#define NBN_EVENT_QUEUE_CAPACITY 1024

#define NBN_CLIENT_CLOSED_MESSAGE_TYPE UINT8_MAX
#define NBN_CLIENT_ACCEPTED_MESSAGE_TYPE (UINT8_MAX - 1)
#define NBN_DISCONNECTION_MESSAGE_TYPE (UINT8_MAX - 2)
#define NBN_CONNECTION_REQUEST_MESSAGE_TYPE (UINT8_MAX - 3)

/*
 * Maximum allowed packet size (including header) in bytes.
 * The 1400 value has been chosen based on this statement:
 *
 * With the IPv4 header being 20 bytes and the UDP header being 8 bytes, the payload
 * of a UDP packet should be no larger than 1500 - 20 - 8 = 1472 bytes to avoid fragmentation.
 */
#define NBN_PACKET_MAX_SIZE 1400

#define NBN_MAX_MESSAGES_PER_PACKET UINT8_MAX
#define NBN_MESSAGE_HEADER_SIZE 6 /* See NBN_MessageHeader struct */
#define NBN_PACKET_HEADER_SIZE 13

/* Maximum size of packet's data (NBN_PACKET_MAX_DATA_SIZE + NBN_PACKET_HEADER_SIZE = NBN_PACKET_MAX_SIZE) */
#define NBN_PACKET_MAX_DATA_SIZE (NBN_PACKET_MAX_SIZE - NBN_PACKET_HEADER_SIZE)

#define NBN_MAX_PACKET_ENTRIES 1024

#define NBN_MAX_CHANNEL_COUNT 16
#define NBN_CHANNEL_DEFAULT_BUFFER_SIZE 128
#define NBN_CHANNEL_DEFAULT_MAX_MESSAGE_SIZE 256

typedef enum NBN_PacketResult {
    NBN_PACKET_WRITE_ERROR = -1,
    NBN_PACKET_WRITE_OK,
    NBN_PACKET_WRITE_NO_SPACE,
} NBN_PacketResult;

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

typedef struct NBN_MessageEntry {
    uint16_t id;
    uint8_t channel_id;
} NBN_MessageEntry;

typedef struct NBN_PacketEntry {
    uint8_t acked : 1;
    uint8_t lost : 1;
    uint8_t messages_count;
    float send_time;
    NBN_MessageEntry messages[NBN_MAX_MESSAGES_PER_PACKET];
} NBN_PacketEntry;

// IMPORTANT: make sure you update NBN_MESSAGE_HEADER_SIZE if you modify NBN_MessageHeader struct
typedef struct NBN_MessageHeader {
    uint16_t id;
    uint16_t length;
    uint8_t type;
    uint8_t channel_id;
} NBN_MessageHeader;

typedef struct NBN_Message {
    NBN_MessageHeader header;
    NBN_Connection *sender;
    uint8_t *data;
} NBN_Message;

typedef struct NBN_OutgoingMessage {
    NBN_Message message;
    NBN_Writer writer;
    float last_send_time;
    bool free;
} NBN_OutgoingMessage;

typedef struct NBN_IncomingMessage {
    NBN_Message message;
    bool free;
} NBN_IncomingMessage;

struct NBN_Channel {
    uint8_t id;
    NBN_Channel_Mode mode;
    uint16_t next_outgoing_message_id;
    uint16_t next_recv_message_id;
    uint16_t oldest_unacked_message_id;
    uint16_t most_recent_message_id;
    uint16_t last_received_message_id;
    unsigned int next_outgoing_message_slot;
    unsigned int outgoing_message_count;
    unsigned int buffer_size;
    unsigned int max_message_len;
    unsigned int current_capacity;
    NBN_OutgoingMessage *outgoing_messages_buffer;
    NBN_IncomingMessage *incoming_messages_buffer;
    bool *ack_buffer; // TODO: needed?
};

typedef struct NBN_IPAddress {
    uint32_t host;
    uint16_t port;
} NBN_IPAddress;

#ifdef __EMSCRIPTEN__

typedef uint32_t NBN_WebRTC_Peer_ID;

#endif

#ifdef NBN_WEBRTC_NATIVE

typedef int NBN_WebRTC_Peer_ID;

#endif

struct NBN_Connection {
    NBN_ConnectionHandle handle;
    float last_recv_packet_time;  /* Used to detect stale connections */
    float last_flush_time;        /* Last time the send queue was flushed */
    float last_read_packets_time; /* Last time packets were read from the network driver */
    /* Keep track of bytes read from the socket (used for download bandwith calculation) */
    unsigned int downloaded_bytes;
    uint8_t is_accepted : 1;
    uint8_t is_stale : 1;
    uint8_t is_closed : 1;
    NBN_Driver *driver;    /* Network driver used for that connection */
    NBN_Channel *channels; /* Message channels (sending & receiving) */
    unsigned int channel_count;
    NBN_ConnectionStats stats;

    /*
     *  Packet sequencing & acking
     */
    uint16_t next_packet_seq_number;
    uint16_t last_received_packet_seq_number;
    uint32_t packet_send_seq_buffer[NBN_MAX_PACKET_ENTRIES];
    uint32_t packet_recv_seq_buffer[NBN_MAX_PACKET_ENTRIES];
    NBN_PacketEntry packet_send_buffer[NBN_MAX_PACKET_ENTRIES];

    /* Driver-related data attached to the connection */
    struct {
#ifdef NBN_UDP
        struct {
            NBN_IPAddress ip_address;
        } udp;
#endif // NBN_UDP

#if defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
        struct {
            NBN_WebRTC_Peer_ID peer_id;
#ifdef NBN_WEBRTC_NATIVE
            int channel_id;
            int ws;
#endif // NBN_WEBRTC_NATIVE
        } webrtc;
#endif // defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)

        void *endpoint_ptr;
    } driver_data;
};

typedef union NBN_EventData {
    NBN_MessageInfo message_info;
    NBN_DisconnectionInfo disconnection;
    NBN_Connection *connection;
} NBN_EventData;

typedef struct NBN_Event {
    int type;
    NBN_EventData data;
} NBN_Event;

/**
 * ====== DATA STRUCTURES ======
 */

typedef struct NBN_ConnectionListNode NBN_ConnectionListNode;

/* Linked list of connections */
struct NBN_ConnectionListNode {
    NBN_Connection *conn;
    NBN_ConnectionListNode *next;
    NBN_ConnectionListNode *prev;
};

typedef struct NBN_EventQueue {
    NBN_Event events[NBN_EVENT_QUEUE_CAPACITY];
    unsigned int head;
    unsigned int tail;
    unsigned int count;
} NBN_EventQueue;

static void EventQueue_Init(NBN_EventQueue *event_queue) {
    event_queue->head = 0;
    event_queue->tail = 0;
    event_queue->count = 0;
}

static bool EventQueue_Enqueue(NBN_EventQueue *event_queue, NBN_Event ev) {
    if (event_queue->count >= NBN_EVENT_QUEUE_CAPACITY)
        return false;

    event_queue->events[event_queue->tail] = ev;

    event_queue->tail = (event_queue->tail + 1) % NBN_EVENT_QUEUE_CAPACITY;
    event_queue->count++;

    return true;
}

static bool EventQueue_IsEmpty(NBN_EventQueue *event_queue) { return event_queue->count == 0; }

static bool EventQueue_Dequeue(NBN_EventQueue *event_queue, NBN_Event *ev) {
    if (EventQueue_IsEmpty(event_queue))
        return false;

    memcpy(ev, &event_queue->events[event_queue->head], sizeof(NBN_Event));
    event_queue->head = (event_queue->head + 1) % NBN_EVENT_QUEUE_CAPACITY;
    event_queue->count--;

    return true;
}

// END DATA STRUCTURES
// ===================================================

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#ifndef NBN_PLATFORM_WINDOWS
#include <pthread.h>
#endif /* NBN_PLATFORM_WINDOWS */

typedef struct NBN_PacketSimulatorEntry NBN_PacketSimulatorEntry;

struct NBN_PacketSimulatorEntry {
    NBN_Packet packet;
    NBN_Connection *receiver;
    float delay;
    float enqueued_at;
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
    float ping;
    float jitter;
} NBN_PacketSimulator;

static void PacketSimulator_Init(NBN_PacketSimulator *, NBN_Endpoint *);
static int PacketSimulator_EnqueuePacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *);
static void PacketSimulator_Start(NBN_PacketSimulator *);
static void PacketSimulator_Stop(NBN_PacketSimulator *);
static int PacketSimulator_SendPacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *receiver);
static unsigned int PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *);

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

typedef struct NBN_Channel_Config {
    NBN_Channel_Mode mode;
    unsigned int buffer_size;
    unsigned int max_message_len;
} NBN_Channel_Config;

struct NBN_Endpoint {
    NBN_EventQueue event_queue;
    uint32_t protocol_id;
    bool is_server;
    float time;
    NBN_Reader message_reader;
    uint8_t server_initial_data_buffer[NBN_SERVER_INITIAL_DATA_MAX_SIZE];
    uint8_t connection_request_data_buffer[NBN_CONNECTION_REQUEST_DATA_MAX_SIZE];
    unsigned int client_connection_request_data_len;
    unsigned int server_initial_data_len;
    unsigned int channel_count;
    NBN_Channel_Config *channels;
    uint8_t default_reliable_channel;
    uint8_t default_unreliable_channel;
    NBN_Packet read_packet;
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator packet_simulator;
#endif
};

typedef struct NBN_Server_Config {
    const char *protocol_name;
    uint16_t port;
    NBN_Channel_Config channels[NBN_MAX_CHANNEL_COUNT];
    unsigned int channel_count;
} NBN_Server_Config;

struct NBN_Server {
    NBN_Endpoint endpoint;
    NBN_Server_Config config;
    struct {
        NBN_Connection_ID key;
        NBN_Connection *value;
    } *clients;
    NBN_ConnectionListNode *closed_clients_head;
    NBN_ServerStats stats;
    NBN_Event last_event;
    NBN_Writer server_data_writer;
    NBN_Reader client_data_reader;

    struct {
#ifdef NBN_UDP
        struct {
            SOCKET sock;
        } udp;
#endif

#if defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
        struct {
            NBN_WebRTC_Config cfg;
            int ws_server;
        } webrtc;
#endif // defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
    } driver_data;
};

typedef struct NBN_Client_Config {
    const char *protocol_name;
    const char *host;
    uint16_t port;
    NBN_Channel_Config channels[NBN_MAX_CHANNEL_COUNT];
    unsigned int channel_count;
} NBN_Client_Config;

struct NBN_Client {
    NBN_Endpoint endpoint;
    NBN_Client_Config config;
    NBN_Connection *server_connection;
    bool is_connected;
    NBN_Event last_event;
    int closed_code;
    NBN_Writer client_data_writer;
    NBN_Reader server_data_reader;

    struct {
#ifdef NBN_UDP
        struct {
            SOCKET sock;
        } udp;
#endif

#if defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
        struct {
            NBN_WebRTC_Config cfg;
            bool is_connected;
        } webrtc;
#endif // defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
    } driver_data;
};

typedef int (*NBN_Driver_Func_ClientStart)(NBN_Client *, const char *, uint16_t);
typedef void (*NBN_Driver_Func_ClientStop)(NBN_Client *);
typedef int (*NBN_Driver_Func_ClientSendPacket)(NBN_Client *, NBN_Packet *, NBN_Connection *);
typedef int (*NBN_Driver_Func_ClientRecvPackets)(NBN_Client *);

typedef int (*NBN_Driver_Func_ServerStart)(NBN_Server *, uint16_t);
typedef void (*NBN_Driver_Func_ServerStop)(NBN_Server *);
typedef int (*NBN_Driver_Func_ServerSendPacketTo)(NBN_Server *, NBN_Packet *, NBN_Connection *);
typedef void (*NBN_Driver_Func_ServerCleanupConnection)(NBN_Server *, NBN_Connection *);
typedef int (*NBN_Driver_Func_ServerRecvPackets)(NBN_Server *);

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

enum NBN_Driver_ID { NBN_DRIVER_UDP = 0x01, NBN_DRIVER_WEBRTC_EMSCRIPTEN = 0x02, NBN_DRIVER_WEBRTC_NATIVE = 0x03 };

struct NBN_Driver {
    int id;
    const char *name;
    NBN_Driver_Implementation impl;
};

#ifdef NBN_UDP

static int UDP_Client_Start(NBN_Client *client, const char *host, uint16_t port);
static void UDP_Client_Stop(NBN_Client *client);
static int UDP_Client_RecvPackets(NBN_Client *client);
static int UDP_Client_SendPacket(NBN_Client *client, NBN_Packet *packet, NBN_Connection *connection);

static int UDP_Server_Start(NBN_Server *server, uint16_t port);
static void UDP_Server_Stop(NBN_Server *server);
static int UDP_Server_RecvPackets(NBN_Server *server);
static int UDP_Server_SendPacketTo(NBN_Server *server, NBN_Packet *packet, NBN_Connection *connection);
static void UDP_Server_CleanupConnection(NBN_Server *server, NBN_Connection *connection);

static NBN_Driver nbn_udp_driver = {.id = NBN_DRIVER_UDP,
                                    .name = "UDP",
                                    .impl = {// Client implementation
                                             .cli_start = UDP_Client_Start,
                                             .cli_stop = UDP_Client_Stop,
                                             .cli_recv_packets = UDP_Client_RecvPackets,
                                             .cli_send_packet = UDP_Client_SendPacket,

                                             // Server implementation
                                             .serv_start = UDP_Server_Start,
                                             .serv_stop = UDP_Server_Stop,
                                             .serv_recv_packets = UDP_Server_RecvPackets,
                                             .serv_send_packet_to = UDP_Server_SendPacketTo,
                                             .serv_cleanup_connection = UDP_Server_CleanupConnection}};
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__

#ifdef NBN_UDP
#error "Cannot compile UDP driver with emscripten"
#endif

#ifdef NBN_WEBRTC_NATIVE
#error "Cannot compile native WebRTC driver with emscripten"
#endif

// TODO: add a check for webrtc native as well

#include <emscripten.h>

static int WebRTC_Client_Start(NBN_Client *client, const char *host, uint16_t port);
static void WebRTC_Client_Stop(NBN_Client *client);
static int WebRTC_Client_RecvPackets(NBN_Client *client);
static int WebRTC_Client_SendPacket(NBN_Client *client, NBN_Packet *packet, NBN_Connection *connection);

static int WebRTC_Server_Start(NBN_Server *server, uint16_t port);
static void WebRTC_Server_Stop(NBN_Server *server);
static int WebRTC_Server_RecvPackets(NBN_Server *server);
static int WebRTC_Server_SendPacketTo(NBN_Server *server, NBN_Packet *packet, NBN_Connection *connection);
static void WebRTC_Server_CleanupConnection(NBN_Server *server, NBN_Connection *connection);

static NBN_Driver nbn_webrtc_em_driver = {.id = NBN_DRIVER_WEBRTC_EMSCRIPTEN,
                                          .name = "WebRTC_EMSCRIPTEN",
                                          .impl = {// Client implementation
                                                   .cli_start = WebRTC_Client_Start,
                                                   .cli_stop = WebRTC_Client_Stop,
                                                   .cli_recv_packets = WebRTC_Client_RecvPackets,
                                                   .cli_send_packet = WebRTC_Client_SendPacket,

                                                   // Server implementation
                                                   .serv_start = WebRTC_Server_Start,
                                                   .serv_stop = WebRTC_Server_Stop,
                                                   .serv_recv_packets = WebRTC_Server_RecvPackets,
                                                   .serv_send_packet_to = WebRTC_Server_SendPacketTo,
                                                   .serv_cleanup_connection = WebRTC_Server_CleanupConnection}};

#endif // __EMSCRIPTEN__

#ifdef NBN_WEBRTC_NATIVE

static int WebRTC_Native_Client_Start(NBN_Client *client, const char *host, uint16_t port);
static void WebRTC_Native_Client_Stop(NBN_Client *client);
static int WebRTC_Native_Client_RecvPackets(NBN_Client *client);
static int WebRTC_Native_Client_SendPacket(NBN_Client *client, NBN_Packet *packet, NBN_Connection *connection);

static int WebRTC_Native_Server_Start(NBN_Server *server, uint16_t port);
static void WebRTC_Native_Server_Stop(NBN_Server *server);
static int WebRTC_Native_Server_RecvPackets(NBN_Server *server);
static int WebRTC_Native_Server_SendPacketTo(NBN_Server *server, NBN_Packet *packet, NBN_Connection *connection);
static void WebRTC_Native_Server_CleanupConnection(NBN_Server *server, NBN_Connection *connection);

static NBN_Driver nbn_webrtc_native_driver = {
    .id = NBN_DRIVER_WEBRTC_NATIVE,
    .name = "WebRTC_NATIVE",
    .impl = {// Client implementation
             .cli_start = WebRTC_Native_Client_Start,
             .cli_stop = WebRTC_Native_Client_Stop,
             .cli_recv_packets = WebRTC_Native_Client_RecvPackets,
             .cli_send_packet = WebRTC_Native_Client_SendPacket,

             // Server implementation
             .serv_start = WebRTC_Native_Server_Start,
             .serv_stop = WebRTC_Native_Server_Stop,
             .serv_recv_packets = WebRTC_Native_Server_RecvPackets,
             .serv_send_packet_to = WebRTC_Native_Server_SendPacketTo,
             .serv_cleanup_connection = WebRTC_Native_Server_CleanupConnection}};

#endif // NBN_WEBRTC_NATIVE

/**
 * ====== LOGGING ======
 */

#define LogInfo(msg, ...) Log(NBN_LOG_INFO, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define LogWarning(msg, ...) Log(NBN_LOG_WARNING, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define LogError(msg, ...) Log(NBN_LOG_ERROR, __FILE__, __LINE__, msg, ##__VA_ARGS__)

#ifdef NBN_DEBUG
#define LogDebug(msg, ...) Log(NBN_LOG_DEBUG, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#else
#define LogDebug(...)                                                                                                  \
    ;                                                                                                                  \
    ;
#endif // NBN_DEBUG

static void Log(NBN_LogLevel level, const char *filename, int line, const char *msg, ...);

// END OF LOGGING
// ===================================================

/**
 * ====== SERIALIZATION ======
 */

static union {
    uint64_t v;
    uint8_t bytes[8];
} swap;

static uint64_t SwapBytes64(uint64_t v) {
    if (htonl(1) == 1) {
        return v;
    }

    swap.v = v;

    return ((uint64_t)swap.bytes[0] << 56) | ((uint64_t)swap.bytes[1] << 48) | ((uint64_t)swap.bytes[2] << 40) |
           ((uint64_t)swap.bytes[3] << 32) | ((uint64_t)swap.bytes[4] << 24) | ((uint64_t)swap.bytes[5] << 16) |
           ((uint64_t)swap.bytes[6] << 8) | (uint64_t)swap.bytes[7];
}

void NBN_Writer_Init(NBN_Writer *writer, uint8_t *buffer, unsigned int length) {
    writer->buffer = buffer;
    writer->length = length;
    writer->position = 0;
}

void NBN_Writer_WriteInt8(NBN_Writer *writer, int8_t value) { NBN_Writer_WriteUInt8(writer, value); }

void NBN_Writer_WriteInt16(NBN_Writer *writer, int16_t value) { NBN_Writer_WriteUInt16(writer, value); }

void NBN_Writer_WriteInt32(NBN_Writer *writer, int32_t value) { NBN_Writer_WriteUInt32(writer, value); }

void NBN_Writer_WriteInt64(NBN_Writer *writer, int64_t value) { NBN_Writer_WriteUInt64(writer, value); }

void NBN_Writer_WriteUInt8(NBN_Writer *writer, uint8_t value) {
    NBN_Assert(writer->position + 1 <= writer->length);

    writer->buffer[writer->position] = value;
    writer->position++;
}

void NBN_Writer_WriteUInt16(NBN_Writer *writer, uint16_t value) {
    NBN_Assert(writer->position + 2 <= writer->length);

    *((uint16_t *)(writer->buffer + writer->position)) = htons(value);
    writer->position += 2;
}

void NBN_Writer_WriteUInt32(NBN_Writer *writer, uint32_t value) {
    NBN_Assert(writer->position + 4 <= writer->length);

    *((uint32_t *)(writer->buffer + writer->position)) = htonl(value);
    writer->position += 4;
}

void NBN_Writer_WriteUInt64(NBN_Writer *writer, uint64_t value) {
    NBN_Assert(writer->position + 8 <= writer->length);

    *((uint64_t *)(writer->buffer + writer->position)) = htonl(1) == 1 ? value : SwapBytes64(value);
    writer->position += 8;
}

void NBN_Writer_WriteFloat(NBN_Writer *writer, float value) {
    NBN_Assert(writer->position + 4 <= writer->length);

    uint32_t *val_u = (uint32_t *)&value;
    NBN_Writer_WriteUInt32(writer, htonl(*val_u));
}

void NBN_Writer_WriteBool(NBN_Writer *writer, bool value) { NBN_Writer_WriteUInt8(writer, value); }

void NBN_Writer_WriteBytes(NBN_Writer *writer, uint8_t *bytes, unsigned int length) {
    NBN_Assert(writer->position + length <= writer->length);

    memcpy(writer->buffer + writer->position, bytes, length);
    writer->position += length;
}

void NBN_Writer_WriteString(NBN_Writer *writer, const char *str, unsigned int max_len) {
    unsigned int len = strnlen(str, max_len);

    NBN_Writer_WriteUInt32(writer, len);
    NBN_Writer_WriteBytes(writer, (uint8_t *)str, len);
}

void NBN_Reader_Init(NBN_Reader *reader, uint8_t *buffer, unsigned int length) {
    reader->buffer = buffer;
    reader->length = length;
    reader->position = 0;
}

int NBN_Reader_ReadInt8(NBN_Reader *reader, int8_t *value) { return NBN_Reader_ReadUInt8(reader, (uint8_t *)value); }

int NBN_Reader_ReadInt16(NBN_Reader *reader, int16_t *value) {
    return NBN_Reader_ReadUInt16(reader, (uint16_t *)value);
}

int NBN_Reader_ReadInt32(NBN_Reader *reader, int32_t *value) {
    return NBN_Reader_ReadUInt32(reader, (uint32_t *)value);
}

int NBN_Reader_ReadInt64(NBN_Reader *reader, int64_t *value) {
    return NBN_Reader_ReadUInt64(reader, (uint64_t *)value);
}

int NBN_Reader_ReadUInt8(NBN_Reader *reader, uint8_t *value) {
    if (reader->position + 1 > reader->length) {
        return NBN_ERROR;
    }

    *value = reader->buffer[reader->position];
    reader->position++;

    return 0;
}

int NBN_Reader_ReadUInt16(NBN_Reader *reader, uint16_t *value) {
    if (reader->position + 2 > reader->length) {
        return NBN_ERROR;
    }

    *value = ntohs(*((uint16_t *)(reader->buffer + reader->position)));
    reader->position += 2;

    return 0;
}

int NBN_Reader_ReadUInt32(NBN_Reader *reader, uint32_t *value) {
    if (reader->position + 4 > reader->length) {
        return NBN_ERROR;
    }

    *value = ntohl(*((uint32_t *)(reader->buffer + reader->position)));
    reader->position += 4;

    return 0;
}

int NBN_Reader_ReadUInt64(NBN_Reader *reader, uint64_t *value) {
    if (reader->position + 8 > reader->length) {
        return NBN_ERROR;
    }

    *value = *((uint64_t *)(reader->buffer + reader->position));

    if (htonl(1) != 1) {
        *value = SwapBytes64(*value);
    }

    reader->position += 8;

    return 0;
}

int NBN_Reader_ReadFloat(NBN_Reader *reader, float *value) {
    if (NBN_Reader_ReadUInt32(reader, (uint32_t *)value) < 0) {
        return -1;
    }

    return 0;
}

int NBN_Reader_ReadBool(NBN_Reader *reader, bool *value) {
    if (NBN_Reader_ReadUInt8(reader, (uint8_t *)value) < 0) {
        return -1;
    }

    return 0;
}

int NBN_Reader_ReadBytes(NBN_Reader *reader, uint8_t *bytes, unsigned int length) {
    if (reader->position + length > reader->length) {
        LogError("reader->position = %d, length = %d, reader->length = %d", reader->position, length, reader->length);
        return NBN_ERROR;
    }

    memcpy(bytes, reader->buffer + reader->position, length);
    reader->position += length;

    return 0;
}

int NBN_Reader_ReadString(NBN_Reader *reader, char *str, unsigned int max_len) {
    unsigned int len;

    if (NBN_Reader_ReadUInt32(reader, &len) < 0) {
        return NBN_ERROR;
    }

    if (len > max_len - 1) {
        return NBN_ERROR;
    }

    NBN_Reader_ReadBytes(reader, (uint8_t *)str, len);
    str[len] = 0;

    return 0;
}

// END OF SERIALIZATION
// ===================================================

/**
 * ====== PACKET ======
 */

static void Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
static NBN_PacketResult Packet_WriteMessage(NBN_Packet *, NBN_Message *);
static int Packet_Seal(NBN_Packet *);
static int Packet_InitRead(NBN_Packet *, uint32_t, unsigned int);

static void Packet_InitWrite(NBN_Packet *packet, uint32_t protocol_id, uint16_t seq_number, uint16_t ack,
                             uint32_t ack_bits) {
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

static NBN_PacketResult Packet_WriteMessage(NBN_Packet *packet, NBN_Message *message) {
    LogDebug("Write message %d (type: %d, length: %d, channel: %d) to packet %d", message->header.id,
             message->header.type, message->header.length, message->header.channel_id, packet->header.seq_number);

    if (packet->mode != NBN_PACKET_MODE_WRITE || packet->sealed)
        return NBN_PACKET_WRITE_ERROR;

    unsigned int message_size = NBN_MESSAGE_HEADER_SIZE + message->header.length;

    if (packet->header.messages_count >= NBN_MAX_MESSAGES_PER_PACKET ||
        packet->size + message_size > NBN_PACKET_MAX_SIZE) {
        return NBN_PACKET_WRITE_NO_SPACE;
    }

    NBN_Writer writer;

    NBN_Writer_Init(&writer, packet->buffer + packet->size, sizeof(packet->buffer) - packet->size);

    NBN_Writer_WriteUInt16(&writer, message->header.id);
    NBN_Writer_WriteUInt16(&writer, message->header.length);
    NBN_Writer_WriteUInt8(&writer, message->header.type);
    NBN_Writer_WriteUInt8(&writer, message->header.channel_id);

    NBN_Assert(writer.position == NBN_MESSAGE_HEADER_SIZE);

    if (message->header.length > 0) {
        NBN_Writer_WriteBytes(&writer, message->data, message->header.length);
    }

    NBN_Assert(writer.position == message_size);

    packet->size += writer.position;
    packet->header.messages_count++;

    return NBN_PACKET_WRITE_OK;
}

static int Packet_Seal(NBN_Packet *packet) {
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

static int Packet_InitRead(NBN_Packet *packet, uint32_t protocol_id, unsigned int size) {
    if (size < NBN_PACKET_HEADER_SIZE || size > NBN_PACKET_MAX_SIZE) {
        return NBN_ERROR;
    }

    packet->mode = NBN_PACKET_MODE_READ;
    packet->size = size;
    packet->sender = NULL; // IMPORTANT: must be set by the drivers
    packet->sealed = false;

    NBN_Reader reader;

    NBN_Reader_Init(&reader, packet->buffer, NBN_PACKET_HEADER_SIZE);

    if (NBN_Reader_ReadUInt32(&reader, &packet->header.protocol_id) < 0) {
        LogDebug("Failed to read packet's protocol id");
        return NBN_ERROR;
    }

    if (packet->header.protocol_id != protocol_id) {
        LogDebug("Packet's protocol id did not match (expected: %d, received: %d)", protocol_id,
                 packet->header.protocol_id);
        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt32(&reader, &packet->header.ack_bits) < 0) {
        LogDebug("Failed to read packet's acked bits");
        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt16(&reader, &packet->header.seq_number) < 0) {
        LogDebug("Failed to read packet's sequence number");
        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt16(&reader, &packet->header.ack) < 0) {
        LogDebug("Failed to read packet's ack");
        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(&reader, &packet->header.messages_count) < 0) {
        LogDebug("Failed to read packet's message count");
        return NBN_ERROR;
    }

    return 0;
}

// END OF PACKET
// ===================================================

/**
 * ====== CHANNEL ======
 */

static unsigned int Channel_ComputeMessageIdDelta(uint16_t id1, uint16_t id2) {
    if (SEQUENCE_NUMBER_GT(id1, id2))
        return (id1 >= id2) ? id1 - id2 : ((0xFFFF + 1) - id2) + id1;
    else
        return (id2 >= id1) ? id2 - id1 : (((0xFFFF + 1) - id1) + id2) % 0xFFFF;
}

static void Channel_Init(NBN_Channel *channel, uint8_t id, NBN_Channel_Config cfg) {
    channel->id = id;
    channel->mode = cfg.mode;
    channel->next_outgoing_message_id = 0;
    channel->next_recv_message_id = 0;
    channel->outgoing_message_count = 0;
    channel->last_received_message_id = -1;
    channel->next_outgoing_message_slot = 0;
    channel->oldest_unacked_message_id = 0;
    channel->most_recent_message_id = 0;
    channel->buffer_size = cfg.buffer_size;
    channel->max_message_len = cfg.max_message_len;
    channel->current_capacity = cfg.buffer_size;
    channel->outgoing_messages_buffer = malloc(sizeof(NBN_OutgoingMessage) * cfg.buffer_size);
    channel->incoming_messages_buffer = malloc(sizeof(NBN_IncomingMessage) * cfg.buffer_size);
    channel->ack_buffer = malloc(sizeof(bool) * cfg.buffer_size);

    for (unsigned int i = 0; i < cfg.buffer_size; i++) {
        channel->incoming_messages_buffer[i].message.data = malloc(cfg.max_message_len);
        channel->incoming_messages_buffer[i].free = true;

        channel->outgoing_messages_buffer[i].message.data = malloc(cfg.max_message_len);
        channel->outgoing_messages_buffer[i].free = true;
    }

    for (unsigned int i = 0; i < cfg.buffer_size; i++) {
        channel->ack_buffer[i] = false;
    }
}

static void Channel_UpdateMessageSendTime(NBN_Channel *channel, uint16_t msg_id, float time) {
    NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[msg_id % channel->buffer_size];

    NBN_Assert(msg_id == out_msg->message.header.id);
    out_msg->last_send_time = time;
}

static NBN_IncomingMessage *Channel_AddReceivedMessage(NBN_Channel *channel, NBN_MessageHeader *header) {
    // TODO: check if current slot not free
    // buffer ran out of slots

    NBN_Assert(header->length <= channel->max_message_len); // TODO: replace with channel msg len

    if (channel->mode == NBN_CHANNEL_UNRELIABLE) {
        if (SEQUENCE_NUMBER_GT(header->id, channel->last_received_message_id)) {
            NBN_IncomingMessage *inc_msg = &channel->incoming_messages_buffer[header->id % channel->buffer_size];

            inc_msg->free = false;
            inc_msg->message.header = *header;

            channel->last_received_message_id = header->id;

            LogDebug("Add incomoing message %d of type %d to unreliable channel %d (last received msg id: %d)",
                     header->id, header->type, channel->id, channel->last_received_message_id);

            return inc_msg;
        }

        return NULL;
    } else if (channel->mode == NBN_CHANNEL_RELIABLE) {
        unsigned int dt = Channel_ComputeMessageIdDelta(header->id, channel->most_recent_message_id);

        if (SEQUENCE_NUMBER_GT(header->id, channel->most_recent_message_id)) {
            NBN_Assert(dt < channel->buffer_size);

            channel->most_recent_message_id = header->id;
        } else {
            if (dt >= channel->buffer_size)
                return NULL;

            if (SEQUENCE_NUMBER_LT(header->id, channel->next_recv_message_id)) {
                return NULL;
            }
        }

        LogDebug("Add incomoing message %d of type %d to reliable channel %d (most recent msg id: %d, dt: %d)",
                 header->id, header->type, channel->id, channel->most_recent_message_id, dt);

        NBN_IncomingMessage *inc_msg = &channel->incoming_messages_buffer[header->id % channel->buffer_size];

        inc_msg->free = false;
        inc_msg->message.header = *header;

        return inc_msg;
    }

    NBN_Abort();
}

static NBN_Writer *Channel_AddOutgoingMessage(NBN_Channel *channel, uint8_t type) {
    NBN_Assert(channel->mode == NBN_CHANNEL_UNRELIABLE || channel->mode == NBN_CHANNEL_RELIABLE);

    uint16_t msg_id = channel->next_outgoing_message_id;
    int index = msg_id % channel->buffer_size;
    NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[index];

    // make sure the outgoing message is not already in use
    if (channel->current_capacity == 0) {
        LogError("Channel %d outgoing buffer reached it's capacity (mode: %d, outgoing message count: %d, msg_id: %d, "
                 "index: %d, oldest unacked msg: %d)",
                 channel->id, channel->mode, channel->outgoing_message_count, msg_id, index,
                 channel->oldest_unacked_message_id);
#ifdef NBN_DEBUG
        NBN_Abort();
#endif

        return NULL;
    }

    NBN_Assert(out_msg->free);

    out_msg->free = false;
    out_msg->last_send_time = -1;
    out_msg->message.header.id = msg_id;
    out_msg->message.header.channel_id = channel->id;
    out_msg->message.header.type = type;
    out_msg->message.header.length = 0;

    channel->next_outgoing_message_id++;
    channel->outgoing_message_count++;
    channel->current_capacity--;

    NBN_Writer_Init(&out_msg->writer, out_msg->message.data, channel->max_message_len);

    LogDebug("Outgoing message %d (type: %d) added to channel %d", msg_id, type, channel->id);

    return &out_msg->writer;
}

static NBN_Message *Channel_GetNextRecvedMessage(NBN_Channel *channel) {
    if (channel->mode == NBN_CHANNEL_UNRELIABLE) {
        while (SEQUENCE_NUMBER_LTE(channel->next_recv_message_id, channel->last_received_message_id)) {
            NBN_IncomingMessage *inc_msg =
                &channel->incoming_messages_buffer[channel->next_recv_message_id % channel->buffer_size];
            uint16_t msg_id = channel->next_recv_message_id;

            channel->next_recv_message_id++;

            if (!inc_msg->free && inc_msg->message.header.id == msg_id) {
                inc_msg->free = true;

                return &inc_msg->message;
            }
        }

        return NULL;
    } else if (channel->mode == NBN_CHANNEL_RELIABLE) {
        NBN_IncomingMessage *inc_msg =
            &channel->incoming_messages_buffer[channel->next_recv_message_id % channel->buffer_size];

        if (!inc_msg->free && inc_msg->message.header.id == channel->next_recv_message_id) {
            inc_msg->free = true;
            channel->next_recv_message_id++;

            return &inc_msg->message;
        }

        return NULL;
    } else {
        NBN_Abort();
    }
}

static bool Channel_GetNextOutgoingMessage(NBN_Channel *channel, NBN_Message *res_msg, float time) {
    if (channel->mode == NBN_CHANNEL_UNRELIABLE) {
        NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[channel->next_outgoing_message_slot];

        if (out_msg->free)
            return false;

        *res_msg = out_msg->message;
        res_msg->header.length = out_msg->writer.position;
        out_msg->free = true;

        channel->next_outgoing_message_slot++;
        channel->next_outgoing_message_slot %= channel->buffer_size;

        return true;
    } else if (channel->mode == NBN_CHANNEL_RELIABLE) {
        int max_message_id = (channel->oldest_unacked_message_id + (channel->buffer_size - 1)) % (0xFFFF + 1);

        if (SEQUENCE_NUMBER_LT(channel->next_outgoing_message_id, max_message_id))
            max_message_id = channel->next_outgoing_message_id;

        uint16_t msg_id = channel->oldest_unacked_message_id;

        while (SEQUENCE_NUMBER_LT(msg_id, max_message_id)) {
            NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[msg_id % channel->buffer_size];

            if (!out_msg->free &&
                (out_msg->last_send_time < 0 || time - out_msg->last_send_time >= NBN_MESSAGE_RESEND_DELAY)) {
                *res_msg = out_msg->message;
                res_msg->header.length = out_msg->writer.position;
                return true;
            }

            msg_id++;
        }

        return false;
    } else {
        NBN_Abort();
    }
}

static int Channel_OnMessageSent(NBN_Channel *channel) {
    if (channel->mode == NBN_CHANNEL_UNRELIABLE) {
        channel->outgoing_message_count--;
        channel->current_capacity++;
    }

    return 0;
}

static int Channel_OnOutgoingMessageAcked(NBN_Channel *channel, uint16_t msg_id) {
    if (channel->mode != NBN_CHANNEL_RELIABLE) {
        return 0;
    }

    int index = msg_id % channel->buffer_size;
    NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[index];

    if (out_msg->free || out_msg->message.header.id != msg_id)
        return 0;

    out_msg->free = true;

    LogDebug("Message %d acked on channel %d (buffer index: %d, oldest unacked: %d)", msg_id, channel->id, index,
             channel->oldest_unacked_message_id);

    channel->ack_buffer[index] = true;
    channel->outgoing_message_count--;

    if (msg_id == channel->oldest_unacked_message_id) {
        for (unsigned int i = 0; i < channel->buffer_size; i++) {
            uint16_t ack_msg_id = msg_id + i;
            int index = ack_msg_id % channel->buffer_size;

            if (channel->ack_buffer[index]) {
                channel->ack_buffer[index] = false;
                channel->oldest_unacked_message_id++;
                channel->current_capacity++;
                NBN_Assert(channel->current_capacity <= channel->buffer_size);
            } else {
                break;
            }
        }

        LogDebug("Updated oldest unacked message id on channel %d: %d", channel->id,
                 channel->oldest_unacked_message_id);
    }

    return 0;
}

// END OF CHANNEL
// ===================================================

/**
 * ====== CONNECTION ======
 */

static void Connection_Destroy(NBN_Connection *);
static uint32_t Connection_BuildPacketAckBits(NBN_Connection *);
static int Connection_DecodePacketHeader(NBN_Connection *, NBN_Packet *, float);
static int Connection_AckPacket(NBN_Connection *, uint16_t, float);
static void Connection_InitOutgoingPacket(NBN_Connection *, uint32_t, NBN_Packet *, NBN_PacketEntry **);
static NBN_PacketEntry *Connection_InsertOutgoingPacketEntry(NBN_Connection *, uint16_t);
static bool Connection_InsertReceivedPacketEntry(NBN_Connection *, uint16_t);
static NBN_PacketEntry *Connection_FindSendPacketEntry(NBN_Connection *, uint16_t);
static bool Connection_IsPacketReceived(NBN_Connection *, uint16_t);
static int Connection_SendPacket(NBN_Endpoint *, NBN_Connection *, NBN_Packet *, NBN_PacketEntry *, float, bool);
static int Connection_ReadNextMessageHeader(NBN_Reader *, NBN_MessageHeader *);
static void Connection_UpdateAveragePing(NBN_Connection *, float);
static void Connection_UpdateAveragePacketLoss(NBN_Connection *, uint16_t);
static void Connection_UpdateAverageUploadBandwidth(NBN_Connection *, float);
static void Connection_UpdateAverageDownloadBandwidth(NBN_Connection *, float);
static int Connection_ProcessReceivedPacket(NBN_Endpoint *, NBN_Connection *, NBN_Packet *, float);
static int Connection_FlushChannels(NBN_Endpoint *, NBN_Connection *, uint32_t, float);
static bool Connection_CheckIfStale(NBN_Connection *, float);

static int Connection_ProcessReceivedPacket(NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_Packet *packet,
                                            float time) {
    if (Connection_DecodePacketHeader(connection, packet, time) < 0) {
        LogError("Failed to decode packet %d header", packet->header.seq_number);

        return NBN_ERROR;
    }

    Connection_UpdateAveragePacketLoss(connection, packet->header.ack);

    if (!Connection_InsertReceivedPacketEntry(connection, packet->header.seq_number))
        return 0;

    if (SEQUENCE_NUMBER_GT(packet->header.seq_number, connection->last_received_packet_seq_number))
        connection->last_received_packet_seq_number = packet->header.seq_number;

    NBN_Reader msg_reader;

    NBN_Reader_Init(&msg_reader, packet->buffer + NBN_PACKET_HEADER_SIZE, packet->size - NBN_PACKET_HEADER_SIZE);

    LogDebug("Processing received packet %d (message count: %d)", packet->header.seq_number,
             packet->header.messages_count);

    for (int i = 0; i < packet->header.messages_count; i++) {
        LogDebug("Reading message number %d from packet %d", i, packet->header.seq_number);

        NBN_MessageHeader header = {0};
        int msg_len = Connection_ReadNextMessageHeader(&msg_reader, &header);

        if (msg_len < 0) {
            LogError("Failed to read packet: invalid message header");

            return NBN_ERROR;
        }

        uint8_t channel_id = header.channel_id;

        if (channel_id > endpoint->channel_count - 1) {
            LogError("Failed to read packet: invalid channel %d", channel_id);

            return NBN_ERROR;
        }

        NBN_Channel *channel = &connection->channels[channel_id];

        if ((unsigned int)msg_len > channel->max_message_len) {
            LogError("Failed to read packet: message %d too large for channel %d (%d > %d)", header.id, channel_id,
                     header.length, channel->max_message_len);

            return NBN_ERROR;
        }

        NBN_IncomingMessage *inc_msg = Channel_AddReceivedMessage(channel, &header);

        if (inc_msg) {
            if (msg_len > 0) {
                if (NBN_Reader_ReadBytes(&msg_reader, inc_msg->message.data, msg_len) < 0) {
                    LogError("Failed to read message data");

                    return NBN_ERROR;
                }
            }

            LogDebug("Received message %d (type: %d) on channel %d", header.id, header.type, channel->id);
        } else {
            LogDebug("Message %d was discarded by channel %d", header.id, channel->id);

            // NBN_Reader_ReadBytes is not called for discarded messages, so we need to
            // advance the reader position "manually"
            msg_reader.position += msg_len;
        }
    }

    return 0;
}

static int Connection_FlushChannels(NBN_Endpoint *endpoint, NBN_Connection *connection, uint32_t protocol_id,
                                    float time) {
    LogDebug("Flushing all channels");

    NBN_PacketEntry *packet_entry;
    NBN_Packet *packet = &endpoint->read_packet;

    unsigned int sent_packet_count = 0;
    unsigned int sent_bytes = 0;

    Connection_InitOutgoingPacket(connection, protocol_id, packet, &packet_entry);

    for (unsigned int i = 0; i < endpoint->channel_count; i++) {
        NBN_Channel *channel = &connection->channels[i];

        LogDebug("Flushing channel %d (message count: %d)", channel->id, channel->outgoing_message_count);

        NBN_Message out_msg = {0};
        unsigned int j = 0;

        // TODO: use bandwidth to determine how many packets to send at most
        while (j < channel->outgoing_message_count && sent_packet_count < 16 &&
               Channel_GetNextOutgoingMessage(channel, &out_msg, time)) {
            uint8_t msg_type = out_msg.header.type;
            uint16_t msg_id = out_msg.header.id;
            uint16_t msg_len = out_msg.header.length;
            bool message_sent = false;
            NBN_PacketResult ret = Packet_WriteMessage(packet, &out_msg);

            if (ret == NBN_PACKET_WRITE_OK) {
                message_sent = true;
            } else if (ret == NBN_PACKET_WRITE_NO_SPACE) {
                if (Connection_SendPacket(endpoint, connection, packet, packet_entry, time, endpoint->is_server) < 0) {
                    LogError("Failed to send packet %d", packet->header.seq_number);

                    return NBN_ERROR;
                }

                sent_packet_count++;
                sent_bytes += packet->size;

                Connection_InitOutgoingPacket(connection, protocol_id, packet, &packet_entry);

                NBN_PacketResult ret = Packet_WriteMessage(packet, &out_msg);

                if (ret != NBN_PACKET_WRITE_OK) {
                    LogError("Failed to send packet %d", packet->header.seq_number);

                    return NBN_ERROR;
                }

                message_sent = true;
            } else if (ret == NBN_PACKET_WRITE_ERROR) {
                LogError("Failed to write message %d of type %d to packet %d", msg_id, msg_type,
                         packet->header.seq_number);

                return NBN_ERROR;
            }

            if (message_sent) {
                LogDebug("Message %d added to packet %d (length: %d, type: %d)", msg_id, packet->header.seq_number,
                         msg_len, msg_type);

                Channel_UpdateMessageSendTime(channel, msg_id, time);

                packet_entry->messages[packet_entry->messages_count++] = (NBN_MessageEntry){msg_id, channel->id};

                Channel_OnMessageSent(channel);
            }

            j++;
        }
    }

    if (Connection_SendPacket(endpoint, connection, packet, packet_entry, time, endpoint->is_server) < 0) {
        LogError("Failed to send packet %d to connection %lld", packet->header.seq_number, connection->handle.id);

        return NBN_ERROR;
    }

    sent_bytes += packet->size;
    sent_packet_count++;

    float t = time - connection->last_flush_time;

    if (t > 0)
        Connection_UpdateAverageUploadBandwidth(connection, sent_bytes / t);

    connection->last_flush_time = time;

    return 0;
}

static bool Connection_CheckIfStale(NBN_Connection *connection, float time) {
#if defined(NBN_DEBUG) && defined(NBN_DISABLE_STALE_CONNECTION_DETECTION)
    /* When testing under bad network conditions (in soak test for instance), we don't want to deal
       with stale connections */
    return false;
#else
    return time - connection->last_recv_packet_time > NBN_CONNECTION_STALE_TIME_THRESHOLD;
#endif
}

static int Connection_DecodePacketHeader(NBN_Connection *connection, NBN_Packet *packet, float time) {
    if (Connection_AckPacket(connection, packet->header.ack, time) < 0) {
        LogError("Failed to ack packet %d", packet->header.seq_number);

        return NBN_ERROR;
    }

    for (unsigned int i = 0; i < 32; i++) {
        if (B_IS_UNSET(packet->header.ack_bits, i))
            continue;

        if (Connection_AckPacket(connection, packet->header.ack - (i + 1), time) < 0) {
            LogError("Failed to ack packet %d", packet->header.seq_number);

            return NBN_ERROR;
        }
    }

    return 0;
}

static void Connection_Destroy(NBN_Connection *connection) {
    for (unsigned int i = 0; i < connection->channel_count; i++) {
        NBN_Channel *channel = &connection->channels[i];

        for (unsigned int j = 0; j < channel->buffer_size; j++) {
            free(channel->incoming_messages_buffer[j].message.data);
            free(channel->outgoing_messages_buffer[j].message.data);
        }

        free(channel->incoming_messages_buffer);
        free(channel->outgoing_messages_buffer);
        free(channel->ack_buffer);
    }

    free(connection->channels);
    free(connection);
}

static uint32_t Connection_BuildPacketAckBits(NBN_Connection *connection) {
    uint32_t ack_bits = 0;

    for (int i = 0; i < 32; i++) {
        /*
           when last_received_packet_seq_number is lower than 32, the value of acked_packet_seq_number will
           eventually wrap around, which means the packets from before the wrap around will naturally be acked
           */

        uint16_t acked_packet_seq_number = connection->last_received_packet_seq_number - (i + 1);

        if (Connection_IsPacketReceived(connection, acked_packet_seq_number))
            B_SET(ack_bits, i);
    }

    return ack_bits;
}

static int Connection_AckPacket(NBN_Connection *connection, uint16_t ack_packet_seq_number, float time) {
    NBN_PacketEntry *packet_entry = Connection_FindSendPacketEntry(connection, ack_packet_seq_number);

    if (packet_entry && !packet_entry->acked) {
        LogDebug("Packet %d acked (connection: %lld)", ack_packet_seq_number, connection->handle.id);

        packet_entry->acked = true;

        Connection_UpdateAveragePing(connection, time - packet_entry->send_time);

        for (unsigned int i = 0; i < packet_entry->messages_count; i++) {
            NBN_MessageEntry *msg_entry = &packet_entry->messages[i];
            NBN_Channel *channel = &connection->channels[msg_entry->channel_id];

            NBN_Assert(channel != NULL);

            if (Channel_OnOutgoingMessageAcked(channel, msg_entry->id) < 0) {
                return NBN_ERROR;
            }
        }
    }

    return 0;
}

static void Connection_InitOutgoingPacket(NBN_Connection *connection, uint32_t protocol_id, NBN_Packet *outgoing_packet,
                                          NBN_PacketEntry **packet_entry) {
    Packet_InitWrite(outgoing_packet, protocol_id, connection->next_packet_seq_number++,
                     connection->last_received_packet_seq_number, Connection_BuildPacketAckBits(connection));

    *packet_entry = Connection_InsertOutgoingPacketEntry(connection, outgoing_packet->header.seq_number);
}

static NBN_PacketEntry *Connection_InsertOutgoingPacketEntry(NBN_Connection *connection, uint16_t seq_number) {
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    connection->packet_send_seq_buffer[index] = seq_number;

    NBN_PacketEntry *entry = &connection->packet_send_buffer[index];
    entry->acked = false;
    entry->lost = false;
    entry->send_time = 0;
    entry->messages_count = 0;

    return entry;
}

static bool Connection_InsertReceivedPacketEntry(NBN_Connection *connection, uint16_t seq_number) {
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
    if (SEQUENCE_NUMBER_GT(seq_number, connection->last_received_packet_seq_number)) {
        for (uint16_t seq = connection->last_received_packet_seq_number + 1; SEQUENCE_NUMBER_LT(seq, seq_number); seq++)
            connection->packet_recv_seq_buffer[seq % NBN_MAX_PACKET_ENTRIES] = 0xFFFFFFFF;
    }

    connection->packet_recv_seq_buffer[index] = seq_number;

    return true;
}

static NBN_PacketEntry *Connection_FindSendPacketEntry(NBN_Connection *connection, uint16_t seq_number) {
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;

    if (connection->packet_send_seq_buffer[index] == seq_number)
        return &connection->packet_send_buffer[index];

    return NULL;
}

static bool Connection_IsPacketReceived(NBN_Connection *connection, uint16_t packet_seq_number) {
    uint16_t index = packet_seq_number % NBN_MAX_PACKET_ENTRIES;

    return connection->packet_recv_seq_buffer[index] == packet_seq_number;
}

static int Connection_SendPacket(NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_Packet *packet,
                                 NBN_PacketEntry *packet_entry, float time, bool is_server) {
    LogDebug("Send packet %d to connection %lld (messages count: %d)", packet->header.seq_number, connection->handle.id,
             packet->header.messages_count);

    NBN_Assert(packet_entry->messages_count == packet->header.messages_count);

    if (Packet_Seal(packet) < 0) {
        LogError("Failed to seal packet");

        return NBN_ERROR;
    }

    packet_entry->send_time = time;

    if (is_server) {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return PacketSimulator_EnqueuePacket(&endpoint->packet_simulator, packet, connection);
#else
        if (connection->is_stale)
            return 0;

        return connection->driver->impl.serv_send_packet_to((NBN_Server *)endpoint, packet, connection);
#endif
    } else {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return PacketSimulator_EnqueuePacket(&endpoint->packet_simulator, packet, connection);
#else
        return connection->driver->impl.cli_send_packet((NBN_Client *)endpoint, packet, connection);
#endif
    }
}

static int Connection_ReadNextMessageHeader(NBN_Reader *reader, NBN_MessageHeader *header) {
    if (NBN_Reader_ReadUInt16(reader, &header->id) < 0) {
        LogError("Failed to read message id");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt16(reader, &header->length) < 0) {
        LogError("Failed to read message length");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(reader, &header->type) < 0) {
        LogError("Failed to read message type");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(reader, &header->channel_id) < 0) {
        LogError("Failed to read message channel");

        return NBN_ERROR;
    }

    return header->length;
}

static void Connection_UpdateAveragePing(NBN_Connection *connection, float ping) {
    /* exponential smoothing with a factor of 0.05 */
    connection->stats.ping = connection->stats.ping + .05f * (ping - connection->stats.ping);
}

static void Connection_UpdateAveragePacketLoss(NBN_Connection *connection, uint16_t seq) {
    unsigned int lost_packet_count = 0;
    uint16_t start_seq = seq - 64;

    for (int i = 0; i < 100; i++) {
        uint16_t s = start_seq - i;
        NBN_PacketEntry *entry = Connection_FindSendPacketEntry(connection, s);

        if (entry && !entry->acked) {
            lost_packet_count++;

            if (!entry->lost) {
                entry->lost = true;
                connection->stats.total_lost_packets++;
            }
        }
    }

    float packet_loss = lost_packet_count / 100.f;

    /* exponential smoothing with a factor of 0.1 */
    connection->stats.packet_loss = connection->stats.packet_loss + .1f * (packet_loss - connection->stats.packet_loss);
}

static void Connection_UpdateAverageUploadBandwidth(NBN_Connection *connection, float bytes_per_sec) {
    /* exponential smoothing with a factor of 0.1 */
    connection->stats.upload_bandwidth =
        connection->stats.upload_bandwidth + .1f * (bytes_per_sec - connection->stats.upload_bandwidth);
}

static void Connection_UpdateAverageDownloadBandwidth(NBN_Connection *connection, float time) {
    float t = time - connection->last_read_packets_time;

    if (t == 0)
        return;

    float bytes_per_sec = connection->downloaded_bytes / t;

    /* exponential smoothing with a factor of 0.1 */
    connection->stats.download_bandwidth =
        connection->stats.download_bandwidth + .1f * (bytes_per_sec - connection->stats.download_bandwidth);

    connection->downloaded_bytes = 0;
}

// END OF CONNECTION
// ===================================================

/**
 * ====== ENDPOINT ======
 */

static void Endpoint_Init(NBN_Endpoint *, uint32_t, bool, NBN_Channel_Config *, unsigned int);
static void Endpoint_Deinit(NBN_Endpoint *);
static NBN_Connection *Endpoint_CreateConnection(NBN_Endpoint *, NBN_Connection_ID, NBN_Driver_ID);
static uint32_t Endpoint_BuildProtocolId(const char *);
static int Endpoint_ProcessReceivedPacket(NBN_Endpoint *, NBN_Packet *, NBN_Connection *);
static void Endpoint_UpdateTime(NBN_Endpoint *);
static NBN_Writer *Endpoint_CreateOutgoingMessage(NBN_Endpoint *, NBN_Connection *, uint8_t, uint8_t);

static void Endpoint_Init(NBN_Endpoint *endpoint, uint32_t protocol_id, bool is_server, NBN_Channel_Config *channels,
                          unsigned int channel_count) {
    NBN_Assert(channel_count >= 2 && channel_count <= NBN_MAX_CHANNEL_COUNT);

    endpoint->is_server = is_server;
    endpoint->protocol_id = protocol_id;
    endpoint->channel_count = channel_count;
    endpoint->channels = malloc(sizeof(NBN_Channel_Config) * channel_count);

    memcpy(endpoint->channels, channels, sizeof(NBN_Channel_Config) * channel_count);

    EventQueue_Init(&endpoint->event_queue);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    PacketSimulator_Init(&endpoint->packet_simulator, endpoint);
    PacketSimulator_Start(&endpoint->packet_simulator);
#endif

    Endpoint_UpdateTime(endpoint);
}

static void Endpoint_Deinit(NBN_Endpoint *endpoint) {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    PacketSimulator_Stop(&endpoint->packet_simulator);
#endif
}

static NBN_Connection *Endpoint_CreateConnection(NBN_Endpoint *endpoint, NBN_Connection_ID id,
                                                 NBN_Driver_ID driver_id) {
    NBN_Connection *connection = (NBN_Connection *)malloc(sizeof(NBN_Connection));

    connection->handle.id = id;
    connection->handle.user_data = NULL;
    connection->last_recv_packet_time = endpoint->time;
    connection->next_packet_seq_number = 1;
    connection->last_received_packet_seq_number = 0;
    connection->last_flush_time = endpoint->time;
    connection->last_read_packets_time = endpoint->time;
    connection->downloaded_bytes = 0;
    connection->is_accepted = false;
    connection->is_stale = false;
    connection->is_closed = false;

    for (int i = 0; i < NBN_MAX_PACKET_ENTRIES; i++) {
        connection->packet_send_seq_buffer[i] = 0xFFFFFFFF;
        connection->packet_recv_seq_buffer[i] = 0xFFFFFFFF;
    }

    NBN_ConnectionStats stats = {0};

    connection->stats = stats;
    connection->channels = malloc(sizeof(NBN_Channel) * endpoint->channel_count);
    connection->channel_count = endpoint->channel_count;

    for (unsigned int i = 0; i < endpoint->channel_count; i++) {
        Channel_Init(&connection->channels[i], i, endpoint->channels[i]);
    }

    switch (driver_id) {
#ifdef NBN_UDP
    case NBN_DRIVER_UDP:
        connection->driver = &nbn_udp_driver;
        break;
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    case NBN_DRIVER_WEBRTC_NATIVE:
        connection->driver = &nbn_webrtc_native_driver;
        break;
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    case NBN_DRIVER_WEBRTC_EMSCRIPTEN:
        connection->driver = &nbn_webrtc_em_driver;
        break;
#endif
    default:
        LogError("Unsupported driver: %d", driver_id);
        NBN_Abort();
    }

    return connection;
}

static uint32_t Endpoint_BuildProtocolId(const char *protocol_name) {
    uint32_t protocol_id = 2166136261;

    for (unsigned int i = 0; i < strlen(protocol_name); i++) {
        protocol_id *= 16777619;
        protocol_id ^= protocol_name[i];
    }

    return protocol_id;
}

static int Endpoint_ProcessReceivedPacket(NBN_Endpoint *endpoint, NBN_Packet *packet, NBN_Connection *connection) {
    (void)endpoint;

    LogDebug("Received packet %d (conn id: %lld, ack: %d, messages count: %d)", packet->header.seq_number,
             connection->handle.id, packet->header.ack, packet->header.messages_count);

    if (Connection_ProcessReceivedPacket(endpoint, connection, packet, endpoint->time) < 0) {
        LogError("Error when processing packet");
        return NBN_ERROR;
    }

    connection->last_recv_packet_time = endpoint->time;
    connection->downloaded_bytes += packet->size;

    return 0;
}

static NBN_Writer *Endpoint_CreateOutgoingMessage(NBN_Endpoint *endpoint, NBN_Connection *connection, uint8_t type,
                                                  uint8_t channel_id) {
    NBN_Assert(channel_id < endpoint->channel_count);
    NBN_Assert(!connection->is_closed || type == NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Assert(!connection->is_stale);

    LogDebug("Create outgoing message of type %d on channel %d", type, channel_id);

    NBN_Channel *channel = &connection->channels[channel_id];
    NBN_Writer *writer = Channel_AddOutgoingMessage(channel, type);

    if (!writer) {
        LogError("Failed to enqueue outgoing message of type %d on channel %d", type, channel_id);

        return NULL;
    }

    return writer;
}

static void Endpoint_UpdateTime(NBN_Endpoint *endpoint) {
#if defined(NBN_PLATFORM_WINDOWS)
    endpoint->time = GetTickCount64() / 1000.0;
#elif defined(__EMSCRIPTEN__)
    endpoint->time = emscripten_get_now() / 1000;
#else
    static struct timespec tp;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &tp) < 0) {
        LogError("gettimeofday() failed");
        NBN_Abort();
    }

    endpoint->time = tp.tv_sec + (tp.tv_nsec / (double)1e9);
#endif // NBN_PLATFORM_WINDOWS
}

// END OF ENDPOINT
// ===================================================

/**
 * ====== CLIENT ======
 */

static int Client_ProcessReceivedMessage(NBN_Client *, NBN_Message *, NBN_Connection *);
static NBN_Client_Event Client_HandleEvent(NBN_Client *);
static NBN_Client_Event Client_HandleMessageReceivedEvent(NBN_Client *);
static NBN_Connection *CreateServerConnection(NBN_Client *client, NBN_Driver_ID driver_id);

NBN_Client *NBN_Client_Create(const char *protocol_name, const char *host, uint16_t port) {
    NBN_Client *client = malloc(sizeof(NBN_Client));

    client->config = (NBN_Client_Config){.protocol_name = protocol_name, .host = host, .port = port};
    client->client_data_writer.position = 0;

    client->endpoint.default_reliable_channel = NBN_Client_CreateChannel(
        client, NBN_CHANNEL_RELIABLE, NBN_CHANNEL_DEFAULT_BUFFER_SIZE, NBN_CHANNEL_DEFAULT_MAX_MESSAGE_SIZE);
    client->endpoint.default_unreliable_channel = NBN_Client_CreateChannel(
        client, NBN_CHANNEL_UNRELIABLE, NBN_CHANNEL_DEFAULT_BUFFER_SIZE, NBN_CHANNEL_DEFAULT_MAX_MESSAGE_SIZE);

#if defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
    client->driver_data.webrtc.cfg = NBN_WEBRTC_DEFAULT_CONFIG;
    client->driver_data.webrtc.is_connected = false;
#endif

    return client;
}

uint8_t NBN_Client_CreateChannel(NBN_Client *client, NBN_Channel_Mode mode, unsigned int buffer_size,
                                 unsigned int max_message_len) {
    NBN_Client_Config *cfg = &client->config;

    NBN_Assert(cfg->channel_count < NBN_MAX_CHANNEL_COUNT);

    uint8_t channel_id = cfg->channel_count;

    cfg->channels[channel_id] =
        (NBN_Channel_Config){.mode = mode, .buffer_size = buffer_size, .max_message_len = max_message_len};
    cfg->channel_count++;

    return channel_id;
}

unsigned int NBN_Client_GetChannelCurrentCapacity(NBN_Client *client, uint8_t channel_id) {
    NBN_Assert(channel_id < client->endpoint.channel_count);

    NBN_Channel *channel = &client->server_connection->channels[channel_id];

    return channel->current_capacity;
}

NBN_Writer *NBN_Client_WriteConnectionRequestData(NBN_Client *client) {
    NBN_Writer_Init(&client->client_data_writer, client->endpoint.connection_request_data_buffer,
                    sizeof(client->endpoint.connection_request_data_buffer));

    return &client->client_data_writer;
}

static int StartClientDrivers(NBN_Client *client, const char *host, uint16_t port) {
    int driver_count = 0;

#ifdef NBN_UDP
    client->server_connection = CreateServerConnection(client, NBN_DRIVER_UDP);

    if (nbn_udp_driver.impl.cli_start(client, host, port) < 0) {
        LogError("Failed to start driver %s", nbn_udp_driver.name);
        return NBN_ERROR;
    }

    LogInfo("%s driver started", nbn_udp_driver.name);
    driver_count++;
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    client->server_connection = CreateServerConnection(client, NBN_DRIVER_WEBRTC_NATIVE);

    if (nbn_webrtc_native_driver.impl.cli_start(client, host, port) < 0) {
        LogError("Failed to start driver %s", nbn_webrtc_native_driver.name);
        return NBN_ERROR;
    }

    LogInfo("%s driver started", nbn_webrtc_native_driver.name);
    driver_count++;
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    client->server_connection = CreateServerConnection(client, NBN_DRIVER_WEBRTC_EMSCRIPTEN);

    if (nbn_webrtc_em_driver.impl.cli_start(client, host, port) < 0) {
        LogError("Failed to start driver %s", nbn_webrtc_em_driver.name);
        return NBN_ERROR;
    }

    LogInfo("%s driver started", nbn_webrtc_em_driver.name);
    driver_count++;
#endif // __EMSCRIPTEN__

    client->server_connection->driver_data.endpoint_ptr = client;

    return driver_count;
}

int NBN_Client_Start(NBN_Client *client) {
    NBN_Client_Config config = client->config;
    const char *protocol_name = config.protocol_name;
    const char *host = config.host;
    uint16_t port = config.port;
    uint32_t protocol_id = Endpoint_BuildProtocolId(protocol_name);

    Endpoint_Init(&client->endpoint, protocol_id, false, config.channels, config.channel_count);

    int driver_count = StartClientDrivers(client, host, port);

    if (driver_count < 1) {
        LogError("At least one network driver has to be activated");
        NBN_Abort();
    } else if (driver_count > 1) {
        LogError("Only one network driver can be activated for the client");
        NBN_Abort();
    }

    client->is_connected = false;
    client->closed_code = -1;

    unsigned int connection_data_len = client->client_data_writer.position;

    NBN_Writer *writer = NBN_Client_CreateReliableMessage(client, NBN_CONNECTION_REQUEST_MESSAGE_TYPE);

    if (!writer) {
        return NBN_ERROR;
    }

    if (connection_data_len > 0) {
        NBN_Assert(connection_data_len <= sizeof(client->endpoint.connection_request_data_buffer));

        NBN_Writer_WriteUInt32(writer, connection_data_len);
        NBN_Writer_WriteBytes(writer, client->endpoint.connection_request_data_buffer, connection_data_len);
    } else {
        NBN_Writer_WriteUInt32(writer, 0);
    }

    LogInfo("Started");

    return 0;
}

void NBN_Client_Stop(NBN_Client *client) {
    // Poll remaining events to clear the event queue
    while (NBN_Client_Poll(client) != NBN_CLIENT_NO_EVENT) {
    }

    if (client->server_connection) {
        if (!client->server_connection->is_closed && !client->server_connection->is_stale) {
            LogInfo("Disconnecting...");

            if (!NBN_Client_CreateReliableMessage(client, NBN_DISCONNECTION_MESSAGE_TYPE)) {
                LogError("Failed to send disconnection message");
            }

            if (NBN_Client_Flush(client) < 0) {
                LogError("Failed to send packets");
            }

            client->server_connection->is_closed = true;

            LogInfo("Disconnected");
        }

        Connection_Destroy(client->server_connection);
        client->server_connection = NULL;
    }

    LogInfo("Stopping all drivers...");

#ifdef NBN_UDP
    nbn_udp_driver.impl.cli_stop(client);
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    nbn_webrtc_native_driver.impl.cli_stop(client);
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    nbn_webrtc_em_driver.impl.cli_stop(client);
#endif // __EMSCRIPTEN__

    client->is_connected = false;
    client->closed_code = -1;
    client->endpoint.server_initial_data_len = 0;

    Endpoint_Deinit(&client->endpoint);
    free(client);

    LogInfo("Stopped");
}

NBN_Reader *NBN_Client_ReadServerData(NBN_Client *client) {
    NBN_Endpoint *endpoint = &client->endpoint;

    NBN_Reader_Init(&client->server_data_reader, endpoint->server_initial_data_buffer,
                    endpoint->server_initial_data_len);

    return &client->server_data_reader;
}

static int ReadPacketsFromClientDrivers(NBN_Client *client) {
#ifdef NBN_UDP
    if (nbn_udp_driver.impl.cli_recv_packets(client) < 0) {
        LogError("Failed to read packets from driver %s", nbn_udp_driver.name);
        return NBN_ERROR;
    }
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    if (nbn_webrtc_native_driver.impl.cli_recv_packets(client) < 0) {
        LogError("Failed to read packets from driver %s", nbn_webrtc_native_driver.name);
        return NBN_ERROR;
    }
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    if (nbn_webrtc_em_driver.impl.cli_recv_packets(client) < 0) {
        LogError("Failed to read packets from driver %s", nbn_webrtc_em_driver.name);
        return NBN_ERROR;
    }
#endif // __EMSCRIPTEN__

    return 0;
}

NBN_Client_Event NBN_Client_Poll(NBN_Client *client) {
    NBN_Endpoint *endpoint = &client->endpoint;

    Endpoint_UpdateTime(endpoint);

    if (client->server_connection->is_stale)
        return NBN_CLIENT_NO_EVENT;

    if (EventQueue_IsEmpty(&endpoint->event_queue)) {
        if (Connection_CheckIfStale(client->server_connection, client->endpoint.time)) {
            client->server_connection->is_stale = true;
            client->is_connected = false;

            LogInfo("Server connection is stale. Disconnected.");

            NBN_Event e;

            e.type = NBN_CLIENT_DISCONNECTED;
            e.data.connection = (NBN_Connection *)NULL;

            if (!EventQueue_Enqueue(&endpoint->event_queue, e))
                return NBN_ERROR;
        } else {
            if (ReadPacketsFromClientDrivers(client) < 0) {
                return NBN_ERROR;
            }

            NBN_Connection *server_conn = client->server_connection;

            for (unsigned int i = 0; i < endpoint->channel_count; i++) {
                NBN_Channel *channel = &server_conn->channels[i];
                NBN_Message *msg;

                while ((msg = Channel_GetNextRecvedMessage(channel)) != NULL) {
                    LogDebug("Got message %d of type %d from channel %d", msg->header.id, msg->header.type,
                             channel->id);

                    if (Client_ProcessReceivedMessage(client, msg, server_conn) < 0) {
                        LogError("Failed to process received message");

                        return NBN_ERROR;
                    }
                }
            }

            Connection_UpdateAverageDownloadBandwidth(server_conn, client->endpoint.time);

            server_conn->last_read_packets_time = client->endpoint.time;
        }
    }

    bool ret = EventQueue_Dequeue(&endpoint->event_queue, &client->last_event);

    return ret ? Client_HandleEvent(client) : NBN_CLIENT_NO_EVENT;
}

int NBN_Client_Flush(NBN_Client *client) {
    return Connection_FlushChannels((NBN_Endpoint *)client, client->server_connection, client->endpoint.protocol_id,
                                    client->endpoint.time);
}

NBN_Writer *NBN_Client_CreateMessage(NBN_Client *client, uint8_t type, uint8_t channel_id) {
    return Endpoint_CreateOutgoingMessage(&client->endpoint, client->server_connection, type, channel_id);
}

NBN_Writer *NBN_Client_CreateReliableMessage(NBN_Client *client, uint8_t type) {
    return NBN_Client_CreateMessage(client, type, client->endpoint.default_reliable_channel);
}

NBN_Writer *NBN_Client_CreateUnreliableMessage(NBN_Client *client, uint8_t type) {
    return NBN_Client_CreateMessage(client, type, client->endpoint.default_unreliable_channel);
}

NBN_Reader *NBN_Client_ReadMessage(NBN_Client *client) {
    NBN_Assert(client->last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    NBN_MessageInfo msg_info = client->last_event.data.message_info;
    NBN_Reader *reader = &client->endpoint.message_reader;

    NBN_Reader_Init(reader, msg_info.data, msg_info.length);

    return reader;
}

static NBN_Connection *CreateServerConnection(NBN_Client *client, NBN_Driver_ID driver_id) {
    NBN_Connection *server_connection = Endpoint_CreateConnection(&client->endpoint, 0, driver_id);

    client->server_connection = server_connection;

    return server_connection;
}

NBN_MessageInfo NBN_Client_GetMessageInfo(NBN_Client *client) {
    NBN_Assert(client->last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    return client->last_event.data.message_info;
}

NBN_ConnectionStats NBN_Client_GetStats(NBN_Client *client) { return client->server_connection->stats; }

int NBN_Client_GetServerCloseCode(NBN_Client *client) { return client->closed_code; }

bool NBN_Client_IsConnected(NBN_Client *client) { return client->is_connected; }

static int Client_ProcessReceivedMessage(NBN_Client *client, NBN_Message *message, NBN_Connection *server_connection) {
    NBN_Assert(client->server_connection == server_connection);

    NBN_Event ev;

    ev.type = NBN_CLIENT_MESSAGE_RECEIVED;

    NBN_MessageInfo msg_info;

    msg_info.type = message->header.type;
    msg_info.channel_id = message->header.channel_id;
    msg_info.length = message->header.length;
    msg_info.sender = (NBN_ConnectionHandle *)server_connection;
    msg_info.data = message->data;

    ev.data.message_info = msg_info;

    if (!EventQueue_Enqueue(&client->endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static NBN_Client_Event Client_HandleEvent(NBN_Client *client) {
    switch (client->last_event.type) {
    case NBN_CLIENT_MESSAGE_RECEIVED:
        return Client_HandleMessageReceivedEvent(client);

    default:
        return client->last_event.type;
    }
}

static NBN_Client_Event Client_HandleMessageReceivedEvent(NBN_Client *client) {
    NBN_MessageInfo message_info = client->last_event.data.message_info;
    NBN_Endpoint *endpoint = &client->endpoint;

    int ret = NBN_CLIENT_NO_EVENT;

    if (message_info.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE) {
        client->is_connected = false;
        NBN_Reader *reader = NBN_Client_ReadMessage(client);

        if (NBN_Reader_ReadInt32(reader, &client->closed_code) < 0) {
            LogError("Failed to read code from client closed message");

            return NBN_ERROR;
        }

        ret = NBN_CLIENT_DISCONNECTED;
    } else if (message_info.type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE) {
        if (message_info.length < 4) {
            LogError("Accept message invalid length");

            return NBN_ERROR;
        }

        NBN_Reader *reader = NBN_Client_ReadMessage(client);
        unsigned int data_length;

        if (NBN_Reader_ReadUInt32(reader, &data_length) < 0) {
            LogError("Failed to read client data length");

            return NBN_ERROR;
        }

        if (data_length > 0) {
            if (data_length > sizeof(endpoint->server_initial_data_buffer)) {
                LogError("Received invalid connection data from the server");

                return NBN_ERROR;
            }

            if (NBN_Reader_ReadBytes(reader, endpoint->server_initial_data_buffer, data_length) < 0) {
                LogError("Failed to read server data");

                return NBN_ERROR;
            }
        }

        endpoint->server_initial_data_len = data_length;
        client->is_connected = true;
        ret = NBN_CLIENT_CONNECTED;
    } else {
        ret = NBN_CLIENT_MESSAGE_RECEIVED;
    }

    return ret;
}

static void ClientDriver_OnPacketReceived(NBN_Client *client, NBN_Packet *packet) {
    if (Endpoint_ProcessReceivedPacket(&client->endpoint, packet, client->server_connection) < 0) {
        // packets from the server should always be valid
        LogError("Received invalid packet from server");
        NBN_Abort();
    }
}

// END OF CLIENT
// ===================================================

/**
 * ====== SERVER ======
 */

static void Server_AddClient(NBN_Server *, NBN_Connection *);
static int Server_CloseClientWithCode(NBN_Server *, NBN_Connection *, int, bool);
static void Server_AddClientToClosedList(NBN_Server *, NBN_Connection *);
static int Server_ProcessReceivedMessage(NBN_Server *, NBN_Message *, NBN_Connection *);
static int Server_CloseStaleClientConnections(NBN_Server *);
static void Server_RemoveClosedClientConnections(NBN_Server *);
static bool Server_HandleEvent(NBN_Server *, NBN_Server_Event *);
static bool Server_HandleMessageReceivedEvent(NBN_Server *, NBN_Server_Event *);

NBN_Server *NBN_Server_Create(const char *protocol_name, uint16_t port) {
    NBN_Server *server = malloc(sizeof(NBN_Server));

    server->config = (NBN_Server_Config){.protocol_name = protocol_name, .port = port};
    server->server_data_writer.position = 0;
    server->clients = NULL;

    hmdefault(server->clients, NULL);

    server->endpoint.default_reliable_channel = NBN_Server_CreateChannel(
        server, NBN_CHANNEL_RELIABLE, NBN_CHANNEL_DEFAULT_BUFFER_SIZE, NBN_CHANNEL_DEFAULT_MAX_MESSAGE_SIZE);
    server->endpoint.default_unreliable_channel = NBN_Server_CreateChannel(
        server, NBN_CHANNEL_UNRELIABLE, NBN_CHANNEL_DEFAULT_BUFFER_SIZE, NBN_CHANNEL_DEFAULT_MAX_MESSAGE_SIZE);

#if defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)
    server->driver_data.webrtc.ws_server = -1;
    server->driver_data.webrtc.cfg = NBN_WEBRTC_DEFAULT_CONFIG;
#endif // defined(__EMSCRIPTEN__) || defined(NBN_WEBRTC_NATIVE)

    return server;
}

uint8_t NBN_Server_CreateChannel(NBN_Server *server, NBN_Channel_Mode mode, unsigned int buffer_size,
                                 unsigned int max_message_len) {
    NBN_Server_Config *cfg = &server->config;

    NBN_Assert(cfg->channel_count < NBN_MAX_CHANNEL_COUNT);

    uint8_t channel_id = cfg->channel_count;

    cfg->channels[channel_id] =
        (NBN_Channel_Config){.mode = mode, .buffer_size = buffer_size, .max_message_len = max_message_len};
    cfg->channel_count++;

    return channel_id;
}

unsigned int NBN_Server_GetChannelCurrentCapacity(NBN_Server *server, uint8_t channel_id, NBN_ConnectionHandle *conn) {
    NBN_Assert(channel_id < server->endpoint.channel_count);

    NBN_Channel *channel = &HANDLE_TO_CONN(conn)->channels[channel_id];

    return channel->current_capacity;
}

static int StartServerDrivers(NBN_Server *server, uint16_t port) {
    int driver_count = 0;

#ifdef NBN_UDP
    if (nbn_udp_driver.impl.serv_start(server, port) < 0) {
        LogError("Failed to start driver %s", nbn_udp_driver.name);
        return NBN_ERROR;
    }

    LogInfo("%s driver started", nbn_udp_driver.name);
    driver_count++;
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    if (nbn_webrtc_native_driver.impl.serv_start(server, port) < 0) {
        LogError("Failed to start driver %s", nbn_webrtc_native_driver.name);
        return NBN_ERROR;
    }

    LogInfo("%s driver started", nbn_webrtc_native_driver.name);
    driver_count++;
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    if (nbn_webrtc_em_driver.impl.serv_start(server, port) < 0) {
        LogError("Failed to start driver %s", nbn_webrtc_em_driver.name);
        return NBN_ERROR;
    }

    LogInfo("%s driver started", nbn_webrtc_em_driver.name);
    driver_count++;
#endif // __EMSCRIPTEN__

    return driver_count;
}

int NBN_Server_Start(NBN_Server *server) {
    NBN_Server_Config config = server->config;
    const char *protocol_name = config.protocol_name;
    uint16_t port = config.port;
    uint32_t protocol_id = Endpoint_BuildProtocolId(protocol_name);

    Endpoint_Init(&server->endpoint, protocol_id, true, config.channels, config.channel_count);

    server->closed_clients_head = NULL;

    int driver_count = StartServerDrivers(server, port);

    if (driver_count < 1) {
        LogError("At least one network driver has to be activated");
        NBN_Abort();
    }

    LogInfo("Started (channel count: %d)", server->endpoint.channel_count);

    return 0;
}

void NBN_Server_Stop(NBN_Server *server) {
    // Poll remaning events to clear the event queue
    while (NBN_Server_Poll(server) != NBN_SERVER_NO_EVENT) {
    }

    for (unsigned int i = 0; i < hmlen(server->clients); i++) {
        NBN_Connection *conn = server->clients[i].value;

        conn->driver->impl.serv_cleanup_connection(server, conn);
        Connection_Destroy(conn);
    }

    hmfree(server->clients);

#ifdef NBN_UDP
    nbn_udp_driver.impl.serv_stop(server);
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    nbn_webrtc_native_driver.impl.serv_stop(server);
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    nbn_webrtc_em_driver.impl.serv_stop(server);
#endif // __EMSCRIPTEN__

    // Free closed clients list
    NBN_ConnectionListNode *current = server->closed_clients_head;

    while (current) {
        NBN_ConnectionListNode *next = current->next;

        free(current);

        current = next;
    }

    server->closed_clients_head = NULL;
    Endpoint_Deinit(&server->endpoint);
    free(server);

    LogInfo("Stopped");
}

static NBN_Connection_ID NBN_BuildConnectionHash(NBN_Connection_ID id, NBN_Driver_ID driver_id) {
    NBN_Assert(id <= UINT64_MAX - 0xFF);
    uint8_t driver_byte = driver_id;

    return ((NBN_Connection_ID)driver_byte << 56) | id;
}

NBN_ConnectionHandle *NBN_Server_GetConnection(NBN_Server *server, NBN_Connection_ID id) {
    return (NBN_ConnectionHandle *)hmget(server->clients, id);
}

unsigned int NBN_Server_GetClientCount(NBN_Server *server) { return hmlen(server->clients); }

NBN_ConnectionHandle *NBN_Server_GetNextClient(NBN_Server *server, NBN_Client_Iterator *it) {
    for (; *it < hmlen(server->clients);) {
        NBN_Connection *conn = (NBN_Connection *)server->clients[*it].value;

        (*it)++;

        if (conn->is_accepted) {
            return (NBN_ConnectionHandle *)conn;
        }
    }

    return NULL;
}

static void ReadPacketsFromServerDrivers(NBN_Server *server) {
#ifdef NBN_UDP
    if (nbn_udp_driver.impl.serv_recv_packets(server) < 0) {
        LogError("Failed to read packets from driver %s", nbn_udp_driver.name);
    }
#endif // NBN_UDP

#ifdef NBN_WEBRTC_NATIVE
    if (nbn_webrtc_native_driver.impl.serv_recv_packets(server) < 0) {
        LogError("Failed to read packets from driver %s", nbn_webrtc_native_driver.name);
    }
#endif // NBN_WEBRTC_NATIVE

#ifdef __EMSCRIPTEN__
    if (nbn_webrtc_em_driver.impl.serv_recv_packets(server) < 0) {
        LogError("Failed to read packets from driver %s", nbn_webrtc_em_driver.name);
    }
#endif // __EMSCRIPTEN__
}

NBN_Server_Event NBN_Server_Poll(NBN_Server *server) {
    Endpoint_UpdateTime(&server->endpoint);

    NBN_Endpoint *endpoint = &server->endpoint;

    if (EventQueue_IsEmpty(&endpoint->event_queue)) {
        if (Server_CloseStaleClientConnections(server) < 0)
            return NBN_ERROR;

        ReadPacketsFromServerDrivers(server);

        server->stats.download_bandwidth = 0;

        for (unsigned int i = 0; i < hmlen(server->clients); i++) {
            NBN_Connection *client = server->clients[i].value;

            for (unsigned int i = 0; i < endpoint->channel_count; i++) {
                NBN_Channel *channel = &client->channels[i];

                if (channel) {
                    NBN_Message *msg;

                    while ((msg = Channel_GetNextRecvedMessage(channel)) != NULL) {
                        if (Server_ProcessReceivedMessage(server, msg, client) < 0) {
                            LogError("Failed to process received message");

                            return NBN_ERROR;
                        }
                    }
                }
            }

            if (!client->is_closed)
                Connection_UpdateAverageDownloadBandwidth(client, endpoint->time);

            server->stats.download_bandwidth += client->stats.download_bandwidth;
            client->last_read_packets_time = endpoint->time;
        }

        Server_RemoveClosedClientConnections(server);
    }

    NBN_Server_Event ev;

    while (EventQueue_Dequeue(&endpoint->event_queue, &server->last_event)) {
        if (Server_HandleEvent(server, &ev)) {
            return ev;
        }
    }

    return NBN_SERVER_NO_EVENT;
}

int NBN_Server_Flush(NBN_Server *server) {
    server->stats.upload_bandwidth = 0;

    Server_RemoveClosedClientConnections(server);

    for (unsigned int i = 0; i < hmlen(server->clients); i++) {
        NBN_Connection *client = server->clients[i].value;

        NBN_Assert(!(client->is_closed && client->is_stale));

        if (!client->is_stale && Connection_FlushChannels((NBN_Endpoint *)server, client, server->endpoint.protocol_id,
                                                          server->endpoint.time) < 0) {
            return NBN_ERROR;
        }

        server->stats.upload_bandwidth += client->stats.upload_bandwidth;
    }

    return 0;
}

static NBN_Connection *CreateClientConnection(NBN_Server *server, NBN_Driver_ID driver_id, NBN_Connection_ID conn_id) {
    // write the driver ID to the first byte of the connection ID to avoid collisions between drivers
    conn_id = NBN_BuildConnectionHash(conn_id, driver_id);
    NBN_Connection *client = Endpoint_CreateConnection(&server->endpoint, conn_id, driver_id);

    return client;
}

int NBN_Server_CloseClientWithCode(NBN_Server *server, NBN_ConnectionHandle *conn, int code) {
    return Server_CloseClientWithCode(server, HANDLE_TO_CONN(conn), code, false);
}

int NBN_Server_CloseClient(NBN_Server *server, NBN_ConnectionHandle *conn) {
    return Server_CloseClientWithCode(server, HANDLE_TO_CONN(conn), -1, false);
}

NBN_Writer *NBN_Server_CreateMessage(NBN_Server *server, uint8_t type, uint8_t channel_id,
                                     NBN_ConnectionHandle *receiver) {
    NBN_Connection *conn = HANDLE_TO_CONN(receiver);

    NBN_Assert(conn->is_accepted || type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE || type == NBN_CLIENT_CLOSED_MESSAGE_TYPE);

    NBN_Writer *writer = Endpoint_CreateOutgoingMessage(&server->endpoint, conn, type, channel_id);

    if (!writer) {
        LogError("Failed to create outgoing message for client %lld", receiver->id);

        /* Do not close the client if we failed to send the close client message to avoid infinite loops */
        if (type != NBN_CLIENT_CLOSED_MESSAGE_TYPE) {
            Server_CloseClientWithCode(server, conn, -1, false);

            return NULL;
        }
    }

    return writer;
}

NBN_Writer *NBN_Server_CreateReliableMessage(NBN_Server *server, uint8_t type, NBN_ConnectionHandle *receiver) {
    return NBN_Server_CreateMessage(server, type, server->endpoint.default_reliable_channel, receiver);
}

NBN_Writer *NBN_Server_CreateUnreliableMessage(NBN_Server *server, uint8_t type, NBN_ConnectionHandle *receiver) {
    return NBN_Server_CreateMessage(server, type, server->endpoint.default_unreliable_channel, receiver);
}

NBN_Reader *NBN_Server_ReadMessage(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    NBN_MessageInfo msg_info = server->last_event.data.message_info;
    NBN_Reader *reader = &server->endpoint.message_reader;

    NBN_Reader_Init(reader, msg_info.data, msg_info.length);

    return reader;
}

NBN_Writer *NBN_Server_WriteConnectionData(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(server->last_event.data.connection != NULL);

    NBN_Endpoint *endpoint = &server->endpoint;

    NBN_Writer_Init(&server->server_data_writer, endpoint->server_initial_data_buffer,
                    sizeof(endpoint->server_initial_data_buffer));

    return &server->server_data_writer;
}

int NBN_Server_AcceptIncomingConnection(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(server->last_event.data.connection != NULL);

    unsigned data_length = server->server_data_writer.position;
    NBN_Connection *client = server->last_event.data.connection;
    NBN_Writer *writer =
        NBN_Server_CreateReliableMessage(server, NBN_CLIENT_ACCEPTED_MESSAGE_TYPE, (NBN_ConnectionHandle *)client);

    if (!writer) {
        return NBN_ERROR;
    }

    if (data_length > 0) {
        NBN_Assert(data_length <= sizeof(server->endpoint.server_initial_data_buffer));

        NBN_Writer_WriteUInt32(writer, data_length);
        NBN_Writer_WriteBytes(writer, server->endpoint.server_initial_data_buffer, data_length);
    } else {
        NBN_Writer_WriteUInt32(writer, 0);
    }

    client->is_accepted = true;

    LogInfo("Client %lld has been accepted into the server", client->handle.id);

    return 0;
}

int NBN_Server_RejectIncomingConnectionWithCode(NBN_Server *server, int code) {
    NBN_Assert(server->last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(server->last_event.data.connection != NULL);

    NBN_Connection *conn = server->last_event.data.connection;
    LogDebug("Rejecting incoming connection %lld (code: %d)", conn->handle.id, code);

    return Server_CloseClientWithCode(server, conn, code, false);
}

int NBN_Server_RejectIncomingConnection(NBN_Server *server) {
    return NBN_Server_RejectIncomingConnectionWithCode(server, -1);
}

NBN_ConnectionHandle *NBN_Server_GetIncomingConnection(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(server->last_event.data.connection != NULL);

    return (NBN_ConnectionHandle *)server->last_event.data.connection;
}

NBN_Reader *NBN_Server_ReadConnectionRequestData(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_SERVER_NEW_CONNECTION);

    NBN_Endpoint *endpoint = &server->endpoint;

    NBN_Reader_Init(&server->client_data_reader, endpoint->connection_request_data_buffer,
                    endpoint->client_connection_request_data_len);

    return &server->client_data_reader;
}

NBN_DisconnectionInfo NBN_Server_GetDisconnectionInfo(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_SERVER_DISCONNECTION);

    return server->last_event.data.disconnection;
}

NBN_MessageInfo NBN_Server_GetMessageInfo(NBN_Server *server) {
    NBN_Assert(server->last_event.type == NBN_SERVER_MESSAGE_RECEIVED);

    return server->last_event.data.message_info;
}

NBN_ServerStats NBN_Server_GetStats(NBN_Server *server) { return server->stats; }

static void Server_AddClient(NBN_Server *server, NBN_Connection *client) {
    NBN_Assert(hmgeti(server->clients, client->handle.id) == -1);

    hmput(server->clients, client->handle.id, client);
    LogDebug("New client %lld", client->handle.id);
}

static int Server_CloseClientWithCode(NBN_Server *server, NBN_Connection *client, int code, bool disconnection) {
    if (!client->is_closed && client->is_accepted) {
        if (!disconnection) {
            NBN_Event e;

            e.type = NBN_CLIENT_DISCONNECTED;
            e.data.disconnection = (NBN_DisconnectionInfo){client->handle.id, client->handle.user_data};

            if (!EventQueue_Enqueue(&server->endpoint.event_queue, e))
                return NBN_ERROR;
        }
    }

    if (client->is_stale) {
        LogDebug("Closing stale connection %lld", client->handle.id);

        Server_AddClientToClosedList(server, client);
        client->is_closed = true;

        return 0;
    }

    LogDebug("Closing active connection %lld (will send a disconnection message)", client->handle.id);

    Server_AddClientToClosedList(server, client);
    client->is_closed = true;

    if (!disconnection) {
        LogDebug("Send close message for client %lld (code: %d)", client->handle.id, code);

        NBN_Writer *writer =
            NBN_Server_CreateReliableMessage(server, NBN_CLIENT_CLOSED_MESSAGE_TYPE, (NBN_ConnectionHandle *)client);

        if (!writer) {
            return NBN_ERROR;
        }

        NBN_Writer_WriteInt32(writer, code);
    }

    return 0;
}

static void Server_AddClientToClosedList(NBN_Server *server, NBN_Connection *client) {
    if (client->is_closed)
        return;

    // TODO: do we need to use a linked list, maybe use stb dynamic array?
    NBN_ConnectionListNode *node = (NBN_ConnectionListNode *)malloc(sizeof(NBN_ConnectionListNode));

    node->conn = client;
    node->next = NULL;

    if (server->closed_clients_head == NULL) {
        // list is empty
        server->closed_clients_head = node;
        node->prev = NULL;
    } else {
        // list is not empty, add node at the end
        NBN_ConnectionListNode *tail = server->closed_clients_head;

        while (tail->next != NULL)
            tail = tail->next;

        node->prev = tail;
        tail->next = node;
    }
}

static int Server_ProcessReceivedMessage(NBN_Server *server, NBN_Message *message, NBN_Connection *client) {
    NBN_Event ev;

    ev.type = NBN_CLIENT_MESSAGE_RECEIVED;

    NBN_MessageInfo msg_info;

    msg_info.type = message->header.type;
    msg_info.channel_id = message->header.channel_id;
    msg_info.length = message->header.length;
    msg_info.sender = (NBN_ConnectionHandle *)client;
    msg_info.data = message->data;

    LogDebug("Received message (type: %d, id: %d) from client %lld", message->header.type, message->header.id,
             client->handle.id);
    ev.data.message_info = msg_info;

    if (!EventQueue_Enqueue(&server->endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static int Server_CloseStaleClientConnections(NBN_Server *server) {
    for (unsigned int i = 0; i < hmlen(server->clients); i++) {
        NBN_Connection *client = server->clients[i].value;

        if (!client->is_stale && Connection_CheckIfStale(client, server->endpoint.time)) {
            LogInfo("Client %lld connection is stale, closing it.", client->handle.id);

            client->is_stale = true;

            if (Server_CloseClientWithCode(server, client, -1, false) < 0)
                return NBN_ERROR;
        }
    }

    return 0;
}

static void Server_RemoveClosedClientConnections(NBN_Server *server) {
    NBN_ConnectionListNode *current = server->closed_clients_head;

    while (current) {
        NBN_ConnectionListNode *prev = current->prev;
        NBN_ConnectionListNode *next = current->next;
        NBN_Connection *client = current->conn;

        NBN_Assert(client->handle.id > 0);

        if (client->is_stale) {
            LogDebug("Remove closed client connection (ID: %lld)", client->handle.id);

            // Notify the driver to clean up the connection
            client->driver->impl.serv_cleanup_connection(server, client);

            int ret = hmdel(server->clients, client->handle.id);
            NBN_Assert(ret == 1);

            // Destroy the connection

            Connection_Destroy(client);

            // Remove the connection from the closed clients list

            free(current);

            if (current == server->closed_clients_head) {
                // delete the head of the list
                NBN_ConnectionListNode *new_head = next;

                if (new_head) {
                    new_head->prev = NULL;
                }

                server->closed_clients_head = new_head;
            } else {
                // delete a node in the middle of the list
                prev->next = next;

                if (next)
                    next->prev = prev;
            }
        }

        current = next;
    }
}

static bool Server_HandleEvent(NBN_Server *server, NBN_Server_Event *ev) {
    if (server->last_event.type == NBN_SERVER_MESSAGE_RECEIVED) {
        return Server_HandleMessageReceivedEvent(server, ev);
    }

    *ev = server->last_event.type;
    return true;
}

// TODO: big ass function
static bool Server_HandleMessageReceivedEvent(NBN_Server *server, NBN_Server_Event *ev) {
    NBN_Event *last_event = &server->last_event;
    NBN_MessageInfo message_info = last_event->data.message_info;
    NBN_Connection *sender = HANDLE_TO_CONN(message_info.sender);
    NBN_Endpoint *endpoint = &server->endpoint;

    if (sender->is_closed || sender->is_stale) {
        return false;
    }

    if (message_info.type == NBN_DISCONNECTION_MESSAGE_TYPE) {
        LogInfo("Received a disconnection request from client %lld (user_data: %p)", sender->handle.id,
                sender->handle.user_data);

        if (Server_CloseClientWithCode(server, sender, -1, true) < 0) {
            *ev = NBN_ERROR;
            return true;
        }

        sender->is_stale = true;

        last_event->type = NBN_SERVER_DISCONNECTION;
        last_event->data.disconnection = (NBN_DisconnectionInfo){sender->handle.id, sender->handle.user_data};

        Server_RemoveClosedClientConnections(server);

        *ev = NBN_SERVER_DISCONNECTION;
        return true;
    }

    if (message_info.type != NBN_CONNECTION_REQUEST_MESSAGE_TYPE) {
        server->server_data_writer.position = 0;

        *ev = NBN_SERVER_MESSAGE_RECEIVED;
        return true;
    }

    // at this point we know it's a connection request
    NBN_Assert(message_info.type == NBN_CONNECTION_REQUEST_MESSAGE_TYPE);

    LogDebug("Received a connection request from client %lld", sender->handle.id);

    if (message_info.length < 4) {
        LogError("Connection request invalid length");

        *ev = NBN_ERROR;
        return true;
    }

    NBN_Reader *reader = NBN_Server_ReadMessage(server);
    unsigned int data_length;

    if (NBN_Reader_ReadUInt32(reader, &data_length) < 0) {
        LogError("Failed to read client data length");

        *ev = NBN_ERROR;
        return true;
    }

    if (data_length > 0) {
        if (data_length > sizeof(endpoint->connection_request_data_buffer)) {
            LogError("Received invalid connection request data");

            *ev = NBN_ERROR;
            return true;
        }

        if (NBN_Reader_ReadBytes(reader, server->endpoint.connection_request_data_buffer, data_length) < 0) {
            LogError("Failed to read client data");

            *ev = NBN_ERROR;
            return true;
        }
    }

    server->endpoint.client_connection_request_data_len = data_length;

    NBN_Event e;

    e.type = NBN_SERVER_NEW_CONNECTION;
    e.data.connection = sender;

    if (!EventQueue_Enqueue(&endpoint->event_queue, e)) {
        *ev = NBN_ERROR;
        return true;
    }

    *ev = NBN_SERVER_NO_EVENT;
    return true;
}

static void ServerDriver_OnClientConnected(NBN_Server *server, NBN_Connection *client) {
    Server_AddClient(server, client);
}

static int ServerDriver_OnClientPacketReceived(NBN_Server *server, NBN_Packet *packet) {
    if (Endpoint_ProcessReceivedPacket(&server->endpoint, packet, packet->sender) < 0) {
        LogError("An error occured while processing packet from client %d, closing the client",
                 packet->sender->handle.id);

        return Server_CloseClientWithCode(server, packet->sender, -1, false);
    }

    return 0;
}

// END OF SERVER
// ===================================================

/**
 * ====== UDP DRIVER ====== *
 */

#ifdef NBN_UDP

#ifdef NBN_PLATFORM_WINDOWS

static char err_msg[32];

#endif

static SOCKET UDP_InitSocket(void);
static void UDP_DeinitSocket(int);
static int UDP_BindSocket(int, uint16_t);
static char *UDP_GetLastErrorMessage(void);

static SOCKET UDP_InitSocket(void) {
#ifdef NBN_PLATFORM_WINDOWS
    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err < 0) {
        LogError("WSAStartup() failed");

        return NBN_ERROR;
    }
#endif

    SOCKET sock;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
        return NBN_ERROR;

#if defined(NBN_PLATFORM_WINDOWS)
    DWORD non_blocking = 1;

    if (ioctlsocket(nbn_udp_sock, FIONBIO, &non_blocking) != 0) {
        LogError("ioctlsocket() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }
#elif defined(NBN_PLATFORM_MAC) || defined(NBN_PLATFORM_UNIX)
    int non_blocking = 1;

    if (fcntl(sock, F_SETFL, O_NONBLOCK, non_blocking) < 0) {
        LogError("fcntl() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }
#endif

    return sock;
}

static void UDP_DeinitSocket(SOCKET sock) {
    closesocket(sock);

#ifdef NBN_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static int UDP_BindSocket(SOCKET sock, uint16_t port) {
    SOCKADDR_IN sin;

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(sock, (SOCKADDR *)&sin, sizeof(sin)) < 0) {
        LogError("bind() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

static NBN_Connection_ID UDP_BuildConnectionID(NBN_IPAddress address) {
    return ((NBN_Connection_ID)address.host << 2) | address.port;
}

static NBN_Connection *UDP_FindOrCreateClientConnectionByAddress(NBN_Server *server, NBN_IPAddress address) {
    NBN_Connection_ID conn_id = UDP_BuildConnectionID(address);
    conn_id = NBN_BuildConnectionHash(conn_id, NBN_DRIVER_UDP);
    NBN_ConnectionHandle *handle = NBN_Server_GetConnection(server, conn_id);

    if (handle) {
        return HANDLE_TO_CONN(handle);
    }

    // this is a new connection
    NBN_Connection *conn = CreateClientConnection(server, NBN_DRIVER_UDP, conn_id);
    conn->driver_data.udp.ip_address = address;
    conn->driver_data.endpoint_ptr = server;

    LogInfo("New UDP connection (id: %lld, addr: %d, port: %d)", conn->handle.id, address.host, address.port);

    ServerDriver_OnClientConnected(server, conn);
    return conn;
}

#define MAX_IP_ADDR_LEN 15

static void UDP_ParseIpAddress(const char *host, uint16_t port, NBN_IPAddress *address) {
    uint8_t arr[4];
    char *dup_host = strndup(host, MAX_IP_ADDR_LEN + 1);

    char *s;
    int i = 0;

    while ((s = strsep(&dup_host, ".")) != NULL && i < 4) {
        char *end = NULL;
        int v = strtol(s, &end, 10);

        if (*end != '\0' || v < 0 || v > 255) {
            LogError("Invalid IP address: %s", host);
            NBN_Abort();
        }

        arr[i++] = (uint8_t)v;
    }

    free(dup_host);

    address->host = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
    address->port = port;
}

static char *UDP_GetLastErrorMessage(void) {
#ifdef NBN_PLATFORM_WINDOWS
    snprintf(err_msg, sizeof(err_msg), "%d", WSAGetLastError());

    return err_msg;
#else
    return strerror(errno);
#endif
}

static int UDP_Server_Start(NBN_Server *server, uint16_t port) {
    if ((server->driver_data.udp.sock = UDP_InitSocket()) < 0)
        return NBN_ERROR;

    if (UDP_BindSocket(server->driver_data.udp.sock, port) < 0)
        return NBN_ERROR;

    return 0;
}

static void UDP_Server_Stop(NBN_Server *server) { UDP_DeinitSocket(server->driver_data.udp.sock); }

static int UDP_Server_RecvPackets(NBN_Server *server) {
    NBN_Packet *packet = &server->endpoint.read_packet;
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    while (true) {
        int bytes = recvfrom(server->driver_data.udp.sock, (char *)packet->buffer, sizeof(packet->buffer), 0,
                             (SOCKADDR *)&src_addr, &src_addr_len);

        if (bytes <= 0)
            break;

        if (bytes < NBN_PACKET_HEADER_SIZE)
            continue;

        if (Packet_InitRead(packet, server->endpoint.protocol_id, bytes) < 0) {
            LogDebug("Discarded invalid packet");
            continue;
        }

        NBN_IPAddress ip_address;
        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

        LogDebug("Received valid UDP packet from %d:%d", ip_address.host, ip_address.port);

        packet->sender = UDP_FindOrCreateClientConnectionByAddress(server, ip_address);

        ServerDriver_OnClientPacketReceived(server, packet);
    }

    return 0;
}

static void UDP_Server_CleanupConnection(NBN_Server *server, NBN_Connection *connection) {
    (void)server;
    (void)connection;
}

static int UDP_Server_SendPacketTo(NBN_Server *server, NBN_Packet *packet, NBN_Connection *connection) {
    NBN_IPAddress dest_address = connection->driver_data.udp.ip_address;
    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(dest_address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_address.port);

    if (sendto(server->driver_data.udp.sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr,
               sizeof(dest_addr)) == SOCKET_ERROR) {
        LogError("sendto() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

static int UDP_Client_Start(NBN_Client *client, const char *host, uint16_t port) {
    NBN_IPAddress *ip_address = &client->server_connection->driver_data.udp.ip_address;

    UDP_ParseIpAddress(host, port, ip_address);

    if ((client->driver_data.udp.sock = UDP_InitSocket()) < 0)
        return NBN_ERROR;

    if (UDP_BindSocket(client->driver_data.udp.sock, 0) < 0)
        return NBN_ERROR;

    return 0;
}

static void UDP_Client_Stop(NBN_Client *client) { UDP_DeinitSocket(client->driver_data.udp.sock); }

static int UDP_Client_RecvPackets(NBN_Client *client) {
    NBN_IPAddress server_address = client->server_connection->driver_data.udp.ip_address;
    NBN_Packet *packet = &client->endpoint.read_packet;
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    while (true) {
        int bytes = recvfrom(client->driver_data.udp.sock, (char *)packet->buffer, sizeof(packet->buffer), 0,
                             (SOCKADDR *)&src_addr, &src_addr_len);

        if (bytes <= 0)
            break;

        if (bytes < NBN_PACKET_HEADER_SIZE)
            continue;

        uint32_t host = ntohl(src_addr.sin_addr.s_addr);
        uint16_t port = ntohs(src_addr.sin_port);

        if (host != server_address.host || port != server_address.port)
            continue;

        if (Packet_InitRead(packet, client->endpoint.protocol_id, bytes) < 0) {
            LogDebug("Discarded invalid packet");
            continue;
        }

        packet->sender = client->server_connection;

        ClientDriver_OnPacketReceived(client, packet);
    }

    return 0;
}

static int UDP_Client_SendPacket(NBN_Client *client, NBN_Packet *packet, NBN_Connection *connection) {
    NBN_IPAddress server_address = connection->driver_data.udp.ip_address;
    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(server_address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_address.port);

    if (sendto(client->driver_data.udp.sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr,
               sizeof(dest_addr)) == SOCKET_ERROR) {
        LogError("sendto() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

#endif // NBN_UDP

// END OF UDP DRIVER
// ===================================================

/**
 * ====== WEBRTC EMSCRIPTEN DRIVER ====== *
 */

#ifdef __EMSCRIPTEN__

/**
 * JS API
 *
 * See webrtc/js folder for the implementation of these functions.
 */

extern void __js_game_server_init(uint32_t, bool, const char *, const char *);
extern int __js_game_server_start(uint16_t);
extern int __js_game_server_dequeue_packet(uint32_t *, uint8_t *);
extern int __js_game_server_send_packet_to(uint8_t *, unsigned int, uint32_t);
extern void __js_game_server_close_client_peer(unsigned int);
extern void __js_game_server_stop(void);

extern void __js_game_client_init(uint32_t, bool);
extern int __js_game_client_start(const char *, uint16_t);
extern int __js_game_client_dequeue_packet(uint8_t *);
extern int __js_game_client_send_packet(uint8_t *, unsigned int);
extern void __js_game_client_close(void);

void NBN_Client_SetWebRTC_Config(NBN_Client *client, NBN_WebRTC_Config config) {
    client->driver_data.webrtc.cfg = config;
}

void NBN_Server_SetWebRTC_Config(NBN_Server *server, NBN_WebRTC_Config config) {
    server->driver_data.webrtc.cfg = config;
}

static int WebRTC_Server_Start(NBN_Server *server, uint16_t port) {
    NBN_WebRTC_Config cfg = server->driver_data.webrtc.cfg;

    __js_game_server_init(server->endpoint.protocol_id, cfg.enable_tls, cfg.key_path, cfg.cert_path);

    if (__js_game_server_start(port) < 0)
        return -1;

    return 0;
}

static void WebRTC_Server_Stop(NBN_Server *server) {
    (void)server;

    __js_game_server_stop();
}

static int WebRTC_Server_RecvPackets(NBN_Server *server) {
    NBN_Packet *packet = &server->endpoint.read_packet;
    uint32_t peer_id;
    unsigned int len;

    while ((len = __js_game_server_dequeue_packet(&peer_id, packet->buffer)) > 0) {
        NBN_Connection_ID conn_id = NBN_BuildConnectionHash(peer_id, NBN_DRIVER_WEBRTC_EMSCRIPTEN);
        NBN_ConnectionHandle *handle = NBN_Server_GetConnection(server, conn_id);
        NBN_Connection *conn = NULL;

        if (handle == NULL) {
            LogInfo("Peer %d has connected", peer_id);

            conn = CreateClientConnection(server, NBN_DRIVER_WEBRTC_EMSCRIPTEN, conn_id);
            conn->driver_data.webrtc.peer_id = peer_id;
            conn->driver_data.endpoint_ptr = server;

            ServerDriver_OnClientConnected(server, conn);
        } else {
            conn = HANDLE_TO_CONN(handle);
        }

        if (Packet_InitRead(packet, server->endpoint.protocol_id, len) < 0)
            continue;

        packet->sender = conn;

        ServerDriver_OnClientPacketReceived(server, packet);
    }

    return 0;
}

static void WebRTC_Server_CleanupConnection(NBN_Server *server, NBN_Connection *conn) {
    (void)server;

    NBN_Assert(conn != NULL);
    __js_game_server_close_client_peer(conn->driver_data.webrtc.peer_id);
}

static int WebRTC_Server_SendPacketTo(NBN_Server *server, NBN_Packet *packet, NBN_Connection *conn) {
    (void)server;

    return __js_game_server_send_packet_to(packet->buffer, packet->size, conn->driver_data.webrtc.peer_id);
}

static int WebRTC_Client_Start(NBN_Client *client, const char *host, uint16_t port) {
    NBN_WebRTC_Config cfg = client->driver_data.webrtc.cfg;

    __js_game_client_init(client->endpoint.protocol_id, cfg.enable_tls);

    int res;

    if ((res = __js_game_client_start(host, port)) < 0)
        return NBN_ERROR;

    return 0;
}

static void WebRTC_Client_Stop(NBN_Client *client) {
    (void)client;

    __js_game_client_close();
}

static int WebRTC_Client_RecvPackets(NBN_Client *client) {
    NBN_Packet *packet = &client->endpoint.read_packet;
    unsigned int len;

    while ((len = __js_game_client_dequeue_packet(packet->buffer)) > 0) {
        if (Packet_InitRead(packet, client->endpoint.protocol_id, len) < 0)
            continue;

        packet->sender = client->server_connection;

        ClientDriver_OnPacketReceived(client, packet);
    }

    return 0;
}

static int WebRTC_Client_SendPacket(NBN_Client *client, NBN_Packet *packet, NBN_Connection *connection) {
    (void)client;
    (void)connection;

    return __js_game_client_send_packet(packet->buffer, packet->size);
}

#endif // __EMSCRIPTEN__

// END OF WEBRTC EMSCRIPTEN DRIVER
// ===================================================

/**
 * ====== WEBRTC NATIVE DRIVER ====== *
 *
 * WARNING: libdatachannel callbacks can be triggered from different threads.
 * Beware of race conditions in those callbacks.
 * The callbacks used on the server start with WS_Server_
 * The callbacks used on the client start with WS_Client_
 */

#ifdef NBN_WEBRTC_NATIVE

#include "json.h"

void NBN_Server_SetWebRTC_Config(NBN_Server *server, NBN_WebRTC_Config config) {
    server->driver_data.webrtc.cfg = config;
}

void NBN_Client_SetWebRTC_Config(NBN_Client *client, NBN_WebRTC_Config config) {
    client->driver_data.webrtc.cfg = config;
}

static void WS_OnError(int ws, const char *err_msg, void *user_ptr) {
    (void)user_ptr;

    LogError("Error on WS %d: %s", ws, err_msg);
}

static void WebRTC_Native_Log(rtcLogLevel level, const char *msg) {
    switch (level) {
    case RTC_LOG_FATAL:
    case RTC_LOG_ERROR:
        LogError("%s", msg);
        break;

    case RTC_LOG_WARNING:
        LogWarning("%s", msg);
        break;

    case RTC_LOG_INFO:
        LogInfo("%s", msg);
        break;

    case RTC_LOG_DEBUG:
        LogDebug("%s", msg);
        break;

    case RTC_LOG_VERBOSE:
        LogDebug("%s", msg);
        break;

    case RTC_LOG_NONE:
        break;
    }
}

static char *ParseSignalingMessage(const char *msg, size_t msg_len, const char *type) {
    char *sdp = NULL;
    struct json_value_s *root = json_parse(msg, msg_len); // this has to be freed
    struct json_object_s *object = (struct json_object_s *)root->payload;
    struct json_object_element_s *curr = object->start;

    if (root->type != json_type_object) {
        LogDebug("Received an invalid signaling message: %s", msg);
        goto leave_free_root;
    }

    while (curr != NULL) {
        if (strncmp(curr->name->string, "type", 4) == 0) {
            struct json_string_s *str = json_value_as_string(curr->value);

            if (strncmp(str->string, type, str->string_size)) {
                // unexpected type
                LogDebug("Received a signaling message with an unexpected type: %s (expected: %s)", str->string, type);
                sdp = NULL;
                goto leave_free_root;
            }
        } else if (strncmp(curr->name->string, "sdp", 3) == 0) {
            struct json_string_s *str = json_value_as_string(curr->value);

            if (str) {
                size_t len = strnlen(str->string, str->string_size);

                sdp = (char *)malloc(len + 1);
                memcpy(sdp, str->string, len + 1);
            }
        }

        curr = curr->next;
    }

leave_free_root:
    free(root);

    return sdp;
}

static void ProcessSignalingMessage(NBN_WebRTC_Peer_ID peer_id, int ws, const char *msg, int size, const char *type) {
    // for some reason the size of the message is negative
    // in libdatachannel documentation (https://github.com/paullouisageneau/libdatachannel/blob/master/DOC.md) there
    // is mention of: size: if size >= 0, data is interpreted as a binary message of length size, otherwise it is
    // interpreted as a null-terminated UTF-8 string. so I guess in this case msg is a null terminated string? I
    // could not find more information about this so I decided to go with flipping the size to positive even though
    // it feels weird, but it works so... ¯\_(ツ)_/¯

    if (size < 0)
        size *= -1;
    size -= 1;

    LogDebug("Received signaling message on WS %d (size: %d): %s", ws, size, msg);

    char *sdp = ParseSignalingMessage(msg, size, type);

    if (!sdp) {
        LogWarning("Failed to parse signaling data for WS %d", ws);
        return;
    }

    LogDebug("Successfully parsed signaling payload (sdp: %s)", sdp);

    int ret = rtcSetRemoteDescription(peer_id, sdp, type);

    if (ret < 0) {
        LogError("Failed to set remote description for peer %d (WS: %d): %d", peer_id, ws, ret);
        rtcClose(ws);
    }

    // IMPORTANT: not sure I can free this because it's passed to rtcSetRemoteDescription
    free(sdp);
}

char *String_ReplaceAll(const char *str, const char *orig, const char *stub) {
    const char *s = str;
    size_t str_len = strlen(str);
    size_t orig_len = strlen(orig);
    size_t stub_len = strlen(stub);
    size_t occurences = 0;

    while ((s = strstr(s, orig)) != NULL) {
        s += orig_len;
        occurences++;
    }

    size_t res_len = str_len - (occurences * orig_len) + (occurences * stub_len);
    char *res = malloc(res_len + 1);
    size_t res_offset = 0;

    while ((s = strstr(str, orig)) != NULL) {
        size_t len = s - str;

        memcpy(res + res_offset, str, len);
        res_offset += len;
        memcpy(res + res_offset, stub, stub_len);
        res_offset += stub_len;
        str = s + orig_len;
    }

    res[res_len] = 0;

    return res;
}

static int ProcessLocalDescription(int ws, const char *sdp, const char *type) {
    char *escaped_sdp = String_ReplaceAll(sdp, "\r\n", "\\r\\n");
    size_t signaling_json_size = snprintf(NULL, 0, "{\"type\":\"%s\", \"sdp\":\"%s\"}", type, escaped_sdp) + 1;
    char *signaling_json = (char *)malloc(signaling_json_size);
    snprintf(signaling_json, signaling_json_size, "{\"type\":\"%s\", \"sdp\":\"%s\"}", type, escaped_sdp);

    LogDebug("Send signaling message of type %s to remote connection: %s", type, signaling_json);

    int ret = 0;

    // pass -1 as the size (assume signaling_json to be a null-terminated string)
    if (rtcSendMessage(ws, signaling_json, -1) < 0) {
        ret = NBN_ERROR;
    }

    free(signaling_json);
    free(escaped_sdp);

    return ret;
}

static void ClosePeer(NBN_Connection *conn) {
    int channel_id = conn->driver_data.webrtc.channel_id;
    NBN_WebRTC_Peer_ID peer_id = conn->driver_data.webrtc.peer_id;
    int ws = conn->driver_data.webrtc.ws;

    LogDebug("Closing peer %d (ws: %d)", peer_id, ws);

    if (channel_id >= 0) {
        rtcDeleteDataChannel(channel_id);
    }

    rtcDeletePeerConnection(peer_id);
    rtcDelete(ws);
}

static void WS_Server_OnLocalDescription(int pc, const char *sdp, const char *type, void *user_ptr) {
    (void)pc;

    LogDebug("Processing local description of type '%s'", type);

    if (strncmp(type, "answer", strlen("answer")) != 0) {
        LogWarning("Ignoring local description of type '%s' (expected 'answer')", type);
        return;
    }

    NBN_Connection *conn = (NBN_Connection *)user_ptr;
    int ws = conn->driver_data.webrtc.ws;
    NBN_WebRTC_Peer_ID peer_id = conn->driver_data.webrtc.peer_id;

    if (ProcessLocalDescription(ws, sdp, "answer") < 0) {
        LogError("Failed to process local description for peer %d, closing peer", peer_id);
        ClosePeer(conn);
    }
}

static void WS_Server_OnPeerStateChanged(int pc, rtcState state, void *user_ptr) {
    LogDebug("Peer %d state changed to %d", pc, state);

    if (state == RTC_CONNECTED) {
        NBN_Connection *conn = (NBN_Connection *)user_ptr;
        NBN_Server *server = (NBN_Server *)conn->driver_data.endpoint_ptr;

        LogDebug("Peer %d is connected !", pc);
        ServerDriver_OnClientConnected(server, conn);
    }
}

static int CreatePeer(NBN_WebRTC_Peer_ID *peer_id, int *channel_id, NBN_WebRTC_Config cfg,
                      rtcDescriptionCallbackFunc on_rtc_description_cb, rtcStateChangeCallbackFunc state_changed_cb) {
    rtcConfiguration rtcCfg = {
        .iceServers = cfg.ice_servers, .iceServersCount = (int)cfg.ice_servers_count, .disableAutoNegotiation = false};
    *peer_id = rtcCreatePeerConnection(&rtcCfg);

    if (*peer_id < 0) {
        LogError("Failed to create peer: %d", *peer_id);
        return NBN_ERROR;
    }

    int ret = rtcSetLocalDescriptionCallback(*peer_id, on_rtc_description_cb);

    if (ret < 0) {
        LogError("Failed to register local description callback for peer %d: %d", *peer_id, ret);
        return NBN_ERROR;
    }

    ret = rtcSetStateChangeCallback(*peer_id, state_changed_cb);

    if (ret < 0) {
        LogError("Failed to register state change callback for peer %d: %d", *peer_id, ret);
        return NBN_ERROR;
    }
    rtcDataChannelInit rtcDataChannel = {
        .reliability = {.unordered = true, .unreliable = true, .maxPacketLifeTime = 1000, .maxRetransmits = 0},
        .negotiated = true,
        .manualStream = true,
        .stream = 0};
    *channel_id = rtcCreateDataChannelEx(*peer_id, "unreliable", &rtcDataChannel);

    if (*channel_id < 0) {
        LogError("Failed to create data channel for peer %d: %d", *peer_id, *channel_id);
        return NBN_ERROR;
    }

    LogDebug("Successfully created data channel for peer %d: %d", *peer_id, *channel_id);

    return 0;
}

static void WS_Server_OnOpen(int ws, void *user_ptr) {
    LogDebug("WS %d is open", ws);

    NBN_Server *server = (NBN_Server *)user_ptr;
    NBN_WebRTC_Peer_ID peer_id;
    int channel_id;

    if (CreatePeer(&peer_id, &channel_id, server->driver_data.webrtc.cfg, WS_Server_OnLocalDescription,
                   WS_Server_OnPeerStateChanged) < 0) {
        LogError("Failed to create peer");
        return;
    }

    NBN_Connection_ID conn_id = NBN_BuildConnectionHash(ws, NBN_DRIVER_WEBRTC_NATIVE);
    NBN_Connection *conn = CreateClientConnection(server, NBN_DRIVER_WEBRTC_NATIVE, conn_id);

    conn->driver_data.webrtc.peer_id = peer_id;
    conn->driver_data.webrtc.ws = ws;
    conn->driver_data.webrtc.channel_id = channel_id;
    conn->driver_data.endpoint_ptr = server;

    rtcSetUserPointer(peer_id, conn);
    rtcSetUserPointer(ws, conn);
}

static void WS_Server_OnClosed(int ws, void *user_ptr) {
    LogDebug("WS %d has closed", ws);

    if (user_ptr) {
        NBN_Connection *conn = (NBN_Connection *)user_ptr;
        NBN_WebRTC_Peer_ID peer_id = conn->driver_data.webrtc.peer_id;
        int channel_id = conn->driver_data.webrtc.channel_id;

        LogDebug("Closing WebRTC peer and channel (peer: %d, channel: %d)", peer_id, channel_id);

        ClosePeer(conn);
    }
}

static void WS_Server_OnMessage(int ws, const char *msg, int size, void *user_ptr) {
    NBN_Connection *conn = (NBN_Connection *)user_ptr;
    NBN_WebRTC_Peer_ID peer_id = conn->driver_data.webrtc.peer_id;

    ProcessSignalingMessage(peer_id, ws, msg, size, "offer");
}

static void WS_Server_OnConnection(int wsserver, int ws, void *user_ptr) {
    (void)wsserver;

    LogDebug("New WS connection %d (user_ptr: %p)", ws, user_ptr);

    rtcSetUserPointer(ws, user_ptr);
    rtcSetOpenCallback(ws, WS_Server_OnOpen);
    rtcSetClosedCallback(ws, WS_Server_OnClosed);
    rtcSetErrorCallback(ws, WS_OnError);
    rtcSetMessageCallback(ws, WS_Server_OnMessage);
}

static int WebRTC_Native_Server_Start(NBN_Server *server, uint16_t port) {
    NBN_WebRTC_Config cfg = server->driver_data.webrtc.cfg;

    rtcInitLogger(cfg.log_level, WebRTC_Native_Log);
    rtcPreload();

    rtcWsServerConfiguration rtc_cfg = {.port = port,
                                        .enableTls = cfg.enable_tls,
                                        .certificatePemFile = cfg.cert_path,
                                        .keyPemFile = cfg.key_path,
                                        .keyPemPass = cfg.passphrase};

    int ws_server = rtcCreateWebSocketServer(&rtc_cfg, WS_Server_OnConnection);

    if (ws_server < 0) {
        LogError("Failed to start WS server (code: %d)", ws_server);

        return NBN_ERROR;
    }

    rtcSetUserPointer(ws_server, server);
    server->driver_data.webrtc.ws_server = ws_server;

    return 0;
}

static void WebRTC_Native_Server_Stop(NBN_Server *server) {
    int ws_server = server->driver_data.webrtc.ws_server;

    if (ws_server >= 0) {
        rtcDeleteWebSocketServer(ws_server);
    }

    rtcCleanup();
}

static int WebRTC_Native_Server_RecvPackets(NBN_Server *server) {
    NBN_Packet *packet = &server->endpoint.read_packet;
    const int buffer_size = sizeof(packet->buffer);
    int size = buffer_size;

    for (unsigned int i = 0; i < hmlen(server->clients); i++) {
        NBN_Connection *conn = server->clients[i].value;

        if (conn->driver->id != NBN_DRIVER_WEBRTC_NATIVE)
            continue;

        int channel_id = conn->driver_data.webrtc.channel_id;

        while (rtcReceiveMessage(channel_id, (char *)packet->buffer, &size) == RTC_ERR_SUCCESS) {
            if (Packet_InitRead(packet, server->endpoint.protocol_id, size) < 0)
                continue;

            packet->sender = conn;
            size = buffer_size;

            ServerDriver_OnClientPacketReceived(server, packet);
        }
    }

    return 0;
}

static void WebRTC_Native_Server_CleanupConnection(NBN_Server *server, NBN_Connection *conn) {
    (void)server;

    NBN_Assert(conn != NULL);
    ClosePeer(conn);
}

static int WebRTC_Native_Server_SendPacketTo(NBN_Server *server, NBN_Packet *packet, NBN_Connection *conn) {
    (void)server;

    NBN_WebRTC_Peer_ID peer_id = conn->driver_data.webrtc.peer_id;
    int channel_id = conn->driver_data.webrtc.channel_id;

    if (rtcSendMessage(channel_id, (char *)packet->buffer, packet->size) < 0) {
        LogError("rtcSendMessage failed for peer %d", peer_id);

        return NBN_ERROR;
    }

    return 0;
}

static void WS_Client_OnLocalDescription(int pc, const char *sdp, const char *type, void *user_ptr) {
    (void)pc;

    LogDebug("Processing local description of type '%s'", type);

    if (strncmp(type, "offer", strlen("offer")) != 0) {
        LogWarning("Ignoring local description of type '%s' (expected 'offer')", type);
        return;
    }

    NBN_Connection *conn = (NBN_Connection *)user_ptr;
    int ws = conn->driver_data.webrtc.ws;

    ProcessLocalDescription(ws, sdp, "offer");
}

static void WS_Client_OnPeerStateChanged(int pc, rtcState state, void *user_ptr) {
    NBN_Connection *conn = (NBN_Connection *)user_ptr;

    LogDebug("Server peer state changed to %d", pc, state);

    if (state == RTC_CONNECTED) {
        LogDebug("Server peer is connected !", pc);
        NBN_Client *client = (NBN_Client *)conn->driver_data.endpoint_ptr;
        client->driver_data.webrtc.is_connected = true;
    }
}

static void WS_Client_OnOpen(int ws, void *user_ptr) {
    NBN_Client *client = (NBN_Client *)user_ptr;

    LogDebug("WS %d is open, creating peer...", ws);

    NBN_WebRTC_Peer_ID peer_id;
    int channel_id;

    if (CreatePeer(&peer_id, &channel_id, client->driver_data.webrtc.cfg, WS_Client_OnLocalDescription,
                   WS_Client_OnPeerStateChanged) < 0) {
        LogError("Failed to create peer");
        return;
    }

    LogDebug("Successfully created peer: %d", peer_id);

    NBN_Connection *server_conn = client->server_connection;

    NBN_Assert(server_conn != NULL);
    rtcSetUserPointer(peer_id, server_conn);
    rtcSetUserPointer(ws, server_conn);
}

static void WS_Client_OnClosed(int ws, void *user_ptr) {
    LogDebug("WS %d has closed", ws);

    if (user_ptr) {
        NBN_Connection *conn = (NBN_Connection *)user_ptr;

        ClosePeer(conn);
    }
}

static void WS_Client_OnMessage(int ws, const char *msg, int size, void *user_ptr) {
    NBN_Connection *conn = (NBN_Connection *)user_ptr;
    NBN_WebRTC_Peer_ID peer_id = conn->driver_data.webrtc.peer_id;

    ProcessSignalingMessage(peer_id, ws, msg, size, "answer");
}

static int WebRTC_Native_Client_Start(NBN_Client *client, const char *host, uint16_t port) {
    NBN_WebRTC_Config cfg = client->driver_data.webrtc.cfg;

    rtcInitLogger(cfg.log_level, WebRTC_Native_Log);
    rtcPreload();

    char ws_addr[256] = {0};
    // TODO: wss?
    snprintf(ws_addr, sizeof(ws_addr), "ws://%s:%d", host, port);

    int cli_ws;

    if ((cli_ws = rtcCreateWebSocket(ws_addr)) < 0) {
        LogError("Failed to create websocket");
        return NBN_ERROR;
    }

    LogDebug("Successfully created client WS: %d", cli_ws);

    rtcSetUserPointer(cli_ws, client);
    rtcSetOpenCallback(cli_ws, WS_Client_OnOpen);
    rtcSetClosedCallback(cli_ws, WS_Client_OnClosed);
    rtcSetErrorCallback(cli_ws, WS_OnError);
    rtcSetMessageCallback(cli_ws, WS_Client_OnMessage);

    // wait for the connection to be established
    const float delay = 0.3f;
    const long timeout = 5; // 5 seconds to connect
    float current_time_sec = 0;

    while (true) {
#if defined(_WIN32) || defined(_WIN64)
        Sleep(delay * 1000);
#elif _POSIX_C_SOURCE >= 199309L
        struct timespec rqtp;
        rqtp.tv_sec = 0;
        rqtp.tv_nsec = delay * 1e9;

        if (nanosleep(&rqtp, NULL) < 0) {
            LogError("nanosleep failed");
            NBN_Abort();
        }
#else
        if (usleep(delay * 1e6) < 0) {
            LogError("usleep failed");
            NBN_Abort();
        }
#endif

        current_time_sec += delay;

        if (current_time_sec >= timeout || client->driver_data.webrtc.is_connected) {
            break;
        }
    }

    return client->driver_data.webrtc.is_connected ? 0 : NBN_ERROR;
}

static void WebRTC_Native_Client_Stop(NBN_Client *client) {
    if (client->driver_data.webrtc.is_connected) {
        ClosePeer(client->server_connection);
    }

    client->driver_data.webrtc.is_connected = false;
    rtcCleanup();
}

static int WebRTC_Native_Client_RecvPackets(NBN_Client *client) {
    NBN_Packet *packet = &client->endpoint.read_packet;
    const int buffer_size = sizeof(packet->buffer);
    int size = buffer_size;
    int channel_id = client->server_connection->driver_data.webrtc.channel_id;

    while (rtcReceiveMessage(channel_id, (char *)packet->buffer, &size) == RTC_ERR_SUCCESS) {
        if (Packet_InitRead(packet, client->endpoint.protocol_id, size) < 0)
            continue;

        packet->sender = NULL;
        size = buffer_size;

        ClientDriver_OnPacketReceived(client, packet);
    }

    return 0;
}

static int WebRTC_Native_Client_SendPacket(NBN_Client *client, NBN_Packet *packet, NBN_Connection *connection) {
    (void)client;

    int channel_id = connection->driver_data.webrtc.channel_id;

    if (rtcSendMessage(channel_id, (char *)packet->buffer, packet->size) < 0) {
        LogError("rtcSendMessage failed");
        return NBN_ERROR;
    }

    return 0;
}

#endif // NBN_WEBRTC_NATIVE

// END OF WEBRTC NATIVE DRIVER
// ===================================================

/**
 * ====== PACKET SIMULATOR ======
 */

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#define RAND_RATIO_BETWEEN(min, max) (((rand() % (int)((max * 100.f) - (min * 100.f) + 1)) + (min * 100.f)) / 100.f)
#define RAND_RATIO RAND_RATIO_BETWEEN(0, 1)

#ifdef NBN_PLATFORM_WINDOWS
DWORD WINAPI PacketSimulator_Routine(LPVOID);
#else
static void *PacketSimulator_Routine(void *);
#endif

void NBN_Client_SetPing(NBN_Client *client, float v) { client->endpoint.packet_simulator.ping = v; }
void NBN_Client_SetJitter(NBN_Client *client, float v) { client->endpoint.packet_simulator.jitter = v; }
void NBN_Client_SetPacketLoss(NBN_Client *client, float v) { client->endpoint.packet_simulator.packet_loss_ratio = v; }
void NBN_Client_SetPacketDuplication(NBN_Client *client, float v) {
    client->endpoint.packet_simulator.packet_duplication_ratio = v;
}

void NBN_Server_SetPing(NBN_Server *server, float v) { server->endpoint.packet_simulator.ping = v; }
void NBN_Server_SetJitter(NBN_Server *server, float v) { server->endpoint.packet_simulator.jitter = v; }
void NBN_Server_SetPacketLoss(NBN_Server *server, float v) { server->endpoint.packet_simulator.packet_loss_ratio = v; }
void NBN_Server_SetPacketDuplication(NBN_Server *server, float v) {
    server->endpoint.packet_simulator.packet_duplication_ratio = v;
}

static void PacketSimulator_Init(NBN_PacketSimulator *packet_simulator, NBN_Endpoint *endpoint) {
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

#ifdef NBN_PLATFORM_WINDOWS
    packet_simulator->queue_mutex = CreateMutex(NULL, FALSE, NULL);
#else
    packet_simulator->queue_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif
}

static int PacketSimulator_EnqueuePacket(NBN_PacketSimulator *packet_simulator, NBN_Packet *packet,
                                         NBN_Connection *receiver) {
#ifdef NBN_PLATFORM_WINDOWS
    WaitForSingleObject(packet_simulator->queue_mutex, INFINITE);
#else
    pthread_mutex_lock(&packet_simulator->queue_mutex);
#endif

    /* Compute jitter in range [ -jitter, +jitter ].
     * Jitter is converted from seconds to milliseconds for the random operation below.
     */

    int jitter = packet_simulator->jitter * 1000;

    jitter = (jitter > 0) ? (rand() % (jitter * 2)) - jitter : 0;

    NBN_PacketSimulatorEntry *entry = (NBN_PacketSimulatorEntry *)malloc(sizeof(NBN_PacketSimulatorEntry));

    entry->delay = packet_simulator->ping + (float)jitter / 1000; /* and converted back to seconds */
    entry->receiver = receiver;
    entry->enqueued_at = packet_simulator->endpoint->time;

    memcpy(&entry->packet, packet, sizeof(NBN_Packet));

    if (packet_simulator->packet_count > 0) {
        entry->prev = packet_simulator->tail_packet;
        entry->next = NULL;

        packet_simulator->tail_packet->next = entry;
        packet_simulator->tail_packet = entry;
    } else // the list is empty
    {
        entry->prev = NULL;
        entry->next = NULL;

        packet_simulator->head_packet = entry;
        packet_simulator->tail_packet = entry;
    }

    packet_simulator->packet_count++;

#ifdef NBN_PLATFORM_WINDOWS
    ReleaseMutex(packet_simulator->queue_mutex);
#else
    pthread_mutex_unlock(&packet_simulator->queue_mutex);
#endif

    return 0;
}

static void PacketSimulator_Start(NBN_PacketSimulator *packet_simulator) {
#ifdef NBN_PLATFORM_WINDOWS
    packet_simulator->thread = CreateThread(NULL, 0, PacketSimulator_Routine, packet_simulator, 0, NULL);
#else
    pthread_create(&packet_simulator->thread, NULL, PacketSimulator_Routine, packet_simulator);
#endif

    packet_simulator->running = true;

    LogDebug("Packet simulator started (Packet loss: %f, Packet duplication: %f, Ping: %f, Jitter: %f)",
             packet_simulator->packet_loss_ratio, packet_simulator->packet_duplication_ratio, packet_simulator->ping,
             packet_simulator->jitter);
}

void PacketSimulator_Stop(NBN_PacketSimulator *packet_simulator) {
    packet_simulator->running = false;

#ifdef NBN_PLATFORM_WINDOWS
    WaitForSingleObject(packet_simulator->thread, INFINITE);
#else
    pthread_join(packet_simulator->thread, NULL);
#endif
}

#ifdef NBN_PLATFORM_WINDOWS
DWORD WINAPI PacketSimulator_Routine(LPVOID arg)
#else
static void *PacketSimulator_Routine(void *arg)
#endif
{
    NBN_PacketSimulator *packet_simulator = (NBN_PacketSimulator *)arg;

    while (packet_simulator->running) {
#ifdef NBN_PLATFORM_WINDOWS
        WaitForSingleObject(packet_simulator->queue_mutex, INFINITE);
#else
        pthread_mutex_lock(&packet_simulator->queue_mutex);
#endif

        NBN_PacketSimulatorEntry *entry = packet_simulator->head_packet;

        while (entry) {
            NBN_PacketSimulatorEntry *next = entry->next;

            if (packet_simulator->endpoint->time - entry->enqueued_at < entry->delay) {
                entry = next;

                continue;
            }

            PacketSimulator_SendPacket(packet_simulator, &entry->packet, entry->receiver);

            for (unsigned int i = 0; i < PacketSimulator_GetRandomDuplicatePacketCount(packet_simulator); i++) {
                LogDebug("Duplicate packet %d (count: %d)", entry->packet.header.seq_number, i + 1);

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
            } else if (entry == packet_simulator->tail_packet) // it's the tail of the list
            {
                NBN_PacketSimulatorEntry *new_tail = entry->prev;

                new_tail->next = NULL;
                packet_simulator->tail_packet = new_tail;
            } else // it's in the middle of the list
            {
                entry->prev->next = entry->next;
                entry->next->prev = entry->prev;
            }

            packet_simulator->packet_count--;

            // release the memory allocated for the entry
            free(entry);

            entry = next;
        }

#ifdef NBN_PLATFORM_WINDOWS
        ReleaseMutex(packet_simulator->queue_mutex);
#else
        pthread_mutex_unlock(&packet_simulator->queue_mutex);
#endif
    }

#ifdef NBN_PLATFORM_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

static int PacketSimulator_SendPacket(NBN_PacketSimulator *packet_simulator, NBN_Packet *packet,
                                      NBN_Connection *receiver) {
    if (RAND_RATIO < packet_simulator->packet_loss_ratio) {
        packet_simulator->total_dropped_packets++;
        LogDebug("Drop packet %d (Total dropped packets: %d)", packet->header.seq_number,
                 packet_simulator->total_dropped_packets);

        return 0;
    }

    NBN_Driver *driver = receiver->driver;
    bool is_server = packet_simulator->endpoint->is_server;

    if (is_server) {
        if (receiver->is_stale)
            return 0;

        return driver->impl.serv_send_packet_to((NBN_Server *)packet_simulator->endpoint, packet, receiver);
    } else {
        return driver->impl.cli_send_packet((NBN_Client *)packet_simulator->endpoint, packet, receiver);
    }
}

static unsigned int PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *packet_simulator) {
    if (RAND_RATIO < packet_simulator->packet_duplication_ratio)
        return rand() % 10 + 1;

    return 0;
}

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

// END OF PACKET SIMULATOR
// ===================================================

/**
 * ====== LOGGING ======
 */

#ifdef NBN_DEBUG

#define NBN_DEFAULT_LOG_LEVEL NBN_LOG_DEBUG

#else

#define NBN_DEFAULT_LOG_LEVEL NBN_LOG_INFO

#endif // NBN_DEBUG

static NBN_LogLevel log_level = NBN_DEFAULT_LOG_LEVEL;

void NBN_SetLogLevel(NBN_LogLevel level) { log_level = level; }

#ifdef NBN_LOG_CUSTOM_FUNCTION

extern void Log(NBN_LogLevel level, const char *filename, int line, const char *msg, ...);

#else

/**
 * Default logging function
 */

/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#include <stdarg.h>

static const char *level_names[] = {"ERROR", "INFO", "WARNING", "DEBUG"};

static void Log(NBN_LogLevel level, const char *filename, int line, const char *msg, ...) {
    if (log_level < level) {
        return;
    }

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    FILE *fp = level == NBN_LOG_ERROR ? stderr : stdout;

    va_list args;
    char buf[32];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
    fprintf(fp, "%s %-5s %s:%d: ", buf, level_names[level], filename, line);
    va_start(args, msg);
    vfprintf(fp, msg, args);
    va_end(args);
    fprintf(fp, "\n");
    fflush(fp);
}

#endif // NBN_CUSTOM_LOG_FUNCTION

// END OF LOGGING
// ===================================================
