package network.streamr.proxyclient

sealed class StreamrError(val message: String) {
    class InvalidEthereumAddress(message: String = "") : StreamrError("Invalid Ethereum address: $message")
    class InvalidStreamPartId(message: String = "") : StreamrError("Invalid stream part ID: $message")
    class InvalidProxyUrl(message: String = "") : StreamrError("Invalid proxy URL: $message")
    class NoProxiesDefined(message: String = "") : StreamrError("No proxies defined: $message")
    class ProxyConnectionFailed(message: String = "") : StreamrError("Proxy connection failed: $message")
    class UnknownError(message: String = "") : StreamrError("Unknown error: $message")
}