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
    --- NBNET UDP DRIVER ---

    Portable single UDP socket network driver for the nbnet library.

    How to use:

        Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro.
*/

#ifdef NBNET_IMPL

#include <stdio.h>
#include <errno.h>
#include <assert.h>

#pragma region Platform detection

#if defined(_WIN32) || defined(_WIN64)
	#ifndef PLATFORM_WINDOWS
		#define PLATFORM_WINDOWS
	#endif
#elif (defined(__APPLE__) && defined(__MACH__))
	#define PLATFORM_MAC
#else
	#define PLATFORM_UNIX
#endif

#pragma endregion /* Platform detection */

#if defined(PLATFORM_WINDOWS)

#include <winsock2.h>

typedef int socklen_t;

#elif defined(PLATFORM_UNIX) || defined(PLATFORM_MAC)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#endif

typedef struct
{
    uint32_t host;
    uint16_t port;
} NBN_IPAddress;

typedef struct
{
    uint32_t id;
    NBN_IPAddress address;
} NBN_UDPConnection;

static SOCKET udp_sock;
static uint32_t protocol_id;

#pragma region Socket functions

#ifdef PLATFORM_WINDOWS

static char err_msg[32];

#endif

static int InitSocket(void);
static void DeinitSocket(void);
static int BindSocket(uint16_t);
static char *GetLastErrorMessage(void);

static int InitSocket(void)
{
#ifdef PLATFORM_WINDOWS
    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err < 0)
    {
        NBN_LogError("WSAStartup() failed");

        return -1;
    }
#endif

    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
        return -1;

#if defined(PLATFORM_WINDOWS)
    DWORD non_blocking = 1;

    if (ioctlsocket(udp_sock, FIONBIO, &non_blocking) != 0)
    {
        NBN_LogError("ioctlsocket() failed: %s", GetLastErrorMessage());

        return -1;
    }
#elif defined(PLATFORM_MAC) || defined(PLATFORM_UNIX)
    int non_blocking = 1;

    if (fcntl(udp_sock, F_SETFL, O_NONBLOCK, non_blocking) < 0)
    {
        NBN_LogError("fcntl() failed: %s", GetLastErrorMessage());

        return -1;
    }
#endif

    return 0;
}

static void DeinitSocket(void)
{
    closesocket(udp_sock);

#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static int BindSocket(uint16_t port)
{
    SOCKADDR_IN sin;

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(udp_sock, (SOCKADDR *)&sin, sizeof(sin)) < 0)
    {
        NBN_LogError("bind() failed: %s", GetLastErrorMessage());

        return -1;
    }
    
    return 0;
}

static int ResolveIpAddress(const char *host, uint16_t port, NBN_IPAddress *address)
{
    char *dup_host = strdup(host);
    uint8_t arr[4];

    for (int i = 0; i < 4; i++)
    {
        char *s;

        if ((s = strtok(i == 0 ? dup_host : NULL, ".")) == NULL)
            return -1;

        char *end = NULL;
        int v = strtol(s, &end, 10);

        if (end == s || v < 0 || v > 255)
            return -1;

        arr[i] = (uint8_t)v;
    }

    address->host = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
    address->port = port;

    free(dup_host);

    return 0;
}

static char *GetLastErrorMessage(void)
{
#ifdef PLATFORM_WINDOWS
    sprintf(err_msg, "%d", WSAGetLastError());

    return err_msg;
#else
    return strerror(errno);
#endif
}

#pragma endregion /* Socket functions */

#pragma region Game server

static uint32_t next_conn_id = 0;

static NBN_Connection *FindOrCreateClientConnectionByAddress(NBN_IPAddress);
static NBN_Connection *FindClientConnectionByAddress(NBN_IPAddress);

int NBN_Driver_GServ_Start(uint32_t proto_id, uint16_t port)
{
    protocol_id = proto_id;

    if (InitSocket() < 0)
        return -1;

    if (BindSocket(port) < 0)
        return -1;

    return 0;
}

void NBN_Driver_GServ_Stop(void)
{
    DeinitSocket();
}

int NBN_Driver_GServ_RecvPackets(void)
{
    uint8_t buffer[NBN_PACKET_MAX_SIZE] = {0};
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    NBN_IPAddress ip_address;

    while (true)
    {
        int bytes = recvfrom(udp_sock, (char *)buffer, sizeof(buffer), 0, (SOCKADDR *)&src_addr, &src_addr_len);

        if (bytes <= 0)
            break;

        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

        if (NBN_Packet_ReadProtocolId(buffer, bytes) != protocol_id)
            continue; /* not matching the protocol of the receiver */ 

        NBN_Connection *conn = FindOrCreateClientConnectionByAddress(ip_address);

        if (conn == NULL)
        {
            NBN_LogError("Failed to retrieve udp connection");

            return NBN_ERROR;
        }

        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, conn, buffer, bytes) < 0)
            continue; /* not a valid packet */

        if (NBN_Driver_GServ_RaiseEvent(NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED, &packet) < 0)
        {
            NBN_LogError("Failed to raise game server event");

            return NBN_ERROR;
        }
    }

    return 0;
}

