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
    --- NBNET NATIVE WEBRTC DRIVER ---

    WebRTC driver for the nbnet library, using a single unreliable data channel. As opposed to the other emscripten/JS
    based WebRTC driver (webrtc.h), this one is fully written in C99 and can be compiled as a native application.

    Dependencies:

        1. libdatachannel (https://github.com/paullouisageneau/libdatachannel)
        2. json.h (https://github.com/sheredom/json.h)
    
    How to use:

        1. Include this header *once* after the nbnet header in the same file where you defined the NBNET_IMPL macro
        2. Call NBN_WebRTC_C_Register in both your client and server code before calling NBN_GameClient_Start or NBN_GameServer_Start
*/

#include <time.h>
#include <string.h>
#include <rtc/rtc.h>
#include "json.h"

#define NBN_WEBRTC_C_DRIVER_ID 2
#define NBN_WEBRTC_C_DRIVER_NAME "WebRTC_C"

typedef struct NBN_WebRTC_C_Config
{
    bool enable_tls;
    const char *cert_path;
    const char *key_path;
    const char *passphrase;
    const char **ice_servers;
    unsigned int ice_servers_count;
    rtcLogLevel log_level;
} NBN_WebRTC_C_Config;

static NBN_WebRTC_C_Config nbn_wrtc_c_cfg;

void NBN_WebRTC_C_Register(NBN_WebRTC_C_Config config);
void NBN_WebRTC_C_Unregister(void);

#ifdef NBNET_IMPL

typedef struct
{
    int id;
    int channel_id;
    int ws;
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
    NBN_WebRTC_C_HTable *htable = (NBN_WebRTC_C_HTable*)NBN_Allocator(sizeof(NBN_WebRTC_C_HTable));

    htable->internal_array = (NBN_WebRTC_C_HTableEntry**)NBN_Allocator(sizeof(NBN_WebRTC_C_HTableEntry *) * capacity);
    htable->capacity = capacity;
    htable->count = 0;
    htable->load_factor = 0;

    for (unsigned int i = 0; i < htable->capacity; i++)
    {
        htable->internal_array[i] = NULL;
    }

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
    NBN_WebRTC_C_HTableEntry *entry = (NBN_WebRTC_C_HTableEntry*)NBN_Allocator(sizeof(NBN_WebRTC_C_HTableEntry));

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
    NBN_WebRTC_C_HTableEntry** new_internal_array = (NBN_WebRTC_C_HTableEntry**)NBN_Allocator(sizeof(NBN_WebRTC_C_HTableEntry*) * new_capacity);

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

// IMPORTANT: res needs to be pre allocated and big enough to old the resulting string
static void NBN_WebRTC_C_StringReplaceAll(char *res, const char *str, const char *a, const char *b)
{
    char *substr = (char*)strstr(str, a);
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (substr)
    {
        int pos = substr - str;

        strncpy(res, str, pos);
        strncpy(res + pos, b, len_b);

        NBN_WebRTC_C_StringReplaceAll(res + pos + len_b, str + pos + len_a, a, b);
    }
    else
    {
        strncpy(res, str, strlen(str) + 1);
    }
}

#pragma endregion /* String utils */

#pragma region WebRTC common

static char *NBN_WebRTC_C_ParseSignalingMessage(const char *msg, size_t msg_len, const char *type)
{
    char *sdp = NULL;
    struct json_value_s* root = json_parse(msg, msg_len); // this has to be freed
    
    struct json_object_s* object = (struct json_object_s*)root->payload;
    struct json_object_element_s *curr = object->start;

    if (root->type != json_type_object)
    {
        NBN_LogDebug("Received an invalid signaling message: %s", msg);
        goto leave_free_root;
    }

    while (curr != NULL)
    {
        if (strncmp(curr->name->string, "type", 4) == 0)
        {
            struct json_string_s *str = json_value_as_string(curr->value);

            if (strncmp(str->string, type, strlen(type)))
            {
                // unexpected type
                NBN_LogDebug("Received a signaling message with an unexpected type: %s (expected: %s)", str->string, type);
                sdp = NULL;
                goto leave_free_root;
            }
        }
        else if (strncmp(curr->name->string, "sdp", 3) == 0)
        {
            struct json_string_s *str = json_value_as_string(curr->value);

            if (str)
            {
                // strdup equivalent using NBN_Allocator, make sure this get freed
                size_t len = strlen(str->string);

                sdp = (char*)NBN_Allocator(len + 1);
                memcpy(sdp, str->string, len + 1);
            }
        }

        curr = curr->next;
    }

leave_free_root:
    free(root);

    return sdp;
}

static char *NBN_WebRTC_C_EscapeSDP(const char *sdp)
{
    size_t len = strlen(sdp) * 2; // TODO: kinda lame way of making sure it's going to be big enough, find a better way
    char *escaped_sdp = (char*)NBN_Allocator(len);

    NBN_WebRTC_C_StringReplaceAll(escaped_sdp, sdp, "\r\n", "\\r\\n");

    return escaped_sdp;
}

static void NBN_WebRTC_C_Log(rtcLogLevel level, const char *msg)
{
    switch (level)
    {
        case RTC_LOG_FATAL:
        case RTC_LOG_ERROR:
            NBN_LogError("%s",msg);
            break;

        case RTC_LOG_WARNING:
            NBN_LogWarning("%s",msg);
            break;

        case RTC_LOG_INFO:
            NBN_LogInfo("%s",msg);
            break;

        case RTC_LOG_DEBUG:
            NBN_LogDebug("%s",msg);
            break;

        case RTC_LOG_VERBOSE:
            NBN_LogTrace("%s",msg);
            break;

        case RTC_LOG_NONE:
            break;
    }
}

static void NBN_WebRTC_C_OnWsError(int ws, const char *err_msg, void *user_ptr)
{
    (void)user_ptr;

    NBN_LogError("Error on WS %d: %s", ws, err_msg);
}

static NBN_WebRTC_C_Peer *NBN_WebRTC_C_CreatePeer(
        int ws,
        rtcDescriptionCallbackFunc on_rtc_description_cb,
        rtcStateChangeCallbackFunc on_state_change_cb)
{
    rtcConfiguration rtcCfg = {
        .iceServers = nbn_wrtc_c_cfg.ice_servers,
        .iceServersCount = (int)nbn_wrtc_c_cfg.ice_servers_count,
        .disableAutoNegotiation = false
    };
    int peer_id = rtcCreatePeerConnection(&rtcCfg);

    if (peer_id < 0)
    {
        NBN_LogError("Failed to create peer: %d", peer_id);
        return NULL;
    }

    NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer *)NBN_Allocator(sizeof(NBN_WebRTC_C_Peer));

    peer->id = peer_id;
    peer->ws = ws;
    peer->channel_id = -1;

    rtcSetUserPointer(peer_id, peer);
    rtcSetUserPointer(ws, peer);

    int ret = rtcSetLocalDescriptionCallback(peer->id, on_rtc_description_cb);

    if (ret < 0)
    {
        NBN_LogError("Failed to register local description callback for peer %d: %d", peer->id, ret);
        NBN_WebRTC_C_DestroyPeer(peer);
        return NULL;
    }

    ret = rtcSetStateChangeCallback(peer_id, on_state_change_cb);

    if (ret < 0)
    {
        NBN_LogError("Failed to register state change callback for peer %d: %d", peer_id, ret);
        NBN_WebRTC_C_DestroyPeer(peer);
        return NULL;
    }
    rtcDataChannelInit rtcDataChannel = {
        .reliability = {.unordered = true, .unreliable = true, .maxPacketLifeTime = 1000, .maxRetransmits = 0},
        .negotiated = true,
        .manualStream = true,
        .stream = 0
    };
    int channel_id = rtcCreateDataChannelEx(peer_id, "unreliable", &rtcDataChannel);

    if (channel_id < 0)
    {
        NBN_LogError("Failed to create data channel for peer %d: %d", peer_id, channel_id);
        NBN_WebRTC_C_DestroyPeer(peer);
        return NULL;
    }

    peer->channel_id = channel_id;

    NBN_LogDebug("Successfully created data channel for peer %d: %d", peer_id, channel_id); 

    return peer;
}

static void NBN_WebRTC_C_DestroyPeer(NBN_WebRTC_C_Peer *peer)
{
    NBN_LogDebug("Destroying peer %d", peer->id);

    if (peer->channel_id >= 0)
    {
        rtcDeleteDataChannel(peer->channel_id);
    }

    rtcDeletePeerConnection(peer->id);
    rtcDelete(peer->ws);
    NBN_Deallocator(peer);
}

static void NBN_WebRTC_C_ProcessLocalDescription(NBN_WebRTC_C_Peer *peer, const char *sdp, const char *type)
{
    char signaling_json[1024];
    char *escaped_sdp = NBN_WebRTC_C_EscapeSDP(sdp);

    snprintf(signaling_json, sizeof(signaling_json), "{\"type\":\"%s\", \"sdp\":\"%s\"}", type, escaped_sdp);
    NBN_LogDebug("Send signaling message of type %s to remote connection: %s", type, signaling_json);

    // pass -1 as the size (assume signaling_json to be a null-terminated string)
    if (rtcSendMessage(peer->ws, signaling_json, -1) < 0)
    {
        NBN_WebRTC_C_DestroyPeer(peer);
    }

    NBN_Deallocator(escaped_sdp);
}

static void NBN_WebRTC_C_ProcessSignalingMessage(NBN_WebRTC_C_Peer *peer, int ws, const char *msg, int size, const char *type)
{
    // for some reason the size of the message is negative
    // in libdatachannel documentation (https://github.com/paullouisageneau/libdatachannel/blob/master/DOC.md) there is mention of:
    // size: if size >= 0, data is interpreted as a binary message of length size, otherwise it is interpreted as a null-terminated UTF-8 string.
    // so I guess in this case msg is a null terminated string? I could not find more information about this so I decided to go with
    // flipping the size to positive even though it feels weird, but it works so... ¯\_(ツ)_/¯

    if (size < 0) size *= -1;
    size -= 1;

    NBN_LogDebug("Received signaling message on WS %d (size: %d): %s", ws, size, msg);

    char *sdp = NBN_WebRTC_C_ParseSignalingMessage(msg, size, type);

    if (!sdp)
    {
        NBN_LogWarning("Failed to parse signaling data for WS %d", ws);
        return;
    }

    NBN_LogDebug("Successfully parsed signaling payload (sdp: %s)", sdp);

    int ret = rtcSetRemoteDescription(peer->id, sdp, type);

    if (ret < 0)
    {
        NBN_LogError("Failed to set remote description for peer %d (WS: %d): %d", peer->id, ws, ret);
        rtcClose(ws);
    }

    // IMPORTANT: not sure I can free this because it's passed to rtcSetRemoteDescription
    NBN_Deallocator(sdp);
}

#pragma endregion /* WebRTC common */

#pragma region Game server

typedef struct NBN_WebRTC_C_Server
{
    int wsserver;
    NBN_WebRTC_C_HTable *peers;
    bool is_encrypted;
    uint16_t ws_port;
    uint32_t protocol_id;
    char packet_buffer[NBN_PACKET_MAX_SIZE];
} NBN_WebRTC_C_Server;

static NBN_WebRTC_C_Server nbn_wrtc_c_serv = {0, NULL, false, 0, 0, {0}};

static void NBN_WebRTC_C_Serv_OnLocalDescription(int pc, const char *sdp, const char *type, void *user_ptr)
{
    NBN_LogDebug("Processing local description of type '%s'", type);

    if (strncmp(type, "answer", strlen("answer")) != 0)
    {
        NBN_LogWarning("Ignoring local description of type '%s' (expected 'answer')", type);
        return;
    }

    NBN_WebRTC_C_ProcessLocalDescription((NBN_WebRTC_C_Peer *)user_ptr, sdp, "answer");
}

static void NBN_WebRTC_C_Serv_OnPeerStateChanged(int pc, rtcState state, void *user_ptr)
{
    NBN_LogDebug("Peer %d state changed to %d", pc, state);

    if (state == RTC_CONNECTED)
    {
        NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer *)user_ptr;

        NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_CONNECTED, peer->conn);
        NBN_LogDebug("Peer %d is connected !", pc);
    }
}

