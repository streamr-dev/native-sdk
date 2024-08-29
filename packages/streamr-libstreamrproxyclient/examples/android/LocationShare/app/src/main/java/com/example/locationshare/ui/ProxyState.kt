package com.example.locationshare.ui

/**
 * Data class that represents the game UI state
 */
data class ProxyState(
    val status: Int = 0,
    var proxyAddress: String = "",
    var proxyId: String = ""
)
