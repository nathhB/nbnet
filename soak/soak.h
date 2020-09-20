#ifndef SOAK_H_INCLUDED
#define SOAK_H_INCLUDED

#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <windows.h>

#endif

#include <stdbool.h>
#include <limits.h>

#include "logging.h"

#define NBN_Allocator malloc
#define NBN_Deallocator free

/* nbnet logging */
#define NBN_LogInfo Soak_LogInfo
#define NBN_LogTrace Soak_LogTrace
#define NBN_LogDebug Soak_LogDebug
#define NBN_LogError Soak_LogError

#include "../nbnet.h"

#define SOAK_PROTOCOL_NAME "nbnet_soak"
#define SOAK_PORT 42042
#define SOAK_TICK_RATE 60
#define SOAK_TICK_DT (1.0 / SOAK_TICK_RATE)
#define SOAK_MESSAGE_MIN_DATA_LENGTH 50
#define SOAK_MESSAGE_MAX_DATA_LENGTH 4096
#define SOAK_MESSAGE 0
#define SOAK_SEED time(NULL)
#define SOAK_MAX_CLIENTS 4

typedef struct
{
    unsigned int messages_count;
    float packet_loss; /* 0 - 1 */
    float packet_duplication; /* 0 - 1 */
    float ping; /* in seconds */
    float jitter; /* in seconds */
} SoakOptions;

typedef struct
{
    uint32_t id;
    unsigned int data_length;
    uint8_t data[SOAK_MESSAGE_MAX_DATA_LENGTH];
} SoakMessage;

int Soak_Init(int, char *[]);
void Soak_Deinit(void);
int Soak_ReadCommandLine(int, char *[]);
int Soak_MainLoop(int (*)(void));
void Soak_Stop(void);
SoakOptions Soak_GetOptions(void);
SoakMessage *SoakMessage_Create(void);
void SoakMessage_Destroy(SoakMessage *);
int SoakMessage_Serialize(SoakMessage*, NBN_Stream *);
void Soak_Debug_PrintAddedToRecvQueue(NBN_Connection *, NBN_Message *);

#endif // SOAK_H_INCLUDED
