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

#ifndef RPC_EXAMPLE_SHARED_H
#define RPC_EXAMPLE_SHARED_H

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

#define RPC_PROTOCOL_NAME "rpc-example"
#define RPC_EXAMPLE_PORT 42042
#define RPC_TICK_RATE 30
#define TEST_RPC_ID 0
#define TEST_RPC_2_ID 1
#define TEST_RPC_SIGNATURE NBN_RPC_BuildSignature(3, NBN_RPC_PARAM_INT, NBN_RPC_PARAM_FLOAT, NBN_RPC_PARAM_BOOL)
#define TEST_RPC_2_SIGNATURE NBN_RPC_BuildSignature(2, NBN_RPC_PARAM_FLOAT, NBN_RPC_PARAM_STRING)

// nbnet logging
// nbnet does not implement any logging capabilities, you need to provide your own
enum
{
    LOG_INFO,
    LOG_ERROR,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_WARNING
};

#define NBN_LogInfo(...) Log(LOG_INFO,  __VA_ARGS__)
#define NBN_LogError(...) Log(LOG_ERROR, __VA_ARGS__)
#define NBN_LogDebug(...) Log(LOG_DEBUG, __VA_ARGS__)
#define NBN_LogWarning(...) Log(LOG_WARNING, __VA_ARGS__)
#define NBN_LogTrace(...) (void)0;

void Log(int, const char *, ...);

#include "../../nbnet.h"

#ifdef __EMSCRIPTEN__
#include "../../net_drivers/webrtc.h"
#else
#include "../../net_drivers/udp.h"
#endif

void ExampleSleep(double);

#endif /* RPC_EXAMPLE_SHARED_H */
