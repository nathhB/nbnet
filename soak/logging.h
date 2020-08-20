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

#define LOG_VERSION "0.1.0"

typedef void (*log_LockFn)(void *udata, int lock);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define Soak_LogTrace(...) log_log(LOG_TRACE, __FILENAME__, __LINE__, __VA_ARGS__)
#define Soak_LogDebug(...) log_log(LOG_DEBUG, __FILENAME__, __LINE__, __VA_ARGS__)
#define Soak_LogInfo(...)  log_log(LOG_INFO,  __FILENAME__, __LINE__, __VA_ARGS__)
#define Soak_LogWarn(...)  log_log(LOG_WARN,  __FILENAME__, __LINE__, __VA_ARGS__)
#define Soak_LogError(...) log_log(LOG_ERROR, __FILENAME__, __LINE__, __VA_ARGS__)
#define Soak_LogFatal(...) log_log(LOG_FATAL, __FILENAME__, __LINE__, __VA_ARGS__)

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_set_quiet(int enable);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif /* SOAK_LOGGING_H */