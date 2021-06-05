/*

Copyright (C) 2020 BIAGINI Nathan

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/

// --- Game server API ---

mergeInto(LibraryManager.library, {
    __js_game_server_send_packet_to__proxy: 'sync',
    __js_game_server_dequeue_packet__proxy: 'sync', 

    __js_game_server_init: function (protocol_id) {
        const nbnet = require('nbnet')
        const signalingServer = new nbnet.Standalone.SignalingServer(protocol_id)

        this.gameServer = new nbnet.GameServer(signalingServer)
    },

    __js_game_server_start: function (port) {
        return Asyncify.handleSleep(function (wakeUp) {
            this.gameServer.start(port).then(() => {
                wakeUp(0)
            }).catch(_ => {
                wakeUp(-1)
            })
        })
    },

    __js_game_server_dequeue_packet: function(peerIdPtr, lenPtr) {
        const packet = this.gameServer.packets.shift()

        if (packet) {
	    const packetData = packet[0]
            const packetSenderId = packet[1]
            const ptr = stackAlloc(packetData.byteLength)
            const byteArray = new Uint8Array(packetData)

            setValue(peerIdPtr, packetSenderId, 'i32')
            setValue(lenPtr, packetData.byteLength, 'i32')
            writeArrayToMemory(byteArray, ptr)

            return ptr
        } else {
            return null
        }
    },

    __js_game_server_send_packet_to: function (packetPtr, packetSize, peerId) {
        const data = new Uint8Array(Module.HEAPU8.subarray(packetPtr, packetPtr + packetSize))

        this.gameServer.send(data, peerId)
    },

    __js_game_server_close_client_peer: function(peerId) {
        this.gameServer.closePeer(peerId)
    },

    __js_game_server_stop: function() {
        this.gameServer.stop()
    }
})

// --- Game client API ---

mergeInto(LibraryManager.library, {
    __js_game_client_send_packet__proxy: 'sync',
    __js_game_client_dequeue_packet__proxy: 'sync',

    __js_game_client_init: function(protocol_id) {
        let nbnet

        if (typeof window === 'undefined') {
            // we are running in node so we can use require
            nbnet = require('nbnet')
        } else {
            // we are running in a web browser so we used to "browserified" nbnet (see nbnet.js)
            nbnet = Module.nbnet
        }
        const signalingClient = new nbnet.Standalone.SignalingClient(protocol_id)

        this.gameClient = new nbnet.GameClient(signalingClient)
    },

    __js_game_client_start: function(hostPtr, port) {
        return Asyncify.handleSleep(function (wakeUp) {
            this.gameClient.connect(UTF8ToString(hostPtr), port).then(() => {
                wakeUp(0)
            }).catch(_ => {
                wakeUp(-1)
            })
        })
    },

    __js_game_client_dequeue_packet: function(lenPtr) {
        const packet = this.gameClient.packets.shift()

        if (packet) {
            const ptr = stackAlloc(packet.byteLength)
            const byteArray = new Uint8Array(packet)

            setValue(lenPtr, packet.byteLength, 'i32')
            writeArrayToMemory(byteArray, ptr)

            return ptr
        } else {
            return null
        }
    },

    __js_game_client_send_packet: function (packetPtr, packetSize) {
        const data = new Uint8Array(Module.HEAPU8.subarray(packetPtr, packetPtr + packetSize))

        this.gameClient.send(data)
    },

    __js_game_client_close: function() {
        Asyncify.handleSleep(function (wakeUp) {
            this.gameClient.close().then(wakeUp).catch(wakeUp)
        });
    }
})
