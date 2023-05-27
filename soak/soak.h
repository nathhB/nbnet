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

#ifndef SOAK_H_INCLUDED
#define SOAK_H_INCLUDED

#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <windows.h>

#endif

#include <stdbool.h>
#include <limits.h>

#include "logging.h"

/* nbnet logging */
#define NBN_LogInfo Soak_LogInfo
#define NBN_LogTrace Soak_LogTrace
#define NBN_LogDebug Soak_LogDebug
#define NBN_LogError Soak_LogError
#define NBN_LogWarning Soak_LogWarn

#include "../nbnet.h"

#define SOAK_PROTOCOL_NAME "nbnet_soak"
#define SOAK_PORT 42043
#define SOAK_TICK_RATE 60
#define SOAK_TICK_DT (1.0 / SOAK_TICK_RATE)
#define SOAK_MESSAGE_MIN_DATA_LENGTH 50
#define SOAK_MESSAGE_MAX_DATA_LENGTH 4096
#define SOAK_BIG_MESSAGE_PERCENTAGE 25
#define SOAK_MESSAGE 0
#define SOAK_SEED time(NULL)
#define SOAK_DONE 1
#define SOAK_MAX_CLIENTS 256
#define SOAK_CLIENT_MAX_PENDING_MESSAGES 50 // max number of unacked messages at a time
#define SOAK_SERVER_FULL_CODE 42
#define SOAK_MAX_CHANNELS (NBN_MAX_CHANNELS - 3)

typedef struct
{
    unsigned int message_count;
    unsigned int channel_count;
    float packet_loss; /* 0 - 1 */
    float packet_duplication; /* 0 - 1 */
    float ping; /* in seconds */
    float jitter; /* in seconds */
} SoakOptions;

typedef struct
{
    uint32_t id;
    unsigned int data_length;
    bool outgoing;
    uint8_t data[SOAK_MESSAGE_MAX_DATA_LENGTH];
} SoakMessage;

int Soak_Init(int, char *[]);
void Soak_Deinit(void);
int Soak_ReadCommandLine(int, char *[]);
int Soak_MainLoop(int (*Tick)(void *), void *data);
void Soak_Stop(void);
SoakOptions Soak_GetOptions(void);
void Soak_Debug_PrintAddedToRecvQueue(NBN_Connection *, NBN_Message *);
unsigned int Soak_GetCreatedOutgoingSoakMessageCount(void);
unsigned int Soak_GetDestroyedOutgoingSoakMessageCount(void);
unsigned int Soak_GetCreatedIncomingSoakMessageCount(void);
unsigned int Soak_GetDestroyedIncomingSoakMessageCount(void);
SoakMessage *SoakMessage_CreateOutgoing(void);
SoakMessage *SoakMessage_CreateIncoming(void);
void SoakMessage_Destroy(SoakMessage *);
int SoakMessage_Serialize(SoakMessage *, NBN_Stream *);

#endif // SOAK_H_INCLUDED
