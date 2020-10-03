const Connection = require('./connection.js')
const loggerFactory = require('../logger.js')

function SignalingServer(protocol_id) {
    this.protocol = protocol_id.toString()
    this.logger = loggerFactory.createLogger('StandaloneSignalingServer')
}

SignalingServer.prototype.start = function(port) {
    return new Promise((resolve, reject) => {
        this.logger.info('Starting (protocol: %s)...', this.protocol)

        const server = require('http').createServer((request, response) => {
            this.logger.info('Received request for ' + request.url)

            response.writeHead(404)
            response.end()
        })

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

module.exports = SignalingServer
