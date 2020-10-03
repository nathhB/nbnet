const loggerFactory = require('../logger.js')

function SignalingClient(protocol_id) {
    this.protocol = protocol_id.toString()
    this.logger = loggerFactory.createLogger('StandaloneSignalingClient')
    this.connected = false
}

SignalingClient.prototype.connect = function(host, port) {
    return new Promise((resolve, reject) => {
        const uri = `ws://${host}:${port}`

        this.logger.info(`Connecting to ${uri} (protocol: %s)...`, this.protocol)

        const WebSocket = require('websocket').w3cwebsocket

        this.ws = new WebSocket(uri, this.protocol, `http://${host}:${port}`)

        this.ws.onclose = (ev) => {
            if (this.connected) {
                this.logger.error('Connection closed')

                this.connected = false

                this.onClosed()
            } else {
                this.logger.error('Connection failed')

                reject()
            }
        }

        this.ws.onopen = () => {
            this.logger.info('Connected')

            this.connected = true
            
            clearTimeout(timeoutId)
            resolve()
        }

        this.ws.onmessage = (ev) => {
            this.logger.info('Received signaling data: %s', ev.data)

            this.onDataReceived(JSON.parse(ev.data))
        }

        const timeoutId = setTimeout(() => {
            this.logger.error('Connection timeout')

            reject()
        }, 3000)
    })
}

SignalingClient.prototype.send = function(data) {
    this.logger.info('Send signaling data: %s', data)

    this.ws.send(JSON.stringify(data))
}

SignalingClient.prototype.close = function() {
    this.logger.info('Closing...')

    const WebSocket = require('websocket').w3cwebsocket

    return new Promise((resolve, reject) => {
        if (this.ws.readyState != WebSocket.OPEN) {
            this.logger.warn('Not opened')

            reject()
        } else {
            this.ws.onclose = (_) => {
                this.logger.info('Closed')

                resolve()
            }

            this.ws.close()
        }
    })
}

module.exports = SignalingClient
