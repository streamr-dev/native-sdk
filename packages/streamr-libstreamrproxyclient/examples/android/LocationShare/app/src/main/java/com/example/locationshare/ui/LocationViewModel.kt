package com.example.locationshare.ui

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update

class LocationViewModel: ViewModel() {
    private val uiState = MutableStateFlow(LocationState())
    val state: StateFlow<LocationState> = uiState;

    fun update(latitude: Double, longitude: Double) {
        uiState.update { it.copy(latitude, longitude) }
    }


}