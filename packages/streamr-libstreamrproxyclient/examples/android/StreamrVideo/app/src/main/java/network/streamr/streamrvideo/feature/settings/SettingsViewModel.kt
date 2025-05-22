package network.streamr.streamrvideo.feature.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import javax.inject.Inject
import network.streamr.streamrvideo.data.repository.SettingsRepository
import kotlinx.coroutines.launch

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val settingsRepository: SettingsRepository
) : ViewModel() {
    // UI state for input fields
    private val _uiState = MutableStateFlow(SettingsUiState())
    val uiState: StateFlow<SettingsUiState> = _uiState.asStateFlow()

    init {
        // Load initial values from repository
        viewModelScope.launch {
            _uiState.value = SettingsUiState(
                proxyId = settingsRepository.proxyId.value,
                proxyAddress = settingsRepository.proxyAddress.value,
                privateKey = settingsRepository.privateKey.value,
                localAddress = settingsRepository.localAddress.value,
                streamPartId = settingsRepository.streamPartId.value
            )
        }
    }

    // Handle UI input changes
    fun onProxyIdChanged(value: String) {
        _uiState.value = _uiState.value.copy(proxyId = value)
    }

    fun onProxyAddressChanged(value: String) {
        _uiState.value = _uiState.value.copy(proxyAddress = value)
    }

    fun onPrivateKeyChanged(value: String) {
        _uiState.value = _uiState.value.copy(privateKey = value)
    }

    fun onLocalAddressChanged(value: String) {
        _uiState.value = _uiState.value.copy(localAddress = value)
    }

    fun onStreamPartIdChanged(value: String) {
        _uiState.value = _uiState.value.copy(streamPartId = value)
    }

    fun onSave() {
        viewModelScope.launch {
            settingsRepository.saveSettings(
                SettingsRepository.Settings(
                    proxyId = _uiState.value.proxyId,
                    proxyAddress = _uiState.value.proxyAddress,
                    privateKey = _uiState.value.privateKey,
                    localAddress = _uiState.value.localAddress,
                    streamPartId = _uiState.value.streamPartId
                )
            )
        }
    }

    fun onCancel() {
        // Reset UI state to current repository values
        viewModelScope.launch {
            _uiState.value = SettingsUiState(
                proxyId = settingsRepository.proxyId.value,
                proxyAddress = settingsRepository.proxyAddress.value,
                privateKey = settingsRepository.privateKey.value,
                localAddress = settingsRepository.localAddress.value,
                streamPartId = settingsRepository.streamPartId.value
            )
        }
    }

    data class SettingsUiState(
        val proxyId: String = "",
        val proxyAddress: String = "",
        val privateKey: String = "",
        val localAddress: String = "",
        val streamPartId: String = "",
        val isLoading: Boolean = false,
        val error: String? = null
    )
} 