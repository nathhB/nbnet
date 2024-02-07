# nbnet

![logo](logo/logo.png "Logo")

![nbnet](https://github.com/nathhB/nbnet/actions/workflows/nbnet.yml/badge.svg)
[![CodeFactor](https://www.codefactor.io/repository/github/nathhb/nbnet/badge/master)](https://www.codefactor.io/repository/github/nathhb/nbnet/overview/master)
[![Docs site](https://img.shields.io/badge/docs-GitHub_Pages-blue)](https://nathhb.github.io/nbnet)

[![](https://dcbadge.vercel.app/api/server/P9N7fy677D)](https://discord.gg/P9N7fy677D)

nbnet is a single header C (C99) library designed to implement client-server architecture, more precisely for online video games. The library is based on this [great series of articles](https://gafferongames.com/) by Glenn Fiedler.

nbnet can target different protocols such as UDP or [WebRTC](WEBRTC.md) through "drivers" (see below for more information).

The API is meant to be as straightforward as possible and relies on event polling, making it easy to integrate into a game loop.

**Disclaimer**: nbnet is in the early stages of its development and is, first and foremost, a learning project of mine as I explore online game development. If you are looking for a battle-tested, production-ready library, this is probably not the one.

You can see nbnet in action [in this video](https://www.youtube.com/watch?v=BJl_XN3QJhQ&ab_channel=NathanBIAGINI).

If you want to discuss the library or need help, join the [nbnet's discord server](https://discord.gg/esR8FSyPnF).

## Features

- Connection management
- Sending/Receiving both reliable ordered and unreliable ordered messages
- Sending/Receiving messages larger than the MTU (using nbnet's message fragmentation)
- Bit-level serialization (for bandwidth optimization): integers (signed and unsigned), floats, booleans, and byte arrays
- Network conditions simulation: ping, jitter, packet loss, packet duplication, and out-of-order packets)
- Network statistics: ping, bandwidth (upload and download) and packet loss
- Web (WebRTC) support (both natively and through WASM using [emscripten](https://emscripten.org/docs/introducing_emscripten/about_emscripten.html))
- Encrypted and authenticated packets

## Thanks

nbnet encryption and packet authentication use the following open-source libraries:

- [tiny-ECDH](https://github.com/kokke/tiny-ECDH-c)
- [tiny-AES](https://github.com/kokke/tiny-AES-c)
- [poly1305-donna](https://github.com/floodyberry/poly1305-donna)

the native WebRTC driver relies on:

- [libdatachannel](https://github.com/paullouisageneau/libdatachannel)
- [json.h](https://github.com/sheredom/json.h)

## Made with nbnet

### Boomshakalaka

A fun action game that runs into a web browser, by Duncan Stead (@duncanstead86).

[See on YouTube](https://www.youtube.com/watch?v=SJHvXV03uwQ).

### nbBR

A WIP battle royal game playable in a web browser.

[See on YouTube](https://youtube.com/playlist?list=PLgcJGzE_fX4criMxQAw3pm24RQYMYRLEI)

### Llamageddon

An online multiplayer RTS made for the Ludum Dare 49.

https://ldjam.com/events/ludum-dare/49/llamageddon

### nb_tanks

A little online tank game prototype.

[See on GitHub](https://github.com/nathhB/nb_tanks)

## Bindings

A list of user-contributed bindings (they are not officially supported, so they may be outdated):

- [nbnet-sunder](https://github.com/ashn-dot-dev/nbnet-sunder) by [@ashn](https://github.com/ashn-dot-dev) ([Sunder](https://github.com/ashn-dot-dev/sunder) is a C-like systems programming language and compiler for x86-64 Linux, ARM64 Linux, and WebAssembly)

## Drivers

nbnet does not directly implement any low-level "transport" code and relies on *drivers*.

A driver is a set of function definitions that live outside the nbnet header and provide a transport layer implementation for nbnet used to send and receive packets.

nbnet comes with three ready-to-use drivers:

- UDP: works with a single UDP socket, designed for desktop games
- WebRTC (WASM): works with a single unreliable/unordered data channel, implemented in JS using emscripten API
- WebRTC (Native): works the same way as the WASM WebRTC driver but can be natively compiled 

## How to use

In *exactly one* of your source files do:

```
#define NBNET_IMPL

#include "nbnet.h"
```

Provide a driver implementation. For instance, for the UDP driver, just add:

```
#include "net_drivers/udp.h"
```

after including the nbnet header in the same source file where you defined `NBNET_IMPL`.

nbnet does not provide any logging capabilities so you have to provide your own:

```
#define NBN_LogInfo(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogError(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogDebug(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogTrace(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogWarning(...) SomeLoggingFunction(__VA_ARGS__)
```

For memory management, nbnet uses `malloc`, `realloc` and `free`. You can redefine them if needed:

```
#define NBN_Allocator malloc
#define NBN_Reallocator realloc
#define NBN_Deallocator free
```

All set, from here, I suggest you hop into the examples. If you are interested in WebRTC, go [here](WEBRTC.md).

## Byte arrays

nbnet comes with a primitive bit-level serialization system; but, if you want to use your own serialization solution, nbnet lets you send and receive raw byte arrays.

See the [echo_bytes](https://github.com/nathhB/nbnet/tree/master/examples/echo_bytes) example.
