package com.example.locationshare

import android.util.Log

object ProxyClientJNI {
    init {
        try {
            System.loadLibrary("locationshare")
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

    @Throws(StreamrProxyException::class)
    external fun proxyClientNew(ownEthereumAddress: String, streamPartId: String): Long

    @Throws(StreamrProxyException::class)
    external fun proxyClientDelete(clientHandle: Long)

    @Throws(StreamrProxyException::class)
    external fun proxyClientConnect(clientHandle: Long, proxies: Array<Proxy>): Long

    /*
    @Throws(StreamrProxyException::class)
    external fun proxyClientDisconnect(clientHandle: Long)
*/
    @Throws(StreamrProxyException::class)
    external fun proxyClientPublish(clientHandle: Long, content: String, ethereumPrivateKey: String)

    data class Proxy(
        val websocketUrl: String,
        val ethereumAddress: String
    )

    class StreamrProxyException(message: String) : Exception(message)

}