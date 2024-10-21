package com.example.locationshare

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.example.locationshare.ui.LocationState
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.StateFlow

enum class Status {
    stopped, setProxy, publishing
}

class StreamrProxyClient(val state: StateFlow<LocationState>) {
    //private val streamrProxyClientCoro = StreamrProxyClientCoroutines()
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.Default + job)
    private var publishingJob: Job? = null
    private val validEthereumAddress = "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5";
    private val validStreamPartId = "0xd7278f1e4a946fa7838b5d1e0fe50c5725fb23de/nativesdktest#01";
    val proxyClientHandle = ProxyClientJNI.proxyClientNew(validEthereumAddress, validStreamPartId)
    companion object {
        var defaultPublishingIntervalInSeconds = "5"
    }
    var status: Status by mutableStateOf(Status.stopped)
        private set

    var proxyAddress by mutableStateOf("0x2ee615edd5d13310f83d8125ff4f0960a7475e33")
        private set

    var proxyId by mutableStateOf("ws://10.0.2.2:44211")
        private set

    var ethereumPrivateKey by mutableStateOf("23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820")
        private set

    var intervalSeconds by mutableStateOf(defaultPublishingIntervalInSeconds)
        private set

    fun updateProxyAddress(proxyAddress: String){
        this.proxyAddress = proxyAddress
    }

    fun updateProxyId(proxyId: String){
        this.proxyId = proxyId
    }

    fun updateEthereumPrivateKey(ethereumPrivateKey: String){
        this.ethereumPrivateKey = ethereumPrivateKey
    }

    fun updateIntervalSeconds(intervalSeconds: String){
        this.intervalSeconds = intervalSeconds
    }

    fun publish() {
        println("Publish start")
        status = Status.publishing
        val locationState = state.value
        var data = "${locationState.latitude}, ${locationState.longitude}"
        scope.launch {
            withContext(Dispatchers.IO) {
                println("proxyClientHandle: ${proxyClientHandle}")
                println("Publish: $data")
                ProxyClientJNI.proxyClientPublish(proxyClientHandle, data, ethereumPrivateKey)
                println("Publish End")
            }
        }
    }

    fun setProxy() {
        val proxyList = arrayOf(ProxyClientJNI.Proxy(proxyId, proxyAddress))
        status = Status.setProxy
        scope.launch {
            withContext(Dispatchers.IO) {
                ProxyClientJNI.proxyClientConnect(proxyClientHandle, proxyList)
            }
        }
    }

    fun startPublishing() {
        println("startPublishing start")
        status = Status.setProxy
        setProxy()
        publishingJob = scope.launch {
            println("startPublishing launch")
            while (isActive) {
                println("startPublishing in loop")
                try {
                    publish()
                } catch (e: Exception) {
                    // Handle any exceptions that occur during the task
                    println("Error in periodic task: ${e.message}")
                }
                delay(intervalSeconds.toLong() * 1000) // Convert seconds to milliseconds
            }
        }
    }

    fun stopPublishing() {
        scope.launch {
            publishingJob?.cancel()
            publishingJob = null
            withContext(Dispatchers.Main) {
                status = Status.stopped
            }
        }

    }
}