/*

Copyright (C) 2023 BIAGINI Nathan

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
    --- NBNET IPC DRIVER ---

    IPC driver for the nbnet library.
    
    IMPORTANT: This driver uses UNXI domain sockets for inter processes communication and therefore is not portable.

    How to use:

        1. Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro
        2. Call NBN_IPC_Register in both your client and server code before calling NBN_GameClient_Start or NBN_GameServer_Start
*/

#if !defined(unix) && !defined(__unix__) && !defined(__unix) && \
    !(defined(__APPLE__) && defined(__MACH__))
#error "The IPC driver can only be used on UNIX systems"
#endif

/**
 * Register the driver to nbnet.
 * 
 * @param sock_path Path of the UNIX socket
 */
void NBN_IPC_Register(const char *sock_path);

#ifdef NBNET_IMPL

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NBN_IPC_DRIVER_ID 2
#define NBN_IPC_DRIVER_NAME "IPC"
// IPC messages have a 4 bytes header to store the connection id
#define NBN_IPC_HEADER_SIZE 4
#define NBN_IPC_BUFFER_SIZE NBN_IPC_HEADER_SIZE + NBN_PACKET_MAX_SIZE

#pragma region Hashtable

#define HTABLE_DEFAULT_INITIAL_CAPACITY 32
#define HTABLE_LOAD_FACTOR_THRESHOLD 0.75

typedef struct
{
    uint32_t id;
    NBN_Connection *conn;
} NBN_IPC_Connection;

typedef struct
{
    uint32_t conn_id;
    unsigned int slot;
    NBN_IPC_Connection *conn;
} NBN_IPC_HTableEntry;

typedef struct
{
    NBN_IPC_HTableEntry **internal_array;
    unsigned int capacity;
    unsigned int count;
    float load_factor;
} NBN_IPC_HTable;

static NBN_IPC_HTable *NBN_IPC_HTable_Create(void);
static NBN_IPC_HTable *NBN_IPC_HTable_CreateWithCapacity(unsigned int);
static void NBN_IPC_HTable_Destroy(NBN_IPC_HTable *);
static void NBN_IPC_HTable_Add(NBN_IPC_HTable *, uint32_t, NBN_IPC_Connection *);
static NBN_IPC_Connection *NBN_IPC_HTable_Get(NBN_IPC_HTable *, uint32_t);
static NBN_IPC_Connection *NBN_IPC_HTable_Remove(NBN_IPC_HTable *, uint32_t);
static void NBN_IPC_HTable_InsertEntry(NBN_IPC_HTable *, NBN_IPC_HTableEntry *);
static void NBN_IPC_HTable_RemoveEntry(NBN_IPC_HTable *, NBN_IPC_HTableEntry *);
static unsigned int NBN_IPC_HTable_FindFreeSlot(NBN_IPC_HTable *, NBN_IPC_HTableEntry *, bool *);
static NBN_IPC_HTableEntry *NBN_IPC_HTable_FindEntry(NBN_IPC_HTable *, uint32_t);
static void NBN_IPC_HTable_Grow(NBN_IPC_HTable *);

static NBN_IPC_HTable *NBN_IPC_HTable_Create()
{
    return NBN_IPC_HTable_CreateWithCapacity(HTABLE_DEFAULT_INITIAL_CAPACITY);
}

static NBN_IPC_HTable *NBN_IPC_HTable_CreateWithCapacity(unsigned int capacity)
{
    NBN_IPC_HTable *htable = NBN_Allocator(sizeof(NBN_IPC_HTable));

    htable->internal_array = NBN_Allocator(sizeof(NBN_IPC_HTableEntry *) * capacity);
    htable->capacity = capacity;
    htable->count = 0;
    htable->load_factor = 0;

    for (unsigned int i = 0; i < htable->capacity; i++)
        htable->internal_array[i] = NULL;

    return htable;
}