static void NBN_WebRTC_C_Serv_OnWsOpen(int ws, void *user_ptr)
{
    NBN_LogDebug("WS %d is open", ws);

    NBN_WebRTC_C_Peer *peer = NBN_WebRTC_C_CreatePeer(ws, NBN_WebRTC_C_Serv_OnLocalDescription, NBN_WebRTC_C_Serv_OnPeerStateChanged);

    if (!peer)
    {
        NBN_LogError("Failed to create peer");
        return;
    }

    peer->conn = NBN_GameServer_CreateClientConnection(
            NBN_WEBRTC_C_DRIVER_ID,
            peer,
            nbn_wrtc_c_serv.protocol_id,
            peer->id,
            nbn_wrtc_c_serv.is_encrypted);
    NBN_WebRTC_C_HTable_Add(nbn_wrtc_c_serv.peers, peer->id, peer);
}

static void NBN_WebRTC_C_Serv_OnWsClosed(int ws, void *user_ptr)
{
    NBN_LogDebug("WS %d has closed", ws);

    if (user_ptr)
    {
        NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer *)user_ptr;

        NBN_LogDebug("Closing WebRTC peer and channel (peer: %d, channel: %d)", peer->id, peer->channel_id);

        rtcClose(peer->channel_id);
        rtcClosePeerConnection(peer->id);
    }
}

