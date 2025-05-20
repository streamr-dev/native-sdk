package network.streamr.streamrvideo.feature.settings

import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import network.streamr.proxyclient.StreamrProxyClientCoro
import javax.inject.Inject

data class ProxySettings(
    val ethereumAddress: String = "",
    val streamPartId: String = "",
    val isConfigured: Boolean = false
)

@HiltViewModel
class ProxySettingsViewModel @Inject constructor(
    private val proxyClient: StreamrProxyClientCoro
) : ViewModel() {
    private val _settings = MutableStateFlow(ProxySettings())
    val settings = _settings.asStateFlow()

    fun updateSettings(ethereumAddress: String, streamPartId: String) {
        _settings.update { currentSettings ->
            currentSettings.copy(
                ethereumAddress = ethereumAddress,
                streamPartId = streamPartId,
                isConfigured = true
            )
        }
        proxyClient.initialize(ethereumAddress, streamPartId)
    }
} 