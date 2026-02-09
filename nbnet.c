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

// TODO: make functions that are not part of the public API static
// TODO: reintroduce webrtc native driver
// TODO: remove pragmas

#include <stdint.h>
#include "nbnet.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

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

#ifndef NBN_PLATFORM_WINDOWS

#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

#endif

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

#define NBN_MAX_MESSAGE_TYPES UINT8_MAX
// IMPORTANT: DO NOT FORGET TO MODIFY 'NBN_RESERVED_MESSAGE_TYPES' if you add or remove library messages
#define NBN_RESERVED_MESSAGE_TYPES 4 /* Number of message types reserved for the library */

#define NBN_CLIENT_CLOSED_MESSAGE_TYPE NBN_MAX_MESSAGE_TYPES
#define NBN_CLIENT_ACCEPTED_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 1)
#define NBN_DISCONNECTION_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 2)
#define NBN_CONNECTION_REQUEST_MESSAGE_TYPE (NBN_MAX_MESSAGE_TYPES - 3)

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

/* Maximum number of packets that can be sent in a single flush
 *
 * IMPORTANT: DO NOT INCREASE THIS, it will break packet acks
 */
#define NBN_CONNECTION_MAX_SENT_PACKET_COUNT 16

#define NBN_MAX_PACKET_ENTRIES 1024

#define NBN_RESERVED_UNRELIABLE_CHANNEL_ID 0
#define NBN_RESERVED_RELIABLE_CHANNEL_ID 1

static int log_level = NBN_LOG_INFO;

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

#include <stdio.h>
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

#define LogInfo(msg, ...) Log(NBN_LOG_INFO, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define LogWarning(msg, ...) Log(NBN_LOG_WARNING, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define LogError(msg, ...) Log(NBN_LOG_ERROR, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define LogDebug(msg, ...) Log(NBN_LOG_DEBUG, __FILE__, __LINE__, msg, ##__VA_ARGS__)

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
    bool acked;
    bool flagged_as_lost;
    unsigned int messages_count;
    double send_time;
    NBN_MessageEntry messages[NBN_MAX_MESSAGES_PER_PACKET];
} NBN_PacketEntry;

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
    uint16_t id;
    double last_send_time;
    bool free;
} NBN_OutgoingMessage;

typedef struct NBN_IncomingMessage {
    NBN_Message message;
    bool free;
} NBN_IncomingMessage;

struct NBN_Channel {
    uint8_t id;
    NBN_ChannelMode type;
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

typedef struct NBN_IPAddress {
    uint32_t host;
    uint16_t port;
} NBN_IPAddress;

typedef uint32_t NBN_WebRTC_Peer_ID;

struct NBN_Connection {
    NBN_ConnectionHandle handle;
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

    /* Driver-related data attached to the connection */
    union {
        struct {
            NBN_IPAddress ip_address;
        } udp;

