package network.streamr.proxyclient

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.*
import org.junit.After

@RunWith(AndroidJUnit4::class)
class StreamrProxyClientCoroTest {

    companion object {
        private const val PROXY_WEBSOCKET_URL = "ws://95.216.15.80:44211"
        private const val PROXY_ETHEREUM_ADDRESS = "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
        private const val VALID_ETHEREUM_ADDRESS = "0x1234567890123456789012345678901234567890"
        private const val VALID_STREAM_PART_ID =
            "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
        private const val INVALID_PROXY_URL = "invalid_url"
        private const val VALID_PROXY_URL = "ws://valid.proxy.url"
        private const val INVALID_ETHEREUM_ADDRESS = "invalid_address"
        private const val INVALID_STREAM_PART_ID = "invalid_stream_id"
        private const val NON_EXISTENT_PROXY_URL_0 = "ws://non.existent.proxy0.url"
        private const val VALID_ETHEREUM_ADDRESS2 = "0x2234567890123456789012345678901234567890"
        private const val VALID_ETHEREUM_ADDRESS3 = "0x3234567890123456789012345678901234567890"
        private const val NON_EXISTENT_PROXY_URL_1 = "ws://non.existent.proxy1.url"
        private const val NON_EXISTENT_PROXY_URL_2 = "ws://non.existent.proxy2.url"
        private const val ETHEREUM_PRIVATE_KEY =
            "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
    }

    private var client: StreamrProxyClientCoro? = null

    @After
    fun tearDown() {
        client?.close()
    }

    private fun verifyFailed(
        result: StreamrProxyResult,
        expectedError: StreamrError
    ) {
        assertEquals(0, result.numConnected)
        assertTrue(result.successful.isEmpty())
        assertEquals(1, result.failed.size)

        val actualError = result.failed[0]
        assertEquals(expectedError::class, actualError.error::class)
    }

    @Throws(ProxyClientException::class)
    private suspend fun createClientAndConnect(
        websocketUrl: String? = null,
        ethereumAddress: String? = null
    ): StreamrProxyResult {
        client = StreamrProxyClientCoro(
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            streamPartId = VALID_STREAM_PART_ID
        )

        val proxies = websocketUrl?.let {
            listOf(
                StreamrProxyAddress(
                    websocketUrl = it,
                    ethereumAddress = ethereumAddress!!
                )
            )
        } ?: emptyList()

        return client!!.connect(proxies)
    }

    @Throws(ProxyClientException::class)
    private suspend fun createClientConnectAndVerify(
        websocketUrl: String? = null,
        ethereumAddress: String? = null,
        expectedError: StreamrError
    ) {
        val result = createClientAndConnect(
            websocketUrl = websocketUrl,
            ethereumAddress = ethereumAddress
        )

        verifyFailed(
            result = result,
            expectedError = expectedError
        )
    }

    @Test
    fun testInvalidEthereumAddress() = runBlocking {
        val exception = assertThrows(ProxyClientException::class.java) {
            StreamrProxyClientCoro(
                ethereumAddress = INVALID_ETHEREUM_ADDRESS,
                streamPartId = VALID_STREAM_PART_ID
            )
        }
        assertEquals(StreamrError.InvalidEthereumAddress::class, exception.error::class)
    }

    @Test
    fun testInvalidStreamPartId() = runBlocking {
        val exception = assertThrows(ProxyClientException::class.java) {
            StreamrProxyClientCoro(
                ethereumAddress = VALID_ETHEREUM_ADDRESS,
                streamPartId = INVALID_STREAM_PART_ID
            )
        }
        assertEquals(StreamrError.InvalidStreamPartId::class, exception.error::class)
    }

    @Test
    fun testInvalidProxyUrl() = runBlocking {
        createClientConnectAndVerify(
            websocketUrl = INVALID_PROXY_URL,
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            expectedError = StreamrError.InvalidProxyUrl()
        )
    }

    @Test
    fun testNoProxiesDefined() = runBlocking {
        createClientConnectAndVerify(
            expectedError = StreamrError.NoProxiesDefined()
        )
    }

    @Test
    fun testInvalidProxyEthereumAddress() = runBlocking {
        createClientConnectAndVerify(
            websocketUrl = VALID_PROXY_URL,
            ethereumAddress = INVALID_ETHEREUM_ADDRESS,
            expectedError = StreamrError.InvalidEthereumAddress()
        )
    }

    @Test
    fun testProxyConnectionFailed() = runBlocking {
        createClientConnectAndVerify(
            websocketUrl = NON_EXISTENT_PROXY_URL_0,
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            expectedError = StreamrError.ProxyConnectionFailed()
        )
    }

    @Test
    fun testThreeProxyConnectionsFailed() = runBlocking {
        val client = StreamrProxyClientCoro(
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            streamPartId = VALID_STREAM_PART_ID
        )

        val proxies = listOf(
            StreamrProxyAddress(
                websocketUrl = NON_EXISTENT_PROXY_URL_0,
                ethereumAddress = VALID_ETHEREUM_ADDRESS
            ),
            StreamrProxyAddress(
                websocketUrl = NON_EXISTENT_PROXY_URL_1,
                ethereumAddress = VALID_ETHEREUM_ADDRESS2
            ),
            StreamrProxyAddress(
                websocketUrl = NON_EXISTENT_PROXY_URL_2,
                ethereumAddress = VALID_ETHEREUM_ADDRESS3
            )
        )

        val result = client.connect(proxies)

        assertEquals(0, result.numConnected)
        assertTrue(result.successful.isEmpty())
        assertEquals(3, result.failed.size)

        assertEquals(proxies[0].websocketUrl + ":80", result.failed[0].proxy.websocketUrl)
        assertEquals(proxies[1].websocketUrl + ":80", result.failed[1].proxy.websocketUrl)
        assertEquals(proxies[2].websocketUrl + ":80", result.failed[2].proxy.websocketUrl)

        val actualError = result.failed[0]
        assertEquals(StreamrError.ProxyConnectionFailed()::class, actualError.error::class)
    }

    @Test
    fun testConnectAndPublishSuccessfully() = runBlocking {
        val result = createClientAndConnect(
            websocketUrl = PROXY_WEBSOCKET_URL,
            ethereumAddress = PROXY_ETHEREUM_ADDRESS
        )

        assertEquals(1, result.numConnected)
        assertFalse(result.successful.isEmpty())
        assertEquals(0, result.failed.size)

        val successfulProxy = result.successful.first()
        assertEquals(PROXY_WEBSOCKET_URL, successfulProxy.websocketUrl)
        assertEquals(PROXY_ETHEREUM_ADDRESS, successfulProxy.ethereumAddress)

        val testMessage = "test message"
        val publishResult = client!!.publish(
            content = testMessage,
            ethereumPrivateKey = ETHEREUM_PRIVATE_KEY
        )

        assertTrue(publishResult.failed.isEmpty())
        assertTrue(publishResult.numConnected > 0)
    }

    @Test
    fun testPublishWithoutConnection() = runBlocking {
        client = StreamrProxyClientCoro(
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            streamPartId = VALID_STREAM_PART_ID
        )

        val testMessage = "test message"
        val publishResult = client!!.publish(
            content = testMessage,
            ethereumPrivateKey = ETHEREUM_PRIVATE_KEY
        )

        assertEquals(0, publishResult.numConnected)
    }
}