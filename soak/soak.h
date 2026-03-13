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

#ifndef SOAK_H_INCLUDED
#define SOAK_H_INCLUDED

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <winsock2.h>

#endif

#include <limits.h>
#include <stdbool.h>
#include "../nbnet.h"

#define SOAK_PROTOCOL_NAME "nbnet_soak"
#define SOAK_PORT 42044
#define SOAK_TICK_RATE 60
#define SOAK_TICK_DT (1.0 / SOAK_TICK_RATE)
#define SOAK_MESSAGE_HEADER_LENGTH 8 // 4 bytes for ID, 4 bytes for data length
#define SOAK_MESSAGE_SMALL_MIN_DATA_LENGTH 50
#define SOAK_MESSAGE_SMALL_MAX_DATA_LENGTH 200
#define SOAK_MESSAGE_BIG_MIN_DATA_LENGTH 1024
#define SOAK_MESSAGE_BIG_MAX_DATA_LENGTH 4096
#define SOAK_MESSAGE_SMALL_MAX_LENGTH (SOAK_MESSAGE_HEADER_LENGTH + SOAK_MESSAGE_SMALL_MAX_DATA_LENGTH)
#define SOAK_MESSAGE_BIG_MAX_LENGTH (SOAK_MESSAGE_HEADER_LENGTH + SOAK_MESSAGE_BIG_MAX_DATA_LENGTH)
#define SOAK_BIG_MESSAGE_PERCENTAGE 0 // TODO: chunks are currently unsupported
#define SOAK_MESSAGE_SMALL 42
#define SOAK_MESSAGE_BIG 43 // may get chunked
#define SOAK_SEED time(NULL)
#define SOAK_DONE 1
#define SOAK_CLIENT_MAX_PENDING_MESSAGES 50 // max number of unacked messages at a time
#define SOAK_SERVER_FULL_CODE 1234
#define SOAK_MAX_MESSAGE_SIZE 256
#define SOAK_CHANNEL_COUNT 4
#define SOAK_CHANNEL_BUFFER_SIZE 128

typedef struct {
    unsigned int message_count;
    float packet_loss;        /* 0 - 1 */
    float packet_duplication; /* 0 - 1 */
    float ping;               /* in seconds */
    float jitter;             /* in seconds */
    bool webrtc;              /* use native WebRTC driver */
} SoakOptions;

int Soak_Init(int, char *[]);
void Soak_Deinit(void);
int Soak_ReadCommandLine(int, char *[]);
int Soak_MainLoop(int (*Tick)(void *), void *data);
void Soak_Stop(void);
SoakOptions Soak_GetOptions(void);
unsigned int Soak_GetCreatedOutgoingSoakMessageCount(void);
unsigned int Soak_GetDestroyedOutgoingSoakMessageCount(void);
unsigned int Soak_GetCreatedIncomingSoakMessageCount(void);
unsigned int Soak_GetDestroyedIncomingSoakMessageCount(void);
void SoakMessage_Write(NBN_Writer *, unsigned int, uint8_t *, unsigned int);
int SoakMessage_Read(NBN_Reader *reader, unsigned int *msg_id, uint8_t *data, unsigned int *data_length);

#endif // SOAK_H_INCLUDED
