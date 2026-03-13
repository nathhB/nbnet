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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define NBN_ERROR -1

// TODO: doc
#ifndef NBN_SERVER_INITIAL_DATA_MAX_SIZE
#define NBN_SERVER_INITIAL_DATA_MAX_SIZE 256
#endif

// TODO: doc
#ifndef NBN_CONNECTION_REQUEST_DATA_MAX_SIZE
#define NBN_CONNECTION_REQUEST_DATA_MAX_SIZE 256
#endif

// TODO: doc
#ifndef NBN_MESSAGE_RESEND_DELAY
#define NBN_MESSAGE_RESEND_DELAY 0.1 /* Number of seconds before a message is resent (reliable messages redundancy) */
#endif

/**
 * Number of seconds before a connection is considered stale and closes
 */
#ifndef NBN_CONNECTION_STALE_TIME_THRESHOLD
#define NBN_CONNECTION_STALE_TIME_THRESHOLD 3
#endif

typedef uint64_t NBN_Connection_ID;

typedef struct NBN_ConnectionHandle {
    NBN_Connection_ID id;
    void *user_data;
} NBN_ConnectionHandle;

typedef struct NBN_ConnectionStats {
    double ping;
    unsigned int total_lost_packets;
    float packet_loss;
    float upload_bandwidth;
    float download_bandwidth;
} NBN_ConnectionStats;

/**
 * Information about a received message.
 */
typedef struct NBN_MessageInfo {
    /**
     * Type of the message
     */
    uint8_t type;

    /**
     * Channel the message was received on
     */
    uint8_t channel_id;

    /**
     * Pointer to the internal message data buffer
     */
    uint8_t *data;

    /*
     * Length of the message data in bytes
     */
    uint16_t length;

    /**
     * A handle to the connection that sent the message.
     *
     * On the client, it will always be NULL as all messages are received from the server.
     */
    NBN_ConnectionHandle *sender;
} NBN_MessageInfo;

// TODO: doc
typedef enum NBN_Channel_Mode { NBN_CHANNEL_UNRELIABLE, NBN_CHANNEL_RELIABLE } NBN_Channel_Mode;

typedef enum NBN_Client_Event {
    NBN_CLIENT_ERROR = NBN_ERROR,

    NBN_CLIENT_NO_EVENT = 0,

    /* Client is connected to server */
    NBN_CLIENT_CONNECTED,

    /* Client is disconnected from the server */
    NBN_CLIENT_DISCONNECTED,

    /* Client has received a message from the server */
    NBN_CLIENT_MESSAGE_RECEIVED
} NBN_Client_Event;

typedef enum NBN_Server_Event {
    NBN_SERVER_ERROR = NBN_ERROR,

    NBN_SERVER_NO_EVENT = 0,

    /* A new client has connected */
    NBN_SERVER_NEW_CONNECTION,

    /* A client has disconnected */
    NBN_SERVER_DISCONNECTION,

    /* A message has been received from a client */
    NBN_SERVER_MESSAGE_RECEIVED
} NBN_Server_Event;

typedef struct NBN_GameServerStats {
    float upload_bandwidth;   /* Total upload bandwith of the game server */
    float download_bandwidth; /* Total download bandwith of the game server */
} NBN_GameServerStats;

typedef struct NBN_DisconnectionInfo {
    NBN_Connection_ID conn_id; /* ID if the disconnected connection */
    void *user_data;           /* Pointer to user-defined data associated with this connection */
} NBN_DisconnectionInfo;

typedef unsigned int NBN_Client_Iterator;

typedef struct NBN_Writer {
    uint8_t *buffer;
    uint16_t length;
    uint16_t position;
} NBN_Writer;

typedef struct NBN_Reader {
    uint8_t *buffer;
    uint16_t length;
    uint16_t position;
} NBN_Reader;

typedef enum NBN_LogLevel { NBN_LOG_ERROR, NBN_LOG_INFO, NBN_LOG_WARNING, NBN_LOG_DEBUG } NBN_LogLevel;

void NBN_SetLogLevel(NBN_LogLevel);

