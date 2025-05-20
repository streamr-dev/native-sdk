package network.streamr.proxyclient

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class StreamrProxyClientCoro() : AutoCloseable {
    private lateinit var client: StreamrProxyClient

    @Throws(ProxyClientException::class)
    constructor(
        ethereumAddress: String,
        streamPartId: String
    ) : this() {
        initialize(ethereumAddress, streamPartId)
    }

    @Throws(ProxyClientException::class)
    fun initialize(ethereumAddress: String, streamPartId: String) {
        client = StreamrProxyClient(ethereumAddress, streamPartId)
    }

    suspend fun connect(proxies: List<StreamrProxyAddress>): StreamrProxyResult =
        withContext(Dispatchers.IO) {
            client.connect(proxies)
        }

    suspend fun publish(content: String, ethereumPrivateKey: String?): StreamrProxyResult =
        withContext(Dispatchers.IO) {
            client.publish(content, ethereumPrivateKey)
        }

    suspend fun publish(content: ByteArray, ethereumPrivateKey: String?): StreamrProxyResult =
        withContext(Dispatchers.IO) {
            client.publish(content, ethereumPrivateKey)
        }

    override fun close() {
        client.close()
    }

}