        NBN_WebRTC_Peer_ID peer_id;
    } driver_data;

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

typedef union NBN_EventData {
    NBN_MessageInfo message_info;
    NBN_DisconnectionInfo disconnection;
    NBN_Connection *connection;
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

static void PacketSimulator_Init(NBN_PacketSimulator *, NBN_Endpoint *);
static int PacketSimulator_EnqueuePacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *);
static void PacketSimulator_Start(NBN_PacketSimulator *);
static void PacketSimulator_Stop(NBN_PacketSimulator *);

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

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
    NBN_ChannelMode channel_modes[NBN_CHANNEL_COUNT];
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    NBN_PacketSimulator packet_simulator;
#endif
};

typedef struct NBN_GameServer_Config {
    const char *protocol_name;
    uint16_t port;
    NBN_ChannelMode channel_modes[NBN_CHANNEL_COUNT];
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

typedef struct NBN_GameClient_Config {
    const char *protocol_name;
    const char *host;
    uint16_t port;
    NBN_ChannelMode channel_modes[NBN_CHANNEL_COUNT];
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

static NBN_GameServer nbn_game_server;
static NBN_GameClient nbn_game_client;

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

enum NBN_Driver_ID { NBN_DRIVER_UDP = 0x01, NBN_DRIVER_WEBRTC_EMSCRIPTEN };

struct NBN_Driver {
    int id;
    const char *name;
    NBN_Driver_Implementation impl;
};

void NBN_Packet_InitWrite(NBN_Packet *, uint32_t, uint16_t, uint16_t, uint32_t);
NBN_PacketResult NBN_Packet_WriteMessage(NBN_Packet *, NBN_OutgoingMessage *);
int NBN_Packet_Seal(NBN_Packet *);
int NBN_Packet_InitRead(NBN_Packet *, uint32_t, unsigned int);

int NBN_Connection_ProcessReceivedPacket(NBN_Endpoint *, NBN_Connection *, NBN_Packet *, double);
int NBN_Connection_FlushChannels(NBN_Endpoint *, NBN_Connection *, uint32_t, double);
bool NBN_Connection_CheckIfStale(NBN_Connection *, double);

#pragma endregion /* NBN_Packet */

#ifdef NBN_UDP

#if defined(NBN_PLATFORM_WINDOWS)

#include <winsock2.h>

typedef int socklen_t;

#elif defined(NBN_PLATFORM_UNIX) || defined(NBN_PLATFORM_MAC)

#include <arpa/inet.h>
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

static int UDP_Client_Start(NBN_GameClient *client, const char *host, uint16_t port);
static void UDP_Client_Stop(NBN_GameClient *client);
static int UDP_Client_RecvPackets(NBN_GameClient *client);
static int UDP_Client_SendPacket(NBN_GameClient *client, NBN_Packet *packet);

static int UDP_Server_Start(NBN_GameServer *server, uint16_t port);
static void UDP_Server_Stop(NBN_GameServer *server);
static int UDP_Server_RecvPackets(NBN_GameServer *server);
static int UDP_Server_SendPacketTo(NBN_GameServer *server, NBN_Packet *packet, NBN_Connection *connection);
static void UDP_Server_CleanupConnection(NBN_GameServer *server, NBN_Connection *connection);

static NBN_Driver nbn_udp_driver = {.name = "UDP",
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
// TODO: remove global state
static SOCKET nbn_udp_sock;

#endif // NBN_UDP

#ifdef __EMSCRIPTEN__

#ifdef NBN_UDP
#error "Cannot compile UDP driver with emscripten"
#endif

// TODO: add a check for webrtc native as well

#include <emscripten.h>

static int WebRTC_Client_Start(NBN_GameClient *client, const char *host, uint16_t port);
static void WebRTC_Client_Stop(NBN_GameClient *client);
static int WebRTC_Client_RecvPackets(NBN_GameClient *client);
static int WebRTC_Client_SendPacket(NBN_GameClient *client, NBN_Packet *packet);

static int WebRTC_Server_Start(NBN_GameServer *server, uint16_t port);
static void WebRTC_Server_Stop(NBN_GameServer *server);
static int WebRTC_Server_RecvPackets(NBN_GameServer *server);
static int WebRTC_Server_SendPacketTo(NBN_GameServer *server, NBN_Packet *packet, NBN_Connection *connection);
static void WebRTC_Server_CleanupConnection(NBN_GameServer *server, NBN_Connection *connection);

static NBN_Driver nbn_webrtc_em_driver = {.name = "WebRTC_EMSCRIPTEN",
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

static NBN_WebRTC_Config nbn_wrtc_cfg = {.enable_tls = false, .cert_path = NULL, .key_path = NULL};

void NBN_WebRTC_SetConfig(NBN_WebRTC_Config config);

#endif // __EMSCRIPTEN__

#pragma region Serialization

void NBN_Writer_Init(NBN_Writer *writer, uint8_t *buffer, unsigned int length) {
    writer->buffer = buffer;
    writer->length = length;
    writer->position = 0;
}

void NBN_Writer_WriteInt8(NBN_Writer *writer, int8_t value) { NBN_Writer_WriteUInt8(writer, value); }

void NBN_Writer_WriteInt32(NBN_Writer *writer, int32_t value) { NBN_Writer_WriteUInt32(writer, value); }

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

    *((uint64_t *)(writer->buffer + writer->position)) = htonll(value);
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

int NBN_Reader_ReadInt32(NBN_Reader *reader, int32_t *value) {
    return NBN_Reader_ReadUInt32(reader, (uint32_t *)value);
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

    *value = ntohll(*((uint64_t *)(reader->buffer + reader->position)));
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

#pragma endregion /* Serialization */

#pragma region NBN_Packet

void NBN_Packet_InitWrite(NBN_Packet *packet, uint32_t protocol_id, uint16_t seq_number, uint16_t ack,
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

NBN_PacketResult NBN_Packet_WriteMessage(NBN_Packet *packet, NBN_OutgoingMessage *out_msg) {
    NBN_Message *message = &out_msg->message;

    LogDebug("Write message %d (type: %d, length: %d) to packet %d", out_msg->id, message->header.type,
             message->header.length, packet->header.seq_number);

    if (packet->mode != NBN_PACKET_MODE_WRITE || packet->sealed)
        return NBN_PACKET_WRITE_ERROR;

    unsigned int message_size = NBN_MESSAGE_HEADER_SIZE + message->header.length;

    if (packet->header.messages_count >= NBN_MAX_MESSAGES_PER_PACKET ||
        packet->size + message_size > NBN_PACKET_MAX_SIZE) {
        return NBN_PACKET_WRITE_NO_SPACE;
    }

    NBN_Writer writer;

    NBN_Writer_Init(&writer, packet->buffer + packet->size, sizeof(packet->buffer) - packet->size);

    NBN_Writer_WriteUInt16(&writer, out_msg->id);
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

int NBN_Packet_Seal(NBN_Packet *packet) {
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

int NBN_Packet_InitRead(NBN_Packet *packet, uint32_t protocol_id, unsigned int size) {
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

#pragma endregion /* NBN_Packet */

#pragma region NBN_Channel

static unsigned int Channel_ComputeMessageIdDelta(uint16_t id1, uint16_t id2);

static void Channel_Init(NBN_Channel *channel, uint8_t id, NBN_ChannelMode type) {
    channel->id = id;
    channel->type = type;
    channel->next_outgoing_message_id = 0;
    channel->next_recv_message_id = 0;
    channel->outgoing_message_count = 0;
    channel->last_received_message_id = -1;
    channel->next_outgoing_message_slot = 0;
    channel->oldest_unacked_message_id = 0;
    channel->most_recent_message_id = 0;

    for (unsigned int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++) {
        channel->incoming_messages_buffer[i].free = true;
        channel->outgoing_messages_buffer[i].free = true;
    }

    for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++)
        channel->ack_buffer[i] = false;
}

static void Channel_UpdateMessageSendTime(NBN_Channel *channel, uint16_t msg_id, double time) {
    NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE];

    NBN_Assert(msg_id == out_msg->id);
    out_msg->last_send_time = time;
}

static bool Channel_AddReceivedMessage(NBN_Endpoint *endpoint, NBN_Channel *channel, NBN_Message *message) {
    if (channel->type == NBN_CHANNEL_UNRELIABLE) {
        if (SEQUENCE_NUMBER_GT(message->header.id, channel->last_received_message_id)) {
            NBN_IncomingMessage *inc_msg =
                &channel->incoming_messages_buffer[message->header.id % NBN_CHANNEL_BUFFER_SIZE];

            inc_msg->free = false;
            memcpy(&inc_msg->message, message, sizeof(NBN_Message));

            channel->last_received_message_id = message->header.id;

            LogDebug("Add incomoing message %d of type %d to unreliable channel %d (last received msg id: %d)",
                     message->header.id, message->header.type, channel->id, channel->last_received_message_id);

            return true;
        }

        return false;
    } else if (channel->type == NBN_CHANNEL_RELIABLE) {
        unsigned int dt = Channel_ComputeMessageIdDelta(message->header.id, channel->most_recent_message_id);

        LogDebug("Add incomoing message %d of type %d to reliable channel %d (most recent msg id: %d, dt: %d)",
                 message->header.id, message->header.type, channel->id, channel->most_recent_message_id, dt);

        if (SEQUENCE_NUMBER_GT(message->header.id, channel->most_recent_message_id)) {
            NBN_Assert(dt < NBN_CHANNEL_BUFFER_SIZE);

            channel->most_recent_message_id = message->header.id;
        } else {
            /* This is an old message that has already been received, probably coming from
               an out of order late packet. */
            if (dt >= NBN_CHANNEL_BUFFER_SIZE)
                return false;

            if (SEQUENCE_NUMBER_LT(message->header.id, channel->next_recv_message_id)) {
                return false;
            }
        }

        NBN_IncomingMessage *inc_msg = &channel->incoming_messages_buffer[message->header.id % NBN_CHANNEL_BUFFER_SIZE];

        inc_msg->free = false;
        memcpy(&inc_msg->message, message, sizeof(NBN_Message));

        return true;
    }

    NBN_Abort();
}

static bool Channel_AddOutgoingMessage(NBN_Channel *channel, NBN_Message *message) {
    NBN_Assert(channel->type == NBN_CHANNEL_UNRELIABLE || channel->type == NBN_CHANNEL_RELIABLE);

    uint16_t msg_id = channel->next_outgoing_message_id;
    int index = msg_id % NBN_CHANNEL_BUFFER_SIZE;
    NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[index];

    // make sure the outgoing message is not already in use
    if (!out_msg->free) {
        LogError("No outgoing message available in channel %d (type: %d, outgoing message count: %d)", channel->type,
                 channel->id, channel->outgoing_message_count);
#ifdef NBN_DEBUG
        NBN_Abort();
#endif

        return false;
    }

    out_msg->id = msg_id;
    out_msg->free = false;
    out_msg->last_send_time = -1;
    memcpy(&out_msg->message, message, sizeof(NBN_Message));

    channel->next_outgoing_message_id++;
    channel->outgoing_message_count++;

    return true;
}

static NBN_Message *Channel_GetNextRecvedMessage(NBN_Channel *channel) {
    if (channel->type == NBN_CHANNEL_UNRELIABLE) {
        while (SEQUENCE_NUMBER_LTE(channel->next_recv_message_id, channel->last_received_message_id)) {
            NBN_IncomingMessage *inc_msg =
                &channel->incoming_messages_buffer[channel->next_recv_message_id % NBN_CHANNEL_BUFFER_SIZE];
            uint16_t msg_id = channel->next_recv_message_id;

            channel->next_recv_message_id++;

            if (!inc_msg->free && inc_msg->message.header.id == msg_id) {
                inc_msg->free = true;

                return &inc_msg->message;
            }
        }

        return NULL;
    } else if (channel->type == NBN_CHANNEL_RELIABLE) {
        NBN_IncomingMessage *inc_msg =
            &channel->incoming_messages_buffer[channel->next_recv_message_id % NBN_CHANNEL_BUFFER_SIZE];

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

static bool Channel_GetNextOutgoingMessage(NBN_Channel *channel, NBN_OutgoingMessage *res_out_msg, double time) {
    if (channel->type == NBN_CHANNEL_UNRELIABLE) {
        NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[channel->next_outgoing_message_slot];

        if (out_msg->free)
            return false;

        *res_out_msg = *out_msg;
        out_msg->free = true;
        channel->next_outgoing_message_slot++;
        channel->next_outgoing_message_slot %= NBN_CHANNEL_BUFFER_SIZE;

        return true;
    } else if (channel->type == NBN_CHANNEL_RELIABLE) {
        int max_message_id = (channel->oldest_unacked_message_id + (NBN_CHANNEL_BUFFER_SIZE - 1)) % (0xFFFF + 1);

        if (SEQUENCE_NUMBER_LT(channel->next_outgoing_message_id, max_message_id))
            max_message_id = channel->next_outgoing_message_id;

        uint16_t msg_id = channel->oldest_unacked_message_id;

        while (SEQUENCE_NUMBER_LT(msg_id, max_message_id)) {
            NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[msg_id % NBN_CHANNEL_BUFFER_SIZE];

            if (!out_msg->free &&
                (out_msg->last_send_time < 0 || time - out_msg->last_send_time >= NBN_MESSAGE_RESEND_DELAY)) {
                *res_out_msg = *out_msg;
                return true;
            }

            msg_id++;
        }

        return false;
    } else {
        NBN_Abort();
    }
}

static int Channel_OnMessageSent(NBN_Endpoint *endpoint, NBN_Channel *channel, NBN_Message *message) {
    if (channel->type != NBN_CHANNEL_UNRELIABLE) {
        return 0;
    }

    channel->outgoing_message_count--;

    return 0;
}

static int Channel_OnOutgoingMessageAcked(NBN_Endpoint *endpoint, NBN_Channel *channel, uint16_t msg_id) {
    if (channel->type != NBN_CHANNEL_RELIABLE) {
        return 0;
    }

    int index = msg_id % NBN_CHANNEL_BUFFER_SIZE;
    NBN_OutgoingMessage *out_msg = &channel->outgoing_messages_buffer[index];

    if (out_msg->free || out_msg->id != msg_id)
        return 0;

    out_msg->free = true;

    LogDebug("Message %d acked on channel %d (buffer index: %d, oldest unacked: %d)", msg_id, channel->id, index,
             channel->oldest_unacked_message_id);

    channel->ack_buffer[index] = true;
    channel->outgoing_message_count--;

    if (msg_id == channel->oldest_unacked_message_id) {
        for (int i = 0; i < NBN_CHANNEL_BUFFER_SIZE; i++) {
            uint16_t ack_msg_id = msg_id + i;
            int index = ack_msg_id % NBN_CHANNEL_BUFFER_SIZE;

            if (channel->ack_buffer[index]) {
                channel->ack_buffer[index] = false;
                channel->oldest_unacked_message_id++;
            } else {
                break;
            }
        }

        LogDebug("Updated oldest unacked message id on channel %d: %d", channel->id,
                 channel->oldest_unacked_message_id);
    }

    return 0;
}

static unsigned int Channel_ComputeMessageIdDelta(uint16_t id1, uint16_t id2) {
    if (SEQUENCE_NUMBER_GT(id1, id2))
        return (id1 >= id2) ? id1 - id2 : ((0xFFFF + 1) - id2) + id1;
    else
        return (id2 >= id1) ? id2 - id1 : (((0xFFFF + 1) - id1) + id2) % 0xFFFF;
}

#pragma endregion /* NBN_Channel */

#pragma region NBN_Connection

static void Endpoint_CreateOutgoingMessage(NBN_Endpoint *, uint8_t, uint8_t);

static uint32_t Connection_BuildPacketAckBits(NBN_Connection *);
static int Connection_DecodePacketHeader(NBN_Endpoint *, NBN_Connection *, NBN_Packet *, double);
static int Connection_AckPacket(NBN_Endpoint *, NBN_Connection *, uint16_t, double time);
static void Connection_InitOutgoingPacket(NBN_Connection *, uint32_t, NBN_Packet *, NBN_PacketEntry **);
static NBN_PacketEntry *Connection_InsertOutgoingPacketEntry(NBN_Connection *, uint16_t);
static bool Connection_InsertReceivedPacketEntry(NBN_Connection *, uint16_t);
static NBN_PacketEntry *Connection_FindSendPacketEntry(NBN_Connection *, uint16_t);
static bool Connection_IsPacketReceived(NBN_Connection *, uint16_t);
static int Connection_SendPacket(NBN_Connection *, NBN_Packet *, NBN_PacketEntry *, double);
static int Connection_ReadNextMessageFromBuffer(NBN_Endpoint *, NBN_Reader *, NBN_Message *);
static void Connection_UpdateAveragePing(NBN_Connection *, double);
static void Connection_UpdateAveragePacketLoss(NBN_Connection *, uint16_t);
static void Connection_UpdateAverageUploadBandwidth(NBN_Connection *, float);
static void Connection_UpdateAverageDownloadBandwidth(NBN_Connection *, double);

int NBN_Connection_ProcessReceivedPacket(NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_Packet *packet,
                                         double time) {
    if (Connection_DecodePacketHeader(endpoint, connection, packet, time) < 0) {
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

        static NBN_Message message = {0};
        message.type = NBN_INCOMING_MESSAGE;
        int msg_len = Connection_ReadNextMessageFromBuffer(endpoint, &msg_reader, &message);

        if (msg_len < 0) {
            LogError("Failed to read packet, invalid data");

            return NBN_ERROR;
        }

        uint8_t channel_id = message.header.channel_id;

        if (channel_id > NBN_CHANNEL_COUNT - 1) {
            LogError("Failed to read packet, message had invalid channel");

            return NBN_ERROR;
        }

        NBN_Channel *channel = &connection->channels[channel_id];

        if (Channel_AddReceivedMessage(endpoint, channel, &message)) {
            LogDebug("Received message %d (type: %d) on channel %d", message.header.id, message.header.type,
                     channel->id);
        } else {
            LogDebug("Message %d was discarded by channel %d", message.header.id, channel->id);
        }
    }

    return 0;
}

int NBN_Connection_FlushChannels(NBN_Endpoint *endpoint, NBN_Connection *connection, uint32_t protocol_id,
                                 double time) {
    LogDebug("Flushing all channels");

    NBN_Packet packet = {0};
    NBN_PacketEntry *packet_entry;
    unsigned int sent_packet_count = 0;
    unsigned int sent_bytes = 0;

    Connection_InitOutgoingPacket(connection, protocol_id, &packet, &packet_entry);

    for (unsigned int i = 0; i < NBN_CHANNEL_COUNT; i++) {
        NBN_Channel *channel = &connection->channels[i];

        LogDebug("Flushing channel %d (message count: %d)", channel->id, channel->outgoing_message_count);

        NBN_OutgoingMessage out_msg;
        unsigned int j = 0;

        // TODO: use bandwidth to determine how many packets to send at most
        while (j < channel->outgoing_message_count && sent_packet_count < NBN_CONNECTION_MAX_SENT_PACKET_COUNT &&
               Channel_GetNextOutgoingMessage(channel, &out_msg, time)) {
            NBN_Message *message = &out_msg.message;
            uint16_t msg_id = out_msg.id;
            bool message_sent = false;
            NBN_PacketResult ret = NBN_Packet_WriteMessage(&packet, &out_msg);

            if (ret == NBN_PACKET_WRITE_OK) {
                message_sent = true;
            } else if (ret == NBN_PACKET_WRITE_NO_SPACE) {
                if (Connection_SendPacket(connection, &packet, packet_entry, time) < 0) {
                    LogError("Failed to send packet %d", packet.header.seq_number);

                    return NBN_ERROR;
                }

                sent_packet_count++;
                sent_bytes += packet.size;

                Connection_InitOutgoingPacket(connection, protocol_id, &packet, &packet_entry);

                NBN_PacketResult ret = NBN_Packet_WriteMessage(&packet, &out_msg);

                if (ret != NBN_PACKET_WRITE_OK) {
                    LogError("Failed to send packet %d", packet.header.seq_number);

                    return NBN_ERROR;
                }

                message_sent = true;
            } else if (ret == NBN_PACKET_WRITE_ERROR) {
                LogError("Failed to write message %d of type %d to packet %d", msg_id, message->header.type,
                         packet.header.seq_number);

                return NBN_ERROR;
            }

            if (message_sent) {
                LogDebug("Message %d added to packet %d (length: %d, type: %d)", msg_id, packet.header.seq_number,
                         message->header.length, message->header.type);

                Channel_UpdateMessageSendTime(channel, msg_id, time);

                packet_entry->messages[packet_entry->messages_count++] = (NBN_MessageEntry){msg_id, channel->id};

                Channel_OnMessageSent(endpoint, channel, message);
            }

            j++;
        }
    }

    if (Connection_SendPacket(connection, &packet, packet_entry, time) < 0) {
        LogError("Failed to send packet %d to connection %d", packet.header.seq_number, connection->handle.id);

        return NBN_ERROR;
    }

    sent_bytes += packet.size;
    sent_packet_count++;

    double t = time - connection->last_flush_time;

    if (t > 0)
        Connection_UpdateAverageUploadBandwidth(connection, sent_bytes / t);

    connection->last_flush_time = time;

    return 0;
}

bool NBN_Connection_CheckIfStale(NBN_Connection *connection, double time) {
#if defined(NBN_DEBUG) && defined(NBN_DISABLE_STALE_CONNECTION_DETECTION)
    /* When testing under bad network conditions (in soak test for instance), we don't want to deal
       with stale connections */
    return false;
#else
    return time - connection->last_recv_packet_time > NBN_CONNECTION_STALE_TIME_THRESHOLD;
#endif
}

static int Connection_DecodePacketHeader(NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_Packet *packet,
                                         double time) {
    if (Connection_AckPacket(endpoint, connection, packet->header.ack, time) < 0) {
        LogError("Failed to ack packet %d", packet->header.seq_number);

        return NBN_ERROR;
    }

    for (unsigned int i = 0; i < 32; i++) {
        if (B_IS_UNSET(packet->header.ack_bits, i))
            continue;

        if (Connection_AckPacket(endpoint, connection, packet->header.ack - (i + 1), time) < 0) {
            LogError("Failed to ack packet %d", packet->header.seq_number);

            return NBN_ERROR;
        }
    }

    return 0;
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

static int Connection_AckPacket(NBN_Endpoint *endpoint, NBN_Connection *connection, uint16_t ack_packet_seq_number,
                                double time) {
    NBN_PacketEntry *packet_entry = Connection_FindSendPacketEntry(connection, ack_packet_seq_number);

    if (packet_entry && !packet_entry->acked) {
        LogDebug("Packet %d acked (connection: %d)", ack_packet_seq_number, connection->handle.id);

        packet_entry->acked = true;

        Connection_UpdateAveragePing(connection, time - packet_entry->send_time);

        for (unsigned int i = 0; i < packet_entry->messages_count; i++) {
            NBN_MessageEntry *msg_entry = &packet_entry->messages[i];
            NBN_Channel *channel = &connection->channels[msg_entry->channel_id];

            NBN_Assert(channel != NULL);

            if (Channel_OnOutgoingMessageAcked(endpoint, channel, msg_entry->id) < 0) {
                return NBN_ERROR;
            }
        }
    }

    return 0;
}

static void Connection_InitOutgoingPacket(NBN_Connection *connection, uint32_t protocol_id, NBN_Packet *outgoing_packet,
                                          NBN_PacketEntry **packet_entry) {
    NBN_Packet_InitWrite(outgoing_packet, protocol_id, connection->next_packet_seq_number++,
                         connection->last_received_packet_seq_number, Connection_BuildPacketAckBits(connection));

    *packet_entry = Connection_InsertOutgoingPacketEntry(connection, outgoing_packet->header.seq_number);
}

static NBN_PacketEntry *Connection_InsertOutgoingPacketEntry(NBN_Connection *connection, uint16_t seq_number) {
    uint16_t index = seq_number % NBN_MAX_PACKET_ENTRIES;
    NBN_PacketEntry entry = {
        .acked = false, .flagged_as_lost = false, .messages_count = 0, .send_time = 0, .messages = {{0, 0}}};

    connection->packet_send_seq_buffer[index] = seq_number;
    connection->packet_send_buffer[index] = entry;

    return &connection->packet_send_buffer[index];
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

static int Connection_SendPacket(NBN_Connection *connection, NBN_Packet *packet, NBN_PacketEntry *packet_entry,
                                 double time) {
    LogDebug("Send packet %d to connection %d (messages count: %d)", packet->header.seq_number, connection->handle.id,
             packet->header.messages_count);

    NBN_Assert(packet_entry->messages_count == packet->header.messages_count);

    if (NBN_Packet_Seal(packet) < 0) {
        LogError("Failed to seal packet");

        return NBN_ERROR;
    }

    packet_entry->send_time = time;

    if (connection->endpoint->is_server) {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return PacketSimulator_EnqueuePacket(&nbn_game_server.endpoint.packet_simulator, packet, connection);
#else
        if (connection->is_stale)
            return 0;

        return connection->driver->impl.serv_send_packet_to(&nbn_game_server, packet, connection);
#endif
    } else {
#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
        return PacketSimulator_EnqueuePacket(&nbn_game_client.endpoint.packet_simulator, packet, connection);
#else
        NBN_Driver *driver = nbn_game_client.server_connection->driver;

        return driver->impl.cli_send_packet(&nbn_game_client, packet);
#endif
    }
}

static int Connection_ReadNextMessageFromBuffer(NBN_Endpoint *endpoint, NBN_Reader *reader, NBN_Message *message) {
    if (NBN_Reader_ReadUInt16(reader, &message->header.id) < 0) {
        LogError("Failed to read message id");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt16(reader, &message->header.length) < 0) {
        LogError("Failed to read message length");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(reader, &message->header.type) < 0) {
        LogError("Failed to read message type");

        return NBN_ERROR;
    }

    if (NBN_Reader_ReadUInt8(reader, &message->header.channel_id) < 0) {
        LogError("Failed to read message channel");

        return NBN_ERROR;
    }

    uint16_t msg_len = message->header.length;

    if (msg_len > 0) {
        if (msg_len > NBN_MESSAGE_MAX_SIZE) {
            LogError("Failed to read message: too big");
        }

        if (NBN_Reader_ReadBytes(reader, message->data, msg_len) < 0) {
            LogError("Failed to read message data");

            return NBN_ERROR;
        }
    }

    return 0;
}

static void Connection_UpdateAveragePing(NBN_Connection *connection, double ping) {
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

            if (!entry->flagged_as_lost) {
                entry->flagged_as_lost = true;
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

static void Connection_UpdateAverageDownloadBandwidth(NBN_Connection *connection, double time) {
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

#pragma region NBN_EventQueue

void NBN_EventQueue_Init(NBN_EventQueue *event_queue) {
    event_queue->head = 0;
    event_queue->tail = 0;
    event_queue->count = 0;
}

bool NBN_EventQueue_Enqueue(NBN_EventQueue *event_queue, NBN_Event ev) {
    if (event_queue->count >= NBN_EVENT_QUEUE_CAPACITY)
        return false;

    event_queue->events[event_queue->tail] = ev;

    event_queue->tail = (event_queue->tail + 1) % NBN_EVENT_QUEUE_CAPACITY;
    event_queue->count++;

    return true;
}

bool NBN_EventQueue_Dequeue(NBN_EventQueue *event_queue, NBN_Event *ev) {
    if (NBN_EventQueue_IsEmpty(event_queue))
        return false;

    memcpy(ev, &event_queue->events[event_queue->head], sizeof(NBN_Event));
    event_queue->head = (event_queue->head + 1) % NBN_EVENT_QUEUE_CAPACITY;
    event_queue->count--;

    return true;
}

bool NBN_EventQueue_IsEmpty(NBN_EventQueue *event_queue) { return event_queue->count == 0; }

#pragma endregion /* NBN_EventQueue */

#pragma region Endpoint

static void Endpoint_Init(NBN_Endpoint *, uint32_t, bool, NBN_ChannelMode[NBN_CHANNEL_COUNT]);
static void Endpoint_Deinit(NBN_Endpoint *);
static NBN_Connection *Endpoint_CreateConnection(NBN_Endpoint *, NBN_Connection_ID, NBN_Driver_ID);
static uint32_t Endpoint_BuildProtocolId(const char *);
static int Endpoint_ProcessReceivedPacket(NBN_Endpoint *, NBN_Packet *, NBN_Connection *);
static int Endpoint_EnqueueOutgoingMessage(NBN_Endpoint *, NBN_Connection *, NBN_Message *);
static void Endpoint_UpdateTime(NBN_Endpoint *);

static void Endpoint_Init(NBN_Endpoint *endpoint, uint32_t protocol_id, bool is_server,
                          NBN_ChannelMode channel_modes[NBN_CHANNEL_COUNT]) {
    if (NBN_CHANNEL_COUNT < 2) {
        LogError("At least 2 channels are necessary, check the NBN_CHANNEL_COUNT macro");
        NBN_Abort();
    }

    endpoint->is_server = is_server;
    endpoint->protocol_id = protocol_id;

    memcpy(&endpoint->channel_modes, channel_modes, sizeof(NBN_ChannelMode) * NBN_CHANNEL_COUNT);

    NBN_EventQueue_Init(&endpoint->event_queue);

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    PacketSimulator_Init(&endpoint->packet_simulator, endpoint);
    PacketSimulator_Start(&endpoint->packet_simulator);
#endif

    Endpoint_UpdateTime(endpoint);
}

static void Endpoint_Deinit(NBN_Endpoint *endpoint) {
    (void)endpoint;

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)
    PacketSimulator_Stop(&endpoint->packet_simulator);
#endif
}

static NBN_Connection *Endpoint_CreateConnection(NBN_Endpoint *endpoint, NBN_Connection_ID id,
                                                 NBN_Driver_ID driver_id) {
    NBN_Connection *connection = (NBN_Connection *)malloc(sizeof(NBN_Connection));

    connection->handle.id = id;
    connection->handle.user_data = NULL;
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

    for (int i = 0; i < NBN_MAX_PACKET_ENTRIES; i++) {
        connection->packet_send_seq_buffer[i] = 0xFFFFFFFF;
        connection->packet_recv_seq_buffer[i] = 0xFFFFFFFF;
    }

    NBN_ConnectionStats stats = {0};

    connection->stats = stats;

    for (int i = 0; i < NBN_CHANNEL_COUNT; i++) {
        Channel_Init(&connection->channels[i], i, endpoint->channel_modes[i]);
    }

    switch (driver_id) {
#ifdef NBN_UDP
    case NBN_DRIVER_UDP:
        connection->driver = &nbn_udp_driver;
        break;
#endif // NBN_UDP

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

    LogDebug("Received packet %d (conn id: %d, ack: %d, messages count: %d)", packet->header.seq_number,
             connection->handle.id, packet->header.ack, packet->header.messages_count);

    if (NBN_Connection_ProcessReceivedPacket(endpoint, connection, packet, endpoint->time) < 0)
        return NBN_ERROR;

    connection->last_recv_packet_time = endpoint->time;
    connection->downloaded_bytes += packet->size;

    return 0;
}

static void Endpoint_CreateOutgoingMessage(NBN_Endpoint *endpoint, uint8_t type, uint8_t channel_id) {
    NBN_Message *message = &endpoint->write_message;

    message->header = (NBN_MessageHeader){0, 0, type, channel_id};
    message->sender = NULL;
    message->type = NBN_OUTGOING_MESSAGE;

    endpoint->message_writer.position = 0;
}

static int Endpoint_EnqueueOutgoingMessage(NBN_Endpoint *endpoint, NBN_Connection *connection, NBN_Message *message) {
    NBN_Assert(!connection->is_closed || message->header.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE);
    NBN_Assert(!connection->is_stale);

    uint8_t channel_id = message->header.channel_id;
    NBN_Channel *channel = &connection->channels[channel_id];

    NBN_Assert(channel);
    NBN_Assert(channel_id != NBN_RESERVED_RELIABLE_CHANNEL_ID || channel->type == NBN_CHANNEL_RELIABLE);

    LogDebug("Enqueue message of type %d on channel %d", message->header.type, channel->id);

    if (!Channel_AddOutgoingMessage(channel, message)) {
        LogError("Failed to enqueue outgoing message of type %d on channel %d", message->header.type,
                 message->header.channel_id);

        return NBN_ERROR;
    }

    return 0;
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

#pragma endregion /* NBN_Endpoint */

#pragma region Network driver

static void ClientDriver_OnPacketReceived(NBN_Packet *packet);
static void ServerDriver_OnClientConnected(NBN_Connection *);
static int ServerDriver_OnClientPacketReceived(NBN_Packet *);

#pragma endregion /* Network driver */

#pragma region NBN_GameClient

static int GameClient_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static NBN_Client_Event GameClient_HandleEvent(void);
static NBN_Client_Event GameClient_HandleMessageReceivedEvent(void);
static NBN_Connection *CreateServerConnection(NBN_Driver_ID driver_id);

static void InitChannelModes(NBN_ChannelMode channel_modes[NBN_CHANNEL_COUNT]) {
    // channel 0 is always unreliable, channel 1 is always reliable
    // additional channels are reliable by default
    for (int i = 0; i < NBN_CHANNEL_COUNT; i++) {
        if (i == NBN_RESERVED_UNRELIABLE_CHANNEL_ID) {
            channel_modes[i] = NBN_CHANNEL_UNRELIABLE;
        } else {
            channel_modes[i] = NBN_CHANNEL_RELIABLE;
        }
    }
}

void NBN_GameClient_Init(const char *protocol_name, const char *host, uint16_t port) {
    nbn_game_client.config = (NBN_GameClient_Config){.protocol_name = protocol_name, .host = host, .port = port};

    InitChannelModes(nbn_game_client.config.channel_modes);
    nbn_game_client.client_data_writer.position = 0;
}

void NBN_GameClient_SetChannelMode(uint8_t channel_id, NBN_ChannelMode mode) {
    if (channel_id == NBN_RESERVED_RELIABLE_CHANNEL_ID && mode != NBN_CHANNEL_RELIABLE) {
        LogError("Cannot change the mode of channel %d as it's used internally by the library",
                 NBN_RESERVED_RELIABLE_CHANNEL_ID);
        NBN_Abort();
    }

    nbn_game_client.config.channel_modes[channel_id] = mode;
}

NBN_Writer *NBN_GameClient_WriteConnectionRequestData(void) {
    NBN_Writer_Init(&nbn_game_client.client_data_writer, nbn_game_client.endpoint.connection_request_data_buffer,
                    sizeof(nbn_game_client.endpoint.connection_request_data_buffer));

    return &nbn_game_client.client_data_writer;
}

static int StartClientDrivers(const char *host, uint16_t port) {
    int driver_count = 0;

#ifdef NBN_UDP
    nbn_game_client.server_connection = CreateServerConnection(NBN_DRIVER_UDP);

    if (nbn_udp_driver.impl.cli_start(&nbn_game_client, host, port) < 0) {
        LogError("Failed to start driver %s", nbn_udp_driver.name);
        return NBN_ERROR;
    }

    driver_count++;
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__
    nbn_game_client.server_connection = CreateServerConnection(NBN_DRIVER_WEBRTC_EMSCRIPTEN);

    if (nbn_webrtc_em_driver.impl.cli_start(&nbn_game_client, host, port) < 0) {
        LogError("Failed to start driver %s", nbn_webrtc_em_driver.name);
        return NBN_ERROR;
    }

    driver_count++;
#endif // __EMSCRIPTEN__

    return driver_count;
}

int NBN_GameClient_Start(void) {
    NBN_GameClient_Config config = nbn_game_client.config;
    const char *protocol_name = config.protocol_name;
    const char *host = config.host;
    uint16_t port = config.port;
    uint32_t protocol_id = Endpoint_BuildProtocolId(protocol_name);

    Endpoint_Init(&nbn_game_client.endpoint, protocol_id, false, config.channel_modes);

    int driver_count = StartClientDrivers(host, port);

    if (driver_count < 1) {
        LogError("At least one network driver has to be activated");
        NBN_Abort();
    } else if (driver_count > 1) {
        LogError("Only one network driver can be activated for the client");
        NBN_Abort();
    }

    nbn_game_client.is_connected = false;
    nbn_game_client.closed_code = -1;

    unsigned int connection_data_len = nbn_game_client.client_data_writer.position;

    NBN_Writer *writer =
        NBN_GameClient_CreateMessage(NBN_CONNECTION_REQUEST_MESSAGE_TYPE, NBN_RESERVED_RELIABLE_CHANNEL_ID);

    if (connection_data_len > 0) {
        NBN_Assert(connection_data_len <= sizeof(nbn_game_client.endpoint.connection_request_data_buffer));

        NBN_Writer_WriteUInt32(writer, connection_data_len);
        NBN_Writer_WriteBytes(writer, nbn_game_client.endpoint.connection_request_data_buffer, connection_data_len);
    } else {
        NBN_Writer_WriteUInt32(writer, 0);
    }

    if (NBN_GameClient_EnqueueMessage() < 0)
        return NBN_ERROR;

    LogInfo("Started");

    return 0;
}

void NBN_GameClient_Stop(void) {
    // Poll remaining events to clear the event queue
    while (NBN_GameClient_Poll() != NBN_CLIENT_NO_EVENT) {
    }

    if (nbn_game_client.server_connection) {
        if (!nbn_game_client.server_connection->is_closed && !nbn_game_client.server_connection->is_stale) {
            LogInfo("Disconnecting...");

            NBN_GameClient_CreateMessage(NBN_DISCONNECTION_MESSAGE_TYPE, NBN_RESERVED_RELIABLE_CHANNEL_ID);

            if (NBN_GameClient_EnqueueMessage() < 0) {
                LogError("Failed to send disconnection message");
            }

            if (NBN_GameClient_Flush() < 0) {
                LogError("Failed to send packets");
            }

            nbn_game_client.server_connection->is_closed = true;

            LogInfo("Disconnected");
        }

        free(nbn_game_client.server_connection);
        nbn_game_client.server_connection = NULL;
    }

    LogInfo("Stopping all drivers...");

#ifdef NBN_UDP
    nbn_udp_driver.impl.cli_stop(&nbn_game_client);
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__
    nbn_webrtc_em_driver.impl.cli_stop(&nbn_game_client);
#endif // __EMSCRIPTEN__

    nbn_game_client.is_connected = false;
    nbn_game_client.closed_code = -1;
    nbn_game_client.endpoint.server_initial_data_len = 0;

    Endpoint_Deinit(&nbn_game_client.endpoint);

    LogInfo("Stopped");
}

NBN_Reader *NBN_GameClient_ReadServerData(void) {
    NBN_Endpoint *endpoint = &nbn_game_client.endpoint;

    NBN_Reader_Init(&nbn_game_client.server_data_reader, endpoint->server_initial_data_buffer,
                    endpoint->server_initial_data_len);

    return &nbn_game_client.server_data_reader;
}

static int ReadPacketsFromClientDrivers(void) {
#ifdef NBN_UDP
    if (nbn_udp_driver.impl.cli_recv_packets(&nbn_game_client) < 0) {
        LogError("Failed to read packets from driver %s", nbn_udp_driver.name);
        return NBN_ERROR;
    }
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__
    if (nbn_webrtc_em_driver.impl.cli_recv_packets(&nbn_game_client) < 0) {
        LogError("Failed to read packets from driver %s", nbn_webrtc_em_driver.name);
        return NBN_ERROR;
    }
#endif // __EMSCRIPTEN__

    return 0;
}

NBN_Client_Event NBN_GameClient_Poll(void) {
    Endpoint_UpdateTime(&nbn_game_client.endpoint);

    NBN_Endpoint *endpoint = &nbn_game_client.endpoint;

    if (nbn_game_client.server_connection->is_stale)
        return NBN_CLIENT_NO_EVENT;

    if (NBN_EventQueue_IsEmpty(&endpoint->event_queue)) {
        if (NBN_Connection_CheckIfStale(nbn_game_client.server_connection, nbn_game_client.endpoint.time)) {
            nbn_game_client.server_connection->is_stale = true;
            nbn_game_client.is_connected = false;

            LogInfo("Server connection is stale. Disconnected.");

            NBN_Event e;

            e.type = NBN_CLIENT_DISCONNECTED;
            e.data.connection = (NBN_Connection *)NULL;

            if (!NBN_EventQueue_Enqueue(&endpoint->event_queue, e))
                return NBN_ERROR;
        } else {
            if (ReadPacketsFromClientDrivers() < 0) {
                return NBN_ERROR;
            }

            NBN_Connection *server_conn = nbn_game_client.server_connection;

            for (unsigned int i = 0; i < NBN_CHANNEL_COUNT; i++) {
                NBN_Channel *channel = &server_conn->channels[i];

                NBN_Message *msg;

                while ((msg = Channel_GetNextRecvedMessage(channel)) != NULL) {
                    LogDebug("Got message %d of type %d from channel %d", msg->header.id, msg->header.type,
                             channel->id);

                    if (GameClient_ProcessReceivedMessage(msg, server_conn) < 0) {
                        LogError("Failed to process received message");

                        return NBN_ERROR;
                    }
                }
            }

            Connection_UpdateAverageDownloadBandwidth(server_conn, nbn_game_client.endpoint.time);

            server_conn->last_read_packets_time = nbn_game_client.endpoint.time;
        }
    }

    bool ret = NBN_EventQueue_Dequeue(&endpoint->event_queue, &nbn_game_client.last_event);

    return ret ? GameClient_HandleEvent() : NBN_CLIENT_NO_EVENT;
}

int NBN_GameClient_Flush(void) {
    return NBN_Connection_FlushChannels(&nbn_game_client.endpoint, nbn_game_client.server_connection,
                                        nbn_game_client.endpoint.protocol_id, nbn_game_client.endpoint.time);
}

NBN_Writer *NBN_GameClient_CreateMessage(uint8_t type, uint8_t channel_id) {
    NBN_Assert(channel_id < NBN_CHANNEL_COUNT);

    NBN_Endpoint *endpoint = &nbn_game_client.endpoint;
    NBN_Writer *writer = &endpoint->message_writer;

    NBN_Writer_Init(writer, endpoint->write_message.data, sizeof(endpoint->write_message.data));
    Endpoint_CreateOutgoingMessage(endpoint, type, channel_id);

    return writer;
}

NBN_Writer *NBN_GameClient_CreateReliableMessage(uint8_t type) {
    return NBN_GameClient_CreateMessage(type, NBN_RESERVED_RELIABLE_CHANNEL_ID);
}

NBN_Writer *NBN_GameClient_CreateUnreliableMessage(uint8_t type) {
    return NBN_GameClient_CreateMessage(type, NBN_RESERVED_UNRELIABLE_CHANNEL_ID);
}

int NBN_GameClient_EnqueueMessage(void) {
    NBN_Endpoint *endpoint = &nbn_game_client.endpoint;
    NBN_Message *message = &endpoint->write_message;

    message->header.length = endpoint->message_writer.position;

    if (Endpoint_EnqueueOutgoingMessage(endpoint, nbn_game_client.server_connection, message) < 0) {
        LogError("Failed to create outgoing message");

        return NBN_ERROR;
    }

    return 0;
}

NBN_Reader *NBN_GameClient_ReadMessage(void) {
    NBN_Assert(nbn_game_client.last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    NBN_MessageInfo msg_info = nbn_game_client.last_event.data.message_info;
    NBN_Assert(msg_info.length > 0 && msg_info.data != NULL);

    NBN_Reader *reader = &nbn_game_client.endpoint.message_reader;

    NBN_Reader_Init(reader, msg_info.data, msg_info.length);

    return reader;
}

static NBN_Connection *CreateServerConnection(NBN_Driver_ID driver_id) {
    NBN_Connection *server_connection = Endpoint_CreateConnection(&nbn_game_client.endpoint, 0, driver_id);

    nbn_game_client.server_connection = server_connection;

    return server_connection;
}

NBN_MessageInfo NBN_GameClient_GetMessageInfo(void) {
    NBN_Assert(nbn_game_client.last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    return nbn_game_client.last_event.data.message_info;
}

NBN_ConnectionStats NBN_GameClient_GetStats(void) { return nbn_game_client.server_connection->stats; }

int NBN_GameClient_GetServerCloseCode(void) { return nbn_game_client.closed_code; }

bool NBN_GameClient_IsConnected(void) { return nbn_game_client.is_connected; }

static int GameClient_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *server_connection) {
    NBN_Assert(nbn_game_client.server_connection == server_connection);

    NBN_Event ev;

    ev.type = NBN_CLIENT_MESSAGE_RECEIVED;

    NBN_MessageInfo msg_info;

    msg_info.type = message->header.type;
    msg_info.channel_id = message->header.channel_id;
    msg_info.length = message->header.length;
    msg_info.sender = (NBN_ConnectionHandle *)server_connection;
    msg_info.data = message->data;

    ev.data.message_info = msg_info;

    if (!NBN_EventQueue_Enqueue(&nbn_game_client.endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static NBN_Client_Event GameClient_HandleEvent(void) {
    switch (nbn_game_client.last_event.type) {
    case NBN_CLIENT_MESSAGE_RECEIVED:
        return GameClient_HandleMessageReceivedEvent();

    default:
        return nbn_game_client.last_event.type;
    }
}

static NBN_Client_Event GameClient_HandleMessageReceivedEvent(void) {
    NBN_MessageInfo message_info = nbn_game_client.last_event.data.message_info;
    NBN_Endpoint *endpoint = &nbn_game_client.endpoint;

    int ret = NBN_CLIENT_NO_EVENT;

    if (message_info.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE) {
        nbn_game_client.is_connected = false;
        NBN_Reader *reader = NBN_GameClient_ReadMessage();

        if (NBN_Reader_ReadInt32(reader, &nbn_game_client.closed_code) < 0) {
            LogError("Failed to read code from client closed message");

            return NBN_ERROR;
        }

        ret = NBN_CLIENT_DISCONNECTED;
    } else if (message_info.type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE) {
        if (message_info.length < 4) {
            LogError("Accept message invalid length");

            return NBN_ERROR;
        }

        NBN_Reader *reader = NBN_GameClient_ReadMessage();
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
        nbn_game_client.is_connected = true;
        ret = NBN_CLIENT_CONNECTED;
    } else {
        ret = NBN_CLIENT_MESSAGE_RECEIVED;
    }

    return ret;
}

#pragma endregion /* NBN_GameClient */

#pragma region Game client driver

static void ClientDriver_OnPacketReceived(NBN_Packet *packet) {
    if (Endpoint_ProcessReceivedPacket(&nbn_game_client.endpoint, packet, nbn_game_client.server_connection) < 0) {
        // packets from the server should always be valid
        LogError("Received invalid packet from server");
        NBN_Abort();
    }
}

#pragma endregion /* Game Client driver */

#pragma region NBN_GameServer

static int GameServer_EnqueueMessageFor(NBN_Connection *client, NBN_Message *message);
static void GameServer_AddClient(NBN_Connection *);
static int GameServer_CloseClientWithCode(NBN_Connection *client, int code, bool disconnection);
static void GameServer_AddClientToClosedList(NBN_Connection *client);
static int GameServer_ProcessReceivedMessage(NBN_Message *, NBN_Connection *);
static int GameServer_CloseStaleClientConnections(void);
static void GameServer_RemoveClosedClientConnections(void);
static bool GameServer_HandleEvent(NBN_Server_Event *ev);
static bool GameServer_HandleMessageReceivedEvent(NBN_Server_Event *ev);

void NBN_GameServer_Init(const char *protocol_name, uint16_t port) {
    nbn_game_server.config = (NBN_GameServer_Config){.protocol_name = protocol_name, .port = port};

    InitChannelModes(nbn_game_server.config.channel_modes);
    nbn_game_server.server_data_writer.position = 0;
    hmdefault(nbn_game_server.clients, NULL);
}

void NBN_GameServer_SetChannelMode(uint8_t channel_id, NBN_ChannelMode mode) {
    if (channel_id == NBN_RESERVED_RELIABLE_CHANNEL_ID && mode != NBN_CHANNEL_RELIABLE) {
        LogError("Cannot change the mode of channel %d as it's used internally by the library",
                 NBN_RESERVED_RELIABLE_CHANNEL_ID);
        NBN_Abort();
    }

    nbn_game_server.config.channel_modes[channel_id] = mode;
}

static int StartServerDrivers(uint16_t port) {
    int driver_count = 0;

#ifdef NBN_UDP
    if (nbn_udp_driver.impl.serv_start(&nbn_game_server, port) < 0) {
        LogError("Failed to start driver %s", nbn_udp_driver.name);
        return NBN_ERROR;
    }

    driver_count++;
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__
    if (nbn_webrtc_em_driver.impl.serv_start(&nbn_game_server, port) < 0) {
        LogError("Failed to start driver %s", nbn_webrtc_em_driver.name);
        return NBN_ERROR;
    }

    driver_count++;
#endif // __EMSCRIPTEN__

    return driver_count;
}

int NBN_GameServer_Start(void) {
    NBN_GameServer_Config config = nbn_game_server.config;
    const char *protocol_name = config.protocol_name;
    uint16_t port = config.port;
    uint32_t protocol_id = Endpoint_BuildProtocolId(protocol_name);

    Endpoint_Init(&nbn_game_server.endpoint, protocol_id, true, config.channel_modes);

    nbn_game_server.closed_clients_head = NULL;

    int driver_count = StartServerDrivers(port);

    if (driver_count < 1) {
        LogError("At least one network driver has to be activated");
        NBN_Abort();
    }

    LogInfo("Started (channel count: %d)", NBN_CHANNEL_COUNT);

    return 0;
}

void NBN_GameServer_Stop(void) {
    // Poll remaning events to clear the event queue
    while (NBN_GameServer_Poll() != NBN_SERVER_NO_EVENT) {
    }

    for (unsigned int i = 0; i < hmlen(nbn_game_server.clients); i++) {
        NBN_Connection *conn = nbn_game_server.clients[i].value;

        conn->driver->impl.serv_cleanup_connection(&nbn_game_server, conn);
        free(conn);
    }

    hmfree(nbn_game_server.clients);

#ifdef NBN_UDP
    nbn_udp_driver.impl.serv_stop(&nbn_game_server);
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__
    nbn_webrtc_em_driver.impl.serv_stop(&nbn_game_server);
#endif // __EMSCRIPTEN__

    // Free closed clients list
    NBN_ConnectionListNode *current = nbn_game_server.closed_clients_head;

    while (current) {
        NBN_ConnectionListNode *next = current->next;

        free(current);

        current = next;
    }

    nbn_game_server.closed_clients_head = NULL;
    Endpoint_Deinit(&nbn_game_server.endpoint);

    LogInfo("Stopped");
}

static NBN_Connection_ID NBN_BuildConnectionHash(NBN_Connection_ID id, NBN_Driver_ID driver_id) {
    NBN_Assert(id <= UINT64_MAX - 0xFF);
    uint8_t driver_byte = driver_id;

    return ((NBN_Connection_ID)driver_byte << 56) | id;
}

NBN_ConnectionHandle *NBN_GameServer_FindConnection(NBN_Connection_ID id) {
    return (NBN_ConnectionHandle *)hmget(nbn_game_server.clients, id);
}

unsigned int NBN_GameServer_GetClientCount(void) { return hmlen(nbn_game_server.clients); }

NBN_ConnectionHandle *NBN_GameServer_GetNextClient(NBN_Client_Iterator *it) {
    for (; *it < hmlen(nbn_game_server.clients);) {
        NBN_Connection *conn = (NBN_Connection *)nbn_game_server.clients[*it].value;

        (*it)++;

        if (conn->is_accepted) {
            return (NBN_ConnectionHandle *)conn;
        }
    }

    return NULL;
}

static void ReadPacketsFromServerDrivers(void) {
#ifdef NBN_UDP
    if (nbn_udp_driver.impl.serv_recv_packets(&nbn_game_server) < 0) {
        LogError("Failed to read packets from driver %s", nbn_udp_driver.name);
    }
#endif // NBN_UDP

#ifdef __EMSCRIPTEN__
    if (nbn_webrtc_em_driver.impl.serv_recv_packets(&nbn_game_server) < 0) {
        LogError("Failed to read packets from driver %s", nbn_webrtc_em_driver.name);
    }
#endif // __EMSCRIPTEN__
}

NBN_Server_Event NBN_GameServer_Poll(void) {
    Endpoint_UpdateTime(&nbn_game_server.endpoint);

    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;

    if (NBN_EventQueue_IsEmpty(&endpoint->event_queue)) {
        if (GameServer_CloseStaleClientConnections() < 0)
            return NBN_ERROR;

        ReadPacketsFromServerDrivers();

        nbn_game_server.stats.download_bandwidth = 0;

        for (unsigned int i = 0; i < hmlen(nbn_game_server.clients); i++) {
            NBN_Connection *client = nbn_game_server.clients[i].value;

            for (unsigned int i = 0; i < NBN_CHANNEL_COUNT; i++) {
                NBN_Channel *channel = &client->channels[i];

                if (channel) {
                    NBN_Message *msg;

                    while ((msg = Channel_GetNextRecvedMessage(channel)) != NULL) {
                        if (GameServer_ProcessReceivedMessage(msg, client) < 0) {
                            LogError("Failed to process received message");

                            return NBN_ERROR;
                        }
                    }
                }
            }

            if (!client->is_closed)
                Connection_UpdateAverageDownloadBandwidth(client, endpoint->time);

            nbn_game_server.stats.download_bandwidth += client->stats.download_bandwidth;
            client->last_read_packets_time = endpoint->time;
        }

        GameServer_RemoveClosedClientConnections();
    }

    NBN_Server_Event ev;

    while (NBN_EventQueue_Dequeue(&endpoint->event_queue, &nbn_game_server.last_event)) {
        if (GameServer_HandleEvent(&ev)) {
            return ev;
        }
    }

    return NBN_SERVER_NO_EVENT;
}

int NBN_GameServer_Flush(void) {
    nbn_game_server.stats.upload_bandwidth = 0;

    GameServer_RemoveClosedClientConnections();

    for (unsigned int i = 0; i < hmlen(nbn_game_server.clients); i++) {
        NBN_Connection *client = nbn_game_server.clients[i].value;

        NBN_Assert(!(client->is_closed && client->is_stale));

        if (!client->is_stale &&
            NBN_Connection_FlushChannels(&nbn_game_server.endpoint, client, nbn_game_server.endpoint.protocol_id,
                                         nbn_game_server.endpoint.time) < 0) {
            return NBN_ERROR;
        }

        nbn_game_server.stats.upload_bandwidth += client->stats.upload_bandwidth;
    }

    return 0;
}

static NBN_Connection *CreateClientConnection(NBN_Driver_ID driver_id, NBN_Connection_ID conn_id) {
    // write the driver ID to the first byte of the connection ID to avoid collisions between drivers
    conn_id = NBN_BuildConnectionHash(conn_id, driver_id);
    NBN_Connection *client = Endpoint_CreateConnection(&nbn_game_server.endpoint, conn_id, driver_id);

    return client;
}

int NBN_GameServer_CloseClientWithCode(NBN_ConnectionHandle *conn, int code) {
    return GameServer_CloseClientWithCode(HANDLE_TO_CONN(conn), code, false);
}

int NBN_GameServer_CloseClient(NBN_ConnectionHandle *conn) {
    return GameServer_CloseClientWithCode(HANDLE_TO_CONN(conn), -1, false);
}

NBN_Writer *NBN_GameServer_CreateMessage(uint8_t type, uint8_t channel_id) {
    NBN_Assert(channel_id < NBN_CHANNEL_COUNT);
    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;
    NBN_Writer *writer = &endpoint->message_writer;

    NBN_Writer_Init(writer, endpoint->write_message.data, sizeof(endpoint->write_message.data));
    Endpoint_CreateOutgoingMessage(endpoint, type, channel_id);

    return writer;
}

NBN_Writer *NBN_GameServer_CreateReliableMessage(uint8_t type) {
    return NBN_GameServer_CreateMessage(type, NBN_RESERVED_RELIABLE_CHANNEL_ID);
}

NBN_Writer *NBN_GameServer_CreateUnreliableMessage(uint8_t type) {
    return NBN_GameServer_CreateMessage(type, NBN_RESERVED_UNRELIABLE_CHANNEL_ID);
}

int NBN_GameServer_EnqueueMessageFor(NBN_ConnectionHandle *conn) {
    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;
    NBN_Message *message = &endpoint->write_message;
    message->header.length = endpoint->message_writer.position;

    int ret = GameServer_EnqueueMessageFor(HANDLE_TO_CONN(conn), message);

    return ret;
}

int NBN_GameServer_EnqueueBroadcastMessage(void) {
    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;
    NBN_Message *message = &endpoint->write_message;
    message->header.length = endpoint->message_writer.position;

    int ret = 0;

    for (unsigned int i = 0; i < hmlen(nbn_game_server.clients); i++) {
        NBN_Connection *conn = nbn_game_server.clients[i].value;

        if (!conn->is_accepted || conn->is_closed)
            continue;

        if (GameServer_EnqueueMessageFor(conn, &endpoint->write_message) < 0) {
            LogError("Failed to send message to client %d when broadcasting", conn->handle.id);
            ret = NBN_ERROR;
            break;
        }
    }

    return ret;
}

NBN_Reader *NBN_GameServer_ReadMessage(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_CLIENT_MESSAGE_RECEIVED);

    NBN_MessageInfo msg_info = nbn_game_server.last_event.data.message_info;
    NBN_Assert(msg_info.length > 0 && msg_info.data != NULL);

    NBN_Reader *reader = &nbn_game_server.endpoint.message_reader;

    NBN_Reader_Init(reader, msg_info.data, msg_info.length);

    return reader;
}

NBN_Writer *NBN_GameServer_WriteConnectionData(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(nbn_game_server.last_event.data.connection != NULL);

    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;

    NBN_Writer_Init(&nbn_game_server.server_data_writer, endpoint->server_initial_data_buffer,
                    sizeof(endpoint->server_initial_data_buffer));

    return &nbn_game_server.server_data_writer;
}

int NBN_GameServer_AcceptIncomingConnection(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(nbn_game_server.last_event.data.connection != NULL);

    unsigned data_length = nbn_game_server.server_data_writer.position;
    NBN_Connection *client = nbn_game_server.last_event.data.connection;
    NBN_Writer *writer =
        NBN_GameServer_CreateMessage(NBN_CLIENT_ACCEPTED_MESSAGE_TYPE, NBN_RESERVED_RELIABLE_CHANNEL_ID);

    if (data_length > 0) {
        NBN_Assert(data_length <= sizeof(nbn_game_server.endpoint.server_initial_data_buffer));

        NBN_Writer_WriteUInt32(writer, data_length);
        NBN_Writer_WriteBytes(writer, nbn_game_server.endpoint.server_initial_data_buffer, data_length);
    } else {
        NBN_Writer_WriteUInt32(writer, 0);
    }

    if (NBN_GameServer_EnqueueMessageFor((NBN_ConnectionHandle *)client) < 0)
        return NBN_ERROR;

    client->is_accepted = true;

    LogInfo("Client %d has been accepted into the server", client->handle.id);

    return 0;
}

int NBN_GameServer_RejectIncomingConnectionWithCode(int code) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(nbn_game_server.last_event.data.connection != NULL);

    NBN_Connection *conn = nbn_game_server.last_event.data.connection;
    LogDebug("Rejecting incoming connection %d (code: %d)", conn->handle.id, code);

    return GameServer_CloseClientWithCode(conn, code, false);
}

int NBN_GameServer_RejectIncomingConnection(void) { return NBN_GameServer_RejectIncomingConnectionWithCode(-1); }

NBN_ConnectionHandle *NBN_GameServer_GetIncomingConnection(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_NEW_CONNECTION);
    NBN_Assert(nbn_game_server.last_event.data.connection != NULL);

    return (NBN_ConnectionHandle *)nbn_game_server.last_event.data.connection;
}

NBN_Reader *NBN_GameServer_ReadConnectionRequestData(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_NEW_CONNECTION);

    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;

    NBN_Reader_Init(&nbn_game_server.client_data_reader, endpoint->connection_request_data_buffer,
                    endpoint->client_connection_request_data_len);

    return &nbn_game_server.client_data_reader;
}

NBN_DisconnectionInfo NBN_GameServer_GetDisconnectionInfo(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_DISCONNECTION);

    return nbn_game_server.last_event.data.disconnection;
}

NBN_MessageInfo NBN_GameServer_GetMessageInfo(void) {
    NBN_Assert(nbn_game_server.last_event.type == NBN_SERVER_MESSAGE_RECEIVED);

    return nbn_game_server.last_event.data.message_info;
}

NBN_GameServerStats NBN_GameServer_GetStats(void) { return nbn_game_server.stats; }

static int GameServer_EnqueueMessageFor(NBN_Connection *client, NBN_Message *message) {
    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;

    /* Only NBN_CLIENT_ACCEPTED_MESSAGE_TYPE and NBN_CLIENT_CLOSED_MESSAGE_TYPE messages can be sent to an
     * unaccapted client */
    NBN_Assert(client->is_accepted || message->header.type == NBN_CLIENT_ACCEPTED_MESSAGE_TYPE ||
               message->header.type == NBN_CLIENT_CLOSED_MESSAGE_TYPE);

    if (Endpoint_EnqueueOutgoingMessage(endpoint, client, message) < 0) {
        LogError("Failed to create outgoing message for client %d", client->handle.id);

        /* Do not close the client if we failed to send the close client message to avoid infinite loops */
        if (message->header.type != NBN_CLIENT_CLOSED_MESSAGE_TYPE) {
            GameServer_CloseClientWithCode(client, -1, false);

            return NBN_ERROR;
        }
    }

    return 0;
}

static void GameServer_AddClient(NBN_Connection *client) {
    NBN_Assert(hmgeti(nbn_game_server.clients, client->handle.id) == -1);

    hmput(nbn_game_server.clients, client->handle.id, client);
    LogDebug("New client %llu", client->handle.id);
}

static int GameServer_CloseClientWithCode(NBN_Connection *client, int code, bool disconnection) {
    if (!client->is_closed && client->is_accepted) {
        if (!disconnection) {
            NBN_Event e;

            e.type = NBN_CLIENT_DISCONNECTED;
            e.data.disconnection = (NBN_DisconnectionInfo){client->handle.id, client->handle.user_data};

            if (!NBN_EventQueue_Enqueue(&nbn_game_server.endpoint.event_queue, e))
                return NBN_ERROR;
        }
    }

    if (client->is_stale) {
        LogDebug("Closing stale connection %d", client->handle.id);

        GameServer_AddClientToClosedList(client);
        client->is_closed = true;

        return 0;
    }

    LogDebug("Closing active connection %d (will send a disconnection message)", client->handle.id);

    GameServer_AddClientToClosedList(client);
    client->is_closed = true;

    if (!disconnection) {
        LogDebug("Send close message for client %d (code: %d)", client->handle.id, code);

        NBN_Writer *writer =
            NBN_GameServer_CreateMessage(NBN_CLIENT_CLOSED_MESSAGE_TYPE, NBN_RESERVED_RELIABLE_CHANNEL_ID);
        NBN_Writer_WriteInt32(writer, code);
        NBN_GameServer_EnqueueMessageFor((NBN_ConnectionHandle *)client);
    }

    return 0;
}

static void GameServer_AddClientToClosedList(NBN_Connection *client) {
    if (client->is_closed)
        return;

    // TODO: do we need to use a linked list, maybe use stb dynamic array?
    NBN_ConnectionListNode *node = (NBN_ConnectionListNode *)malloc(sizeof(NBN_ConnectionListNode));

    node->conn = client;
    node->next = NULL;

    if (nbn_game_server.closed_clients_head == NULL) {
        // list is empty
        nbn_game_server.closed_clients_head = node;
        node->prev = NULL;
    } else {
        // list is not empty, add node at the end
        NBN_ConnectionListNode *tail = nbn_game_server.closed_clients_head;

        while (tail->next != NULL)
            tail = tail->next;

        node->prev = tail;
        tail->next = node;
    }
}

static int GameServer_ProcessReceivedMessage(NBN_Message *message, NBN_Connection *client) {
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

    if (!NBN_EventQueue_Enqueue(&nbn_game_server.endpoint.event_queue, ev))
        return NBN_ERROR;

    return 0;
}

static int GameServer_CloseStaleClientConnections(void) {
    for (unsigned int i = 0; i < hmlen(nbn_game_server.clients); i++) {
        NBN_Connection *client = nbn_game_server.clients[i].value;

        if (!client->is_stale && NBN_Connection_CheckIfStale(client, nbn_game_server.endpoint.time)) {
            LogInfo("Client %lld connection is stale, closing it.", client->handle.id);

            client->is_stale = true;

            if (GameServer_CloseClientWithCode(client, -1, false) < 0)
                return NBN_ERROR;
        }
    }

    return 0;
}

static void GameServer_RemoveClosedClientConnections(void) {
    NBN_ConnectionListNode *current = nbn_game_server.closed_clients_head;

    while (current) {
        NBN_ConnectionListNode *prev = current->prev;
        NBN_ConnectionListNode *next = current->next;
        NBN_Connection *client = current->conn;

        NBN_Assert(client->handle.id > 0);

        if (client->is_stale) {
            LogDebug("Remove closed client connection (ID: %d)", client->handle.id);

            client->driver->impl.serv_cleanup_connection(&nbn_game_server,
                                                         client); // Notify the driver to clean up the connection

            int ret = hmdel(nbn_game_server.clients, client->handle.id);
            NBN_Assert(ret == 1);

            // Destroy the connection

            free(client);

            // Remove the connection from the closed clients list

            free(current);

            if (current == nbn_game_server.closed_clients_head) {
                // delete the head of the list
                NBN_ConnectionListNode *new_head = next;

                if (new_head) {
                    new_head->prev = NULL;
                }

                nbn_game_server.closed_clients_head = new_head;
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

static bool GameServer_HandleEvent(NBN_Server_Event *ev) {
    if (nbn_game_server.last_event.type == NBN_SERVER_MESSAGE_RECEIVED) {
        return GameServer_HandleMessageReceivedEvent(ev);
    }

    *ev = nbn_game_server.last_event.type;
    return true;
}

// TODO: big ass function
static bool GameServer_HandleMessageReceivedEvent(NBN_Server_Event *ev) {
    NBN_Event *last_event = &nbn_game_server.last_event;
    NBN_MessageInfo message_info = last_event->data.message_info;
    NBN_Connection *sender = HANDLE_TO_CONN(message_info.sender);
    NBN_Endpoint *endpoint = &nbn_game_server.endpoint;

    if (sender->is_closed || sender->is_stale) {
        return false;
    }

    if (message_info.type == NBN_DISCONNECTION_MESSAGE_TYPE) {
        LogInfo("Received a disconnection request from client %d (user_data: %p)", sender->handle.id,
                sender->handle.user_data);

        if (GameServer_CloseClientWithCode(sender, -1, true) < 0) {
            *ev = NBN_ERROR;
            return true;
        }

        sender->is_stale = true;

        last_event->type = NBN_SERVER_DISCONNECTION;
        last_event->data.disconnection = (NBN_DisconnectionInfo){sender->handle.id, sender->handle.user_data};

        GameServer_RemoveClosedClientConnections();

        *ev = NBN_SERVER_DISCONNECTION;
        return true;
    }

    if (message_info.type != NBN_CONNECTION_REQUEST_MESSAGE_TYPE) {
        nbn_game_server.server_data_writer.position = 0;

        *ev = NBN_SERVER_MESSAGE_RECEIVED;
        return true;
    }

    // at this point we know it's a connection request
    NBN_Assert(message_info.type == NBN_CONNECTION_REQUEST_MESSAGE_TYPE);

    if (message_info.length < 4) {
        LogError("Connection request invalid length");

        *ev = NBN_ERROR;
        return true;
    }

    NBN_Reader *reader = NBN_GameServer_ReadMessage();
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

        if (NBN_Reader_ReadBytes(reader, nbn_game_server.endpoint.connection_request_data_buffer, data_length) < 0) {
            LogError("Failed to read client data");

            *ev = NBN_ERROR;
            return true;
        }
    }

    nbn_game_server.endpoint.client_connection_request_data_len = data_length;

    NBN_Event e;

    e.type = NBN_SERVER_NEW_CONNECTION;
    e.data.connection = sender;

    if (!NBN_EventQueue_Enqueue(&endpoint->event_queue, e)) {
        *ev = NBN_ERROR;
        return true;
    }

    *ev = NBN_SERVER_NO_EVENT;
    return true;
}

#pragma endregion /* NBN_GameServer */

#pragma region Game server driver

static void ServerDriver_OnClientConnected(NBN_Connection *client) { GameServer_AddClient(client); }

static int ServerDriver_OnClientPacketReceived(NBN_Packet *packet) {
    if (Endpoint_ProcessReceivedPacket(&nbn_game_server.endpoint, packet, packet->sender) < 0) {
        LogError("An error occured while processing packet from client %d, closing the client",
                 packet->sender->handle.id);

        return GameServer_CloseClientWithCode(packet->sender, -1, false);
    }

    return 0;
}

#pragma endregion /* Game server driver */

/**
 * ======================= DRIVER IMPLEMENTATIONS ======================= *
 */

#ifdef NBN_UDP

#ifdef NBN_PLATFORM_WINDOWS

static char err_msg[32];

#endif

static int UDP_InitSocket(void);
static void UDP_DeinitSocket(void);
static int UDP_BindSocket(uint16_t);
static char *UDP_GetLastErrorMessage(void);

static int UDP_InitSocket(void) {
#ifdef NBN_PLATFORM_WINDOWS
    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err < 0) {
        LogError("WSAStartup() failed");

        return NBN_ERROR;
    }
#endif

    if ((nbn_udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
        return NBN_ERROR;

#if defined(NBN_PLATFORM_WINDOWS)
    DWORD non_blocking = 1;

    if (ioctlsocket(nbn_udp_sock, FIONBIO, &non_blocking) != 0) {
        LogError("ioctlsocket() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }
#elif defined(NBN_PLATFORM_MAC) || defined(NBN_PLATFORM_UNIX)
    int non_blocking = 1;

    if (fcntl(nbn_udp_sock, F_SETFL, O_NONBLOCK, non_blocking) < 0) {
        LogError("fcntl() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }
#endif

    return 0;
}

static void UDP_DeinitSocket(void) {
    closesocket(nbn_udp_sock);

#ifdef NBN_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static int UDP_BindSocket(uint16_t port) {
    SOCKADDR_IN sin;

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(nbn_udp_sock, (SOCKADDR *)&sin, sizeof(sin)) < 0) {
        LogError("bind() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

static NBN_Connection_ID UDP_BuildConnectionID(NBN_IPAddress address) {
    return ((NBN_Connection_ID)address.host << 2) | address.port;
}

static NBN_Connection *UDP_FindOrCreateClientConnectionByAddress(NBN_IPAddress address) {
    NBN_Connection_ID conn_id = UDP_BuildConnectionID(address);
    conn_id = NBN_BuildConnectionHash(conn_id, NBN_DRIVER_UDP);
    NBN_ConnectionHandle *handle = NBN_GameServer_FindConnection(conn_id);

    if (handle) {
        return HANDLE_TO_CONN(handle);
    }

    // this is a new connection
    NBN_Connection *conn = CreateClientConnection(NBN_DRIVER_UDP, conn_id);
    conn->driver_data.udp.ip_address = address;

    LogInfo("New UDP connection (id: %llu, addr: %d, port: %d)", conn->handle.id, address.host, address.port);

    ServerDriver_OnClientConnected(conn);
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

int UDP_Server_Start(NBN_GameServer *server, uint16_t port) {
    if (UDP_InitSocket() < 0)
        return NBN_ERROR;

    if (UDP_BindSocket(port) < 0)
        return NBN_ERROR;

    return 0;
}

void UDP_Server_Stop(NBN_GameServer *server) { UDP_DeinitSocket(); }

int UDP_Server_RecvPackets(NBN_GameServer *server) {
    static NBN_Packet packet = {0};
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    while (true) {
        int bytes = recvfrom(nbn_udp_sock, (char *)packet.buffer, sizeof(packet.buffer), 0, (SOCKADDR *)&src_addr,
                             &src_addr_len);

        if (bytes <= 0)
            break;

        if (bytes <= NBN_PACKET_HEADER_SIZE)
            continue;

        if (NBN_Packet_InitRead(&packet, server->endpoint.protocol_id, bytes) < 0) {
            LogDebug("Discarded invalid packet");
            continue;
        }

        NBN_IPAddress ip_address;
        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

        LogDebug("Received valid UDP packet from %d:%d", ip_address.host, ip_address.port);

        packet.sender = UDP_FindOrCreateClientConnectionByAddress(ip_address);

        ServerDriver_OnClientPacketReceived(&packet);
    }

    return 0;
}

static void UDP_Server_CleanupConnection(NBN_GameServer *server, NBN_Connection *connection) {}

static int UDP_Server_SendPacketTo(NBN_GameServer *server, NBN_Packet *packet, NBN_Connection *connection) {
    NBN_IPAddress dest_address = connection->driver_data.udp.ip_address;
    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(dest_address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_address.port);

    if (sendto(nbn_udp_sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr,
               sizeof(dest_addr)) == SOCKET_ERROR) {
        LogError("sendto() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

int UDP_Client_Start(NBN_GameClient *client, const char *host, uint16_t port) {
    NBN_IPAddress *ip_address = &client->server_connection->driver_data.udp.ip_address;

    UDP_ParseIpAddress(host, port, ip_address);

    if (UDP_InitSocket() < 0)
        return NBN_ERROR;

    if (UDP_BindSocket(0) < 0)
        return NBN_ERROR;

    return 0;
}

void UDP_Client_Stop(NBN_GameClient *client) { UDP_DeinitSocket(); }

int UDP_Client_RecvPackets(NBN_GameClient *client) {
    NBN_IPAddress server_address = client->server_connection->driver_data.udp.ip_address;
    static NBN_Packet packet = {0};
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    while (true) {
        int bytes = recvfrom(nbn_udp_sock, (char *)packet.buffer, sizeof(packet.buffer), 0, (SOCKADDR *)&src_addr,
                             &src_addr_len);

        if (bytes <= 0)
            break;

        if (bytes < NBN_PACKET_HEADER_SIZE)
            continue;

        uint32_t host = ntohl(src_addr.sin_addr.s_addr);
        uint16_t port = ntohs(src_addr.sin_port);

        if (host != server_address.host || port != server_address.port)
            continue;

        if (NBN_Packet_InitRead(&packet, client->endpoint.protocol_id, bytes) < 0) {
            LogDebug("Discarded invalid packet");
            continue;
        }

        packet.sender = client->server_connection;

        ClientDriver_OnPacketReceived(&packet);
    }

    return 0;
}

static int UDP_Client_SendPacket(NBN_GameClient *client, NBN_Packet *packet) {
    NBN_IPAddress server_address = client->server_connection->driver_data.udp.ip_address;
    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(server_address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_address.port);

    if (sendto(nbn_udp_sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr,
               sizeof(dest_addr)) == SOCKET_ERROR) {
        LogError("sendto() failed: %s", UDP_GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

#endif // NBN_UDP

#ifdef __EMSCRIPTEN__

/**
 * JS API
 *
 * See net_drivers/webrtc/js for the implementation of these functions.
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

void NBN_WebRTC_SetConfig(NBN_WebRTC_Config config) { nbn_wrtc_cfg = config; }

static int WebRTC_Server_Start(NBN_GameServer *server, uint16_t port) {
    __js_game_server_init(server->endpoint.protocol_id, nbn_wrtc_cfg.enable_tls, nbn_wrtc_cfg.key_path,
                          nbn_wrtc_cfg.cert_path);

    if (__js_game_server_start(port) < 0)
        return -1;

    return 0;
}

static void WebRTC_Server_Stop(NBN_GameServer *server) { __js_game_server_stop(); }

static int WebRTC_Server_RecvPackets(NBN_GameServer *server) {
    static NBN_Packet packet = {0};
    uint32_t peer_id;
    unsigned int len;

    while ((len = __js_game_server_dequeue_packet(&peer_id, packet.buffer)) > 0) {
        NBN_Connection_ID conn_id = NBN_BuildConnectionHash(peer_id, NBN_DRIVER_WEBRTC_EMSCRIPTEN);
        NBN_ConnectionHandle *handle = NBN_GameServer_FindConnection(conn_id);
        NBN_Connection *conn = NULL;

        if (handle == NULL) {
            LogInfo("Peer %d has connected", peer_id);

            conn = CreateClientConnection(NBN_DRIVER_WEBRTC_EMSCRIPTEN, conn_id);
            conn->driver_data.peer_id = peer_id;

            ServerDriver_OnClientConnected(conn);
        } else {
            conn = HANDLE_TO_CONN(handle);
        }

        if (NBN_Packet_InitRead(&packet, server->endpoint.protocol_id, len) < 0)
            continue;

        packet.sender = conn;

        ServerDriver_OnClientPacketReceived(&packet);
    }

    return 0;
}

static void WebRTC_Server_CleanupConnection(NBN_GameServer *server, NBN_Connection *conn) {
    assert(conn != NULL);

    __js_game_server_close_client_peer(conn->driver_data.peer_id);
}

static int WebRTC_Server_SendPacketTo(NBN_GameServer *server, NBN_Packet *packet, NBN_Connection *conn) {
    return __js_game_server_send_packet_to(packet->buffer, packet->size, conn->driver_data.peer_id);
}

static int WebRTC_Client_Start(NBN_GameClient *client, const char *host, uint16_t port) {
    __js_game_client_init(client->endpoint.protocol_id, nbn_wrtc_cfg.enable_tls);

    int res;

    if ((res = __js_game_client_start(host, port)) < 0)
        return NBN_ERROR;

    return 0;
}

static void WebRTC_Client_Stop(NBN_GameClient *client) { __js_game_client_close(); }

static int WebRTC_Client_RecvPackets(NBN_GameClient *client) {
    static NBN_Packet packet = {0};
    unsigned int len;

    while ((len = __js_game_client_dequeue_packet(packet.buffer)) > 0) {
        if (NBN_Packet_InitRead(&packet, client->endpoint.protocol_id, len) < 0)
            continue;

        packet.sender = client->server_connection;

        ClientDriver_OnPacketReceived(&packet);
    }

    return 0;
}

static int WebRTC_Client_SendPacket(NBN_GameClient *client, NBN_Packet *packet) {
    return __js_game_client_send_packet(packet->buffer, packet->size);
}

#endif // __EMSCRIPTEN__

/**
 * ====================================================================== *
 */

#pragma region Packet simulator

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

#define RAND_RATIO_BETWEEN(min, max) (((rand() % (int)((max * 100.f) - (min * 100.f) + 1)) + (min * 100.f)) / 100.f)
#define RAND_RATIO RAND_RATIO_BETWEEN(0, 1)

#ifdef NBN_PLATFORM_WINDOWS
DWORD WINAPI PacketSimulator_Routine(LPVOID);
#else
static void *PacketSimulator_Routine(void *);
#endif

void NBN_GameClient_SetPing(float v) { nbn_game_client.endpoint.packet_simulator.ping = v; }
void NBN_GameClient_SetJitter(float v) { nbn_game_client.endpoint.packet_simulator.jitter = v; }
void NBN_GameClient_SetPacketLoss(float v) { nbn_game_client.endpoint.packet_simulator.packet_loss_ratio = v; }
void NBN_GameClient_SetPacketDuplication(float v) {
    nbn_game_client.endpoint.packet_simulator.packet_duplication_ratio = v;
}

void NBN_GameServer_SetPing(float v) { nbn_game_server.endpoint.packet_simulator.ping = v; }
void NBN_GameServer_SetJitter(float v) { nbn_game_server.endpoint.packet_simulator.jitter = v; }
void NBN_GameServer_SetPacketLoss(float v) { nbn_game_server.endpoint.packet_simulator.packet_loss_ratio = v; }
void NBN_GameServer_SetPacketDuplication(float v) {
    nbn_game_server.endpoint.packet_simulator.packet_duplication_ratio = v;
}

static int PacketSimulator_SendPacket(NBN_PacketSimulator *, NBN_Packet *, NBN_Connection *receiver);
static unsigned int PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *);

void PacketSimulator_Init(NBN_PacketSimulator *packet_simulator, NBN_Endpoint *endpoint) {
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

int PacketSimulator_EnqueuePacket(NBN_PacketSimulator *packet_simulator, NBN_Packet *packet, NBN_Connection *receiver) {
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

    entry->delay = packet_simulator->ping + (double)jitter / 1000; /* and converted back to seconds */
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

void PacketSimulator_Start(NBN_PacketSimulator *packet_simulator) {
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

    if (receiver->endpoint->is_server) {
        if (receiver->is_stale)
            return 0;

        return driver->impl.serv_send_packet_to(&nbn_game_server, packet, receiver);
    } else {
        return driver->impl.cli_send_packet(&nbn_game_client, packet);
    }
}

static unsigned int PacketSimulator_GetRandomDuplicatePacketCount(NBN_PacketSimulator *packet_simulator) {
    if (RAND_RATIO < packet_simulator->packet_duplication_ratio)
        return rand() % 10 + 1;

    return 0;
}

#endif /* NBN_DEBUG && NBN_USE_PACKET_SIMULATOR */

#pragma endregion /* Packet simulator */
