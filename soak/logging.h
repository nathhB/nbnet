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

#ifndef SOAK_LOGGING_H
#define SOAK_LOGGING_H

/* I did not write this library: https://github.com/rxi/log.c */

/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "../nbnet.h"

#define LOG_VERSION "0.1.0"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LogDebug(...) Log(NBN_LOG_DEBUG, __FILENAME__, __LINE__, __VA_ARGS__)
#define LogInfo(...) Log(NBN_LOG_INFO, __FILENAME__, __LINE__, __VA_ARGS__)
#define LogWarn(...) Log(NBN_LOG_WARN, __FILENAME__, __LINE__, __VA_ARGS__)
#define LogError(...) Log(NBN_LOG_ERROR, __FILENAME__, __LINE__, __VA_ARGS__)

void SetLogLevel(NBN_LogLevel level);
void Log(NBN_LogLevel level, const char *file, int line, const char *fmt, ...);

#endif /* SOAK_LOGGING_H */
