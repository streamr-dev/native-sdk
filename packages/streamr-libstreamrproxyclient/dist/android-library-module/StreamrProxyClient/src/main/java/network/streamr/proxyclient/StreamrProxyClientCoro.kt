package network.streamr.proxyclient

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class StreamrProxyClientCoro @Throws(ProxyClientException::class) constructor(
    ethereumAddress: String,
    streamPartId: String
) : AutoCloseable {
    private val client: StreamrProxyClient = StreamrProxyClient(ethereumAddress, streamPartId)

    suspend fun connect(proxies: List<StreamrProxyAddress>): StreamrProxyResult =
        withContext(Dispatchers.IO) {
            client.connect(proxies)
        }

    suspend fun publish(content: String, ethereumPrivateKey: String?): StreamrProxyResult =
        withContext(Dispatchers.IO) {
            client.publish(content, ethereumPrivateKey)
        }

    override fun close() {
        client.close()
    }

}