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
    --- NBNET WEBRTC DRIVER ---

    WebRTC driver using a single unreliable data channel for the nbnet library.

    How to use:

        1. Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro
        2. Call NBN_WebRTC_Register in both your client and server code before calling NBN_GameClient_Start or NBN_GameServer_Start
*/

void NBN_WebRTC_Register(void);

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

typedef struct
{
    uint32_t id;
    NBN_Connection *conn;
} NBN_WebRTC_Peer;

#pragma region Hashtable

#define HTABLE_DEFAULT_INITIAL_CAPACITY 32
#define HTABLE_LOAD_FACTOR_THRESHOLD 0.75

typedef struct
{
    uint32_t peer_id;
    NBN_WebRTC_Peer *peer;
    unsigned int slot;
} NBN_WebRTC_HTableEntry;

typedef struct
{
    NBN_WebRTC_HTableEntry **internal_array;
    unsigned int capacity;
    unsigned int count;
    float load_factor;
} NBN_WebRTC_HTable;

static NBN_WebRTC_HTable *NBN_WebRTC_HTable_Create(void);
static NBN_WebRTC_HTable *NBN_WebRTC_HTable_CreateWithCapacity(unsigned int);
static void NBN_WebRTC_HTable_Destroy(NBN_WebRTC_HTable *);
static void NBN_WebRTC_HTable_Add(NBN_WebRTC_HTable *, uint32_t, NBN_WebRTC_Peer *);
static NBN_WebRTC_Peer *NBN_WebRTC_HTable_Get(NBN_WebRTC_HTable *, uint32_t);
static NBN_WebRTC_Peer *NBN_WebRTC_HTable_Remove(NBN_WebRTC_HTable *, uint32_t);
static void NBN_WebRTC_HTable_InsertEntry(NBN_WebRTC_HTable *, NBN_WebRTC_HTableEntry *);
static void NBN_WebRTC_HTable_RemoveEntry(NBN_WebRTC_HTable *, NBN_WebRTC_HTableEntry *);
static unsigned int NBN_WebRTC_HTable_FindFreeSlot(NBN_WebRTC_HTable *, NBN_WebRTC_HTableEntry *, bool *);
static NBN_WebRTC_HTableEntry *NBN_WebRTC_HTable_FindEntry(NBN_WebRTC_HTable *, uint32_t);
static void NBN_WebRTC_HTable_Grow(NBN_WebRTC_HTable *);

static NBN_WebRTC_HTable *NBN_WebRTC_HTable_Create(void)
{
    return NBN_WebRTC_HTable_CreateWithCapacity(HTABLE_DEFAULT_INITIAL_CAPACITY);
}

static NBN_WebRTC_HTable *NBN_WebRTC_HTable_CreateWithCapacity(unsigned int capacity)
{
    NBN_WebRTC_HTable *htable = NBN_Allocator(sizeof(NBN_WebRTC_HTable));

    htable->internal_array = NBN_Allocator(sizeof(NBN_WebRTC_HTableEntry *) * capacity);
    htable->capacity = capacity;
    htable->count = 0;
    htable->load_factor = 0;

    for (unsigned int i = 0; i < htable->capacity; i++)
        htable->internal_array[i] = NULL;

    return htable;
}

static void NBN_WebRTC_HTable_Destroy(NBN_WebRTC_HTable *htable)
{
    for (unsigned int i = 0; i < htable->capacity; i++)
    {
        NBN_WebRTC_HTableEntry *entry = htable->internal_array[i];

        if (entry)
            NBN_Deallocator(entry);
    }

    NBN_Deallocator(htable->internal_array);
    NBN_Deallocator(htable);
}

static void NBN_WebRTC_HTable_Add(NBN_WebRTC_HTable *htable, uint32_t peer_id, NBN_WebRTC_Peer *peer)
{
    NBN_WebRTC_HTableEntry *entry = NBN_Allocator(sizeof(NBN_WebRTC_HTableEntry));

    entry->peer_id = peer_id;
    entry->peer = peer;

    NBN_WebRTC_HTable_InsertEntry(htable, entry);

    if (htable->load_factor >= HTABLE_LOAD_FACTOR_THRESHOLD)
        NBN_WebRTC_HTable_Grow(htable);
}

static NBN_WebRTC_Peer *NBN_WebRTC_HTable_Get(NBN_WebRTC_HTable *htable, uint32_t peer_id)
{
    NBN_WebRTC_HTableEntry *entry = NBN_WebRTC_HTable_FindEntry(htable, peer_id);

    return entry ? entry->peer : NULL;
}

