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
        #define NBN_DRIVER_UDP_IMPL
    before you include this file in *one* C or C++ file (after you included the nbnet header).
*/

#ifdef NBN_DRIVER_UDP_IMPL

#include <errno.h>
#include <assert.h>

#pragma region Platform detection

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#elif (defined(__APPLE__) && defined(__MACH__))
#define PLATFORM_MAC
#else
#define PLATFORM_UNIX
#endif

#pragma endregion /* Platform detection */

#if defined(PLATFORM_WINDOWS)

#include <winsock2.h>

#elif defined(PLATFORM_UNIX) || defined(PLATFORM_MAC)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
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
    NBN_Connection *conn; /* actual nbnet connection */
} NBN_UDPConnection;

static SOCKET udp_sock;
static uint32_t protocol_id;

#ifdef NBN_GAME_CLIENT

static NBN_UDPConnection server_connection;
static bool server_connection_closed = false;

#endif /* NBN_GAME_CLIENT */

#pragma region Socket functions

static int init_socket(void);
static void deinit_socket(void);
static int bind_socket(uint16_t);
static int read_received_packets(void (*)(NBN_Packet *, NBN_IPAddress));
static int resolve_ip_address(const char *, uint16_t, NBN_IPAddress *);

static int init_socket(void)
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

    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

#if defined(PLATFORM_WINDOWS)
    DWORD non_blocking = 1;

    if (ioctlsocket(udp_sock, FIONBIO, &non_blocking) != 0)
    {
        NBN_LogError("ioctlsocket() failed: %d", WSAGetLastError());

        return -1;
    }
#elif defined(PLATFORM_MAC) || defined(PLATFORM_UNIX)
    int non_blocking = 1;

    if (fcntl(udp_sock, F_SETFL, O_NONBLOCK, non_blocking) < 0)
    {
        NBN_LogError("fcntl() failed: %s", strerror(errno));

        return -1;
    }
#endif

    return 0;
}

static void deinit_socket(void)
{
    closesocket(udp_sock);

#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static int bind_socket(uint16_t port)
{
    SOCKADDR_IN sin;

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(udp_sock, (SOCKADDR *)&sin, sizeof(sin)) < 0)
    {
        NBN_LogError("bind() failed: %s", strerror(errno));

        return -1;
    }
    
    return 0;
}

static int read_received_packets(void (*process_packet)(NBN_Packet *, NBN_IPAddress))
{
    uint8_t buffer[NBN_PACKET_MAX_SIZE] = {0};
    SOCKADDR_IN src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    int ret;

    while ((ret = recvfrom(udp_sock, buffer, sizeof(buffer), 0, (SOCKADDR *)&src_addr, &src_addr_len)) > 0)
    {
        NBN_IPAddress ip_address;

        ip_address.host = ntohl(src_addr.sin_addr.s_addr);
        ip_address.port = ntohs(src_addr.sin_port);

#ifdef NBN_GAME_CLIENT
        /* make sure the received packet is from the server */
        if (ip_address.host != server_connection.address.host || ip_address.port != server_connection.address.port)
            continue;
#endif

        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, buffer, ret) < 0)
            continue; /* not a valid packet */

        if (packet.header.protocol_id != protocol_id)
            continue; /* valid packet but not matching the protocol of the receiver */

        process_packet(&packet, ip_address);
    }

    if (ret == 0)
    {
        NBN_LogError("Socket was closed");

        return -1;
    }

    if (ret < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            NBN_LogError("recvfrom() failed: %s", strerror(errno));

            return -1;
        }
    }

    return 0;
}

static int resolve_ip_address(const char *host, uint16_t port, NBN_IPAddress *address)
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

#pragma endregion /* Socket functions */

#pragma region Game server

#ifdef NBN_GAME_SERVER

static NBN_List *connections = NULL;
static uint32_t next_conn_id = 0;

static void process_client_packet(NBN_Packet *, NBN_IPAddress);
static void close_client_connection(NBN_UDPConnection *);
static NBN_UDPConnection *find_connection_by_address(NBN_IPAddress);
static NBN_UDPConnection *find_connection_by_id(uint32_t);

int NBN_Driver_GServ_Start(uint32_t proto_id, uint16_t port)
{
    protocol_id = proto_id;

    if (init_socket() < 0)
        return -1;

    if (bind_socket(port) < 0)
        return -1;

    connections = NBN_List_Create();

    return 0;
}

void NBN_Driver_GServ_Stop(void)
{
    deinit_socket();
    NBN_List_Destroy(connections, true, NULL);
}

int NBN_Driver_GServ_RecvPackets(void)
{
    return read_received_packets(process_client_packet);
}

