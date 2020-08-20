#ifndef RAYLIB_EXAMPLE_SHARED_H
#define RAYLIB_EXAMPLE_SHARED_H

#if defined(_WIN32) || defined(_WIN64)

/*
    The following defines are meant to avoid conflicts between raylib and windows.h.

    https://github.com/raysan5/raylib/issues/857
*/

/* If defined, the following flags inhibit definition of the indicated items.*/
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

/* Type required before windows.h inclusion  */
typedef struct tagMSG *LPMSG;

#endif /* WINDOWS */

#include <raylib.h>

#include "../../nbnet.h"

#define RAYLIB_EXAMPLE_PROTOCOL_NAME "raylib-example"

/* nbnet logging */

#define NBN_LogInfo(...) TraceLog(LOG_INFO, __VA_ARGS__)

/* TraceLog with LOG_ERROR seems to exit the application, i do not want that so i use LOG_WARNING */
#define NBN_LogError(...) TraceLog(LOG_WARNING, __VA_ARGS__)
#define NBN_LogWarning(...) TraceLog(LOG_WARNING, __VA_ARGS__)
#define NBN_LogDebug(...) TraceLog(LOG_DEBUG, __VA_ARGS__)
#define NBN_LogTrace(...) TraceLog(LOG_TRACE, __VA_ARGS__)

/*
    Simulation tick rate.
*/
#define TICK_RATE 60

/*
    Used for client window size but also to cap serialized position values inside messages
*/
#define GAME_WIDTH 800
#define GAME_HEIGHT 600

/* Max number of connected clients */
#define MAX_CLIENTS 4

/*
    nbnet channels

    We are going to use two channels to demonstrate the two existing types of channel in nbnet:

    1.  an unreliable ordered channel on which we are going to send time critical messages
    2.  a reliable ordered channel on which we are going to send reliable messages that we want to be 
        sure are received by the other end
*/

enum
{
    UNRELIABLE_CHANNEL,
    RELIABLE_CHANNEL
};

/*
    We are going to use four different messages:

    1.  SpawnMessage: a reliable message that will be sent by the server to the client when it connects
    2.  ChangeColorMessage: a reliable message that will be sent by the client to the server when it
        wants to change his color
    3.  UpdatePositionMessage: an unreliable message that will be sent by the client every frame to
        update his current position.
    4.  GameStateMessage: an unreliable message that will be sent by the server to all clients with information
        about the latest states of each client (position and color).

    The first two messages are *important* messages that need to be received, therefore they will be channeled into the 
    reliable channel. On the other hand, the last two messages are time critical and sent at a very high rate which means
    we do not care about occasionally losing one of those, therefore it will be channelled into the unreliable channel.
*/

/* Use an enum for message ids */
enum
{
    SPAWN_MESSAGE,
    CHANGE_COLOR_MESSAGE,
    UPDATE_POSITION_MESSAGE,
    GAME_STATE_MESSAGE
};

/* Message structures */

typedef struct
{
    uint32_t client_id;
    int x;
    int y;
} SpawnMessage;

typedef struct
{
    Color color;
} ChangeColorMessage;

typedef struct
{
    int x;
    int y;
} UpdatePositionMessage;

typedef struct
{
    uint32_t client_id;
    int x;
    int y;
    Color color;
} ClientState;

typedef struct
{
    unsigned int client_count;
    ClientState client_states[MAX_CLIENTS];
} GameStateMessage;

/*
    Message builders.

    Each message needs to have a builder attached.
    
    A message builder is a function that returns a pointer to a message.
    A simple message builder implementation is to use malloc to allocate memory for the message and return
    the pointer to that memory region.
*/
SpawnMessage *SpawnMessage_Create(void);
ChangeColorMessage *ChangeColorMessage_Create(void);
UpdatePositionMessage *UpdatePositionMessage_Create(void);
GameStateMessage* GameStateMessage_Create(void);

/*
    Message serializers.

    Each message needs to have a serializer attached (see RegisterMessages function definition)

    A message serializer is a function that takes two parameters:
        - the message to serialize
        - a nbnet stream

    You don't have to worry about the stream parameter but *it has to be named "stream"* due
    to how serialization macros are defined.
    A message serializer has to return 0 upon success, you don't have to worry about handling serialization errors
    since it is done by the serialization macros.
*/
int SpawnMessage_Serialize(SpawnMessage *, NBN_Stream *);
int ChangeColorMessage_Serialize(ChangeColorMessage *, NBN_Stream *);
int UpdatePositionMessage_Serialize(UpdatePositionMessage *, NBN_Stream *);
int GameStateMessage_Serialize(GameStateMessage *, NBN_Stream *);

/*
    Message destructors.

    Each message needs to have a destructor attached.
    
    A message destructor is a function that takes a pointer to previously built message and has to release
    the resources that were allocated. If you opted for a simple malloc implementation in your message builder
    you can simply call free in the message destructor.
*/
void SpawnMessage_Destroy(SpawnMessage *);
void ChangeColorMessage_Destroy(ChangeColorMessage *);
void UpdatePositionMessage_Destroy(UpdatePositionMessage *);
void GameStateMessage_Destroy(GameStateMessage *);

void RegisterChannels(void);
void RegisterMessages(void);
void TickSleep(float);

#endif /* RAYLIB_EXAMPLE_SHARED_H */