package network.streamr.proxyclient

data class StreamrProxyAddress(
    val websocketUrl: String,
    val ethereumAddress: String
)

data class StreamrProxyError(
    val error: StreamrError,
    val proxy: StreamrProxyAddress
)

data class StreamrProxyResult(
    val numConnected: Long,
    val successful: List<StreamrProxyAddress>,
    val failed: List<StreamrProxyError>
)