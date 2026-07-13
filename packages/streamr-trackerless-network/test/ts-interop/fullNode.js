// TS side of the C++<->TS stream interop test (TsInteropStreamTest.cpp).
//
// Starts a TypeScript full node (NetworkStack from the pinned
// @streamr/trackerless-network npm package) as a websocket entry point
// on 127.0.0.1:<port>, joins its own layer-0 DHT and the interop stream
// part, and then speaks a line protocol:
//
//   stdout:
//     DESCRIPTOR <hex>   once ready; protobuf-binary PeerDescriptor
//     RECEIVED <text>    for every newMessage, with the utf8 content
//     SENT <text>        after a PUBLISH command's broadcast was issued
//   stdin:
//     PUBLISH <text>     broadcast a StreamMessage with the utf8 content
//
// region is passed explicitly because without it start() falls back to
// a GeoIP/CDN lookup over the real network.
const crypto = require('crypto')
const readline = require('readline')
const { PeerDescriptor } = require('@streamr/dht')
const { NetworkStack } = require('@streamr/trackerless-network')
// The published package does not export the generated-protos subpath;
// the numeric enum values from protos/NetworkRpc.proto are used
// directly (ContentType.JSON = 0, EncryptionType.NONE = 0,
// SignatureType.ECDSA_SECP256K1_EVM = 1).
const CONTENT_TYPE_JSON = 0
const ENCRYPTION_TYPE_NONE = 0
const SIGNATURE_TYPE_ECDSA_SECP256K1_EVM = 1
const { StreamPartIDUtils, hexToBinary, utf8ToBinary, binaryToUtf8 } =
    require('@streamr/utils')

const port = parseInt(process.argv[2], 10)
if (!(port > 0)) {
    console.error('usage: node fullNode.js <websocket-port>')
    process.exit(1)
}

const streamPartId = StreamPartIDUtils.parse('interop-stream#0')

const createMessage = (text) => {
    return {
        messageId: {
            streamId: StreamPartIDUtils.getStreamID(streamPartId),
            streamPartition: StreamPartIDUtils.getStreamPartition(streamPartId),
            timestamp: Date.now(),
            sequenceNumber: 0,
            publisherId: crypto.randomBytes(20),
            messageChainId: 'ts-interop-chain'
        },
        previousMessageRef: undefined,
        body: {
            oneofKind: 'contentMessage',
            contentMessage: {
                content: utf8ToBinary(text),
                contentType: CONTENT_TYPE_JSON,
                encryptionType: ENCRYPTION_TYPE_NONE
            }
        },
        signatureType: SIGNATURE_TYPE_ECDSA_SECP256K1_EVM,
        signature: hexToBinary('0x1234')
    }
}

const main = async () => {
    const stack = new NetworkStack({
        layer0: {
            websocketHost: '127.0.0.1',
            websocketPortRange: { min: port, max: port },
            websocketServerEnableTls: false,
            region: 0
        }
    })
    // No entry points configured: join our own DHT explicitly (the
    // doJoin path would wait for connectivity that never comes).
    await stack.start(false)
    const ownDescriptor = stack.getControlLayerNode().getLocalPeerDescriptor()
    await stack.getControlLayerNode().joinDht([ownDescriptor])
    stack.getContentDeliveryManager().setStreamPartEntryPoints(
        streamPartId, [ownDescriptor])
    stack.getContentDeliveryManager().joinStreamPart(streamPartId)
    stack.getContentDeliveryManager().on('newMessage', (msg) => {
        if (msg.body.oneofKind === 'contentMessage') {
            console.log(`RECEIVED ${binaryToUtf8(msg.body.contentMessage.content)}`)
        }
    })

    const descriptorHex =
        Buffer.from(PeerDescriptor.toBinary(ownDescriptor)).toString('hex')
    console.log(`DESCRIPTOR ${descriptorHex}`)

    const rl = readline.createInterface({ input: process.stdin })
    rl.on('line', (line) => {
        if (line.startsWith('PUBLISH ')) {
            const text = line.substring('PUBLISH '.length)
            stack.getContentDeliveryManager().broadcast(createMessage(text))
            console.log(`SENT ${text}`)
        }
    })
}

main().catch((err) => {
    console.error(err)
    process.exit(1)
})
