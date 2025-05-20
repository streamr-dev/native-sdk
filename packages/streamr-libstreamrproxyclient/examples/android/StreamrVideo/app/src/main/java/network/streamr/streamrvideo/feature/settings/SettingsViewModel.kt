package network.streamr.streamrvideo.feature.settings

import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import javax.inject.Inject
import network.streamr.streamrvideo.data.repository.SettingsRepository

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val settingsRepository: SettingsRepository
) : ViewModel() {
    val proxyId = settingsRepository.proxyId
    val proxyAddress = settingsRepository.proxyAddress
    val privateKey = settingsRepository.privateKey
    val localAddress = settingsRepository.localAddress
    val streamPartId = settingsRepository.streamPartId

    private val _proxyIdInput = MutableStateFlow(proxyId.value)
    val proxyIdInput: StateFlow<String> = _proxyIdInput.asStateFlow()

    private val _proxyAddressInput = MutableStateFlow(proxyAddress.value)
    val proxyAddressInput: StateFlow<String> = _proxyAddressInput.asStateFlow()

    private val _privateKeyInput = MutableStateFlow(privateKey.value)
    val privateKeyInput: StateFlow<String> = _privateKeyInput.asStateFlow()

    private val _localAddressInput = MutableStateFlow(localAddress.value)
    val localAddressInput: StateFlow<String> = _localAddressInput.asStateFlow()

    private val _streamPartIdInput = MutableStateFlow(streamPartId.value)
    val streamPartIdInput: StateFlow<String> = _streamPartIdInput.asStateFlow()

    fun updateProxyId(value: String) {
        _proxyIdInput.value = value
    }

    fun updateProxyAddress(value: String) {
        _proxyAddressInput.value = value
    }

    fun updatePrivateKey(value: String) {
        _privateKeyInput.value = value
    }

    fun updateLocalAddress(value: String) {
        _localAddressInput.value = value
    }

    fun updateStreamPartId(value: String) {
        _streamPartIdInput.value = value
    }

    fun saveSettings() {
        settingsRepository.updateSettings(
            proxyIdInput.value,
            proxyAddressInput.value,
            privateKeyInput.value,
            localAddressInput.value,
            streamPartIdInput.value
        )
    }
} 