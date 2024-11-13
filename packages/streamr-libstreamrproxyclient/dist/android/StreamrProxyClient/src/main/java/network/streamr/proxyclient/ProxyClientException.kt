package network.streamr.proxyclient

class ProxyClientException(
    val error: StreamrError
) : Exception(error.message)
/*
class ProxyClientException(
    message: String,
    val code: String,
    val proxyUrl: String?,
    val proxyEthereumAddress: String?
) : Exception(message)
*/