void NBN_Driver_GServ_CloseClient(uint32_t conn_id)
{
    close_client_connection(find_connection_by_id(conn_id));
}

int NBN_Driver_GServ_SendPacketTo(NBN_Packet *packet, uint32_t conn_id)
{
    NBN_UDPConnection *connection = find_connection_by_id(conn_id);

    if (connection == NULL)
    {
        NBN_LogError("Connection %d does not exist", conn_id);

        return -1;
    }

    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(connection->address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(connection->address.port);

    if (sendto(udp_sock, packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr, sizeof(dest_addr)) != packet->size)
    {
        NBN_LogError("sendto() failed: %s", strerror(errno));

        return -1;
    }

    return 0;
}

static void process_client_packet(NBN_Packet *packet, NBN_IPAddress client_address)
{
    NBN_UDPConnection *udp_conn = find_connection_by_address(client_address);

    if (udp_conn == NULL) /* this is a new connection */
    {
        udp_conn = malloc(sizeof(NBN_UDPConnection));
        uint32_t conn_id = next_conn_id++;

        udp_conn->id = conn_id;
        udp_conn->address = client_address;
        udp_conn->conn = NBN_GameServer_CreateClientConnection(conn_id);

        NBN_List_PushBack(connections, udp_conn);
        NBN_GameServer_OnClientConnected(udp_conn->conn);
    }

    if (NBN_GameServer_OnPacketReceived(packet, udp_conn->conn) < 0)
        close_client_connection(udp_conn);
}

static void close_client_connection(NBN_UDPConnection *connection)
{
    assert(connection != NULL);

    NBN_Connection *conn = connection->conn;

    free(NBN_List_Remove(connections, connection));
}

static NBN_UDPConnection *find_connection_by_address(NBN_IPAddress address)
{
    NBN_ListNode *current_node = connections->head;

    while (current_node)
    {
        NBN_UDPConnection *connection = current_node->data;

        if (connection->address.host == address.host && connection->address.port == address.port)
            return connection;

        current_node = current_node->next;
    }

    return NULL;
}

static NBN_UDPConnection *find_connection_by_id(uint32_t conn_id)
{
    NBN_ListNode *current_node = connections->head;

    while (current_node)
    {
        NBN_UDPConnection *connection = current_node->data;

        if (connection->id == conn_id)
            return connection;

        current_node = current_node->next;
    }

    return NULL;
}

#endif /* NBN_GAME_SERVER */

#pragma endregion /* Game server */

#pragma region Game client

#ifdef NBN_GAME_CLIENT

static void process_server_packet(NBN_Packet *, NBN_IPAddress);
static void close_server_connection(void);

int NBN_Driver_GCli_Start(uint32_t proto_id, const char *host, uint16_t port)
{
    protocol_id = proto_id;

    if (resolve_ip_address(host, port, &server_connection.address) < 0)
    {
        NBN_LogError("Failed to resolve IP address from %s", host);

        return -1;
    }

    if (init_socket() < 0)
        return -1;

    if (bind_socket(0) < 0)
        return -1;

    server_connection.conn = NBN_GameClient_CreateServerConnection();

    NBN_GameClient_OnConnected();

    return 0;
}

void NBN_Driver_GCli_Stop(void)
{
    deinit_socket();
}

int NBN_Driver_GCli_RecvPackets(void)
{
    if (server_connection_closed)
    {
        NBN_LogError("Server connection is closed");

        return -1;
    }

    return read_received_packets(process_server_packet);
}

int NBN_Driver_GCli_SendPacket(NBN_Packet *packet)
{
    if (server_connection_closed)
    {
        NBN_LogError("Server connection is closed");

        return -1;
    }

    SOCKADDR_IN dest_addr;

    dest_addr.sin_addr.s_addr = htonl(server_connection.address.host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_connection.address.port);

    if (sendto(udp_sock, packet->buffer, packet->size, 0, (SOCKADDR *)&dest_addr, sizeof(dest_addr)) != packet->size)
    {
        NBN_LogError("sendto() failed: %s", strerror(errno));

        return -1;
    }

    return 0;
}

static void process_server_packet(NBN_Packet *packet, NBN_IPAddress server_address)
{
    assert(server_connection.address.host == server_address.host && server_connection.address.port == server_address.port);

    if (NBN_GameClient_OnPacketReceived(packet) < 0)
        close_server_connection();
}

static void close_server_connection(void)
{
    server_connection_closed = true;

    NBN_GameClient_OnDisconnected();
}

#endif /* NBN_GAME_CLIENT */

#pragma endregion /* Game client */

#endif /* NBN_DRIVER_UDP_IMPL */