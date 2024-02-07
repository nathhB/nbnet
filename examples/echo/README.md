# Echo

This is a very basic echo client server example, the server accepts a single client at a time and echoes all
messages it receives.

## UDP

Use the CMake script to build the example:

```
cmake .
make
```

To run the server simply do:

`./echo_server`

and to run the client:

`./echo_client "some message"`

The client will run indefinitely and send the provided string to the server every tick (30 times per second).

## WebRTC

To target WASM:

```
emcmake cmake .
make
npm install
```

To run this example you need to have nodejs installed (see the package.json file).

To run the server simply do:

`npm run server`

and to run the client:

`npm run client "some message"`

You can also compile using the native WebRTC driver:

```
cmake -DWEBRTC_NATIVE_SERVER=ON -DLIBDATACHANNEL_LIBRARY_PATH=<PATH TO LIBDATACHANNEL LIBRARY> -DLIBDATACHANNEL_INCLUDE_PATH=<PATH TO LIBDATACHANNEL HEADERS> .
make
```

To run the server simply do:

`./echo_server`

You should then be able to connect with both UDP and node clients.

Use `-DWEBRTC_NATIVE_CLIENT=ON` if you want to use the WebRTC native driver on the client instead of UDP.
