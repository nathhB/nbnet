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

#ifndef RAYLIB_EXAMPLE_SHARED_H
#define RAYLIB_EXAMPLE_SHARED_H

#include <raylib.h>

#if defined(_WIN32) || defined(_WIN64)

/*
 * The following defines are meant to avoid conflicts between raylib and windows.h
 * https://github.com/raysan5/raylib/issues/857
 */

// If defined, the following flags inhibit definition of the indicated items
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
/*#define NONLS             // All NLS defines and routines*/
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions

// Type required before windows.h inclusion
typedef struct tagMSG *LPMSG;

#include <winsock2.h> // Has to be included before windows.h
#include <windows.h>

#endif // WINDOWS

#define RAYLIB_EXAMPLE_PROTOCOL_NAME "raylib-example"
#define RAYLIB_EXAMPLE_PORT 42042

// nbnet logging, use raylib logging

#define NBN_LogInfo(...) TraceLog(LOG_INFO, __VA_ARGS__)

#define NBN_LogError(...) TraceLog(LOG_ERROR, __VA_ARGS__)
#define NBN_LogWarning(...) TraceLog(LOG_WARNING, __VA_ARGS__)
#define NBN_LogDebug(...) TraceLog(LOG_DEBUG, __VA_ARGS__)
#define NBN_LogTrace(...) TraceLog(LOG_TRACE, __VA_ARGS__)

#include "../../nbnet.h"

#ifdef __EMSCRIPTEN__
#include "../../net_drivers/webrtc.h"
#else
#include "../../net_drivers/udp.h"

#ifdef SOAK_WEBRTC_C_DRIVER
#include "../../net_drivers/webrtc_c.h"
#endif

#endif // __EMSCRIPTEN__

#define TICK_RATE 60 // Simulation tick rate

// Window size, used to display window but also to cap the serialized position values within messages
#define GAME_WIDTH 800
#define GAME_HEIGHT 600

#define MIN_FLOAT_VAL -5 // Minimum value of networked client float value
#define MAX_FLOAT_VAL 5 // Maximum value of networked client float value

// Maximum number of connected clients at a time
#define MAX_CLIENTS 4

// Max number of colors for client to switch between
#define MAX_COLORS 7

// A code passed by the server when closing a client connection due to being full (max client count reached)
#define SERVER_FULL_CODE 42

// Message ids
enum
{
    CHANGE_COLOR_MESSAGE,
    UPDATE_STATE_MESSAGE,
    GAME_STATE_MESSAGE
};

// Messages

typedef struct
{
    int x;
    int y;
    float val;
} UpdateStateMessage;

// Client colors used for ChangeColorMessage and GameStateMessage messages
typedef enum
{
    CLI_RED,
    CLI_GREEN,
    CLI_BLUE,
    CLI_YELLOW,
    CLI_ORANGE,
    CLI_PURPLE,
    CLI_PINK
} ClientColor;

typedef struct
{
    ClientColor color;
} ChangeColorMessage;

// Client state, represents a client over the network
typedef struct
{
    uint32_t client_id;
    int x;
    int y;
    float val;
    ClientColor color;
} ClientState;

typedef struct
{
    unsigned int client_count;
    ClientState client_states[MAX_CLIENTS];
} GameStateMessage;

// Store all options from the command line
typedef struct
{
    float packet_loss;
    float packet_duplication;
    float ping;
    float jitter;
} Options;

ChangeColorMessage *ChangeColorMessage_Create(void);
void ChangeColorMessage_Destroy(ChangeColorMessage *);
int ChangeColorMessage_Serialize(ChangeColorMessage *msg, NBN_Stream *);

UpdateStateMessage *UpdateStateMessage_Create(void);
void UpdateStateMessage_Destroy(UpdateStateMessage *);
int UpdateStateMessage_Serialize(UpdateStateMessage *, NBN_Stream *);

GameStateMessage *GameStateMessage_Create(void);
void GameStateMessage_Destroy(GameStateMessage *);
int GameStateMessage_Serialize(GameStateMessage *, NBN_Stream *);

int ReadCommandLine(int, char *[]);
Options GetOptions(void);

#endif /* RAYLIB_EXAMPLE_SHARED_H */
