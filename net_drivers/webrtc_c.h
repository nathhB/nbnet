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
    --- NBNET C (NATIVE) WEBRTC DRIVER ---

    WebRTC driver for the nbnet library, using a single unreliable data channel. As opposed to the other emscripten/JS
    based WebRTC driver (webrtc.h), this one is fully written in C99 and can be compiled as a native application.

    Dependencies:

        1. facil.io (https://github.com/boazsegev/facil.io)
        2. libdatachannel (https://github.com/paullouisageneau/libdatachannel)
        3. json.h (https://github.com/sheredom/json.h)
    
    How to use:

        1. Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro
        2. Call NBN_WebRTC_C_Register in both your client and server code before calling NBN_GameClient_Start or NBN_GameServer_Start
*/

#include <http.h>
#include <fio_tls.h>
#include <rtc/rtc.h>
#include <pthread.h>
#include "json.h"

#define NBN_WEBRTC_C_DRIVER_ID 2
#define NBN_WEBRTC_C_DRIVER_NAME "WebRTC_C"

void NBN_WebRTC_C_Register(void);

#ifdef NBNET_IMPL

typedef struct
{
    int id;
    int channel_id;
    ws_s *ws;
    NBN_Connection *conn;
} NBN_WebRTC_C_Peer;

static void NBN_WebRTC_C_DestroyPeer(NBN_WebRTC_C_Peer *peer);

#pragma region Hashtable

#define HTABLE_DEFAULT_INITIAL_CAPACITY 32
#define HTABLE_LOAD_FACTOR_THRESHOLD 0.75

typedef struct
{
    int peer_id;
    NBN_WebRTC_C_Peer *peer;
    unsigned int slot;
} NBN_WebRTC_C_HTableEntry;

typedef struct
{
    NBN_WebRTC_C_HTableEntry **internal_array;
    unsigned int capacity;
    unsigned int count;
    float load_factor;
} NBN_WebRTC_C_HTable;

static NBN_WebRTC_C_HTable *NBN_WebRTC_C_HTable_Create(void);
static NBN_WebRTC_C_HTable *NBN_WebRTC_C_HTable_CreateWithCapacity(unsigned int);
static void NBN_WebRTC_C_HTable_Destroy(NBN_WebRTC_C_HTable *);
static void NBN_WebRTC_C_HTable_Add(NBN_WebRTC_C_HTable *, int, NBN_WebRTC_C_Peer *);
static NBN_WebRTC_C_Peer *NBN_WebRTC_C_HTable_Get(NBN_WebRTC_C_HTable *, int);
static NBN_WebRTC_C_Peer *NBN_WebRTC_C_HTable_Remove(NBN_WebRTC_C_HTable *, int);
static void NBN_WebRTC_C_HTable_InsertEntry(NBN_WebRTC_C_HTable *, NBN_WebRTC_C_HTableEntry *);
static void NBN_WebRTC_C_HTable_RemoveEntry(NBN_WebRTC_C_HTable *, NBN_WebRTC_C_HTableEntry *);
static unsigned int NBN_WebRTC_C_HTable_FindFreeSlot(NBN_WebRTC_C_HTable *, NBN_WebRTC_C_HTableEntry *, bool *);
static NBN_WebRTC_C_HTableEntry *NBN_WebRTC_C_HTable_FindEntry(NBN_WebRTC_C_HTable *, int);
static void NBN_WebRTC_C_HTable_Grow(NBN_WebRTC_C_HTable *);

static NBN_WebRTC_C_HTable *NBN_WebRTC_C_HTable_Create(void)
{
    return NBN_WebRTC_C_HTable_CreateWithCapacity(HTABLE_DEFAULT_INITIAL_CAPACITY);
}

static NBN_WebRTC_C_HTable *NBN_WebRTC_C_HTable_CreateWithCapacity(unsigned int capacity)
{
    NBN_WebRTC_C_HTable *htable = NBN_Allocator(sizeof(NBN_WebRTC_C_HTable));

    htable->internal_array = NBN_Allocator(sizeof(NBN_WebRTC_C_HTableEntry *) * capacity);
    htable->capacity = capacity;
    htable->count = 0;
    htable->load_factor = 0;

    for (unsigned int i = 0; i < htable->capacity; i++)
        htable->internal_array[i] = NULL;

    return htable;
}

static void NBN_WebRTC_C_HTable_Destroy(NBN_WebRTC_C_HTable *htable)
{
    for (unsigned int i = 0; i < htable->capacity; i++)
    {
        NBN_WebRTC_C_HTableEntry *entry = htable->internal_array[i];

        if (entry)
        {
            NBN_WebRTC_C_DestroyPeer(entry->peer);
        }
    }

    NBN_Deallocator(htable->internal_array);
    NBN_Deallocator(htable);
}

static void NBN_WebRTC_C_HTable_Add(NBN_WebRTC_C_HTable *htable, int peer_id, NBN_WebRTC_C_Peer *peer)
{
    NBN_WebRTC_C_HTableEntry *entry = NBN_Allocator(sizeof(NBN_WebRTC_C_HTableEntry));

    entry->peer_id = peer_id;
    entry->peer = peer;

    NBN_WebRTC_C_HTable_InsertEntry(htable, entry);

    if (htable->load_factor >= HTABLE_LOAD_FACTOR_THRESHOLD)
        NBN_WebRTC_C_HTable_Grow(htable);
}

static NBN_WebRTC_C_Peer *NBN_WebRTC_C_HTable_Get(NBN_WebRTC_C_HTable *htable, int peer_id)
{
    NBN_WebRTC_C_HTableEntry *entry = NBN_WebRTC_C_HTable_FindEntry(htable, peer_id);

    return entry ? entry->peer : NULL;
}

static NBN_WebRTC_C_Peer *NBN_WebRTC_C_HTable_Remove(NBN_WebRTC_C_HTable *htable, int peer_id)
{
    NBN_WebRTC_C_HTableEntry *entry = NBN_WebRTC_C_HTable_FindEntry(htable, peer_id);

    if (entry)
    {
        NBN_WebRTC_C_Peer *peer = entry->peer;

        NBN_WebRTC_C_HTable_RemoveEntry(htable, entry);

        return peer;
    }

    return NULL;
}

static void NBN_WebRTC_C_HTable_InsertEntry(NBN_WebRTC_C_HTable *htable, NBN_WebRTC_C_HTableEntry *entry)
{
    bool use_existing_slot = false;
    unsigned int slot = NBN_WebRTC_C_HTable_FindFreeSlot(htable, entry, &use_existing_slot);

    entry->slot = slot;
    htable->internal_array[slot] = entry;

    if (!use_existing_slot)
    {
        htable->count++;
        htable->load_factor = (float)htable->count / htable->capacity;
    }
}

static void NBN_WebRTC_C_HTable_RemoveEntry(NBN_WebRTC_C_HTable *htable, NBN_WebRTC_C_HTableEntry *entry)
{
    htable->internal_array[entry->slot] = NULL;

    NBN_Deallocator(entry);

    htable->count--;
    htable->load_factor = htable->count / htable->capacity;
}

static unsigned int NBN_WebRTC_C_HTable_FindFreeSlot(NBN_WebRTC_C_HTable *htable, NBN_WebRTC_C_HTableEntry *entry, bool *use_existing_slot)
{
    unsigned long hash = entry->peer_id;
    unsigned int slot;

    // quadratic probing

    NBN_WebRTC_C_HTableEntry *current_entry;
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

static NBN_WebRTC_C_HTableEntry *NBN_WebRTC_C_HTable_FindEntry(NBN_WebRTC_C_HTable *htable, int peer_id)
{
    unsigned long hash = peer_id;
    unsigned int slot;

    //quadratic probing

    NBN_WebRTC_C_HTableEntry *current_entry;
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

static void NBN_WebRTC_C_HTable_Grow(NBN_WebRTC_C_HTable *htable)
{
    unsigned int old_capacity = htable->capacity;
    unsigned int new_capacity = old_capacity * 2;
    NBN_WebRTC_C_HTableEntry** old_internal_array = htable->internal_array;
    NBN_WebRTC_C_HTableEntry** new_internal_array = NBN_Allocator(sizeof(NBN_WebRTC_C_HTableEntry*) * new_capacity);

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
            NBN_WebRTC_C_HTable_InsertEntry(htable, old_internal_array[i]);
    }

    NBN_Deallocator(old_internal_array);
}

#pragma endregion // Hashtable

#pragma region String utils

static int NBN_WebRTC_C_StringReplaceAll(char *res, unsigned int len, const char *str, const char *a, const char *b)
{
    char *substr = strstr(str, a);
    char *res2 = malloc(len);

    if (substr)
    {
        size_t len_a = strlen(a);
        size_t len_b = strlen(b);

        if (strlen(str) + (len_b - len_a) >= len)
        {
            return -1;
        }

        int pos = substr - str;

        strncpy(res2, str, pos);
        strncpy(res2 + pos, b, len_b);
        strncpy(res2 + pos + len_b, str + pos + len_a, len - (pos + len_a) + 1);
        memcpy(res, res2, len);
        NBN_WebRTC_C_StringReplaceAll(res, len, res2, a, b);
        free(res2);
    }
    else
    {
        memcpy(res, str, len);
    }

    return 0;
}

#pragma endregion /* String utils */

#pragma region Signaling

typedef enum
{
    NBN_WEBRTC_C_UNDEFINED,
    NBN_WEBRTC_C_OFFER
} NBN_WebRTC_C_SignalingPayloadType;

typedef struct
{
    NBN_WebRTC_C_SignalingPayloadType type;
    const char *sdp;
    struct json_value_s *json;
} NBN_WebRTC_C_SignalingPayload;

#define DEFAULT_ICE_SERVER "stun:stun01.sipphone.com"

#ifndef ICE_SERVERS
#define ICE_SERVERS DEFAULT_ICE_SERVER
#endif

static const char *ice_servers[] = {
   DEFAULT_ICE_SERVER
};

static bool NBN_WebRTC_C_ParseSignalingMessage(const char *msg, size_t msg_len, NBN_WebRTC_C_SignalingPayload *payload)
{
    struct json_value_s* root = json_parse(msg, msg_len);
    
    if (root->type != json_type_object)
    {
        free(root);
        return false;
    }

    struct json_object_s* object = (struct json_object_s*)root->payload;
    struct json_object_element_s *curr = object->start;

    payload->json = root;
    payload->type = NBN_WEBRTC_C_UNDEFINED;
    payload->sdp = NULL;

    while (curr != NULL)
    {
        if (strncmp(curr->name->string, "type", 4) == 0)
        {
            payload->type = NBN_WEBRTC_C_OFFER;
        }
        else if (strncmp(curr->name->string, "sdp", 4) == 0)
        {
            struct json_string_s *str = json_value_as_string(curr->value);

            if (str)
            {
                payload->sdp = str->string;
            }
        }

        curr = curr->next;
    }

    if (payload->type == NBN_WEBRTC_C_UNDEFINED)
    {
        free(root);
        return false;
    }

    return true;
}

static char *NBN_WebRTC_C_EscapeSDP(const char *sdp)
{
    unsigned int len = strlen(sdp) * 2;
    char *escaped_sdp = malloc(len);

    if (NBN_WebRTC_C_StringReplaceAll(escaped_sdp, len, sdp, "\r\n", "\\r\\n") < 0)
    {
        free(escaped_sdp);
        return NULL;
    }

    return escaped_sdp;
}

static void NBN_WebRTC_C_OnLocalDescriptionCallback(int pc, const char *sdp, const char *type, void *user_ptr)
{
    if (strncmp(type, "answer", 5) != 0) return;

    ws_s *ws = user_ptr;
    fio_str_info_s msg;
    char data[1024];
    char *escaped_sdp = NBN_WebRTC_C_EscapeSDP(sdp);

    assert(escaped_sdp);
    snprintf(data, sizeof(data), "{\"type\":\"answer\", \"sdp\":\"%s\"}", escaped_sdp);
    free(escaped_sdp);

    msg.data = data;
    msg.len = strnlen(data, sizeof(data)); 

    websocket_write(ws, msg, 1);
}

#pragma endregion /* Signaling */

#pragma region Game server

static NBN_WebRTC_C_HTable *nbn_wrtc_c_peers = NULL;
static pthread_t thread;
static uint16_t ws_port;
static fio_tls_s *tls;

static void NBN_WebRTC_C_DestroyPeer(NBN_WebRTC_C_Peer *peer)
{
    NBN_LogDebug("Destroy peer %d", peer->id);
    rtcClose(peer->channel_id);
    rtcDelete(peer->channel_id);
    rtcClosePeerConnection(peer->id);
    rtcDeletePeerConnection(peer->id);
    NBN_Deallocator(peer);
}

static void NBN_WebRTC_C_OnWsOpen(ws_s *ws)
{
    int peer_id = rtcCreatePeerConnection(&(rtcConfiguration){
        .iceServers = ice_servers,
        .iceServersCount = 1,
        .disableAutoNegotiation = false
    });

    if (peer_id < 0)
    {
        NBN_LogError("Failed to create peer: %d", peer_id);
        return;
    }

    // save a pointer to the WS in peer's user data
    rtcSetUserPointer(peer_id, ws);

    int ret = rtcSetLocalDescriptionCallback(peer_id, NBN_WebRTC_C_OnLocalDescriptionCallback);

    if (ret < 0)
    {
        NBN_LogError("Failed to register local description callback for peer %d: %d", peer_id, ret);
        rtcClosePeerConnection(peer_id);
        rtcDeletePeerConnection(peer_id);
        return;
    }

    int channel_id = rtcCreateDataChannelEx(peer_id, "unreliable", &(rtcDataChannelInit){
        .reliability = {.unordered = true, .unreliable = true, .maxPacketLifeTime = 1000, .maxRetransmits = 0},
        .negotiated = true,
        .manualStream = true,
        .stream = 0
    });

    if (channel_id < 0)
    {
        NBN_LogError("Failed to create data channel for peer %d: %d", peer_id, channel_id);
        rtcClosePeerConnection(peer_id);
        rtcDeletePeerConnection(peer_id);
        return;
    }

    NBN_LogDebug("Successfully created data channel for peer %d: %d", peer_id, channel_id);

    NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer *)NBN_Allocator(sizeof(NBN_WebRTC_C_Peer));

    peer->id = peer_id;
    peer->channel_id = channel_id;
    peer->ws = ws;
    peer->conn = NBN_GameServer_CreateClientConnection(NBN_WEBRTC_C_DRIVER_ID, peer);

    websocket_udata_set(ws, peer); // save a pointer to the peer in websocket's user data
    NBN_WebRTC_C_HTable_Add(nbn_wrtc_c_peers, peer_id, peer);
    NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_CONNECTED, peer->conn);
}

static void NBN_WebRTC_C_OnWsClose(intptr_t uuid, void *udata)
{
    if (uuid >= 0 && udata)
    {
        NBN_WebRTC_C_Peer *peer = udata;

        NBN_LogDebug("WebSocket %d closed (peer id %d)", uuid, peer->id);
    }
}

static void NBN_WebRTC_C_OnWsMessage(ws_s *ws, fio_str_info_s msg, uint8_t is_text)
{
    NBN_WebRTC_C_SignalingPayload payload;

    bool ret = NBN_WebRTC_C_ParseSignalingMessage(msg.data, msg.len, &payload);

    if (!ret) return;

    if (payload.type == NBN_WEBRTC_C_OFFER && payload.sdp)
    {
        NBN_WebRTC_C_Peer *peer = websocket_udata_get(ws);

        int ret = rtcSetRemoteDescription(peer->id, payload.sdp, "offer");

        if (ret < 0)
        {
            NBN_LogError("Failed to set remote description for peer %d: %d", peer->id, ret);
        }
    }

    free(payload.json);
}

static void NBN_WebRTC_C_OnHttpRequest(http_s *request)
{
    const char *msg = "This is a websocket server\n";

    http_set_header(request, HTTP_HEADER_CONTENT_TYPE, http_mimetype_find("txt", 3));
    http_send_body(request, (void *)msg, strlen(msg));
}

static void NBN_WebRTC_C_OnHttpUpgrade(http_s *request, char *target, size_t len)
{
    if (len >= 9 && target[1] == 'e')
    {
        http_upgrade2ws(
            request,
            .on_message = NBN_WebRTC_C_OnWsMessage,
            .on_open = NBN_WebRTC_C_OnWsOpen,
            .on_close = NBN_WebRTC_C_OnWsClose);
    }
    else if (len >= 3 && target[0] == 's')
    {
        http_upgrade2sse(request, .on_open = NULL);
    }
    else
    {
        http_send_error(request, 400);
    }
}

static void *NBN_WebRTC_C_HttpServerThread(void *data)
{
    char port_str[16];

    snprintf(port_str, sizeof(port_str), "%d", ws_port);

    if (http_listen(port_str, NULL,
        .tls = tls,
        .on_request = NBN_WebRTC_C_OnHttpRequest,
        .on_upgrade = NBN_WebRTC_C_OnHttpUpgrade) < 0)
    {
        NBN_LogError("Failed to start websocket server");
        return NULL;
    }

    fio_start(.threads = 1);
    return NULL;
}

static int NBN_WebRTC_C_ServStart(uint32_t protocol_id, uint16_t port)
{
    // protocol_id is not used

    ws_port = port;
    nbn_wrtc_c_peers = NBN_WebRTC_C_HTable_Create();
    tls = NULL;

#ifdef NBN_USE_HTTPS

    tls = fio_tls_new(
        NBN_HTTPS_SERVER_NAME,
        NBN_HTTPS_CERT_PEM,
        NBN_HTTPS_KEY_PEM,
        NULL);

    NBN_LogDebug("Using HTTPS (server name: %s, cert: %s, key: %s)",
        NBN_HTTPS_SERVER_NAME, NBN_HTTPS_CERT_PEM, NBN_HTTPS_KEY_PEM);

#endif // NBN_USE_HTTPS

    if (pthread_create(&thread, NULL, NBN_WebRTC_C_HttpServerThread, tls) < 0)
    {
        NBN_LogError("Failed to start HTTP server thread");
        return NBN_ERROR;
    }

    return 0;
}

static void NBN_WebRTC_C_ServStop(void)
{
    pthread_join(thread, NULL);
    NBN_WebRTC_C_HTable_Destroy(nbn_wrtc_c_peers);
    rtcCleanup();

    if (tls)
    {
        fio_tls_destroy(tls);
    }
}

static int NBN_WebRTC_C_ServRecvPackets(void)
{
    char buffer[NBN_PACKET_MAX_SIZE];
    int size;

    for (unsigned int i = 0; i < nbn_wrtc_c_peers->capacity; i++)
    {
        NBN_WebRTC_C_HTableEntry *entry = nbn_wrtc_c_peers->internal_array[i];

        if (entry)
        {
            size = NBN_PACKET_MAX_SIZE;

            while (rtcReceiveMessage(entry->peer->channel_id, buffer, &size) != RTC_ERR_NOT_AVAIL)
            {
                NBN_Packet packet;

                if (NBN_Packet_InitRead(&packet, entry->peer->conn, (uint8_t *)buffer, size) < 0)
                    continue;

                packet.sender = entry->peer->conn;
                NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED, &packet);
            }
        }
    }
    return 0;
}

static void NBN_WebRTC_C_ServRemoveClientConnection(NBN_Connection *conn)
{
    NBN_WebRTC_C_Peer *peer = conn->driver_data;

    NBN_WebRTC_C_HTable_Remove(nbn_wrtc_c_peers, peer->id);
    NBN_WebRTC_C_DestroyPeer(peer);
}

static int NBN_WebRTC_C_ServSendPacketTo(NBN_Packet *packet, NBN_Connection *conn)
{
    if (!conn->is_accepted) return 0;

    NBN_WebRTC_C_Peer *peer = conn->driver_data;

    if (rtcSendMessage(peer->channel_id, (char *)packet->buffer, packet->size) < 0)
    {
        NBN_LogDebug("rtcSendMessage failed for peer %d", peer->id);
    }

    return 0;
}

#pragma endregion /* Game server */

void NBN_WebRTC_C_Register(void)
{
    NBN_DriverImplementation driver_impl {
        // Client implementation
        NULL,
        NULL,
        NULL,
        NULL,

        // Server implementation
        NBN_WebRTC_C_ServStart,
        NBN_WebRTC_C_ServStop,
        NBN_WebRTC_C_ServRecvPackets,
        NBN_WebRTC_C_ServSendPacketTo,
        NBN_WebRTC_C_ServRemoveClientConnection
    };

    NBN_Driver_Register(
        NBN_WEBRTC_C_DRIVER_ID,
        NBN_WEBRTC_C_DRIVER_NAME,
        driver_impl
    );
}

#endif // NBNET_IMPL
