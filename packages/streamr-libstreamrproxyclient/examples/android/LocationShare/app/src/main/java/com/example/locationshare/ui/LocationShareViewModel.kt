package com.example.locationshare.ui


import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.example.locationshare.Status
import com.example.locationshare.StreamrProxyClient
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * ViewModel containing the app data and methods to process the data
 */
class LocationShareViewModel(val streamrProxyClient: StreamrProxyClient) : ViewModel() {

    // Game UI state
  //  private val _uiState = MutableStateFlow(ProxyState())
   // val streamrProxyClient = StreamrProxyClient()



/*
    fun updateIntervalSeconds(intervalSeconds: String) {
        val parsedInt = intervalSeconds.toIntOrNull()
        if (parsedInt != null) {
            streamrProxyClient.updateIntervalSeconds(parsedInt)
        } else {
            streamrProxyClient.updateIntervalSeconds(StreamrProxyClient.defaultPublishingIntervalInSeconds)
        }
    }

 */
  //  val uiState: StateFlow<ProxyState> = _uiState.asStateFlow()
    /*
    var proxyAddress by mutableStateOf("")
        private set

    var proxyId by mutableStateOf("")
        private set

    fun updateProxyAddress(proxyAddress: String){
        this.proxyAddress = proxyAddress
    }

    fun updateProxyId(proxyId: String){
        this.proxyId = proxyId
    }
*/
    fun buttonClicked() {
        when (streamrProxyClient.status) {
            Status.stopped -> streamrProxyClient.startPublishing()
            Status.setProxy, Status.publishing -> streamrProxyClient.stopPublishing()
         }
    }
}