static void NBN_WebRTC_C_Serv_OnWsMessage(int ws, const char *msg, int size, void *user_ptr)
{
    NBN_WebRTC_C_ProcessSignalingMessage((NBN_WebRTC_C_Peer *)user_ptr, ws, msg, size, "offer");
}

static void NBN_WebRTC_C_Serv_OnWsConnection(int wsserver, int ws, void *user_ptr)
{
    NBN_LogDebug("New WS connection %d (user_ptr: %p)", ws, user_ptr);

    rtcSetOpenCallback(ws, NBN_WebRTC_C_Serv_OnWsOpen);
    rtcSetClosedCallback(ws, NBN_WebRTC_C_Serv_OnWsClosed);
    rtcSetErrorCallback(ws, NBN_WebRTC_C_OnWsError);
    rtcSetMessageCallback(ws, NBN_WebRTC_C_Serv_OnWsMessage);
}

static int NBN_WebRTC_C_ServStart(uint32_t protocol_id, uint16_t port, bool enable_encryption)
{
    nbn_wrtc_c_serv.ws_port = port;
    nbn_wrtc_c_serv.peers = NBN_WebRTC_C_HTable_Create();
    nbn_wrtc_c_serv.is_encrypted = enable_encryption;
    nbn_wrtc_c_serv.protocol_id = protocol_id;
    nbn_wrtc_c_serv.wsserver = -1;

    rtcInitLogger(nbn_wrtc_c_cfg.log_level, NBN_WebRTC_C_Log);
    rtcPreload();

    rtcWsServerConfiguration cfg = {
        .port = port,
        .enableTls = nbn_wrtc_c_cfg.enable_tls,
        .certificatePemFile = nbn_wrtc_c_cfg.cert_path,
        .keyPemFile = nbn_wrtc_c_cfg.key_path,
        .keyPemPass = nbn_wrtc_c_cfg.passphrase
    };

    int wsserver = rtcCreateWebSocketServer(&cfg, NBN_WebRTC_C_Serv_OnWsConnection);

    if (wsserver < 0)
    {
        NBN_LogError("Failed to start WS server (code: %d)", wsserver);
        return NBN_ERROR;
    }

    nbn_wrtc_c_serv.wsserver = wsserver;

    return 0;
}

