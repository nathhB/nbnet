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
    --- NBNET WEBRTC DRIVER ---

    WebRTC driver using a single unreliable data channel for the nbnet library.

    How to use:

        Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro.
*/

#ifdef NBNET_IMPL

#if !defined(EXTERN_C)
    #if defined(__cplusplus)
        #define NBN_EXTERN extern "C"
    #else
        #define NBN_EXTERN extern
    #endif
#endif

#include <emscripten/emscripten.h>

#pragma region Game server

/* --- JS API --- */

NBN_EXTERN void __js_game_server_init(uint32_t, bool, const char *, const char *);
NBN_EXTERN int __js_game_server_start(uint16_t);
NBN_EXTERN uint8_t *__js_game_server_dequeue_packet(uint32_t *, unsigned int *);
NBN_EXTERN int __js_game_server_send_packet_to(uint8_t *, unsigned int, uint32_t);
NBN_EXTERN void __js_game_server_close_client_peer(unsigned int);
NBN_EXTERN void __js_game_server_stop(void);

/* --- Driver implementation --- */

int NBN_Driver_GServ_Start(uint32_t protocol_id, uint16_t port)
{
#ifdef NBN_USE_HTTPS

#ifndef NBN_HTTPS_KEY_PEM
#define NBN_HTTPS_KEY_PEM "key.pem"
#endif

#ifndef NBN_HTTPS_CERT_PEM
#define NBN_HTTPS_CERT_PEM "cert.pem"
#endif
    __js_game_server_init(protocol_id, true, NBN_HTTPS_KEY_PEM, NBN_HTTPS_CERT_PEM);
#else
    __js_game_server_init(protocol_id, false, NULL, NULL);
#endif // NBN_USE_HTTPS
    
    if (__js_game_server_start(port) < 0)
        return -1;

    return 0;
}

void NBN_Driver_GServ_Stop(void)
{
    __js_game_server_stop();
}

int NBN_Driver_GServ_RecvPackets(void)
{
    uint8_t *data;
    uint32_t peer_id;
    unsigned int len;

    while ((data = __js_game_server_dequeue_packet(&peer_id, &len)) != NULL)
    {
        NBN_Packet packet;

        NBN_Connection *cli = NBN_GameServer_FindClientById(peer_id);

        if (cli == NULL)
        {
            NBN_LogTrace("Peer %d has connected", peer_id);

            cli = NBN_GameServer_CreateClientConnection(peer_id, NULL);

            NBN_Driver_GServ_RaiseEvent(NBN_DRIVER_GSERV_CLIENT_CONNECTED, cli);
        }

        if (NBN_Packet_InitRead(&packet, cli, data, len) < 0)
            continue;

        packet.sender = cli;

        NBN_Driver_GServ_RaiseEvent(NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED, &packet);
    }

    return 0;
}

void NBN_Driver_GServ_DestroyClientConnection(NBN_Connection *conn)
{
    __js_game_server_close_client_peer(conn->id);
}

int NBN_Driver_GServ_SendPacketTo(NBN_Packet *packet, NBN_Connection *conn)
{
    return __js_game_server_send_packet_to(packet->buffer, packet->size, conn->id);
}

#pragma endregion /* Game server */

#pragma region Game client

/* --- JS API --- */

NBN_EXTERN void __js_game_client_init(uint32_t, bool);
NBN_EXTERN int __js_game_client_start(const char *, uint16_t);
NBN_EXTERN uint8_t *__js_game_client_dequeue_packet(unsigned int *);
NBN_EXTERN int __js_game_client_send_packet(uint8_t *, unsigned int);
NBN_EXTERN void __js_game_client_close(void);

/* --- Driver implementation --- */

static NBN_Connection *server = NULL;
static bool is_connected_to_server = false;

int NBN_Driver_GCli_Start(uint32_t protocol_id, const char *host, uint16_t port)
{
#ifdef NBN_USE_HTTPS
    __js_game_client_init(protocol_id, true);
#else
    __js_game_client_init(protocol_id, false);
#endif // NBN_USE_HTTPS

    server = NBN_GameClient_CreateServerConnection(NULL);

    int res;

    if ((res = __js_game_client_start(host, port)) < 0)
        return -1;

    return 0;
}

void NBN_Driver_GCli_Stop(void)
{
    __js_game_client_close();
}

int NBN_Driver_GCli_RecvPackets(void)
{
    uint8_t *data;
    unsigned int len;

    while ((data = __js_game_client_dequeue_packet(&len)) != NULL)
    {
        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, server, data, len) < 0)
            continue;

        if (!is_connected_to_server)
        {
            NBN_Driver_GCli_RaiseEvent(NBN_DRIVER_GCLI_CONNECTED, NULL);

            is_connected_to_server = true;
        }

        NBN_Driver_GCli_RaiseEvent(NBN_DRIVER_GCLI_SERVER_PACKET_RECEIVED, &packet);
    }

    return 0;
}

int NBN_Driver_GCli_SendPacket(NBN_Packet *packet)
{
    return __js_game_client_send_packet(packet->buffer, packet->size);
}

#pragma endregion /* Game client */

#endif /* NBNET_IMPL */
