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

/*
    --- NBNET WEBRTC DRIVER ---

    WebRTC driver using a single unreliable data channel for the nbnet library.

    How to use:

        1. Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro
        2. Call NBN_WebRTC_Register in both your client and server code before calling NBN_GameClient_Start or
   NBN_GameServer_Start
*/

#include <stdbool.h>

#ifndef NBNET_H
#include "../nbnet.h"
#endif

typedef struct NBN_WebRTC_Config {
    bool enable_tls;
    const char *cert_path;
    const char *key_path;
} NBN_WebRTC_Config;

void NBN_WebRTC_Register(NBN_WebRTC_Config config);

#ifdef NBNET_IMPL

#if !defined(EXTERN_C)
#if defined(__cplusplus)
#define NBN_EXTERN extern "C"
#else
#define NBN_EXTERN extern
#endif
#endif

#include <emscripten/emscripten.h>

#define NBN_WEBRTC_DRIVER_ID 1
#define NBN_WEBRTC_DRIVER_NAME "WebRTC"

typedef struct {
    uint32_t id;
    NBN_Connection *conn;
} NBN_WebRTC_Peer;

#pragma region Game server

/* --- JS API --- */

NBN_EXTERN void __js_game_server_init(uint32_t, bool, const char *, const char *);
NBN_EXTERN int __js_game_server_start(uint16_t);
NBN_EXTERN int __js_game_server_dequeue_packet(uint32_t *, uint8_t *);
NBN_EXTERN int __js_game_server_send_packet_to(uint8_t *, unsigned int, uint32_t);
NBN_EXTERN void __js_game_server_close_client_peer(unsigned int);
NBN_EXTERN void __js_game_server_stop(void);

/* --- Driver implementation --- */

typedef struct NBN_WebRTC_Server {
    struct {
        int key;
        NBN_WebRTC_Peer *value;
    } *peers;
    uint8_t packet_buffer[NBN_PACKET_MAX_SIZE];
    uint32_t protocol_id;
} NBN_WebRTC_Server;

static NBN_WebRTC_Server nbn_wrtc_serv = {NULL, {0}, 0};
static NBN_WebRTC_Config nbn_wrtc_cfg;

static int NBN_WebRTC_ServStart(uint32_t protocol_id, uint16_t port) {
    __js_game_server_init(protocol_id, nbn_wrtc_cfg.enable_tls, nbn_wrtc_cfg.key_path, nbn_wrtc_cfg.cert_path);

    if (__js_game_server_start(port) < 0)
        return -1;

    nbn_wrtc_serv.protocol_id = protocol_id;

    hmdefault(nbn_wrtc_serv.peers, NULL);

    return 0;
}

static void NBN_WebRTC_ServStop(void) {
    __js_game_server_stop();
    hmfree(nbn_wrtc_serv.peers);
}

static int NBN_WebRTC_ServRecvPackets(void) {
    static NBN_Packet packet = {0};
    uint32_t peer_id;
    unsigned int len;

    while ((len = __js_game_server_dequeue_packet(&peer_id, (uint8_t *)nbn_wrtc_serv.packet_buffer)) > 0) {
        NBN_WebRTC_Peer *peer = hmget(nbn_wrtc_serv.peers, peer_id);

        if (peer == NULL) {
            if (GameServer_GetClientCount() >= NBN_MAX_CLIENTS)
                continue;

            NBN_LogTrace("Peer %d has connected", peer_id);

            peer = (NBN_WebRTC_Peer *)malloc(sizeof(NBN_WebRTC_Peer));

            peer->id = peer_id;
            peer->conn =
                NBN_GameServer_CreateClientConnection(NBN_WEBRTC_DRIVER_ID, peer, nbn_wrtc_serv.protocol_id, peer_id);

            hmput(nbn_wrtc_serv.peers, peer_id, peer);

            NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_CONNECTED, peer->conn);
        }

        if (NBN_Packet_InitRead(&packet, peer->conn, nbn_wrtc_serv.packet_buffer, len) < 0)
            continue;

        packet.sender = peer->conn;

        NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED, &packet);
    }

    return 0;
}

