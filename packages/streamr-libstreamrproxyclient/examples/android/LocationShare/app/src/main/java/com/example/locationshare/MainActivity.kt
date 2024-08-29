package com.example.locationshare


import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Looper
import androidx.core.app.ActivityCompat
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import com.example.locationshare.ui.LocationShareScreen
import com.example.locationshare.ui.LocationViewModel
import com.example.locationshare.ui.theme.LocationShareTheme
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationRequest
import com.google.android.gms.location.LocationServices
import android.Manifest;
import com.example.locationshare.ui.LocationShareViewModel

class MainActivity : ComponentActivity() {

    private val LOCATION_PERMISSION_REQUEST_CODE = 1
    private lateinit var locationClient: FusedLocationProviderClient

    private fun checkPermissions() {

        if (!hasLocationPermissions()) {
            requestLocationPermissions()
            return
        }
    }

    private fun requestLocationPermissions() {
        ActivityCompat.requestPermissions(
            this,
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION),
            LOCATION_PERMISSION_REQUEST_CODE
        )
    }

    private fun hasLocationPermissions(): Boolean {
        return hasPermission(Manifest.permission.ACCESS_FINE_LOCATION) &&
                hasPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
    }

    private fun hasPermission(permission: String): Boolean {
        val result = ActivityCompat.checkSelfPermission(this,permission);

        return result == PackageManager.PERMISSION_GRANTED;
    }

    @SuppressLint("MissingPermission")
    private fun initUpdates(viewModel: LocationViewModel) {
        locationClient = LocationServices.getFusedLocationProviderClient(this);
        locationClient.requestLocationUpdates(
            createLocationRequest(),
            {
                location -> viewModel.update(location.latitude, location.longitude)
            },
            Looper.getMainLooper()
        )
    }

    private fun createLocationRequest(): LocationRequest {
        return LocationRequest.Builder(1000).build()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        println("Start LocationShare")
        super.onCreate(savedInstanceState)
        checkPermissions()
        val locationViewModel = LocationViewModel()
        var locationShareViewModel = LocationShareViewModel(StreamrProxyClient(locationViewModel.state))
        initUpdates(locationViewModel)
        enableEdgeToEdge()
        setContent {
            LocationShareTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    LocationShareScreen(locationViewModel = locationViewModel, locationShareViewModel = locationShareViewModel)
                }
            }
        }
    }
}


