// TS side of the C++<->TS DHT interop test (TsInteropTest.cpp).
//
// Starts a TypeScript layer-0 DhtNode from the pinned @streamr/dht npm
// package as a websocket entry point on 127.0.0.1:<port>, joins its own
// DHT, and then speaks a line protocol on stdout:
//
//   DESCRIPTOR <hex>     once ready; hex is the protobuf-binary encoding of
//                        the node's PeerDescriptor (exact wire fidelity, so
//                        the C++ side parses it with PeerDescriptor protobuf)
//   NEIGHBORS <id,...>   every 500 ms; comma-separated hex node ids of the
//                        node's current DHT neighbors (may be empty)
//
// The entry-point construction mirrors packages/dht/test/end-to-end/
// Layer0.test.ts at the pin. region is passed explicitly because without it
// start() falls back to a GeoIP/CDN lookup over the real network.
const { DhtNode, PeerDescriptor, toNodeId } = require('@streamr/dht')

const port = parseInt(process.argv[2], 10)
if (!(port > 0)) {
    console.error('usage: node entryPoint.js <websocket-port>')
    process.exit(1)
}

const main = async () => {
    const node = new DhtNode({
        websocketHost: '127.0.0.1',
        websocketPortRange: { min: port, max: port },
        websocketServerEnableTls: false,
        region: 0
    })
    await node.start()
    await node.joinDht([node.getLocalPeerDescriptor()])
    const descriptorHex = Buffer.from(
        PeerDescriptor.toBinary(node.getLocalPeerDescriptor())
    ).toString('hex')
    console.log(`DESCRIPTOR ${descriptorHex}`)
    setInterval(() => {
        const neighborIds = node.getNeighbors().map((n) => toNodeId(n))
        console.log(`NEIGHBORS ${neighborIds.join(',')}`)
    }, 500)
}

main().catch((err) => {
    console.error(err)
    process.exit(1)
})
