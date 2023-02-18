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

const webrtc = require('wrtc')
const RTCPeerConnection = webrtc.RTCPeerConnection
const RTCSessionDescription = webrtc.RTCSessionDescription
const loggerFactory = require('./logger.js')

function Peer(id, connection) {
    this.id = id
    this.connection = connection
    this.candidates = []
    this.peerConnection = new RTCPeerConnection({ 'iceServers': [{ 'urls': 'stun:stun01.sipphone.com' }] })
    this.channel = this.peerConnection.createDataChannel('unreliable',
        { negotiated: true, id: 0, maxRetransmits: 0, ordered: false })
    this.channel.binaryType = 'arraybuffer'

    // peer connection event listeners
    this.peerConnection.addEventListener('icecandidate', ({ candidate }) => { onIceCandidate(this, candidate) })
    this.peerConnection.addEventListener('signalingstatechange', () => { onSignalingStateChange(this) })
    this.peerConnection.addEventListener('icegatheringstatechange', () => { onIceGatheringStateChanged(this) })
    this.peerConnection.addEventListener('icecandidateerror', (ev) => {
        this.logger.error('Error while gathering ice candidates: %s', ev.errorCode)
    })

    this.channel.addEventListener('open', () => { onDataChannelOpened(this, this.channel) })
    this.channel.addEventListener('error', () => { onDataChannelError(this, this.channel) })
    this.channel.addEventListener('message', (ev) => { onPacketReceived(this, ev.data) })

    this.logger = loggerFactory.createLogger(`Peer(${this.id})`)
}

Peer.prototype.connect = function() {
    this.state = 'connecting'

    // OfferToReceiveAudio & OfferToReceiveVideo seems to be required in Chrome even if we only need data channels
    this.peerConnection.createOffer({ mandatory: { OfferToReceiveAudio: true, OfferToReceiveVideo: true } }).then((description) => {
        this.peerConnection.setLocalDescription(description).then(() => {
            this.logger.info('Offer created and set as local description')

            this.connection.send(description)
        }).catch((err) => {
            raiseError(this, `setLocalDescription: ${err}`)
        })
    }).catch((err) => {
        raiseError(this, `createOffer: ${err}`)
    })
}

Peer.prototype.close = function() {
    this.logger.info('Closing peer...')

    this.peerConnection.close()
    this.connection.close()
}

Peer.prototype.notifySignalingData = function(data) {
    if ('type' in data) {
        if (data.type === 'offer') {
            handleOffer(this, data)
        } else if (data.type === 'answer') {
            handleAnswer(this, data)
        }
    } else if ('candidate' in data) {
        handleCandidate(this, data.candidate)
    } else if ('signaling' in data) {
        if (data.signaling.ready_for_candidates) {
            this.logger.info('Remote peer is ready to receive our ice candidates')

            this.isRemotePeerReadyToReceiveRemoteIceCandidates = true
        }
    }
}

Peer.prototype.send = function(data) {
    try {
        this.channel.send(data)
    } catch(err) {
        this.logger.error('Failed to send: ' + err)

        this.close()
    }
}

function onIceCandidate(peer, candidate) {
    if (candidate) {
        peer.logger.info('Got new candidate, save it to the candidates list')

        peer.candidates.push(candidate)
    }
}

function onSignalingStateChange(peer) {
    if (peer.peerConnection.signalingState == 'closed') {
        peer.logger.info('Closed')

        peer.onClosed()
    }
}

function onIceGatheringStateChanged(peer) {
    peer.logger.info('Ice gathering state changed: %s', peer.peerConnection.iceGatheringState)

    if (peer.peerConnection.iceGatheringState === 'complete') {
        peer.logger.info('All candidates gathered, waiting for the remote peer to be ready to receive them')

        waitForRemotePeerToBeReadyToReceiveIceCandidates(peer).then(() => {
            if (peer.state !== 'connected') {
                sendCandidates(peer)
            }
        }).catch((err) => {
            raiseError(peer, `waitForRemotePeerToBeReadyToReceiveIceCandidates: ${err}`)
        })
    }
}

function handleOffer(peer, offer) {
    peer.logger.info('Got offer')

    peer.peerConnection.setRemoteDescription(new RTCSessionDescription(offer)).then(() => {
        onRemoteDescriptionSet(peer)
        createAnswer(peer)
    }).catch((err) => {
        raiseError(peer, `setRemoteDescription: ${err}`)
    })
}

function handleAnswer(peer, answer) {
    peer.logger.info('Got answer')

    peer.peerConnection.setRemoteDescription(answer).then(() => {
        onRemoteDescriptionSet(peer)
    }).catch((err) => {
        raiseError(peer, `setRemoteDescription: ${err}`)
    })
}

function onRemoteDescriptionSet(peer) {
    // remote description is set so we can now add the other peer ice candidates
    // notify the other peer that we are ready to treat his ice candidates
    // addIceCandidate fail if remote description is not set

    peer.logger.info('Remote description set, notify remote peer that we are ready to receive his ice candidates')

    peer.connection.send({ signaling: { ready_for_candidates: true } })
}

function handleCandidate(peer, candidate) {
    peer.logger.info(`Got candidate: ${JSON.stringify(candidate)}`)

    if (candidate.candidate) {
        peer.peerConnection.addIceCandidate(candidate).then(() => {
            peer.logger.info('Candidate added')
        }).catch((err) => {
            raiseError(peer, `addIceCandidate: ${err}`)
        })
    }
}

function createAnswer(peer) {
    peer.peerConnection.createAnswer().then((answer) => {
        peer.logger.info('Answer created')

        peer.peerConnection.setLocalDescription(answer).then(() => {
            peer.logger.info('Answer set as local description, signaling it')

            peer.connection.send(answer)
        }).catch((err) => {
            raiseError(peer, `setLocalDescription: ${err}`)
        })
    }).catch((err) => {
        raiseError(peer, `createAnswer: ${err}`)
    })
}

function waitForRemotePeerToBeReadyToReceiveIceCandidates(peer) {
    return new Promise((resolve, reject) => {
        const timeoutId = setTimeout(() => {
            reject('timeout')
        }, 5000)

        const intervalId = setInterval(() => {
            if (peer.state === 'connected' || peer.isRemotePeerReadyToReceiveRemoteIceCandidates) {
                clearTimeout(timeoutId)
                clearInterval(intervalId)
                resolve()
            }
        }, 500)
    })
}

function sendCandidates(peer) {
    peer.logger.info('Sending all gather candidates...')

    peer.candidates.forEach((candidate) => {
        peer.connection.send({ candidate: candidate })
    })
}

function onDataChannelOpened(peer, dataChannel) {
    peer.logger.info('%s data channel opened (id: %d)', dataChannel.label, dataChannel.id)

    peer.state = 'connected'

    peer.onConnected()
}

function onDataChannelError(peer, dataChannel, err) {
    raiseError(peer, `Data channel ${dataChannel.label} (id: ${dataChannel.id}) error: ${err}`)
}

function onPacketReceived(peer, packet) {
    peer.onPacketReceived(packet)
}

function raiseError(peer, err) {
    peer.logger.error(err)

    if (peer.state === 'connecting' && peer.onConnectionError) {
        peer.onConnectionError(this)
    } else if (peer.onError) {
        peer.onError(err)
    }
}

module.exports = Peer
