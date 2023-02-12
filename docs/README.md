# nbnet

![logo](logo/logo.png "Logo")

[![Build Status](https://app.travis-ci.com/nathhB/nbnet.svg?branch=master)](https://app.travis-ci.com/nathhB/nbnet)
[![CodeFactor](https://www.codefactor.io/repository/github/nathhb/nbnet/badge/master)](https://www.codefactor.io/repository/github/nathhb/nbnet/overview/master)

[![](https://dcbadge.vercel.app/api/server/P9N7fy677D)](https://discord.gg/P9N7fy677D)

nbnet is a single header C (C99) library to implement client-server network code for games. It is more precisely designed for fast-paced action games.

nbnet is based on this [great series of articles](https://gafferongames.com/) by Glenn Fiedler.

nbnet aims to be as easy to use as possible. nbnet's API is *friendly* and goes *straight to the point*; it relies on event polling which makes it easy to integrate into a game loop.

**Disclaimer**: nbnet is in the early stages of its development and is, first and foremost, a learning project of mine as I explore online game development. If you are looking for a professional production-ready library, this is not the one.

You can see nbnet in action [in this video](https://www.youtube.com/watch?v=BJl_XN3QJhQ&ab_channel=NathanBIAGINI).

If you want to discuss the library, you can join the [nbnet's discord server](https://discord.gg/esR8FSyPnF).

## Documentation

docsforge appears to be dead so I need to find another documentation solution :(

## Features

- Connection management
- Sending/Receiving both reliable ordered and unreliable ordered messages
- Sending/Receiving messages larger than the MTU (using nbnet's message fragmentation)
- Bit-level serialization (for bandwidth optimization): integers (signed and unsigned), floats, booleans, and byte arrays
- Network conditions simulation: ping, jitter, packet loss, packet duplication, and out of order packets)
- Network statistics: ping, bandwidth (upload and download) and packet loss
- Web (WebRTC) support (powered by [emscripten](https://emscripten.org/docs/introducing_emscripten/about_emscripten.html))
- Encrypted and authenticated packets

## Thanks

nbnet encryption and packet authentication would not have been possible without those three open-source libraries:

- [tiny-ECDH](https://github.com/kokke/tiny-ECDH-c)
- [tiny-AES](https://github.com/kokke/tiny-AES-c)
- [poly1305-donna](https://github.com/floodyberry/poly1305-donna)

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

## Drivers

nbnet does not directly implement any low level "transport" code and rely on *drivers*.

A driver is a set of function definitions that live outside the nbnet header and provide a transport layer implementation for nbnet used to send and receive packets.

nbnet comes with two ready to use drivers:

- UDP : work with a single UDP socket, designed for desktop games
- WebRTC : work with a single unreliable/unordered data channel, designed for web browser games

## Portability

nbnet is developed with portability in mind. I tested (and will continue to do so) the library on the following platforms:

- Windows
- OSX
- Linux
- Web (Chrome/Firefox/Microsoft Edge/Brave)

## How to use

In *exactly one* of your source file do:

```
#define NBNET_IMPL

#include "nbnet.h"
```

Provide a driver implementation. For the UDP driver, just add:

```
#include "net_drivers/udp.h"
```

after including the nbnet header in the same source file where you defined `NBNET_IMPL`.

nbnet does not provide any logging capacibilities so you have to provide your own:

```
#define NBN_LogInfo(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogError(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogDebug(...) SomeLoggingFunction(__VA_ARGS__)
#define NBN_LogTrace(...) SomeLoggingFunction(__VA_ARGS__)
```

For memory management, nbnet uses `malloc`, `realloc` and `free`. You can redefine it using the following macros:

```
#define NBN_Allocator malloc
#define NBN_Reallocator realloc
#define NBN_Deallocator free
```

All set, from here, I suggest you hop into the examples. If you are interested in using the WebRTC driver, read below.

## Byte arrays

nbnet comes with a primitive bit-level serialization system; but, if you want to use your own serialization solution, nbnet lets you send and receive raw byte arrays.

See the [echo_bytes](https://github.com/nathhB/nbnet/tree/master/examples/echo_bytes) example.

## WebRTC

nbnet lets you implement web browser online games in C without writing any JS code.

To do that, you need to compile your code with *emscripten*. You can either use the
`emcc` command directly or use CMake, the examples demonstrate how to use both.

The following emscripten options are mandatory and must always be added to your compilation command line or CMake script.

*emscripten* does not provide a C API for WebRTC, only a JS one. nbnet provides a wrapper around it so you don't have to write any JS code (oof!). All you have to is compile with:

`--js-library "net_drivers/webrtc/js/api.js"`

The nbnet JS API uses a bunch of asynchronous functions that you need to let *emscripten* know about:

`-s ASYNCIFY`

`-s ASYNCIFY_IMPORTS="[\"__js_game_client_start\", \"__js_game_client_close\", \"__js_game_server_start\\"]"`

nbnet network conditions simulation run in a separate thread so if you want to use it you need to compile with:

`-s USE_PTHREADS=1`

If you want to run your code in a web browser, you need to provide a shell file:

`--shell-file <PATH TO YOUR SHELL FILE>`

To learn about shell files: https://emscripten.org/docs/tools_reference/emcc.html

You can also look at the `shell.html` from the raylib example.

Apart from that, you probably want to add:

`-s ALLOW_MEMORY_GROWTH=1` and `-s EXIT_RUNTIME=1`

For more information: https://emscripten.org/docs/tools_reference/emcc.html

### NodeJS

Most of the time, you want your server code to run in a *NodeJS* server. You can get *NodeJS* it from [here](https://nodejs.org/en/download/).

Once it's installed, you need to create a `package.json` file. Check out the *echo* and *raylib* examples to see what this file looks like. (For more information: https://docs.npmjs.com/creating-a-package-json-file)

To run your server, you need to install the required *NodeJS* packages by running:

`npm install`

from the directory containing your `package.json` file.

Then to run your server:

`node server.js`

`server.js` being the JS file generated by *emscripten*.

### Web browser

Unless your client is a non-graphical application, you want your client code to run in a web browser.

With the correct options *emscripten* will output an HTML file. From here, all you need to do is run an HTTP server that serves
this file and open it in your web browser.

For testing purposes, I recommend using Python [SimpleHTTPServer](https://docs.python.org/2/library/simplehttpserver.html).

Just run:

`python -m SimpleHTTPServer 8000`

in the directory containing your HTML file; then, open `http://localhost:8000` in your web browser and open your client HTML file.

One significant difference with running JS code in a web browser compared to running it in *NodeJS* is that you cannot use the *NodeJS* packaging system. nbnet's WebRTC code relies on *NodeJS* packages, so, for nbnet to run in a web browser we need to "bundle" those packages into something that can be used by a web browser.

I recommend using [browserify](https://github.com/browserify/browserify).

Once installed you can run:

`browserify net_drivers/webrtc/js/nbnet.js -o nbnet_bundle.js`

and include the generated `nbnet_bundle.js` script to your HTML shell file:

`<script src="nbnet_bundle.js"></script>`

See the *raylib* example to see this operation integrated into a CMake script.
