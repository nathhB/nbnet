/*

Copyright (C) 2022 BIAGINI Nathan

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

const unix = require('unix-dgram')
const nbnet = require('../index.js')
const loggerFactory = require('../logger.js')

const IPC_PACKET_HEADER_SIZE = 4

function usage() {
    console.log('Usage: node webrtc-ipc_proxy.js PROTOCOL_ID PORT SOCK_PATH')
}

function buildProtocolId(protocol_name) {
    /*let protocol_id = 2166136261

    for (let i = 0; i < protocol_name.length; i++)
    {
        protocol_id *= 16777619
        protocol_id ^= protocol_name[i]
    }

    return protocol_id*/
    return 42
}

function sendPacket(sockPath, peerId, data) {
    let buffer = Buffer.from(data)
    let packetData = Buffer.alloc(IPC_PACKET_HEADER_SIZE + buffer.length)

    packetData.writeUInt32BE(peerId)
    buffer.copy(packetData, IPC_PACKET_HEADER_SIZE)
    clientSocket.sendto(packetData, 0, packetData.length, sockPath)
}

const logger = loggerFactory.createLogger('webrtc-ipc_proxy')
const args = process.argv.slice(2)

if (args.length < 3)
{
    usage()
    process.exit(1)
}

const protocol_id = buildProtocolId(args[0])
const port = args[1]
const sockPath = args[2]
const useHttps = false // TODO: read from command line

const signalingServer = new nbnet.Standalone.SignalingServer(
    protocol_id,
    useHttps ? { https: true, key: UTF8ToString(keyPem), cert: UTF8ToString(certPem) } : {}
)

const gameServer = new nbnet.GameServer(signalingServer)
const serverSocket = unix.createSocket('unix_dgram', (buffer) => {
    console.log('PACKET RECEIVED FROM NBNET')
    let peerId = buffer.readUInt32BE()
    let data = buffer.subarray(IPC_PACKET_HEADER_SIZE)

    gameServer.send(data, peerId)
})
const clientSocket = unix.createSocket('unix_dgram')

serverSocket.on('error', (err) => {
    logger.error(err)
    process.exit(1)
})

clientSocket.on('error', (err) => {
    logger.error(err)
    process.exit(1)
})

serverSocket.on('listening', () => {
    logger.info('Started listening on UNIX socket (path: %s)...', sockPath)
})

gameServer.onPacketReceived = (packet, peerId) => {
    // logger.info('PACKET RECEIVED FROM WEBRTC: %d %d', packet.byteLength, peerId)
    sendPacket(sockPath, peerId, packet) 
}

serverSocket.bind(sockPath)
gameServer.start(port)