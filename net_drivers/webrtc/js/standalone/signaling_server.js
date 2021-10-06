const Connection = require('./connection.js')
const loggerFactory = require('../logger.js')

function SignalingServer(protocol_id, options) {
    this.protocol = protocol_id.toString()
    this.logger = loggerFactory.createLogger('StandaloneSignalingServer')
    this.options = options
}

SignalingServer.prototype.start = function(port) {
    return new Promise((resolve, reject) => {
        this.logger.info('Starting (protocol: %s)...', this.protocol)

        var server

        if (this.options['https']) {
            const fs = require('fs')

            server = createHttpsServer(this, fs.readFileSync(this.options['key']), fs.readFileSync(this.options['cert']))
        } else {
            server = createHttpServer(this)
        }

        const WebSocketServer = require('websocket').server

        this.wsServer = new WebSocketServer({
            httpServer: server,
            autoAcceptConnections: false
        })

        this.wsServer.on('request', (request) => {
            this.logger.info('New connection')

            try {
                this.onConnection(new Connection(request.accept(this.protocol, request.origin)))
            } catch (err) {
                this.logger.error('Connection rejected: %s', err)
            }
        })

        server.listen(port, () => {
            this.logger.info('Started, listening on port %d...', port);

            resolve()
        })
    })
}

SignalingServer.prototype.stop = function() {
    if (this.wsServer) {
        this.wsServer.shutDown()
    } else {
        this.logger.error("Not started")
    }
}

SignalingServer.prototype.isSecure = function() {
    return this.options['https']
}

function createHttpServer(signalingServer) {
    return require('http').createServer((request, response) => {
        signalingServer.logger.info('Received request for ' + request.url)

        response.writeHead(404)
        response.end()
    })
}

function createHttpsServer(signalingServer, key, cert) {
    return require('https').createServer({ key: key, cert: cert }, (request, response) => {
        signalingServer.logger.info('Received request for ' + request.url)

        response.writeHead(404)
        response.end()
    })
}

module.exports = SignalingServer
