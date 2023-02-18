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

#ifndef ECHO_EXAMPLE_SHARED_H
#define ECHO_EXAMPLE_SHARED_H

#define ECHO_PROTOCOL_NAME "echo-example"
#define ECHO_EXAMPLE_PORT 42042
#define ECHO_MESSAGE_TYPE 0
#define ECHO_TICK_RATE 30

// An arbitrary chosen code used when rejecting a client to let it know that another client is already connected
#define ECHO_SERVER_BUSY_CODE 42

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
#define NBN_LogTrace(...) Log(LOG_TRACE, __VA_ARGS__)
#define NBN_LogWarning(...) Log(LOG_WARNING, __VA_ARGS__)

void Log(int, const char *, ...);

#include "../../nbnet.h"

#ifdef __EMSCRIPTEN__
#include "../../net_drivers/webrtc.h"
#else
#include "../../net_drivers/udp.h"
#endif

void Sleep(double);

#endif /* ECHO_EXAMPLE_SHARED_H */
