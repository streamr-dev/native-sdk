package com.example.locationshare

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlin.coroutines.CoroutineContext

class StreamrProxyClientCoroutines(private val coroutineContext: CoroutineContext = Dispatchers.Default) {

    suspend fun newClientAsync(): Long = withContext(coroutineContext) {
        ProxyClientJNI.newClient()
    }

    suspend fun deleteClientAsync(handle: Long): ProxyClientJNI.Result = withContext(coroutineContext) {
        ProxyClientJNI.deleteClient(handle)
    }

    suspend fun publishAsync(handle: Long, data: String): ProxyClientJNI.Result = withContext(coroutineContext) {
        ProxyClientJNI.publish(handle, data)
    }

    suspend fun setProxiesAsync(handle: Long, peerIds: Array<String>, peerAddresses: Array<String>): ProxyClientJNI.Result = withContext(coroutineContext) {
        ProxyClientJNI.setProxies(handle, peerIds, peerAddresses)
    }
}

