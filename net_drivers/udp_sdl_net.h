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
    --- SDL_Net DRIVER ---

    Portable single UDP socket network driver implemented with SDL_Net for the nbnet library.

    How to use:
        #define NBN_DRIVER_UDP_SDL_NET
    before you include this file in *one* C or C++ file (after you included the nbnet header).
*/

#ifdef NBN_DRIVER_UDP_SDL_NET

#ifndef NBN_DRIVER_UDP_SDL_NET_H_INCLUDED
#define NBN_DRIVER_UDP_SDL_NET_H_INCLUDED

#include <stdint.h>
#include <SDL_net.h>

#endif /* NBN_DRIVER_UDP_SDL_NET_H_INCLUDED */

static uint32_t protocol_id;
UDPsocket udp_socket;

#pragma region Game server

#ifdef NBN_GAME_SERVER

typedef struct
{
    uint32_t id;
    IPaddress address;
    NBN_Connection *client;
} UDPConnection;

static NBN_List *connections = NULL;
static uint32_t next_client_id = 0;

static void process_client_packet(NBN_Packet *, IPaddress);
static UDPConnection *find_client_connection_by_address(IPaddress);
static UDPConnection *find_client_connection_by_id(uint32_t);

int NBN_Driver_GServ_Start(uint32_t proto_id, uint16_t port)
{
    protocol_id = proto_id;

    if (SDL_Init(0) < 0)
        return -1;

    if ((udp_socket = SDLNet_UDP_Open(port)) == NULL)
        return -1;

    connections = NBN_List_Create();

    return 0;
}

void NBN_Driver_GServ_Stop(void)
{
    SDLNet_UDP_Close(udp_socket);
    SDL_Quit();
    NBN_List_Destroy(connections, true, NULL);
}

int NBN_Driver_GServ_RecvPackets(void)
{
    UDPpacket *udp_packet = SDLNet_AllocPacket(NBN_PACKET_MAX_SIZE);

    if (udp_packet == NULL)
        return -1;

    int ret;

    while ((ret = SDLNet_UDP_Recv(udp_socket, udp_packet)) != 0)
    {
        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, udp_packet->data, udp_packet->len) < 0)
            continue;

        if (packet.header.protocol_id == protocol_id)
            process_client_packet(&packet, udp_packet->address);
    }

    SDLNet_FreePacket(udp_packet);

    return ret;
}

void NBN_Driver_GServ_CloseClient(uint32_t client_id)
{
    UDPConnection *conn = find_client_connection_by_id(client_id);

    assert(conn != NULL);

    free(NBN_List_Remove(connections, conn));
}

int NBN_Driver_GServ_SendPacketTo(NBN_Packet *packet, uint32_t recv_client_id)
{
    UDPConnection *recv_conn = find_client_connection_by_id(recv_client_id);

    if (recv_conn == NULL)
        return -1;

    UDPpacket *udp_packet = SDLNet_AllocPacket(packet->size);

    udp_packet->len = packet->size;
    udp_packet->address = recv_conn->address;

    memcpy(udp_packet->data, packet->buffer, packet->size);

    if (SDLNet_UDP_Send(udp_socket, -1, udp_packet) == 0)
    {
        SDLNet_FreePacket(udp_packet);

        return -1;
    }

    SDLNet_FreePacket(udp_packet);

    return 0;
}

static void process_client_packet(NBN_Packet *packet, IPaddress address)
{
    UDPConnection *conn = find_client_connection_by_address(address);

    if (conn == NULL)
    {
        conn = malloc(sizeof(UDPConnection));

        conn->id = next_client_id++;
        conn->address = address;
        conn->client = NBN_GameServer_CreateClientConnection(conn->id);

        NBN_List_PushBack(connections, conn);
        NBN_GameServer_OnClientConnected(conn->client);
    }

    if (NBN_GameServer_OnPacketReceived(packet, conn->client) < 0)
        NBN_GameServer_CloseClient(conn->client);
}

static UDPConnection *find_client_connection_by_address(IPaddress address)
{
    NBN_ListNode *current_node = connections->head;

    while (current_node)
    {
        UDPConnection *cli = current_node->data;

        if (cli->address.host == address.host && cli->address.port == address.port)
            return cli;

        current_node = current_node->next;
    }

    return NULL;
}

static UDPConnection *find_client_connection_by_id(uint32_t id)
{
    NBN_ListNode *current_node = connections->head;

    while (current_node)
    {
        UDPConnection *cli = current_node->data;

        if (cli->id == id)
            return cli;

        current_node = current_node->next;
    }

    return NULL;
}

#endif /* NBN_GAME_SERVER */

#pragma endregion /* Game server */

#pragma region Game client

#ifdef NBN_GAME_CLIENT

static NBN_Connection *server;
static IPaddress server_address;

static void process_server_packet(NBN_Packet *);
static void close_server_connection(void);
static bool is_packet_from_server(UDPpacket *);

int NBN_Driver_GCli_Start(uint32_t proto_id, const char *host, uint16_t port)
{
    protocol_id = proto_id;

    if (SDL_Init(0) < 0)
        return -1;

    if ((udp_socket = SDLNet_UDP_Open(0)) == NULL)
        return -1;

    if (SDLNet_ResolveHost(&server_address, host, port) < 0)
        return -1;

    server = NBN_GameClient_CreateServerConnection();

    NBN_GameClient_OnConnected();

    return 0;
}

void NBN_Driver_GCli_Stop(void)
{
    SDLNet_UDP_Close(udp_socket);
    SDL_Quit();
}

int NBN_Driver_GCli_RecvPackets(void)
{
    UDPpacket *udp_packet = SDLNet_AllocPacket(NBN_PACKET_MAX_SIZE);

    if (udp_packet == NULL)
        return -1;

    int ret;

    while ((ret = SDLNet_UDP_Recv(udp_socket, udp_packet)) != 0)
    {
        if (!is_packet_from_server(udp_packet))
            continue;

        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, udp_packet->data, udp_packet->len) < 0)
            continue;

        if (packet.header.protocol_id == protocol_id)
            process_server_packet(&packet);
    }

    SDLNet_FreePacket(udp_packet);

    return ret;
}

int NBN_Driver_GCli_SendPacket(NBN_Packet *packet)
{
    UDPpacket *udp_packet = SDLNet_AllocPacket(packet->size);

    udp_packet->len = packet->size;
    udp_packet->address = server_address;

    memcpy(udp_packet->data, packet->buffer, packet->size);

    if (SDLNet_UDP_Send(udp_socket, -1, udp_packet) == 0)
    {
        SDLNet_FreePacket(udp_packet);

        return -1;
    }

    SDLNet_FreePacket(udp_packet);

    return 0;
}

static void process_server_packet(NBN_Packet *packet)
{
    if (NBN_GameClient_OnPacketReceived(packet) < 0)
        close_server_connection();
}

static void close_server_connection(void)
{
    NBN_GameClient_OnDisconnected();
}

static bool is_packet_from_server(UDPpacket *udp_packet)
{
    return udp_packet->address.host == server_address.host && udp_packet->address.port == server_address.port;
}

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* Game client */

#endif /* NBN_DRIVER_UDP_SDL_NET */
