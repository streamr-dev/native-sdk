package network.streamr.proxyclient

import android.util.Log

object ProxyClientJNI {

    init {
        try {
            System.loadLibrary("ProxyClient")
            System.loadLibrary("streamrproxyclient")
        } catch (e: UnsatisfiedLinkError) {
            // Log the error
            Log.e("ProxyClientJNI", "Failed to load native library: locationshare", e)
            // You might want to throw a custom exception or handle the error in some way
            throw RuntimeException("Unable to load locationshare library. Check if the library is properly included in the project.", e)
        } catch (e: SecurityException) {
            // This can occur if there's a security manager preventing the library from being loaded
            Log.e("ProxyClientJNI", "Security exception when loading native library: locationshare", e)
            throw RuntimeException("Security exception when loading locationshare library.", e)
        }

    }

    @Throws(ProxyClientException::class)
    external fun proxyClientNew(ownEthereumAddress: String, streamPartId: String): Long

    external fun proxyClientDelete(handle: Long)

    external fun proxyClientConnect(
        handle: Long,
        proxies: List<StreamrProxyAddress>
    ): StreamrProxyResult

    external fun proxyClientPublish(
        handle: Long,
        content: String,
        ethereumPrivateKey: String?
    ): StreamrProxyResult

}