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
import android.os.Handler
import android.util.Log
import androidx.activity.viewModels
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.example.locationshare.ui.LocationShareViewModel
import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.android.gms.location.LocationCallback
import com.google.android.gms.location.LocationResult
import com.google.android.gms.location.Priority
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    private val LOCATION_PERMISSION_REQUEST_CODE = 1
    private val scope = CoroutineScope(Dispatchers.IO + Job())
    private lateinit var locationClient: FusedLocationProviderClient
    private val locationViewModel: LocationViewModel by viewModels()
    private val locationShareViewModel: LocationShareViewModel by viewModels {
        LocationShareViewModelFactory(ProxyClient(locationViewModel.state))
    }

    @SuppressLint("MissingPermission")
    private fun initUpdates(viewModel: LocationViewModel) {
        scope.launch {
            try {
                withContext(Dispatchers.Main) {
                    locationClient = LocationServices.getFusedLocationProviderClient(this@MainActivity)
                }
                val request = createLocationRequest()
                val locationCallback = object : LocationCallback() {
                    override fun onLocationResult(result: LocationResult) {
                        scope.launch {
                            result.lastLocation?.let { location ->
                                withContext(Dispatchers.Main) {
                                    viewModel.update(location.latitude, location.longitude)
                                }
                            }
                        }
                    }
                }

                withContext(Dispatchers.Main) {
                    locationClient.requestLocationUpdates(
                        request,
                        locationCallback,
                        Looper.getMainLooper()
                    )
                }
            } catch (e: Exception) {
                Log.e("LocationShare", "Error initializing location updates", e)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel() // Clean up coroutines
    }

    private fun createLocationRequest(): LocationRequest {
        return LocationRequest.Builder(1000)
            .setPriority(Priority.PRIORITY_HIGH_ACCURACY)
            .setWaitForAccurateLocation(false)
            .setMinUpdateIntervalMillis(500)
            .setMaxUpdateDelayMillis(1000)
            .build()
    }

    private fun checkPermissions() {
        if (!hasLocationPermissions()) {
            requestLocationPermissions()
            return
        }
    }

    private fun requestLocationPermissions() {
        ActivityCompat.requestPermissions(
            this,
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            ),
            LOCATION_PERMISSION_REQUEST_CODE
        )
    }

    private fun hasPermission(permission: String): Boolean {
        return ActivityCompat.checkSelfPermission(
            this,
            permission
        ) == PackageManager.PERMISSION_GRANTED
    }

    private fun hasLocationPermissions(): Boolean {
        return hasPermission(Manifest.permission.ACCESS_FINE_LOCATION) &&
                hasPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
    }

    // Add this to handle permission results
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            LOCATION_PERMISSION_REQUEST_CODE -> {
                if (grantResults.isNotEmpty() &&
                    grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // Permission granted, initialize location updates
                    initUpdates(locationViewModel)
                } else {
                    // Permission denied, handle accordingly
                    Log.w("LocationShare", "Location permission denied")
                    // You might want to show a message to the user here
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        try {
            enableEdgeToEdge()

            // Check Google Play Services availability
            val availability = GoogleApiAvailability.getInstance()
            val resultCode = availability.isGooglePlayServicesAvailable(this)

            if (resultCode != ConnectionResult.SUCCESS) {
                if (availability.isUserResolvableError(resultCode)) {
                    availability.getErrorDialog(this, resultCode, 1)?.show()
                }
                return
            }

            checkPermissions()
            if (hasLocationPermissions()) {
                initUpdates(locationViewModel)
            }

            setContent {
                LocationShareTheme {
                    Surface(
                        modifier = Modifier.fillMaxSize(),
                        color = MaterialTheme.colorScheme.background
                    ) {
                        LocationShareScreen(
                            locationViewModel = locationViewModel,
                            locationShareViewModel = locationShareViewModel
                        )
                    }
                }
            }
        } catch (e: Exception) {
            Log.e("LocationShare", "Error in onCreate", e)
        }
    }
}

class LocationShareViewModelFactory(private val proxyClient: ProxyClient) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(LocationShareViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return LocationShareViewModel(proxyClient) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}