static void NBN_WebRTC_C_ServStop(void)
{
    NBN_WebRTC_C_HTable_Destroy(nbn_wrtc_c_serv.peers);

    if (nbn_wrtc_c_serv.wsserver >= 0)
    {
        rtcDeleteWebSocketServer(nbn_wrtc_c_serv.wsserver);
    }

    rtcCleanup();
}

static int NBN_WebRTC_C_ServRecvPackets(void)
{
    const int buffer_size = sizeof(nbn_wrtc_c_serv.packet_buffer);
    int size = buffer_size;

    for (unsigned int i = 0; i < nbn_wrtc_c_serv.peers->capacity; i++)
    {
        NBN_WebRTC_C_HTableEntry *entry = nbn_wrtc_c_serv.peers->internal_array[i];

        if (entry)
        {
            while (rtcReceiveMessage(entry->peer->channel_id, nbn_wrtc_c_serv.packet_buffer, &size) == RTC_ERR_SUCCESS)
            {
                NBN_Packet packet;

                if (NBN_Packet_InitRead(&packet, entry->peer->conn, (uint8_t *)nbn_wrtc_c_serv.packet_buffer, size) < 0)
                    continue;

                packet.sender = entry->peer->conn;
                NBN_Driver_RaiseEvent(NBN_DRIVER_SERV_CLIENT_PACKET_RECEIVED, &packet);
                size = buffer_size;
            }
        }
    }

    return 0;
}

