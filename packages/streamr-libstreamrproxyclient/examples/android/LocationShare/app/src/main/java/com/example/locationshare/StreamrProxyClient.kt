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
    private val streamrProxyClientCoro = StreamrProxyClientCoroutines()
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.Default + job)
    private var publishingJob: Job? = null

    val proxyClientHandle = ProxyClientJNI.newClient()
    companion object {
        var defaultPublishingIntervalInSeconds = "5"
    }
    var status: Status by mutableStateOf(Status.stopped)
        private set

    var proxyAddress by mutableStateOf("")
        private set

    var proxyId by mutableStateOf("")
        private set

    var intervalSeconds by mutableStateOf(defaultPublishingIntervalInSeconds)
        private set

    fun updateProxyAddress(proxyAddress: String){
        this.proxyAddress = proxyAddress
    }

    fun updateProxyId(proxyId: String){
        this.proxyId = proxyId
    }

    fun updateIntervalSeconds(intervalSeconds: String){
        this.intervalSeconds = intervalSeconds
    }

    fun publish() {
        println("Publish start")
        status = Status.publishing
        val locationState = state.value
        var data = "${locationState.latitude}, ${locationState.longitude},"
        scope.launch {
            println("Publish: $data")
            streamrProxyClientCoro.publishAsync(proxyClientHandle, data)
            println("Publish End")
        }
    }

    fun setProxy() {
        val proxyIdArray = arrayOf(proxyId)
        val proxyAddressArray = arrayOf(proxyAddress)
        println("Set Proxy Id: ${proxyId}, address:  ${proxyAddress}")
        status = Status.setProxy
        scope.launch {
            streamrProxyClientCoro.setProxiesAsync(proxyClientHandle, proxyIdArray, proxyAddressArray)
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