void NBN_Writer_Init(NBN_Writer *writer, uint8_t *buffer, unsigned int length);
void NBN_Writer_WriteInt8(NBN_Writer *writer, int8_t value);
void NBN_Writer_WriteInt16(NBN_Writer *writer, int16_t value);
void NBN_Writer_WriteInt32(NBN_Writer *writer, int32_t value);
void NBN_Writer_WriteInt64(NBN_Writer *writer, int64_t value);
void NBN_Writer_WriteUInt8(NBN_Writer *writer, uint8_t value);
void NBN_Writer_WriteUInt16(NBN_Writer *writer, uint16_t value);
void NBN_Writer_WriteUInt32(NBN_Writer *writer, uint32_t value);
void NBN_Writer_WriteUInt64(NBN_Writer *writer, uint64_t value);
void NBN_Writer_WriteFloat(NBN_Writer *writer, float value);
void NBN_Writer_WriteBool(NBN_Writer *writer, bool value);
void NBN_Writer_WriteBytes(NBN_Writer *writer, uint8_t *bytes, unsigned int length);
void NBN_Writer_WriteString(NBN_Writer *writer, const char *str, unsigned int max_len);

void NBN_Reader_Init(NBN_Reader *reader, uint8_t *buffer, unsigned int length);
int NBN_Reader_ReadInt8(NBN_Reader *reader, int8_t *value);
int NBN_Reader_ReadInt16(NBN_Reader *reader, int16_t *value);
int NBN_Reader_ReadInt32(NBN_Reader *reader, int32_t *value);
int NBN_Reader_ReadInt64(NBN_Reader *reader, int64_t *value);
int NBN_Reader_ReadUInt8(NBN_Reader *reader, uint8_t *value);
int NBN_Reader_ReadUInt16(NBN_Reader *reader, uint16_t *value);
int NBN_Reader_ReadUInt32(NBN_Reader *reader, uint32_t *value);
int NBN_Reader_ReadUInt64(NBN_Reader *reader, uint64_t *value);
int NBN_Reader_ReadFloat(NBN_Reader *reader, float *value);
int NBN_Reader_ReadBool(NBN_Reader *reader, bool *value);
int NBN_Reader_ReadBytes(NBN_Reader *reader, uint8_t *bytes, unsigned int length);
int NBN_Reader_ReadString(NBN_Reader *reader, char *str, unsigned int max_len);

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
uint8_t NBN_GameClient_CreateChannel(NBN_Channel_Mode mode, unsigned int buffer_size, unsigned int max_message_len);

// TODO: doc
unsigned int NBN_GameClient_GetChannelCurrentCapacity(uint8_t channel_id);

// TODO: doc
NBN_Writer *NBN_GameClient_WriteConnectionRequestData(void);

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
NBN_Reader *NBN_GameClient_ReadServerData(void);

/**
 * Poll game client events.
 *
 * This function should be called in a loop until it returns NBN_NO_EVENT.
 *
 * @return The code of the polled event or NBN_NO_EVENT when there is no more events.
 */
NBN_Client_Event NBN_GameClient_Poll(void);

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
NBN_Writer *NBN_GameClient_CreateReliableMessage(uint8_t type);

// TODO: doc
NBN_Writer *NBN_GameClient_CreateUnreliableMessage(uint8_t type);

// TODO: doc
NBN_Reader *NBN_GameClient_ReadMessage(void);

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

/**
 * Initialize the game server with minimal configuration.
 *
 * @param protocol_name A unique protocol name, the clients and the server must use the same one or they won't be
 * able to communicate
 * @param port The port clients will connect to
 */
void NBN_GameServer_Init(const char *protocol_name, uint16_t port);

// TODO: doc
uint8_t NBN_GameServer_CreateChannel(NBN_Channel_Mode mode, unsigned int buffer_size, unsigned int max_message_len);

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
NBN_ConnectionHandle *NBN_GameServer_GetConnection(NBN_Connection_ID);

// TODO: doc
unsigned int NBN_GameServer_GetClientCount(void);

NBN_ConnectionHandle *NBN_GameServer_GetNextClient(NBN_Client_Iterator *it);

