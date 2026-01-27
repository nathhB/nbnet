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
    --- NBNET UDP DRIVER ---

    Portable single UDP socket network driver for the nbnet library.

    How to use:

        1. Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro
        2. Call NBN_UDP_Register in both your client and server code before calling NBN_GameClient_Start or
   NBN_GameServer_Start
*/

void NBN_UDP_Register(void);

#ifdef NBNET_IMPL

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NBNET_H
#include "../nbnet.h"
#endif

#define STB_DS_IMPLEMENTATION
#include "../stb_ds.h"

#define NBN_UDP_DRIVER_ID 0
#define NBN_UDP_DRIVER_NAME "UDP"

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

#endif

typedef struct {
    uint32_t host;
    uint16_t port;
} NBN_IPAddress;

typedef struct {
    uint32_t id;
    NBN_IPAddress address;
    NBN_Connection *conn; // nbnet connection associated to this UDP connection
} NBN_UDP_Connection;

static SOCKET nbn_udp_sock;

#pragma region Socket functions

#ifdef PLATFORM_WINDOWS

static char err_msg[32];

#endif

static int InitSocket(void);
static void DeinitSocket(void);
static int BindSocket(uint16_t);
static char *GetLastErrorMessage(void);

static int InitSocket(void) {
#ifdef PLATFORM_WINDOWS
    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err < 0) {
        NBN_LogError("WSAStartup() failed");

        return NBN_ERROR;
    }
