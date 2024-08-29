package com.example.locationshare

import android.util.Log

object ProxyClientJNI {
    init {
        try {
            System.loadLibrary("locationshare")
        } catch (e: UnsatisfiedLinkError) {
            // Log the error
            Log.e("ProxyClientJNI", "Failed to load native library: locationshare", e)
            // You might want to throw a custom exception or handle the error in some way
            throw RuntimeException("Unable to load locationshare library. Check if the library is properly included in the project.", e)
        } catch (e: SecurityException) {
            // This can occur if there's a security manager preventing the library from being loaded
            Log.e("ProxyClientJNI", "Security exception when loading native library: locationshare", e)
            throw RuntimeException("Security exception when loading locationshare library.", e)
        }

    }

    data class Result(var code: Int = 0, var message: String = "")

    external fun newClient(): Long
    external fun deleteClient(handle: Long): Result
    external fun publish(handle: Long, data: String): Result
    external fun setProxies(handle: Long, peerIds: Array<String>, peerAddresses: Array<String>): Result


}