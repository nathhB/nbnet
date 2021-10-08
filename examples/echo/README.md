# Echo

This is a very basic echo client server example, the server accepts a single client at a time and echoes all
messages it receives.

Here is how to compile it with gcc:

`gcc client.c shared.c -lm -o client`

`gcc server.c shared.c -lm -o server`

To run the server simply do:

`./server`

and to run the client:

`./client "some message"`

The client will run indefinitely and send the given string to the server every tick (30 times per second).

## WebRTC

Here is how to copile this example with the WebRTC driver:

`emcc -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS="[\"__js_game_server_start\"]" --js-library "../../net_drivers/webrtc/js/api.js" server.c shared.c -o server.js`
`emcc -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS="[\"__js_game_client_start\", \"__js_game_client_close\"]" --js-library "../../net_drivers/webrtc/js/api.js" client.c shared.c -o client.js`

To run this example you need to have nodejs installed (see the package.json file).

To run the server:

`npm run server`

and to run the client:

`npm run client "some message"`
