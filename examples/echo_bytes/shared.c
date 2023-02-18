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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Sleep function
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <synchapi.h>
#else
#include <time.h>
#endif

#include "shared.h"

// Sleep for a given amount of seconds
// Used to limit client and server tick rate
void Sleep(double sec)
{
#if defined(__EMSCRIPTEN__)
    emscripten_sleep(sec * 1000);
#elif defined(_WIN32) || defined(_WIN64)
    Sleep(sec * 1000);
#else /* UNIX / OSX */
    long nanos = sec * 1e9;
    struct timespec t = {.tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999};

    nanosleep(&t, &t);
#endif
}

static const char *log_type_strings[] = {
    "INFO",
    "ERROR",
    "DEBUG",
    "TRACE",
    "WARNING"
};

// Basic logging function
void Log(int type, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    printf("[%s] ", log_type_strings[type]);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}
