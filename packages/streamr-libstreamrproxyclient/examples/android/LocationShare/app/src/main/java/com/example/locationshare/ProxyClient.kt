package com.example.locationshare

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.example.locationshare.ui.LocationState
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.StateFlow
import network.streamr.proxyclient.StreamrProxyClientCoro
import network.streamr.proxyclient.StreamrProxyAddress

enum class Status {
    stopped, setProxy, publishing
}

class ProxyClient(private val state: StateFlow<LocationState>) {
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.Default + job)
    private var publishingJob: Job? = null
    private val validEthereumAddress = "0x1234567890123456789012345678901234567890"
    private val validStreamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    private val proxyClient: StreamrProxyClientCoro

    init {
        proxyClient = StreamrProxyClientCoro(validEthereumAddress, validStreamPartId)
    }

    companion object {
        var defaultPublishingIntervalInSeconds = "5"
    }

    var status: Status by mutableStateOf(Status.stopped)
        private set

    var proxyAddress by mutableStateOf("0xce119099e2839e522b9657e40d331fe8d322375c")
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

    private suspend fun publish() {
        println("Publish start")
        withContext(Dispatchers.Main) {
            status = Status.publishing
        }
        val locationState = state.value
        val data = "${locationState.latitude}, ${locationState.longitude}"
        val result = proxyClient.publish(data, ethereumPrivateKey)
        println("Publish result: numConnected=${result.numConnected}")
        println("Publish End")
    }

    fun startPublishing() {
        println("startPublishing start")

        publishingJob = scope.launch {
            println("startPublishing launch")
            withContext(Dispatchers.Main) {
                status = Status.setProxy
            }
            proxyClient.connect(listOf(StreamrProxyAddress(proxyId, proxyAddress)))
            while (isActive) {
                println("startPublishing in loop")
                publish()
                delay(intervalSeconds.toLong() * 1000)
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