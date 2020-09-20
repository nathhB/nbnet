# nbnet

nbnet is a single header C (C99) library to implement client-server network code for games, it is more precisely designed for fast paced action games.
nbnet is based on this [great series of articles](https://gafferongames.com/) by Glenn Fiedler.

The library currently implements the following features:

- Connection management
- Sending/Receiving both reliable ordered and unreliable ordered messages
- Sending/Receiving messages larger than MTU via a message fragmentation system
- Serialization of integers (signed and unsigned), floats, booleans and byte arrays (at bit level to reduce bandwith usage)
- Simulates bad network conditions for testing purposes (ping, jitter, packet loss, packet duplication and out of order packets)
- Estimates of ping, bandwith (upload and download) and packet loss

What is not *yet* implemented but will be in the future:

- Encrypted packets
- Secure authentification system
- WebRTC support

nbnet does not directly implement any low level "transport" code and rely on the concept of *drivers*. A driver is a set of function definitions that live outside nbnet header and provide a transport layer implementation that will be used by the library to send and receive packets.

nbnet currently comes with a single UDP driver that you can use for desktop games. I have plans for a WebRTC driver so that the library could be used for browser games.

## How to use

In *exactly one* of your source file do:

```
#define NBNET_IMPL

#include "nbnet.h"
```

You also need to provide a driver implementation, if you decide to use the nbnet UDP driver just add:

```
#include "net_drivers/udp.h"
```

after including the nbnet header in the same source file where you defined `NBNET_IMPL`.


nbnet uses two slightly different APIs, one for client code and the other one for server code, you have to use a pair of macros to let nbnet know what API to use: `NBN_GAME_CLIENT` and `NBN_GAME_SERVER`.
What you want to do is define the former when compiling your client code and the later when compiling your server code, with gcc it would look something like:

`gcc -DNBN_GAME_CLIENT client.c -o client`

`gcc -DNBN_GAME_SERVER server.c -o server`

nbnet does not provide any logging capacibilities so you have to provide your own using a bunch of macros:

```
#define NBN_LogInfo(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogError(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogDebug(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogTrace(...) SomeLoggingFunction(__VA_ARGS__)
```

Same thing for memory allocation and deallocation:

```
#define NBN_Allocator malloc
#define NBN_Deallocator free
```
From here I suggest you look into the examples folder.
