# nbnet

nbnet is a single header C (C99) library to implement client-server network code for games, it is more precisely designed for fast paced action games.
nbnet is based on this [great series of articles](https://gafferongames.com/) by Glenn Fiedler.

**Disclaimer**: nbnet is in the early stages of his development and is first and foremost a learning project of mine as I explore online game development. If you are looking for a profesionnal production ready library, this is not the one.

The library currently implements the following features:

- Connection management
- Sending/Receiving both reliable ordered and unreliable ordered messages
- Sending/Receiving messages larger than MTU via a message fragmentation system
- Serialization of integers (signed and unsigned), floats, booleans and byte arrays (at bit level to reduce bandwith usage)
- Simulates bad network conditions for testing purposes (ping, jitter, packet loss, packet duplication and out of order packets)
- Estimates of ping, bandwith (upload and download) and packet loss
- WebRTC support

What is not *yet* implemented but will be in the future:

- Encrypted packets
- Secure authentification system

nbnet does not directly implement any low level "transport" code and rely on the concept of *drivers*. A driver is a set of function definitions that live outside nbnet header and provide a transport layer implementation that will be used by the library to send and receive packets.

nbnet comes with two drivers:

- UDP : work with a single UDP socket, designed for desktop games
- WebRTC : work with a single unreliable/unordered data channel, designed for web browser games

You can see it in action [in this video](https://www.youtube.com/watch?v=BJl_XN3QJhQ&ab_channel=NathanBIAGINI).

## Portability

nbnet is developed with portability in mind. I tested (and will continue to do so) the library on the following systems:

- Windows
- OSX
- Linux

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

From here I suggest you hop into the examples. If you are interested in using the WebRTC driver, read below.

### Byte arrays

If you don't want to use the built in nbnet message serialization and use your own, you can work with raw byte arrays.

See the [echo_bytes]() example.

## WebRTC

nbnet lets you implement web browser online games in C without writing any JS code.

In order to do that you need to compile your code with [emscripten](https://emscripten.org/). You can either use the
`emcc` command directly or use CMake, the examples demonstrate how to do both.

The following emscripten options are mandatory and must always be added to your compilation command line or CMakeLists.txt.

emscripten does not provide a C API for WebRTC so it has to be implemented in JS. nbnet does that for and all you have to
do is add:

`--js-library "net_drivers/webrtc/js/api.js"`

The nbnet JS API uses a bunch of async functions that you need to let emscripten know about:

`-s ASYNCIFY`

`-s ASYNCIFY_IMPORTS="[\"__js_game_client_start\", \"__js_game_client_close\", \"__js_game_server_start\\"]"`

nbnet connection simulation run in a separate thread so if you want to use it you need to compile with:

`-s USE_PTHREADS=1`

If you want to run your code in a web browser you also need to provide a shell file with:

`--shell-file <PATH TO YOUR SHELL FILE>`

To learn about what a shell file is: https://emscripten.org/docs/tools_reference/emcc.html

You can also look at the `shell.html` from the raylib example.

Apart from that you will probably want to add:

`-s ALLOW_MEMORY_GROWTH=1` and `-s EXIT_RUNTIME=1`

For more information: https://emscripten.org/docs/tools_reference/emcc.html

### NodeJS

Most of the time you will want your server code to run in NodeJS. You can get it from [here](https://nodejs.org/en/download/).

Once it's installed you also need to create a `package.json` file. For more information about what this file is:

https://docs.npmjs.com/creating-a-package-json-file

You can check out the `package.json` files used for the echo and raylib examples.

In order to run your server you need to install the needed node packages by running:

`npm install`

from the directory containing your `package.json` file.

Then to run your server:

`node server.js`

`server.js` being the JS file generated by emscripten.

### Web browser

Unless your client is a non graphical application, you will want your client code to run in a web browser.

With the correct options emscripten will output an HTML file, rom here all you need to do is run an HTTP server that serves
this file and open the correct URL in your web browser.

For testing purposes I personally recommend using Python [SimpleHTTPServer](https://docs.python.org/2/library/simplehttpserver.html).

Just run:

`python -m SimpleHTTPServer 8000`

in the directory containing your HTML file. Then, assuming the HTML file is named `client.html`, open `http://localhost:8000/client.html` in your web browser.

One major diffrence with running JS code in a web browser compared to running it in NodeJS is that you cannot use the NodeJS
packaging system. The thing is that the nbnet WebRTC code has some NodeJS packages as dependencies, so in order for nbnet to
run in a web browser we need to "bundle" those packages in something that can be served to a browser.

This seems complicated but it's really not, I personally recommend using [browserify](https://github.com/browserify/browserify).

Once installed you can simply run:

`browserify net_drivers/webrtc/js/nbnet.js -o nbnet_bundle.js`

and include the generated `nbnet_bundle.js` script to your HTML:

`<script src="nbnet_bundle.js"></script>`

See the raylib example.
