package network.streamr.streamrvideo.data.repository

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class SettingsRepository @Inject constructor() {
    private val _proxyId = MutableStateFlow("ws://95.216.15.80:44211")
    val proxyId: StateFlow<String> = _proxyId.asStateFlow()

    private val _proxyAddress = MutableStateFlow("0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f")
    val proxyAddress: StateFlow<String> = _proxyAddress.asStateFlow()

    private val _privateKey = MutableStateFlow("23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820")
    val privateKey: StateFlow<String> = _privateKey.asStateFlow()

    private val _localAddress = MutableStateFlow("0x1234567890123456789012345678901234567890")
    val localAddress: StateFlow<String> = _localAddress.asStateFlow()

    private val _streamPartId = MutableStateFlow("0xd7278f1e4a946fa7838b5d1e0fe50c5725fb23de/nativesdktest#01")
    val streamPartId: StateFlow<String> = _streamPartId.asStateFlow()

    data class Settings(
        val proxyId: String,
        val proxyAddress: String,
        val privateKey: String,
        val localAddress: String,
        val streamPartId: String
    )

    fun saveSettings(settings: Settings) {
        _proxyId.value = settings.proxyId
        _proxyAddress.value = settings.proxyAddress
        _privateKey.value = settings.privateKey
        _localAddress.value = settings.localAddress
        _streamPartId.value = settings.streamPartId
    }
} 