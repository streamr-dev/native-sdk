package com.example.locationshare

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlin.coroutines.CoroutineContext

class StreamrProxyClientCoroutines(private val coroutineContext: CoroutineContext = Dispatchers.Default) {


    suspend fun newClient(ownEthereumAddress: String, streamPartId: String): Long =
        withContext(Dispatchers.Default) {
            try {
                ProxyClientJNI.proxyClientNew(ownEthereumAddress, streamPartId)
            } catch (e: ProxyClientJNI.StreamrProxyException) {
                throw StreamrProxyClientException("Failed to create new client: ${e.message}")
            }
        }

    suspend fun deleteClient(clientHandle: Long) = withContext(Dispatchers.Default) {
        try {
            ProxyClientJNI.proxyClientDelete(clientHandle)
        } catch (e: ProxyClientJNI.StreamrProxyException) {
            throw StreamrProxyClientException("Failed to delete client: ${e.message}")
        }
    }

    suspend fun connect(clientHandle: Long, proxies: List<ProxyClientJNI.Proxy>): Long =
        withContext(Dispatchers.Default) {
            try {
                ProxyClientJNI.proxyClientConnect(clientHandle, proxies.toTypedArray())
            } catch (e: ProxyClientJNI.StreamrProxyException) {
                throw StreamrProxyClientException("Failed to connect: ${e.message}")
            }
        }

    /*
    suspend fun disconnect(clientHandle: Long) = withContext(Dispatchers.Default) {
        try {
            ProxyClientJNI.proxyClientDisconnect(clientHandle)
        } catch (e: ProxyClientJNI.StreamrProxyException) {
            throw StreamrProxyClientException("Failed to disconnect: ${e.message}")
        }
    }
*/
    suspend fun publish(clientHandle: Long, content: String) = withContext(Dispatchers.Default) {
        try {
            ProxyClientJNI.proxyClientPublish(clientHandle, content)
        } catch (e: ProxyClientJNI.StreamrProxyException) {
            throw StreamrProxyClientException("Failed to publish: ${e.message}")
        }
    }

    class StreamrProxyClientException(message: String) : Exception(message)

}

