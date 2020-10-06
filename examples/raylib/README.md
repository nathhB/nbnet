# Raylib

This example demonstrates almost all the features of nbnet, it is much more advanced than the echo example.

Each client is represented as a colored square and can:

- move inside the window (with the directional keys)
- switch color (with the spacebar)
- increase or decrease a float value (with K and J jeys, used to demonstrate float serialization)

[See it running](https://www.youtube.com/watch?v=BJl_XN3QJhQ&ab_channel=NathanBIAGINI).

Each client is responsible of sending updates about his own state to the server.
The server gathers states from all clients and stores them.
Every tick, the server packs the latest received client states in a message that is then broadcasted to all clients.
Each client displays a representation of other clients based on the latest received states from the server.

This example protocol is implemented using three messages:

- UpdateStateMessage (unreliabe)

	This message is sent by a client to the server every tick. It contains the most up to date client state data (position and float value).

- ChangeColorMessage (reliable)

	This message is sent by a client to the server every time it changes its color.

- GameStateMessage (unreliable)

	This message is broadcasted by the server to all connected clients. It contains the most up-to-date states of all clients.

This example also demonstrates how to use the nbnet network conditions simulation, both client and server accept the following command line options:

`--packet_loss=<value> # percentage (0 - 1), float value`

`--packet_duplication=<value> # percentage (0 -1), float value`

`--ping=<value> # in secondes, float value`

`--jitter=<value> # in seconds, float value`

Information about the state of the connection will be displayed in the bottom right of the client window.

## Web

To compile the example for the web:

`mkdir build`

`cd build`

`emcmake cmake -DRAYLIB_LIBRARY_PATH=<path to libraylib.bc file> -DRAYLIB_INCLUDE_PATH=<path to raylib headers>`

To run the server:

`npm run server`

You can pass options to the server like so:

`npm run server -- --packet_loss=<value> ...`

To run the client you need to have an HTTP server running and serving the build directory (it contains the HTML file), then you just have to open `http://localhost:<PORT>/client.html` in your browser.

*NOTE: For now there is no way to pass options to the client when it's running in the browser*
