package network.streamr.proxyclient

class StreamrProxyClient @Throws(ProxyClientException::class) constructor(
    ethereumAddress: String,
    streamPartId: String
) : AutoCloseable  {
    private val handle: Long

    init {
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
        return ProxyClientJNI.proxyClientPublish(handle, content, ethereumPrivateKey)
    }

    override fun close() {
        ProxyClientJNI.proxyClientDelete(handle)
    }
}