static NBN_WebRTC_Peer *NBN_WebRTC_HTable_Remove(NBN_WebRTC_HTable *htable, uint32_t peer_id)
{
    NBN_WebRTC_HTableEntry *entry = NBN_WebRTC_HTable_FindEntry(htable, peer_id);

    if (entry)
    {
        NBN_WebRTC_Peer *peer = entry->peer;

        NBN_WebRTC_HTable_RemoveEntry(htable, entry);

        return peer;
    }

    return NULL;
}

static void NBN_WebRTC_HTable_InsertEntry(NBN_WebRTC_HTable *htable, NBN_WebRTC_HTableEntry *entry)
{
    bool use_existing_slot = false;
    unsigned int slot = NBN_WebRTC_HTable_FindFreeSlot(htable, entry, &use_existing_slot);

    entry->slot = slot;
    htable->internal_array[slot] = entry;

    if (!use_existing_slot)
    {
        htable->count++;
        htable->load_factor = (float)htable->count / htable->capacity;
    }
}

static void NBN_WebRTC_HTable_RemoveEntry(NBN_WebRTC_HTable *htable, NBN_WebRTC_HTableEntry *entry)
{
    htable->internal_array[entry->slot] = NULL;

    NBN_Deallocator(entry);

    htable->count--;
    htable->load_factor = htable->count / htable->capacity;
}

static unsigned int NBN_WebRTC_HTable_FindFreeSlot(NBN_WebRTC_HTable *htable, NBN_WebRTC_HTableEntry *entry, bool *use_existing_slot)
{
    unsigned long hash = entry->peer_id;
    unsigned int slot;

    // quadratic probing

    NBN_WebRTC_HTableEntry *current_entry;
    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % htable->capacity;
        current_entry = htable->internal_array[slot];

        i++;
    } while (current_entry != NULL && current_entry->peer_id != entry->peer_id);

    if (current_entry != NULL) // it means the current entry as the same key as the inserted entry
    {
        *use_existing_slot = true;

        NBN_Deallocator(current_entry);
    }
    
    return slot;
}

static NBN_WebRTC_HTableEntry *NBN_WebRTC_HTable_FindEntry(NBN_WebRTC_HTable *htable, uint32_t peer_id)
{
    unsigned long hash = peer_id;
    unsigned int slot;

    //quadratic probing

    NBN_WebRTC_HTableEntry *current_entry;
    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % htable->capacity;
        current_entry = htable->internal_array[slot];

        if (current_entry != NULL && current_entry->peer_id == peer_id)
        {
            return current_entry;
        }

        i++;
    } while (i < htable->capacity);
    
    return NULL;
}

static void NBN_WebRTC_HTable_Grow(NBN_WebRTC_HTable *htable)
{
    unsigned int old_capacity = htable->capacity;
    unsigned int new_capacity = old_capacity * 2;
    NBN_WebRTC_HTableEntry** old_internal_array = htable->internal_array;
    NBN_WebRTC_HTableEntry** new_internal_array = NBN_Allocator(sizeof(NBN_WebRTC_HTableEntry*) * new_capacity);

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
            NBN_WebRTC_HTable_InsertEntry(htable, old_internal_array[i]);
    }

    NBN_Deallocator(old_internal_array);
}

#pragma endregion // Hashtable

#pragma region Game server

/* --- JS API --- */

NBN_EXTERN void __js_game_server_init(uint32_t, bool, const char *, const char *);
NBN_EXTERN int __js_game_server_start(uint16_t);
NBN_EXTERN int __js_game_server_dequeue_packet(uint32_t *, uint8_t *);
NBN_EXTERN int __js_game_server_send_packet_to(uint8_t *, unsigned int, uint32_t);
NBN_EXTERN void __js_game_server_close_client_peer(unsigned int);
NBN_EXTERN void __js_game_server_stop(void);

/* --- Driver implementation --- */

static NBN_WebRTC_HTable *nbn_wrtc_peers = NULL;
static uint8_t nbn_packet_buffer[NBN_PACKET_MAX_SIZE];

static int NBN_WebRTC_ServStart(uint32_t protocol_id, uint16_t port)
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

    nbn_wrtc_peers = NBN_WebRTC_HTable_Create();

    return 0;
}

