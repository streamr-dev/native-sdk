package com.example.locationshare.ui


import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.example.locationshare.Status
import com.example.locationshare.ProxyClient
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * ViewModel containing the app data and methods to process the data
 */
class LocationShareViewModel(val proxyClient: ProxyClient) : ViewModel() {

    fun buttonClicked() {
        when (proxyClient.status) {
            Status.stopped -> proxyClient.startPublishing()
            Status.setProxy, Status.publishing -> proxyClient.stopPublishing()
         }
    }
}