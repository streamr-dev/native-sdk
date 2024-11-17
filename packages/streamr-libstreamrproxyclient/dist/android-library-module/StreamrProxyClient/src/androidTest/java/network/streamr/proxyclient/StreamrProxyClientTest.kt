package network.streamr.proxyclient

import androidx.test.ext.junit.runners.AndroidJUnit4

import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.*

import org.junit.After
//import com.example.streamrnative.ProxyClientJNI
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class StreamrProxyClientTest {

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

    private var client: StreamrProxyClient? = null

    @After
    fun tearDown() {
        client?.close()
    }

    private fun verifyFailed(
        result: StreamrProxyResult,
        expectedError: StreamrError
    ) {
        // Basic result validation
        assertEquals(0, result.numConnected)
        assertTrue(result.successful.isEmpty())
        assertEquals(1, result.failed.size)

        // Error validation
        val actualError = result.failed[0]
        assertEquals(expectedError::class, actualError.error::class)
    }

    @Throws(ProxyClientException::class)
    private fun createClientAndConnect(
        websocketUrl: String? = null,
        ethereumAddress: String? = null
    ): StreamrProxyResult {
        // Create client
        client = StreamrProxyClient(
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            streamPartId = VALID_STREAM_PART_ID
        )

        // Create and connect proxies
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
    private fun createClientConnectAndVerify(
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
    fun testInvalidEthereumAddress() {
        val exception = assertThrows(ProxyClientException::class.java) {
            StreamrProxyClient(
                ethereumAddress = INVALID_ETHEREUM_ADDRESS,
                streamPartId = VALID_STREAM_PART_ID
            )
        }
        assertEquals(StreamrError.InvalidEthereumAddress::class, exception.error::class)
    }

    @Test
    fun testInvalidStreamPartId() {
        val exception = assertThrows(ProxyClientException::class.java) {
            StreamrProxyClient(
                ethereumAddress = VALID_ETHEREUM_ADDRESS,
                streamPartId = INVALID_STREAM_PART_ID
            )
        }
        assertEquals(StreamrError.InvalidStreamPartId::class, exception.error::class)
    }

    @Test
    fun testInvalidProxyUrl() {
        createClientConnectAndVerify(
            websocketUrl = INVALID_PROXY_URL,
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            expectedError = StreamrError.InvalidProxyUrl()
        )
    }

    @Test
    @Throws(ProxyClientException::class)
    fun testNoProxiesDefined() {
        createClientConnectAndVerify(
            expectedError = StreamrError.NoProxiesDefined()
        )
    }

    @Test
    @Throws(ProxyClientException::class)
    fun testInvalidProxyEthereumAddress() {
        createClientConnectAndVerify(
            websocketUrl = VALID_PROXY_URL,
            ethereumAddress = INVALID_ETHEREUM_ADDRESS,
            expectedError = StreamrError.InvalidEthereumAddress()
        )
    }

    @Test
    @Throws(ProxyClientException::class)
    fun testProxyConnectionFailed() {
        createClientConnectAndVerify(
            websocketUrl = NON_EXISTENT_PROXY_URL_0,
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            expectedError = StreamrError.ProxyConnectionFailed()
        )
    }

    @Test
    @Throws(ProxyClientException::class)
    fun testThreeProxyConnectionsFailed() {
        val client = StreamrProxyClient(
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

        // Verify that errors match the original proxies
        assertEquals(proxies[0].websocketUrl + ":80", result.failed[0].proxy.websocketUrl)
        assertEquals(proxies[1].websocketUrl + ":80", result.failed[1].proxy.websocketUrl)
        assertEquals(proxies[2].websocketUrl + ":80", result.failed[2].proxy.websocketUrl)

        val actualError = result.failed[0]
        assertEquals(StreamrError.ProxyConnectionFailed()::class, actualError.error::class)
    }

    @Test
    @Throws(ProxyClientException::class)
    fun testConnectAndPublishSuccessfully() {
        val result = createClientAndConnect(
            websocketUrl = PROXY_WEBSOCKET_URL,
            ethereumAddress = PROXY_ETHEREUM_ADDRESS
        )

        // Verify connection results
        assertEquals(1, result.numConnected)
        assertFalse(result.successful.isEmpty())
        assertEquals(0, result.failed.size)

        val successfulProxy = result.successful.first()
        assertEquals(PROXY_WEBSOCKET_URL, successfulProxy.websocketUrl)
        assertEquals(PROXY_ETHEREUM_ADDRESS, successfulProxy.ethereumAddress)

        // Test publishing
        val testMessage = "test message"
        val publishResult = client!!.publish(
            content = testMessage,
            ethereumPrivateKey = ETHEREUM_PRIVATE_KEY
        )

        // Verify publish results
        assertTrue(publishResult.failed.isEmpty())
        assertTrue(publishResult.numConnected > 0)

    }

    @Test
    @Throws(ProxyClientException::class)
    fun testPublishWithoutConnection() {
        // Create client without connecting
        client = StreamrProxyClient(
            ethereumAddress = VALID_ETHEREUM_ADDRESS,
            streamPartId = VALID_STREAM_PART_ID
        )

        val testMessage = "test message"

        // Try to publish without any connections
        val publishResult = client!!.publish(
            content = testMessage,
            ethereumPrivateKey = ETHEREUM_PRIVATE_KEY
        )

        // Verify publish results
        assertEquals(0, publishResult.numConnected)
    }
}