#endif

    if ((nbn_udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
        return NBN_ERROR;

#if defined(PLATFORM_WINDOWS)
    DWORD non_blocking = 1;

    if (ioctlsocket(nbn_udp_sock, FIONBIO, &non_blocking) != 0) {
        NBN_LogError("ioctlsocket() failed: %s", GetLastErrorMessage());

        return NBN_ERROR;
    }
#elif defined(PLATFORM_MAC) || defined(PLATFORM_UNIX)
    int non_blocking = 1;

    if (fcntl(nbn_udp_sock, F_SETFL, O_NONBLOCK, non_blocking) < 0) {
        NBN_LogError("fcntl() failed: %s", GetLastErrorMessage());

        return NBN_ERROR;
    }
#endif

    return 0;
}

static void DeinitSocket(void) {
    closesocket(nbn_udp_sock);

#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static int BindSocket(uint16_t port) {
    SOCKADDR_IN sin;

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(nbn_udp_sock, (SOCKADDR *)&sin, sizeof(sin)) < 0) {
        NBN_LogError("bind() failed: %s", GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

static int ResolveIpAddress(const char *host, uint16_t port, NBN_IPAddress *address) {
    size_t host_len = strlen(host);
    char *dup_host = (char *)malloc(host_len + 1);
    memcpy(dup_host, host, host_len + 1);
    uint8_t arr[4];

    for (int i = 0; i < 4; i++) {
        char *s;

        // TODO: replace strtok with strsep
        if ((s = strtok(i == 0 ? dup_host : NULL, ".")) == NULL)
            return NBN_ERROR;

        char *end = NULL;
        int v = strtol(s, &end, 10);

        if (end == s || v < 0 || v > 255)
            return NBN_ERROR;

        arr[i] = (uint8_t)v;
    }

    address->host = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
    address->port = port;

    free(dup_host);

    return 0;
}

static char *GetLastErrorMessage(void) {
#ifdef PLATFORM_WINDOWS
    snprintf(err_msg, sizeof(err_msg), "%d", WSAGetLastError());

    return err_msg;
#else
    return strerror(errno);
#endif
}

#pragma endregion /* Socket functions */

#pragma region Game server

typedef struct NBN_UDP_Server {
    struct {
        NBN_IPAddress key;
        NBN_UDP_Connection *value;
    } *connections;
    uint32_t next_conn_id; // nbnet connection ids, starts at 1
    uint32_t protocol_id;
} NBN_UDP_Server;

static NBN_UDP_Server nbn_udp_serv = {NULL, 1, 0};

static NBN_Connection *FindOrCreateClientConnectionByAddress(NBN_IPAddress);

static int NBN_UDP_ServStart(uint32_t protocol_id, uint16_t port) {
    nbn_udp_serv.protocol_id = protocol_id;

    hmdefault(nbn_udp_serv.connections, NULL);

    if (InitSocket() < 0)
        return NBN_ERROR;

    if (BindSocket(port) < 0)
        return NBN_ERROR;

    return 0;
}

static void NBN_UDP_ServStop(void) {
    hmfree(nbn_udp_serv.connections);
    DeinitSocket();
}

static int NBN_UDP_ServRecvPackets(void) {
    NBN_Packet packet = {0};
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    NBN_IPAddress ip_address;

    while (true) {
        int bytes = recvfrom(nbn_udp_sock, (char *)packet.buffer, sizeof(packet.buffer), 0, (SOCKADDR *)&src_addr,
                             &src_addr_len);

        if (bytes <= 0)
            break;

        if (bytes <= NBN_PACKET_HEADER_SIZE)
            continue;

        if (NBN_Packet_InitRead(&packet, nbn_udp_serv.protocol_id, bytes) < 0) {
            NBN_LogDebug("Discarded invalid packet");
            continue;
        }

        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

        NBN_Connection *conn = FindOrCreateClientConnectionByAddress(ip_address);

        if (conn == NULL)
            continue;

        packet.sender = conn;

        if (NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED, &packet) < 0) {
            NBN_LogError("Failed to raise game server event");

            return NBN_ERROR;
        }
    }

    return 0;
}

static void NBN_UDP_ServRemoveClientConnection(NBN_Connection *connection) {
    NBN_Assert(connection != NULL);

    NBN_IPAddress address = ((NBN_UDP_Connection *)connection->driver_data)->address;
    NBN_UDP_Connection *udp_conn = hmget(nbn_udp_serv.connections, address);

    if (udp_conn) {
        NBN_LogDebug("Destroyed UDP connection %d", connection->id);

        int ret = hmdel(nbn_udp_serv.connections, address);

        NBN_Assert(ret == 1);
        free(udp_conn);
    }
}

static int NBN_UDP_ServSendPacketTo(NBN_Packet *packet, NBN_Connection *connection) {
    NBN_UDP_Connection *udp_conn = (NBN_UDP_Connection *)connection->driver_data;

    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(udp_conn->address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(udp_conn->address.port);

    if (sendto(nbn_udp_sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr,
               sizeof(dest_addr)) == SOCKET_ERROR) {
        NBN_LogError("sendto() failed: %s", GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

static NBN_Connection *FindOrCreateClientConnectionByAddress(NBN_IPAddress address) {
    NBN_UDP_Connection *udp_conn = hmget(nbn_udp_serv.connections, address);

    if (udp_conn == NULL) {
        /* this is a new connection */

        if (GameServer_GetClientCount() >= NBN_MAX_CLIENTS)
            return NULL;

        udp_conn = (NBN_UDP_Connection *)malloc(sizeof(NBN_UDP_Connection));

        udp_conn->id = nbn_udp_serv.next_conn_id++;
        udp_conn->address = address;
        udp_conn->conn = NBN_GameServer_CreateClientConnection(NBN_UDP_DRIVER_ID, udp_conn, udp_conn->id);

        hmput(nbn_udp_serv.connections, address, udp_conn);

        NBN_LogDebug("New UDP connection (id: %d)", udp_conn->id);

        if (NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_CONNECTED, udp_conn->conn) < 0) {
            NBN_LogError("Failed to raise game server event");

            return NULL;
        }
    }

    return udp_conn->conn;
}

#pragma endregion /* Game server */

#pragma region Game client

typedef struct NBN_UDP_Client {
    NBN_Connection *server_conn;
    uint32_t protocol_id;
} NBN_UDP_Client;

static NBN_UDP_Client nbn_udp_cli = {NULL, 0};

static int ResolveIpAddress(const char *, uint16_t, NBN_IPAddress *);

static int NBN_UDP_CliStart(uint32_t protocol_id, const char *host, uint16_t port) {
    NBN_UDP_Connection *udp_conn = (NBN_UDP_Connection *)malloc(sizeof(NBN_Connection));

    nbn_udp_cli.protocol_id = protocol_id;

    if (ResolveIpAddress(host, port, &udp_conn->address) < 0) {
        NBN_LogError("Failed to resolve IP address from %s", host);

        return NBN_ERROR;
    }

    if (InitSocket() < 0)
        return NBN_ERROR;

    if (BindSocket(0) < 0)
        return NBN_ERROR;

    nbn_udp_cli.server_conn = NBN_GameClient_CreateServerConnection(NBN_UDP_DRIVER_ID, udp_conn);

    return 0;
}

static void NBN_UDP_CliStop(void) {
    NBN_UDP_Connection *udp_conn = (NBN_UDP_Connection *)nbn_udp_cli.server_conn->driver_data;

    free(udp_conn);
    DeinitSocket();
}

static int NBN_UDP_CliRecvPackets(void) {
    NBN_UDP_Connection *udp_conn = (NBN_UDP_Connection *)nbn_udp_cli.server_conn->driver_data;
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

        NBN_IPAddress ip_address;

        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

        if (ip_address.host != udp_conn->address.host || ip_address.port != udp_conn->address.port)
            continue;

        if (NBN_Packet_InitRead(&packet, nbn_udp_cli.protocol_id, bytes) < 0) {
            NBN_LogDebug("Discarded invalid packet");
            continue;
        }

        packet.sender = nbn_udp_cli.server_conn;

        NBN_Driver_RaiseEvent(NBN_DRIVER_CLI_PACKET_RECEIVED, &packet);
    }

    return 0;
}

static int NBN_UDP_CliSendPacket(NBN_Packet *packet) {
    NBN_UDP_Connection *udp_conn = (NBN_UDP_Connection *)nbn_udp_cli.server_conn->driver_data;
    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(udp_conn->address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(udp_conn->address.port);

    if (sendto(nbn_udp_sock, (const char *)packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr,
               sizeof(dest_addr)) == SOCKET_ERROR) {
        NBN_LogError("sendto() failed: %s", GetLastErrorMessage());

        return NBN_ERROR;
    }

    return 0;
}

#pragma endregion /* Game client */

#pragma region Driver registering

void NBN_UDP_Register(void) {
    NBN_DriverImplementation driver_impl = {// Client implementation
                                            NBN_UDP_CliStart, NBN_UDP_CliStop, NBN_UDP_CliRecvPackets,
                                            NBN_UDP_CliSendPacket,

                                            // Server implementation
                                            NBN_UDP_ServStart, NBN_UDP_ServStop, NBN_UDP_ServRecvPackets,
                                            NBN_UDP_ServSendPacketTo, NBN_UDP_ServRemoveClientConnection};

    NBN_Driver_Register(NBN_UDP_DRIVER_ID, NBN_UDP_DRIVER_NAME, driver_impl);
}

#pragma endregion /* Driver registering */

#endif /* NBNET_IMPL */