static void NBN_WebRTC_C_ServRemoveClientConnection(NBN_Connection *conn)
{
    NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer*)conn->driver_data;

    NBN_WebRTC_C_HTable_Remove(nbn_wrtc_c_serv.peers, peer->id);
    NBN_WebRTC_C_DestroyPeer(peer);
}

static int NBN_WebRTC_C_ServSendPacketTo(NBN_Packet *packet, NBN_Connection *conn)
{
    if (!conn->is_accepted) return 0;

    NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer*)conn->driver_data;

    if (rtcSendMessage(peer->channel_id, (char *)packet->buffer, packet->size) < 0)
    {
        NBN_LogError("rtcSendMessage failed for peer %d", peer->id);
    }

    return 0;
}

#pragma endregion /* Game server */

#pragma region Game client

typedef struct NBN_WebRTC_C_Client
{
    uint32_t protocol_id;
    bool is_encrypyed;
    bool is_connected;
    NBN_WebRTC_C_Peer *peer;
    char packet_buffer[NBN_PACKET_MAX_SIZE];
} NBN_WebRTC_C_Client;

static NBN_WebRTC_C_Client nbn_wrtc_c_cli = {0, false, false, NULL, {0}};

static void NBN_WebRTC_C_Cli_OnLocalDescription(int pc, const char *sdp, const char *type, void *user_ptr)
{
    NBN_LogDebug("Processing local description of type '%s'", type);

    if (strncmp(type, "offer", strlen("offer")) != 0)
    {
        NBN_LogWarning("Ignoring local description of type '%s' (expected 'offer')", type);
        return;
    }

    NBN_WebRTC_C_ProcessLocalDescription((NBN_WebRTC_C_Peer *)user_ptr, sdp, "offer");
}

static void NBN_WebRTC_C_Cli_OnPeerStateChanged(int pc, rtcState state, void *user_ptr)
{
    NBN_LogDebug("Server peer state changed to %d", pc, state);

    if (state == RTC_CONNECTED)
    {
        NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer *)user_ptr;

        NBN_LogDebug("Server peer is connected !", pc);
        nbn_wrtc_c_cli.is_connected = true;
    }
}

