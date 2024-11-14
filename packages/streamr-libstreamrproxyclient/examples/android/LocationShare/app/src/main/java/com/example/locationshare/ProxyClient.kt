package com.example.locationshare

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.example.locationshare.ui.LocationState
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.StateFlow
import network.streamr.proxyclient.ProxyClientJNI
import network.streamr.proxyclient.StreamrProxyAddress

enum class Status {
    STOPPED, SET_PROXY, PUBLISHING
}

class StreamrProxyClient(private val state: StateFlow<LocationState>) {
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.Default + job)
    private var publishingJob: Job? = null
    private val validEthereumAddress = "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5"
    private val validStreamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    private val proxyClientHandle: Long

    init {
        proxyClientHandle = ProxyClientJNI.proxyClientNew(validEthereumAddress, validStreamPartId)
    }

    companion object {
        var defaultPublishingIntervalInSeconds = "5"
    }

    var status: Status by mutableStateOf(Status.STOPPED)
        private set

    var proxyAddress by mutableStateOf("0x2ee615edd5d13310f83d8125ff4f0960a7475e33")
        private set

    var proxyId by mutableStateOf("ws://10.0.2.2:44211")
        private set

    var ethereumPrivateKey by mutableStateOf("23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820")
        private set

    var intervalSeconds by mutableStateOf(defaultPublishingIntervalInSeconds)
        private set

    fun updateProxyAddress(proxyAddress: String) {
        this.proxyAddress = proxyAddress
    }

    fun updateProxyId(proxyId: String) {
        this.proxyId = proxyId
    }

    fun updateEthereumPrivateKey(ethereumPrivateKey: String) {
        this.ethereumPrivateKey = ethereumPrivateKey
    }

    fun updateIntervalSeconds(intervalSeconds: String) {
        this.intervalSeconds = intervalSeconds
    }

    private fun publish() {
        println("Publish start")
        status = Status.PUBLISHING
        val locationState = state.value
        val data = "${locationState.latitude}, ${locationState.longitude}"
        
        scope.launch {
            withContext(Dispatchers.IO) {
                try {
                    println("proxyClientHandle: $proxyClientHandle")
                    println("Publish: $data")
                    val result = ProxyClientJNI.proxyClientPublish(
                        proxyClientHandle,
                        data,
                        ethereumPrivateKey
                    )
                    println("Publish result: numConnected=${result.numConnected}")
                    println("Publish End")
                } catch (e: Exception) {
                    println("Error publishing: ${e.message}")
                }
            }
        }
    }

    private fun setProxy() {
        status = Status.SET_PROXY
        scope.launch {
            withContext(Dispatchers.IO) {
                try {
                    val proxy = StreamrProxyAddress(proxyId, proxyAddress)
                    val result = ProxyClientJNI.proxyClientConnect(
                        proxyClientHandle,
                        listOf(proxy)
                    )
                    println("Connect result: numConnected=${result.numConnected}")
                } catch (e: Exception) {
                    println("Error connecting: ${e.message}")
                }
            }
        }
    }

    fun startPublishing() {
        println("startPublishing start")
        status = Status.SET_PROXY
        setProxy()
        
        publishingJob = scope.launch {
            println("startPublishing launch")
            while (isActive) {
                println("startPublishing in loop")
                try {
                    publish()
                } catch (e: Exception) {
                    println("Error in periodic task: ${e.message}")
                }
                delay(intervalSeconds.toLong() * 1000)
            }
        }
    }

    fun stopPublishing() {
        scope.launch {
            publishingJob?.cancel()
            publishingJob = null
            withContext(Dispatchers.Main) {
                status = Status.STOPPED
            }
        }
    }

    protected fun finalize() {
        if (proxyClientHandle != 0L) {
            ProxyClientJNI.proxyClientDelete(proxyClientHandle)
        }
    }
}