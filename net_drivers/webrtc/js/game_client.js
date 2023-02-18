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

function GameClient(signalingClient) {
    this.signalingClient = signalingClient
    this.peer = new Peer(0, signalingClient)
    this.packets = []
    this.logger = loggerFactory.createLogger('GameClient')

    this.signalingClient.onDataReceived = (data) => {
        this.peer.notifySignalingData(data)
    }

    this.signalingClient.onClosed = () => {
        // this.onClosed()
    }

    this.peer.onError = (err) => {
        this.logger.info('Peer has ecountered an error: %s. Closing connection', err)

        this.signalingClient.close()
    }

    this.peer.onClosed = (err) => {
        this.logger.info('Peer closed')

        this.signalingClient.close()
    }

    this.peer.onPacketReceived = (packet) => {
        this.packets.push(packet)
    }
}

GameClient.prototype.connect = function(host, port) {
    this.logger.info('Connecting...')

    return new Promise((resolve, reject) => {
        this.signalingClient.connect(host, port).then(() => {
            this.logger.info('Creating server peer...')

            this.peer.onConnected = () => {
                this.logger.info('Connected')
                
                resolve()
            }

            this.peer.onConnectionError = (err) => {
                this.logger.info('Connection failed: %s', err)

                reject(err)
            }

            this.peer.connect()
        }).catch((err) => {
            reject(err)
        })
    })
}

GameClient.prototype.send = function(data) {
    this.peer.send(data)
}

GameClient.prototype.close = function() {
    return new Promise((resolve, reject) => {
        this.signalingClient.close()
            .then(() => resolve())
            .catch(() => reject())
    })
}

module.exports = GameClient