static void NBN_WebRTC_C_Cli_OnWsOpen(int ws, void *user_ptr)
{
    NBN_LogDebug("WS %d is open, creating peer...", ws);

    NBN_WebRTC_C_Peer *peer = NBN_WebRTC_C_CreatePeer(ws, NBN_WebRTC_C_Cli_OnLocalDescription, NBN_WebRTC_C_Cli_OnPeerStateChanged);

    if (!peer)
    {
        NBN_LogError("Failed to create peer");
        return;
    } 

    NBN_LogDebug("Successfully created peer: %d", peer->id); 

    peer->conn = NBN_GameClient_CreateServerConnection(NBN_WEBRTC_C_DRIVER_ID, peer, nbn_wrtc_c_cli.protocol_id, nbn_wrtc_c_cli.is_encrypyed);
    nbn_wrtc_c_cli.peer = peer;
}

static void NBN_WebRTC_C_Cli_OnWsClosed(int ws, void *user_ptr)
{
    NBN_LogDebug("WS %d has closed", ws);

    if (user_ptr)
    {
        NBN_WebRTC_C_Peer *peer = (NBN_WebRTC_C_Peer *)user_ptr;

        NBN_LogDebug("Destroying server peer (peer: %d, channel: %d)", peer->id, peer->channel_id);
        NBN_WebRTC_C_DestroyPeer(peer);
    }
}

static void NBN_WebRTC_C_Cli_OnWsMessage(int ws, const char *msg, int size, void *user_ptr)
{
    NBN_WebRTC_C_ProcessSignalingMessage((NBN_WebRTC_C_Peer *)user_ptr, ws, msg, size, "answer");
}

static int AttemptConnection(void)
{
    double waitTime = 1.0 / 3.0; // wait time between connection attempts in seconds

    int retries = 9; // approximatively 3 seconds to get connected
    
    struct timespec rqtp;

    rqtp.tv_sec = 0;
    rqtp.tv_nsec = waitTime * 1e9;

    while (true)
    {
    #if defined(_WIN32) || defined(_WIN64)
        Sleep(waitTime * 1000);
    #else
        if (nanosleep(&rqtp, NULL) < 0)
        {
            NBN_LogError("nanosleep failed");
            return NBN_ERROR;
        }
    #endif
        if (--retries <= 0 || nbn_wrtc_c_cli.is_connected)
        {
            break;
        }
    }

    if (!nbn_wrtc_c_cli.is_connected)
    {
        NBN_LogError("Failed to connect");

        return NBN_ERROR;
    }

    return 0;
}

static int NBN_WebRTC_C_CliStart(uint32_t protocol_id, const char *host, uint16_t port, bool enable_encryption)
{
    rtcInitLogger(nbn_wrtc_c_cfg.log_level, NBN_WebRTC_C_Log);
    rtcPreload();

    char ws_addr[256] = {0};

    snprintf(ws_addr, sizeof(ws_addr), "ws://%s:%d", host, port);

    int cli_ws;

    if ((cli_ws = rtcCreateWebSocket(ws_addr)) < 0)
    {
        NBN_LogError("Failed to create websocket");
        return NBN_ERROR;
    }

    nbn_wrtc_c_cli.protocol_id = protocol_id;
    nbn_wrtc_c_cli.is_encrypyed = enable_encryption;

    NBN_LogDebug("Successfully created client WS: %d", cli_ws);

    rtcSetOpenCallback(cli_ws, NBN_WebRTC_C_Cli_OnWsOpen);
    rtcSetClosedCallback(cli_ws, NBN_WebRTC_C_Cli_OnWsClosed);
    rtcSetErrorCallback(cli_ws, NBN_WebRTC_C_OnWsError);
    rtcSetMessageCallback(cli_ws, NBN_WebRTC_C_Cli_OnWsMessage);

    return AttemptConnection();
}