/**
 * Poll game server events.
 *
 * This function should be called in a loop until it returns NBN_NO_EVENT.
 *
 * @return The code of the polled event or NBN_NO_EVENT when there is no more events.
 */
NBN_Server_Event NBN_GameServer_Poll(void);

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
 * Close a client's connection without a specific code (default code is -1)
 *
 * @param conn The connection to close
 *
 * @return 0 when successful, -1 otherwise
 */
int NBN_GameServer_CloseClient(NBN_ConnectionHandle *conn);

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
int NBN_GameServer_CloseClientWithCode(NBN_ConnectionHandle *conn, int code);

// TODO: doc
NBN_Writer *NBN_GameServer_CreateMessage(uint8_t type, uint8_t channel_id, NBN_ConnectionHandle *receiver);

// TODO: doc
NBN_Writer *NBN_GameServer_CreateReliableMessage(uint8_t type, NBN_ConnectionHandle *receiver);

// TODO: doc
NBN_Writer *NBN_GameServer_CreateUnreliableMessage(uint8_t type, NBN_ConnectionHandle *receiver);

// TODO: doc
NBN_Reader *NBN_GameServer_ReadMessage(void);

// TODO: doc
NBN_Writer *NBN_GameServer_WriteConnectionData(void);

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
NBN_ConnectionHandle *NBN_GameServer_GetIncomingConnection(void);

// TODO: doc
NBN_Reader *NBN_GameServer_ReadConnectionRequestData(void);

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

#ifdef __EMSCRIPTEN__

/* EMSCRIPTEN WEBRTC DRIVER SPECIFIC API */

typedef struct NBN_WebRTC_Config {
    bool enable_tls;
    const char *cert_path;
    const char *key_path;
} NBN_WebRTC_Config;

void NBN_WebRTC_SetConfig(NBN_WebRTC_Config config);

// TODO: ice servers currently hard coded in driver js code
#define NBN_WEBRTC_DEFAULT_CONFIG (NBN_WebRTC_Config){.enable_tls = false, .cert_path = NULL, .key_path = NULL};

#endif // __EMSCRIPTEN__

#ifdef NBN_WEBRTC_NATIVE

/* NATIVE WEBRTC DRIVER SPECIFIC API */

#include <rtc/rtc.h>

typedef struct NBN_WebRTC_Config {
    bool enable_tls;
    const char *cert_path;
    const char *key_path;
    const char *passphrase;
    const char **ice_servers;
    unsigned int ice_servers_count;
    rtcLogLevel log_level;
} NBN_WebRTC_Config;

static const char *default_ice_servers[] = {"stun:stun01.sipphone.com"};

#ifdef NBN_DEBUG

#define NBN_DEFAULT_RTC_LOG_LEVEL RTC_LOG_DEBUG

#else

#define NBN_DEFAULT_RTC_LOG_LEVEL RTC_LOG_ERROR

#endif // NBN_DEBUG

#define NBN_WEBRTC_DEFAULT_CONFIG                                                                                      \
    (NBN_WebRTC_Config) {                                                                                              \
        .enable_tls = false, .cert_path = NULL, .key_path = NULL, .passphrase = NULL,                                  \
        .ice_servers = default_ice_servers,                                                                            \
        .ice_servers_count = sizeof(default_ice_servers) / sizeof(default_ice_servers[0]),                             \
        .log_level = NBN_DEFAULT_RTC_LOG_LEVEL                                                                         \
    }

void NBN_WebRTC_SetConfig(NBN_WebRTC_Config config);

#endif

#if defined(NBN_DEBUG) && defined(NBN_USE_PACKET_SIMULATOR)

void NBN_GameClient_SetPing(float v);
void NBN_GameClient_SetJitter(float v);
void NBN_GameClient_SetPacketLoss(float v);
void NBN_GameClient_SetPacketDuplication(float v);

void NBN_GameServer_SetPing(float v);
void NBN_GameServer_SetJitter(float v);
void NBN_GameServer_SetPacketLoss(float v);
void NBN_GameServer_SetPacketDuplication(float v);

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

#ifdef __cplusplus
}
#endif

#endif /* NBNET_H */
