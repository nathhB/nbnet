#ifndef SOAK_H_INCLUDED
#define SOAK_H_INCLUDED

#include <stdbool.h>

#include "logging.h"

/* nbnet logging */
#define NBN_LogInfo Soak_LogInfo
#define NBN_LogTrace Soak_LogTrace
#define NBN_LogDebug Soak_LogDebug
#define NBN_LogError Soak_LogError

#include "../nbnet.h"

#define SOAK_PROTOCOL_NAME "nbnet_soak"
#define SOAK_PORT 42042
#define SOAK_TICK_RATE 60
#define SOAK_MESSAGE_MIN_DATA_LENGTH 50
#define SOAK_MESSAGE_MAX_DATA_LENGTH 100
#define SOAK_MESSAGE 0
#define SOAK_SEED time(NULL)

typedef struct
{
    unsigned int messages_count;
    float packet_loss;
    float packet_duplication;
    unsigned int ping; /* in ms */
    unsigned int jitter; /* in ms */
} SoakOptions;

typedef struct
{
    uint32_t id;
    unsigned int data_length;
    uint8_t data[SOAK_MESSAGE_MAX_DATA_LENGTH];
} SoakMessage;

enum
{
    SOAK_CHAN_RELIABLE_ORDERED_1,
    SOAK_CHAN_RELIABLE_ORDERED_2,
    SOAK_CHAN_RELIABLE_ORDERED_3
};

int Soak_Init(int, char *[]);
void Soak_Deinit(void);
int Soak_ReadCommandLine(int, char *[]);
int Soak_MainLoop(int (*)(void));
void Soak_Stop(void);
SoakOptions Soak_GetOptions(void);
SoakMessage *SoakMessage_Create(void);
int SoakMessage_Serialize(SoakMessage*, NBN_Stream *);
void Soak_Debug_PrintAddedToRecvQueue(NBN_Connection *, NBN_Message *);

#endif // SOAK_H_INCLUDED