static void NBN_IPC_HTable_Destroy(NBN_IPC_HTable *htable)
{
    for (unsigned int i = 0; i < htable->capacity; i++)
    {
        NBN_IPC_HTableEntry *entry = htable->internal_array[i];

        if (entry)
            NBN_Deallocator(entry);
    }

    NBN_Deallocator(htable->internal_array);
    NBN_Deallocator(htable);
}

static void NBN_IPC_HTable_Add(NBN_IPC_HTable *htable, uint32_t conn_id, NBN_IPC_Connection *conn)
{
    NBN_IPC_HTableEntry *entry = NBN_Allocator(sizeof(NBN_IPC_HTableEntry));

    entry->conn_id = conn_id;
    entry->conn = conn;

    NBN_IPC_HTable_InsertEntry(htable, entry);

    if (htable->load_factor >= HTABLE_LOAD_FACTOR_THRESHOLD)
        NBN_IPC_HTable_Grow(htable);
}

static NBN_IPC_Connection *NBN_IPC_HTable_Get(NBN_IPC_HTable *htable, uint32_t conn_id)
{
    NBN_IPC_HTableEntry *entry = NBN_IPC_HTable_FindEntry(htable, conn_id);

    return entry ? entry->conn : NULL;
}

static NBN_IPC_Connection *NBN_IPC_HTable_Remove(NBN_IPC_HTable *htable, uint32_t conn_id)
{
    NBN_IPC_HTableEntry *entry = NBN_IPC_HTable_FindEntry(htable, conn_id);

    if (entry)
    {
        NBN_IPC_HTable_RemoveEntry(htable, entry);

        return entry->conn;
    }

    return NULL;
}

static void NBN_IPC_HTable_InsertEntry(NBN_IPC_HTable *htable, NBN_IPC_HTableEntry *entry)
{
    bool use_existing_slot = false;
    unsigned int slot = NBN_IPC_HTable_FindFreeSlot(htable, entry, &use_existing_slot);

    entry->slot = slot;
    htable->internal_array[slot] = entry;

    if (!use_existing_slot)
    {
        htable->count++;
        htable->load_factor = (float)htable->count / htable->capacity;
    }
}

static void NBN_IPC_HTable_RemoveEntry(NBN_IPC_HTable *htable, NBN_IPC_HTableEntry *entry)
{
    htable->internal_array[entry->slot] = NULL;

    NBN_Deallocator(entry);

    htable->count--;
    htable->load_factor = htable->count / htable->capacity;
}

static unsigned int NBN_IPC_HTable_FindFreeSlot(NBN_IPC_HTable *htable, NBN_IPC_HTableEntry *entry, bool *use_existing_slot)
{
    unsigned long hash = entry->conn_id;
    unsigned int slot;

    // quadratic probing

    NBN_IPC_HTableEntry *current_entry;
    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % htable->capacity;
        current_entry = htable->internal_array[slot];

        i++;
    } while (current_entry != NULL && current_entry->conn_id != entry->conn_id);

    if (current_entry != NULL) // it means the current entry as the same key as the inserted entry
    {
        *use_existing_slot = true;

        NBN_Deallocator(current_entry);
    }
    
    return slot;
}

static NBN_IPC_HTableEntry *NBN_IPC_HTable_FindEntry(NBN_IPC_HTable *htable, uint32_t conn_id)
{
    unsigned long hash = conn_id;
    unsigned int slot;

    //quadratic probing

    NBN_IPC_HTableEntry *current_entry;
    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % htable->capacity;
        current_entry = htable->internal_array[slot];

        if (current_entry != NULL && current_entry->conn_id == conn_id)
        {
            return current_entry;
        }

        i++;
    } while (i < htable->capacity);
    
    return NULL;
}

