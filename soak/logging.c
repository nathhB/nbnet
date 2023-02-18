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

#include "logging.h"

/* I did not write this library: https://github.com/rxi/log.c */

/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

static struct
{
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
} L;

static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

static void lock(void)
{
    if (L.lock)
    {
        L.lock(L.udata, 1);
    }
}

static void unlock(void)
{
    if (L.lock)
    {
        L.lock(L.udata, 0);
    }
}

void log_set_udata(void *udata)
{
    L.udata = udata;
}

void log_set_lock(log_LockFn fn)
{
    L.lock = fn;
}

void log_set_fp(FILE *fp)
{
    L.fp = fp;
}

void log_set_level(int level)
{
    L.level = level;
}

void log_set_quiet(int enable)
{
    L.quiet = enable ? 1 : 0;
}

void log_log(int level, const char *file, int line, const char *fmt, ...)
{
    if (level < L.level)
    {
        return;
    }

    /* Acquire lock */
    lock();

    /* Get current time */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    /* Log to stderr */
    if (!L.quiet)
    {
        va_list args;
        char buf[16];
        buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
#ifdef LOG_USE_COLOR
        fprintf(
                stderr, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
                buf, level_colors[level], level_names[level], file, line);
#else
        fprintf(stderr, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
#endif
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    /* Log to file */
    if (L.fp)
    {
        va_list args;
        char buf[32];
        buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
        fprintf(L.fp, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
        va_start(args, fmt);
        vfprintf(L.fp, fmt, args);
        va_end(args);
        fprintf(L.fp, "\n");
        fflush(L.fp);
    }

    /* Release lock */
    unlock();
}
