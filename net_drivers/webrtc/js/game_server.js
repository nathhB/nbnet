/*

Copyright (C) 2023 BIAGINI Nathan

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

const loggerFactory = require('./logger.js')
const Peer = require('./peer.js')

function GameServer(signalingServer) {
    this.signalingServer = signalingServer
    this.peers = {}
    this.packets = []
    this.nextPeerId = 0;
    this.logger = loggerFactory.createLogger('GameServer')
}

GameServer.prototype.start = function(port) {
    if (this.signalingServer.isSecure()) {
        this.logger.info('Starting (HTTPS is enabled)...')
    } else {
        this.logger.info('Starting (HTTPS is disabled)...')
    }

    return new Promise((resolve, reject) => {
        this.signalingServer.onConnection = (connection) => { handleConnection(this, connection) }

        this.signalingServer.start(port).then(() => {
            this.logger.info('Started')

            resolve()
        }).catch((err) => {
            this.logger.error('Failed to start: %s', err)

            reject(err)
        })
    })
}

GameServer.prototype.send = function(packet, peerId) {
    const peer = this.peers[peerId]

    if (peer) {
        peer.send(packet)
    } else {
        this.logger.warn("Trying to send packet to an unknown peer: " + peerId)
    }
}

GameServer.prototype.closePeer = function(peerId) {
    const peer = this.peers[peerId]

    if (peer) {
        peer.close()
    }
}

GameServer.prototype.stop = function() {
    this.signalingServer.stop()
}

function handleConnection(gameServer, connection) {
    const peer = new Peer(gameServer.nextPeerId++, connection)

    peer.onConnected = () => {
        gameServer.logger.info('Peer %d is connected', peer.id)

        // gameServer.onPeerConnected(peer.id)
    }

    peer.onClosed = () => {
        gameServer.logger.info('Peer %d has disconnected', peer.id)

        removePeer(gameServer, peer)
        // gameServer.onPeerDisconnected(peer.id)
    }

    peer.onError = (err) => {
        gameServer.logger.info('Peer %d has ecountered an error: %s. Closing peer', peer.id, err)

        peer.close()
        removePeer(gameServer, peer) // make sure the peer is acutally removed, may not be needed
    }

    peer.onPacketReceived = (packet) => {
        if (gameServer.onPacketReceived) {
            gameServer.onPacketReceived(packet, peer.id)
        } else {
            gameServer.packets.push([packet, peer.id])
        }
    }

    gameServer.logger.info('Created new peer (id: %d) for connection %d', peer.id, connection.id)

    gameServer.peers[peer.id] = peer

    connection.onMessageReceived = (msg) => {
        gameServer.logger.info('Received signaling message for connection %s: %s', connection.id, msg)

        peer.notifySignalingData(JSON.parse(msg))
    }

    connection.onClosed = () => {
        gameServer.logger.info('Connection %s closed, will close peer %d', connection.id, peer.id)

        peer.close()
    }
}

function removePeer(gameServer, peer) {
    gameServer.logger.info('Remove peer %d', peer.id)

    for (const key in gameServer.peers) {
        if (gameServer.peers.hasOwnProperty(key) && gameServer.peers[key].id === peer.id) {
            delete gameServer.peers[key]
        }
    }
}

module.exports = GameServer