static void NBN_WebRTC_ServRemoveClientConnection(NBN_Connection *conn) {
    assert(conn != NULL);

    __js_game_server_close_client_peer(conn->id);

    NBN_WebRTC_Peer *peer = (NBN_WebRTC_Native_Peer *)conn->driver_data;
    int ret = hmdel(nbn_wrtc_c_serv.peers, peer->id);

    if (ret == 1) {
        NBN_LogDebug("Destroyed peer %d", peer->id);

        free(peer);
    }
}

static int NBN_WebRTC_ServSendPacketTo(NBN_Packet *packet, NBN_Connection *conn) {
    return __js_game_server_send_packet_to(packet->buffer, packet->size, conn->id);
}

#pragma endregion /* Game server */

#pragma region Game client

/* --- JS API --- */

NBN_EXTERN void __js_game_client_init(uint32_t, bool);
NBN_EXTERN int __js_game_client_start(const char *, uint16_t);
NBN_EXTERN int __js_game_client_dequeue_packet(uint8_t *);
NBN_EXTERN int __js_game_client_send_packet(uint8_t *, unsigned int);
NBN_EXTERN void __js_game_client_close(void);

/* --- Driver implementation --- */

typedef struct NBN_WebRTC_Client {
    NBN_Connection *server_conn;
} NBN_WebRTC_Client;

static NBN_WebRTC_Client nbn_wrtc_cli = {NULL};

static int NBN_WebRTC_CliStart(uint32_t protocol_id, const char *host, uint16_t port) {
    __js_game_client_init(protocol_id, nbn_wrtc_cfg.enable_tls);

    nbn_wrtc_cli.server_conn = NBN_GameClient_CreateServerConnection(NBN_WEBRTC_DRIVER_ID, NULL, protocol_id);

    int res;

    if ((res = __js_game_client_start(host, port)) < 0)
        return -1;

    return 0;
}

static void NBN_WebRTC_CliStop(void) { __js_game_client_close(); }

static int NBN_WebRTC_CliRecvPackets(void) {
    static NBN_Packet packet = {0};
    unsigned int len;

    while ((len = __js_game_client_dequeue_packet((uint8_t *)nbn_wrtc_serv.packet_buffer)) > 0) {
        if (NBN_Packet_InitRead(&packet, nbn_wrtc_cli.server_conn, nbn_wrtc_serv.packet_buffer, len) < 0)
            continue;

        NBN_Driver_RaiseEvent(NBN_DRIVER_CLI_PACKET_RECEIVED, &packet);
    }

    return 0;
}

static int NBN_WebRTC_CliSendPacket(NBN_Packet *packet) {
    return __js_game_client_send_packet(packet->buffer, packet->size);
}

#pragma endregion /* Game client */

#pragma region Driver registering

void NBN_WebRTC_Register(NBN_WebRTC_Config config) {
    NBN_DriverImplementation driver_impl = {// Client implementation
                                            NBN_WebRTC_CliStart, NBN_WebRTC_CliStop, NBN_WebRTC_CliRecvPackets,
                                            NBN_WebRTC_CliSendPacket,

                                            // Server implementation
                                            NBN_WebRTC_ServStart, NBN_WebRTC_ServStop, NBN_WebRTC_ServRecvPackets,
                                            NBN_WebRTC_ServSendPacketTo, NBN_WebRTC_ServRemoveClientConnection};

    nbn_wrtc_cfg = config;

    NBN_Driver_Register(NBN_WEBRTC_DRIVER_ID, NBN_WEBRTC_DRIVER_NAME, driver_impl);
}

#pragma endregion /* Driver registering */

#endif /* NBNET_IMPL */
