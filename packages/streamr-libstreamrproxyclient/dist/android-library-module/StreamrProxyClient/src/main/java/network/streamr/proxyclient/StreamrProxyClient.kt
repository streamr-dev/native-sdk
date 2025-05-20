package network.streamr.proxyclient

class StreamrProxyClient() : AutoCloseable {
    private var handle: Long = -1L

    @Throws(ProxyClientException::class)
    constructor(
        ethereumAddress: String,
        streamPartId: String
    ) : this() {
        initialize(ethereumAddress, streamPartId)
    }

    @Throws(ProxyClientException::class)  // Moved to method level
    fun initialize(ethereumAddress: String, streamPartId: String) {
        handle = ProxyClientJNI.proxyClientNew(ethereumAddress, streamPartId)
    }

    fun connect(proxies: List<StreamrProxyAddress>): StreamrProxyResult {
        if (proxies.isEmpty()) {
            return StreamrProxyResult(
                numConnected = 0,
                successful = emptyList(),
                failed = listOf(
                    StreamrProxyError(
                        error = StreamrError.NoProxiesDefined(),
                        proxy = StreamrProxyAddress("", "")
                    )
                )
            )
        }

        // Convert to native proxies and make the call
        val result = ProxyClientJNI.proxyClientConnect(handle, proxies)
        return result
    }

    fun publish(content: String, ethereumPrivateKey: String?): StreamrProxyResult {
        return ProxyClientJNI.proxyClientPublishString(handle, content, ethereumPrivateKey)
    }

    fun publish(content: ByteArray, ethereumPrivateKey: String?): StreamrProxyResult {
        return ProxyClientJNI.proxyClientPublishBytes(handle, content, ethereumPrivateKey)
    }

    override fun close() {
        ProxyClientJNI.proxyClientDelete(handle)
    }
}