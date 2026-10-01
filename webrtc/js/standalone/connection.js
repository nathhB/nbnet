function Connection(connection) {
    this.connection = connection
    this.id = connection.remoteAddress

    connection.on('message', (msg) => { this.onMessageReceived(msg.utf8Data) })
    connection.on('close', () => { this.onClosed() })
}

Connection.prototype.send = function(data) {
    this.connection.sendUTF(JSON.stringify(data))
}

Connection.prototype.close = function() {
    this.connection.close()
}

module.exports = Connection