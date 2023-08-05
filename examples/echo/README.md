# Echo

This is a very basic echo client server example, the server accepts a single client at a time and echoes all
messages it receives.

Use the CMake script to compile the example:

```
cmake .
make
```

To run the server simply do:

`./echo_server`

and to run the client:

`./echo_client "some message"`

The client will run indefinitely and send the provided string to the server every tick (30 times per second).

## WebRTC (JS driver)

Use the CMake script to compile the example:

```
EMSCRIPTEN=1 cmake .
make
npm install
```

To run this example you need to have nodejs installed (see the package.json file).

To run the server simply do:

`npm run server`

and to run the client:

`npm run client "some message"`