void NBN_Driver_GServ_DestroyClientConnection(NBN_Connection *connection)
{
    assert(connection != NULL);

    NBN_LogDebug("Destroy UDP connection %d", connection->id);

    NBN_Deallocator(connection->driver_data);
}

int NBN_Driver_GServ_SendPacketTo(NBN_Packet *packet, NBN_Connection *connection)
{
    NBN_UDPConnection *udp_conn = (NBN_UDPConnection*)connection->driver_data;

    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(udp_conn->address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(udp_conn->address.port);

    if (sendto(udp_sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr, sizeof(dest_addr)) == SOCKET_ERROR)
    {
        NBN_LogError("sendto() failed: %s", GetLastErrorMessage());

        return -1;
    }

    return 0;
}

static NBN_Connection *FindOrCreateClientConnectionByAddress(NBN_IPAddress address)
{
    NBN_Connection *conn = FindClientConnectionByAddress(address);

    if (conn == NULL && NBN_GameServer_GetClientCount() < NBN_MAX_CLIENTS)
    {
        /* this is a new connection */

        NBN_UDPConnection *udp_conn = (NBN_UDPConnection*)NBN_Allocator(sizeof(NBN_UDPConnection));

        udp_conn->id = next_conn_id++;
        udp_conn->address = address;

        conn = NBN_GameServer_CreateClientConnection(udp_conn->id, udp_conn);

        NBN_LogDebug("New UDP connection (id: %d)", udp_conn->id);

        if (NBN_Driver_GServ_RaiseEvent(NBN_DRIVER_GSERV_CLIENT_CONNECTED, conn) < 0)
        {
            NBN_LogError("Failed to raise game server event");

            return NULL;
        }
    }

    return conn;
}

static NBN_Connection *FindClientConnectionByAddress(NBN_IPAddress address)
{
    for (int i = 0; i < NBN_MAX_CLIENTS; i++)
    {
        NBN_Connection *connection = __game_server.clients[i];

        if (connection)
        {
            NBN_UDPConnection *udp_conn = (NBN_UDPConnection*)connection->driver_data;

            if (udp_conn->address.host == address.host && udp_conn->address.port == address.port)
                return connection;
        }
    }

    return NULL;
}

#pragma endregion /* Game server */

#pragma region Game client

static NBN_Connection *server_connection;
static bool is_connected_to_server = false;

static int ResolveIpAddress(const char *, uint16_t, NBN_IPAddress *);

int NBN_Driver_GCli_Start(uint32_t proto_id, const char *host, uint16_t port)
{
    NBN_UDPConnection *udp_conn = (NBN_UDPConnection*)NBN_Allocator(sizeof(NBN_Connection));

    protocol_id = proto_id;

    if (ResolveIpAddress(host, port, &udp_conn->address) < 0)
    {
        NBN_LogError("Failed to resolve IP address from %s", host);

        return -1;
    }

    if (InitSocket() < 0)
        return -1;

    if (BindSocket(0) < 0)
        return -1;

    server_connection = NBN_GameClient_CreateServerConnection(udp_conn);

    return 0;
}

void NBN_Driver_GCli_Stop(void)
{
    DeinitSocket();
}

int NBN_Driver_GCli_RecvPackets(void)
{
    NBN_UDPConnection *udp_conn = (NBN_UDPConnection*)server_connection->driver_data;
    uint8_t buffer[NBN_PACKET_MAX_SIZE] = {0};
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    while (true)
    {
        int bytes = recvfrom(udp_sock, (char *)buffer, sizeof(buffer), 0, (SOCKADDR *)&src_addr, &src_addr_len);

        if (bytes <= 0)
            break;

        NBN_IPAddress ip_address;

        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

        /* make sure the received packet is from the server */
        if (ip_address.host != udp_conn->address.host || ip_address.port != udp_conn->address.port)
            continue;

        if (NBN_Packet_ReadProtocolId(buffer, bytes) != protocol_id)
            continue; /* not matching the protocol of the receiver */

        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, server_connection, buffer, bytes) < 0)
            continue; /* not a valid packet */ 

        /* First received packet from server triggers the client connected event */
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
    NBN_UDPConnection *udp_conn = (NBN_UDPConnection*)server_connection->driver_data;
    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(udp_conn->address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(udp_conn->address.port);

    if (sendto(udp_sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr, sizeof(dest_addr)) == SOCKET_ERROR)
    {
        NBN_LogError("sendto() failed: %s", GetLastErrorMessage());

        return -1;
    }

    return 0;
}

#pragma endregion /* Game client */

#endif /* NBNET_IMPL */