static void NBN_IPC_HTable_Grow(NBN_IPC_HTable *htable)
{
    unsigned int old_capacity = htable->capacity;
    unsigned int new_capacity = old_capacity * 2;
    NBN_IPC_HTableEntry** old_internal_array = htable->internal_array;
    NBN_IPC_HTableEntry** new_internal_array = NBN_Allocator(sizeof(NBN_IPC_HTableEntry*) * new_capacity);

    for (unsigned int i = 0; i < new_capacity; i++)
    {
        new_internal_array[i] = NULL;
    }

    htable->internal_array = new_internal_array;
    htable->capacity = new_capacity;
    htable->count = 0;
    htable->load_factor = 0;

    // rehash

    for (unsigned int i = 0; i < old_capacity; i++)
    {
        if (old_internal_array[i])
            NBN_IPC_HTable_InsertEntry(htable, old_internal_array[i]);
    }

    NBN_Deallocator(old_internal_array);
}

#pragma endregion // Hashtable

#pragma region Game server

static int unix_sock, unix_sock2;
static struct sockaddr_un unix_sock_addr;
static const char *unix_sock_path;
static NBN_IPC_HTable *nbn_fifo_connections = NULL;

static NBN_Connection *FindOrCreateClientConnectionById(uint32_t id);

static int NBN_IPC_ServStart(uint32_t proto_id, uint16_t port)
{
    if ((unix_sock = socket(PF_LOCAL, SOCK_DGRAM, 0)) < 0)
    {
        NBN_LogError("socket() faied: %s", strerror(errno));
        return NBN_ERROR;
	}

    if ((unix_sock2 = socket(PF_LOCAL, SOCK_DGRAM, 0)) < 0)
    {
        NBN_LogError("socket() faied: %s", strerror(errno));
        return NBN_ERROR;
	}

    memset(&unix_sock_addr, 0, sizeof(unix_sock_addr));
	unix_sock_addr.sun_family = AF_UNIX;
    strncpy(unix_sock_addr.sun_path, unix_sock_path, sizeof(unix_sock_addr.sun_path) - 1);
	unlink(unix_sock_path);

	if (bind(unix_sock, (SOCKADDR *)&unix_sock_addr, sizeof(unix_sock_addr)) < 0)
    {
        NBN_LogError("bind() failed: %s", strerror(errno));
        return NBN_ERROR;
	}

    if (fcntl(unix_sock, F_SETFL, O_NONBLOCK, 1) < 0)
    {
        NBN_LogError("fcntl() failed: %s", strerror(errno));
        return NBN_ERROR;
    } 

    if (fcntl(unix_sock2, F_SETFL, O_NONBLOCK, 1) < 0)
    {
        NBN_LogError("fcntl() failed: %s", strerror(errno));
        return NBN_ERROR;
    }

    protocol_id = proto_id;
    nbn_fifo_connections = NBN_IPC_HTable_Create();

    return 0;
}

static void NBN_IPC_ServStop(void)
{
    close(unix_sock);
    NBN_IPC_HTable_Destroy(nbn_fifo_connections);
}

static int NBN_IPC_ServRecvPackets(void)
{
    uint8_t buffer[NBN_IPC_BUFFER_SIZE] = {0};

    while (true)
    {
        int bytes = recvfrom(unix_sock, (char *)buffer, sizeof(buffer), 0, NULL, NULL);

        if (bytes <= 0)
            break;

        // datagrams should have a valid header followed by the nbnet packet data
        if (bytes <= NBN_IPC_HEADER_SIZE)
        {
            continue;
        } 

        // Start by reading the connection id from the IPC message header
        uint32_t conn_id =
            ((uint32_t)buffer[0] << 24) +
            ((uint32_t)buffer[1] << 16) +
            ((uint32_t)buffer[2] << 8) +
            (uint32_t)buffer[3];

        // Then read and process the actual nbnet packet
        uint8_t *packet_buffer = buffer + NBN_IPC_HEADER_SIZE;
        unsigned int packet_bytes = bytes - NBN_IPC_HEADER_SIZE;

        if (NBN_Packet_ReadProtocolId(packet_buffer, packet_bytes) != protocol_id)
            continue; // not matching the protocol of the receiver

        NBN_Connection *conn = FindOrCreateClientConnectionById(conn_id);

        if (conn == NULL)
            continue; // skip the connection

        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, conn, packet_buffer, packet_bytes) < 0)
            continue; /* not a valid packet */

        if (NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED, &packet) < 0)
        {
            NBN_LogError("Failed to raise game server event");

            return NBN_ERROR;
        }
    }

    return 0;
}

