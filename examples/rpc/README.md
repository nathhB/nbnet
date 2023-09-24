# RPC

Simple example demonstrating how to use RPCs.

Use the CMake script to compile the example:

```
cmake .
make
```

To run the server simply do:

`./rpc_server`

and to run the client:

`./rpc_client`

## WebRTC

Use the CMake script to compile the example:

```
EMSCRIPTEN=1 cmake .
make
```

To run this example you need to have nodejs installed (see the package.json file).

To run the server simply do:

`npm run server`

and to run the client:

`npm run client`
