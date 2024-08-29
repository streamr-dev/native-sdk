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

    fun buttonClicked() {
        when (streamrProxyClient.status) {
            Status.stopped -> streamrProxyClient.startPublishing()
            Status.setProxy, Status.publishing -> streamrProxyClient.stopPublishing()
         }
    }
}