static void NBN_IPC_ServRemoveClientConnection(NBN_Connection *connection)
{
    assert(connection != NULL);

    NBN_IPC_Connection *fifo_conn = NBN_IPC_HTable_Remove(nbn_fifo_connections, ((NBN_IPC_Connection *)connection->driver_data)->id);

    if (fifo_conn)
    {
        NBN_LogDebug("Destroyed IPC connection %d", connection->id);

        NBN_Deallocator(fifo_conn);

        // TODO: notify removed connection to other service
    }
}

static int NBN_IPC_ServSendPacketTo(NBN_Packet *packet, NBN_Connection *connection)
{
    NBN_IPC_Connection *fifo_conn = (NBN_IPC_Connection *)connection->driver_data;
    uint8_t buffer[NBN_IPC_BUFFER_SIZE];
    unsigned int bytes = 0;

    memcpy(buffer, &fifo_conn->id, NBN_IPC_HEADER_SIZE);
    bytes += NBN_IPC_HEADER_SIZE;
    memcpy(buffer + NBN_IPC_HEADER_SIZE, packet->buffer, packet->size);
    bytes += packet->size;

    NBN_LogDebug("SEND: %s", unix_sock_addr.sun_path);
    if (sendto(unix_sock2, (const char *)buffer, bytes, 0, (SOCKADDR *)&unix_sock_addr, sizeof(unix_sock_addr)) == SOCKET_ERROR)
    {
        NBN_LogError("sendto() failed: %s", strerror(errno));

        return NBN_ERROR;
    }

    return 0;
}

static NBN_Connection *FindOrCreateClientConnectionById(uint32_t id)
{
    NBN_IPC_Connection *fifo_conn = NBN_IPC_HTable_Get(nbn_fifo_connections, id);

    if (fifo_conn == NULL)
    {
        /* this is a new connection */

        if (GameServer_GetClientCount() >= NBN_MAX_CLIENTS)
            return NULL;

        fifo_conn = (NBN_IPC_Connection *)NBN_Allocator(sizeof(NBN_IPC_Connection));

        fifo_conn->id = id;
        fifo_conn->conn = NBN_GameServer_CreateClientConnection(fifo_conn->id, NBN_IPC_DRIVER_ID, fifo_conn);

        NBN_IPC_HTable_Add(nbn_fifo_connections, id, fifo_conn);

        NBN_LogDebug("New UDP connection (id: %d)", fifo_conn->id);

        if (NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_CONNECTED, fifo_conn->conn) < 0)
        {
            NBN_LogError("Failed to raise game server event");

            return NULL;
        }
    }

    return fifo_conn->conn;
}

#pragma endregion /* Game server */

#pragma region Driver registering

void NBN_IPC_Register(const char *sock_path)
{
    unix_sock_path = sock_path;

    NBN_Driver_Register(
        NBN_IPC_DRIVER_ID,
        NBN_IPC_DRIVER_NAME,
        (NBN_DriverImplementation){
            // No client implementation
            NULL,
            NULL,
            NULL,
            NULL,

            // Server implementation
            NBN_IPC_ServStart,
            NBN_IPC_ServStop,
            NBN_IPC_ServRecvPackets,
            NBN_IPC_ServSendPacketTo,
            NBN_IPC_ServRemoveClientConnection
        }
    );
}

#pragma endregion /* Driver registering */

#endif /* NBNET_IMPL */
