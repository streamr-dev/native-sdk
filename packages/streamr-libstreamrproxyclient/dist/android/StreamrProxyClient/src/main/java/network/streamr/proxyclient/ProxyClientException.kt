package network.streamr.proxyclient

class ProxyClientException(
    message: String,
    val code: String,
    val proxyUrl: String?,
    val proxyEthereumAddress: String?
) : Exception(message)