static void NBN_WebRTC_C_CliStop(void)
{
    NBN_WebRTC_C_Peer *peer = nbn_wrtc_c_cli.peer;

    if (peer)
    {
        NBN_WebRTC_C_DestroyPeer(peer);
    }

    nbn_wrtc_c_cli.is_connected = false;
    rtcCleanup();
}

static int NBN_WebRTC_C_CliRecvPackets(void)
{
    const int buffer_size = sizeof(nbn_wrtc_c_cli.packet_buffer);
    int size = buffer_size;
    NBN_WebRTC_C_Peer *peer = nbn_wrtc_c_cli.peer;

    while (rtcReceiveMessage(peer->channel_id, nbn_wrtc_c_cli.packet_buffer, &size) == RTC_ERR_SUCCESS)
    {
        NBN_Packet packet;

        if (NBN_Packet_InitRead(&packet, peer->conn, (uint8_t *)nbn_wrtc_c_cli.packet_buffer, size) < 0)
            continue;

        packet.sender = peer->conn;
        NBN_Driver_RaiseEvent(NBN_DRIVER_CLI_PACKET_RECEIVED, &packet);
        size = buffer_size;
    }

    return 0;
}

static int NBN_WebRTC_C_CliSendPacket(NBN_Packet *packet)
{
    NBN_WebRTC_C_Peer *peer = nbn_wrtc_c_cli.peer;

    if (rtcSendMessage(peer->channel_id, (char *)packet->buffer, packet->size) < 0)
    {
        NBN_LogError("rtcSendMessage failed for peer %d", peer->id);
        return NBN_ERROR;
    }

    return 0;
}

#pragma endregion /* Game client */

void NBN_WebRTC_C_Register(NBN_WebRTC_C_Config config)
{
    const char **ice_servers = (const char**)NBN_Allocator(sizeof(char *) * config.ice_servers_count);

    for (unsigned int i = 0; i < config.ice_servers_count; i++)
    {
        // strdup equivalent using NBN_Allocator, make sure this get freed
        const char *str = config.ice_servers[i];
        size_t len = strlen(str);
        char *dup_str = (char*)NBN_Allocator(len + 1);

        memcpy(dup_str, str, len + 1);
        ice_servers[i] = dup_str;
    }

    nbn_wrtc_c_cfg.ice_servers = ice_servers;
    nbn_wrtc_c_cfg.ice_servers_count = config.ice_servers_count;
    nbn_wrtc_c_cfg.enable_tls = config.enable_tls;

    if (config.enable_tls)
    {
        nbn_wrtc_c_cfg.cert_path = config.cert_path;
        nbn_wrtc_c_cfg.key_path = config.key_path;
        nbn_wrtc_c_cfg.passphrase = config.passphrase;
    }

    NBN_DriverImplementation driver_impl = {
        // Client implementation
        NBN_WebRTC_C_CliStart,
        NBN_WebRTC_C_CliStop,
        NBN_WebRTC_C_CliRecvPackets,
        NBN_WebRTC_C_CliSendPacket,

        // Server implementation
        NBN_WebRTC_C_ServStart,
        NBN_WebRTC_C_ServStop,
        NBN_WebRTC_C_ServRecvPackets,
        NBN_WebRTC_C_ServSendPacketTo,
        NBN_WebRTC_C_ServRemoveClientConnection
    };

    NBN_Driver_Register(NBN_WEBRTC_C_DRIVER_ID, NBN_WEBRTC_C_DRIVER_NAME, driver_impl);
}

void NBN_WebRTC_C_Unregister(void)
{
    for (unsigned int i = 0; i < nbn_wrtc_c_cfg.ice_servers_count; i++)
    {
        NBN_Deallocator((void *)nbn_wrtc_c_cfg.ice_servers[i]);
    }

    NBN_Deallocator(nbn_wrtc_c_cfg.ice_servers);
}

#endif // NBNET_IMPL