static void NBN_WebRTC_ServStop(void)
{
    __js_game_server_stop();
    NBN_WebRTC_HTable_Destroy(nbn_wrtc_peers);
}

static int NBN_WebRTC_ServRecvPackets(void)
{
    uint32_t peer_id;
    unsigned int len;

    while ((len = __js_game_server_dequeue_packet(&peer_id, (uint8_t *)nbn_packet_buffer)) > 0)
    {
        NBN_Packet packet;

        NBN_WebRTC_Peer *peer = NBN_WebRTC_HTable_Get(nbn_wrtc_peers, peer_id);

        if (peer == NULL)
        {
            if (GameServer_GetClientCount() >= NBN_MAX_CLIENTS)
                continue;

            NBN_LogTrace("Peer %d has connected", peer_id);

            peer = (NBN_WebRTC_Peer *)NBN_Allocator(sizeof(NBN_WebRTC_Peer));

            peer->id = peer_id; 
            peer->conn = NBN_GameServer_CreateClientConnection(NBN_WEBRTC_DRIVER_ID, peer);

            NBN_WebRTC_HTable_Add(nbn_wrtc_peers, peer_id, peer);

            NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_CONNECTED, peer->conn);
        }

        if (NBN_Packet_InitRead(&packet, peer->conn, nbn_packet_buffer, len) < 0)
            continue;

        packet.sender = peer->conn;

        NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED, &packet);
    }

    return 0;
}

static void NBN_WebRTC_ServRemoveClientConnection(NBN_Connection *conn)
{
    assert(conn != NULL);

    __js_game_server_close_client_peer(conn->id);

    NBN_WebRTC_Peer *peer = NBN_WebRTC_HTable_Remove(nbn_wrtc_peers, ((NBN_WebRTC_Peer *)conn->driver_data)->id);

    if (peer)
    {
        NBN_LogDebug("Destroyed peer %d", peer->id);

        NBN_Deallocator(peer);
    }
}

static int NBN_WebRTC_ServSendPacketTo(NBN_Packet *packet, NBN_Connection *conn)
{
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

static NBN_Connection *nbn_wrtc_server = NULL;
static bool is_connected_to_server = false;

static int NBN_WebRTC_CliStart(uint32_t protocol_id, const char *host, uint16_t port)
{
#ifdef NBN_USE_HTTPS
    __js_game_client_init(protocol_id, true);
#else
    __js_game_client_init(protocol_id, false);
#endif // NBN_USE_HTTPS

    nbn_wrtc_server = NBN_GameClient_CreateServerConnection(NBN_WEBRTC_DRIVER_ID, NULL);

    int res;

    if ((res = __js_game_client_start(host, port)) < 0)
        return -1;

    return 0;
}

static void NBN_WebRTC_CliStop(void)
{
    __js_game_client_close();
}

static int NBN_WebRTC_CliRecvPackets(void)
{
    unsigned int len;

    while ((len = __js_game_client_dequeue_packet((uint8_t *)nbn_packet_buffer)) > 0)
    {
        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, nbn_wrtc_server, nbn_packet_buffer, len) < 0)
            continue;

        if (!is_connected_to_server)
        {
            NBN_Driver_RaiseEvent(NBN_DRIVER_CLI_CONNECTED, NULL);

            is_connected_to_server = true;
        }

        NBN_Driver_RaiseEvent(NBN_DRIVER_CLI_PACKET_RECEIVED, &packet);
    }

    return 0;
}

static int NBN_WebRTC_CliSendPacket(NBN_Packet *packet)
{
    return __js_game_client_send_packet(packet->buffer, packet->size);
}

#pragma endregion /* Game client */

#pragma region Driver registering

void NBN_WebRTC_Register(void)
{
    NBN_DriverImplementation driver_impl = {
        // Client implementation
        NBN_WebRTC_CliStart,
        NBN_WebRTC_CliStop,
        NBN_WebRTC_CliRecvPackets,
        NBN_WebRTC_CliSendPacket,

        // Server implementation
        NBN_WebRTC_ServStart,
        NBN_WebRTC_ServStop,
        NBN_WebRTC_ServRecvPackets,
        NBN_WebRTC_ServSendPacketTo,
        NBN_WebRTC_ServRemoveClientConnection
    };

    NBN_Driver_Register(
        NBN_WEBRTC_DRIVER_ID,
        NBN_WEBRTC_DRIVER_NAME,
        driver_impl
    );
}

#pragma endregion /* Driver registering */

#endif /* NBNET_IMPL */
