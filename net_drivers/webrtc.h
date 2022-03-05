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

typedef struct
{
    uint32_t id;
    NBN_Connection *conn;
} NBN_Peer;

#pragma region Hashtable

#define HTABLE_DEFAULT_INITIAL_CAPACITY 32
#define HTABLE_LOAD_FACTOR_THRESHOLD 0.75

typedef struct
{
    uint32_t peer_id;
    NBN_Peer *peer;
    unsigned int slot;
} HTableEntry;

typedef struct
{
    HTableEntry **internal_array;
    unsigned int capacity;
    unsigned int count;
    float load_factor;
} HTable;

static HTable *HTable_Create();
static HTable *HTable_CreateWithCapacity(unsigned int);
static void HTable_Destroy(HTable *);
static void HTable_Add(HTable *, uint32_t, NBN_Peer *);
static NBN_Peer *HTable_Get(HTable *, uint32_t);
static NBN_Peer *HTable_Remove(HTable *, uint32_t);
static void HTable_InsertEntry(HTable *, HTableEntry *);
static void HTable_RemoveEntry(HTable *, HTableEntry *);
static unsigned int HTable_FindFreeSlot(HTable *, HTableEntry *, bool *);
static HTableEntry *HTable_FindEntry(HTable *, uint32_t);
static void HTable_Grow(HTable *);

HTable *HTable_Create()
{
    return HTable_CreateWithCapacity(HTABLE_DEFAULT_INITIAL_CAPACITY);
}

HTable *HTable_CreateWithCapacity(unsigned int capacity)
{
    HTable *htable = NBN_Allocator(sizeof(HTable));

    htable->internal_array = NBN_Allocator(sizeof(HTableEntry *) * capacity);
    htable->capacity = capacity;
    htable->count = 0;
    htable->load_factor = 0;

    for (unsigned int i = 0; i < htable->capacity; i++)
        htable->internal_array[i] = NULL;

    return htable;
}

void HTable_Destroy(HTable *htable)
{
    for (unsigned int i = 0; i < htable->capacity; i++)
    {
        HTableEntry *entry = htable->internal_array[i];

        if (entry)
            NBN_Deallocator(entry);
    }

    NBN_Deallocator(htable->internal_array);
    NBN_Deallocator(htable);
}

static void HTable_Add(HTable *htable, uint32_t peer_id, NBN_Peer *peer)
{
    HTableEntry *entry = NBN_Allocator(sizeof(HTableEntry));

    entry->peer_id = peer_id;
    entry->peer = peer;

    HTable_InsertEntry(htable, entry);

    if (htable->load_factor >= HTABLE_LOAD_FACTOR_THRESHOLD)
        HTable_Grow(htable);
}

NBN_Peer *HTable_Get(HTable *htable, uint32_t peer_id)
{
    HTableEntry *entry = HTable_FindEntry(htable, peer_id);

    return entry ? entry->peer : NULL;
}

static NBN_Peer *HTable_Remove(HTable *htable, uint32_t peer_id)
{
    HTableEntry *entry = HTable_FindEntry(htable, peer_id);

    if (entry)
    {
        HTable_RemoveEntry(htable, entry);

        return entry->peer;
    }

    return NULL;
}

static void HTable_InsertEntry(HTable *htable, HTableEntry *entry)
{
    bool use_existing_slot = false;
    unsigned int slot = HTable_FindFreeSlot(htable, entry, &use_existing_slot);

    entry->slot = slot;
    htable->internal_array[slot] = entry;

    if (!use_existing_slot)
    {
        htable->count++;
        htable->load_factor = (float)htable->count / htable->capacity;
    }
}

static void HTable_RemoveEntry(HTable *htable, HTableEntry *entry)
{
    htable->internal_array[entry->slot] = NULL;

    NBN_Deallocator(entry);

    htable->count--;
    htable->load_factor = htable->count / htable->capacity;
}

static unsigned int HTable_FindFreeSlot(HTable *htable, HTableEntry *entry, bool *use_existing_slot)
{
    unsigned long hash = entry->peer_id;
    unsigned int slot;

    // quadratic probing

    HTableEntry *current_entry;
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

static HTableEntry *HTable_FindEntry(HTable *htable, uint32_t peer_id)
{
    unsigned long hash = peer_id;
    unsigned int slot;

    //quadratic probing

    HTableEntry *current_entry;
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

static void HTable_Grow(HTable *htable)
{
    unsigned int old_capacity = htable->capacity;
    unsigned int new_capacity = old_capacity * 2;
    HTableEntry** old_internal_array = htable->internal_array;
    HTableEntry** new_internal_array = NBN_Allocator(sizeof(HTableEntry*) * new_capacity);

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
            HTable_InsertEntry(htable, old_internal_array[i]);
    }

    NBN_Deallocator(old_internal_array);
}

#pragma endregion // Hashtable

#pragma region Game server

/* --- JS API --- */

NBN_EXTERN void __js_game_server_init(uint32_t, bool, const char *, const char *);
NBN_EXTERN int __js_game_server_start(uint16_t);
NBN_EXTERN uint8_t *__js_game_server_dequeue_packet(uint32_t *, unsigned int *);
NBN_EXTERN int __js_game_server_send_packet_to(uint8_t *, unsigned int, uint32_t);
NBN_EXTERN void __js_game_server_close_client_peer(unsigned int);
NBN_EXTERN void __js_game_server_stop(void);

/* --- Driver implementation --- */

static HTable *__peers = NULL;

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

    __peers = HTable_Create();

    return 0;
}

void NBN_Driver_GServ_Stop(void)
{
    __js_game_server_stop();
    HTable_Destroy(__peers);
}

int NBN_Driver_GServ_RecvPackets(void)
{
    uint8_t *data;
    uint32_t peer_id;
    unsigned int len;

    while ((data = __js_game_server_dequeue_packet(&peer_id, &len)) != NULL)
    {
        NBN_Packet packet;

        NBN_Peer *peer = HTable_Get(__peers, peer_id);

        if (peer == NULL)
        {
            if (GameServer_GetClientCount() >= NBN_MAX_CLIENTS)
                continue;

            NBN_LogTrace("Peer %d has connected", peer_id);

            peer = (NBN_Peer *)NBN_Allocator(sizeof(NBN_Peer));

            peer->id = peer_id; 
            peer->conn = NBN_GameServer_CreateClientConnection(peer_id, peer);

            HTable_Add(__peers, peer_id, peer);

            NBN_Driver_GServ_RaiseEvent(NBN_DRIVER_GSERV_CLIENT_CONNECTED, peer->conn);
        }

        if (NBN_Packet_InitRead(&packet, peer->conn, data, len) < 0)
            continue;

        packet.sender = peer->conn;

        NBN_Driver_GServ_RaiseEvent(NBN_DRIVER_GSERV_CLIENT_PACKET_RECEIVED, &packet);
    }

    return 0;
}

void NBN_Driver_GServ_RemoveClientConnection(NBN_Connection *conn)
{
    assert(conn != NULL);

    __js_game_server_close_client_peer(conn->id);

    NBN_Peer *peer = HTable_Remove(__peers, ((NBN_Peer *)conn->driver_data)->id);

    if (peer)
    {
        NBN_LogDebug("Destroyed peer %d", peer->id);

        NBN_Deallocator(peer);
    }
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
