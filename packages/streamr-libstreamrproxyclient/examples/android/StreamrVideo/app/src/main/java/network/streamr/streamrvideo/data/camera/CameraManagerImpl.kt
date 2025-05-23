package network.streamr.streamrvideo.data.camera

import android.app.Activity
import android.hardware.usb.UsbDevice
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.jiangdg.usbcamera.UVCCameraHelper
import com.serenegiant.usb.widget.CameraViewInterface
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import network.streamr.streamrvideo.data.repository.VideoRepository
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class CameraManagerImpl @Inject constructor(
    private val videoRepository: VideoRepository
) : CameraManager {
    private var mCameraHelper: UVCCameraHelper? = null
    private var isRequest = false
    private var activity: Activity? = null
    private var frameCallback: (suspend (ByteArray) -> Unit)? = null
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var isProcessingFrame = false
    
    private val _cameraState = MutableStateFlow<CameraManager.CameraState>(CameraManager.CameraState.Disconnected)
    override val cameraState: StateFlow<CameraManager.CameraState> = _cameraState.asStateFlow()

    companion object {
        private const val TAG = "CameraManagerImpl"
    }

    override fun initialize(activity: Activity) {
        this.activity = activity
        if (mCameraHelper == null) {
            mCameraHelper = UVCCameraHelper.getInstance().apply {
                setDefaultFrameFormat(UVCCameraHelper.FRAME_FORMAT_MJPEG)
                setDefaultPreviewSize(640, 480)
            }
        }
    }

    override fun startPreview(
        textureView: CameraViewInterface,
        onFrame: suspend (ByteArray) -> Unit
    ) {
        frameCallback = onFrame
        mCameraHelper?.apply {
            activity?.let { act ->
                initUSBMonitor(act, textureView, createUsbListener())
                registerUSB()
                setOnPreviewFrameListener { nv21Yuv ->
                   Log.d(TAG, "handleFrame")
                    handleFrame(nv21Yuv)
                 }
                _cameraState.value = CameraManager.CameraState.Preview
                Log.d(TAG, "Preview started")
            } ?: run {
                Log.e(TAG, "Activity not initialized")
                _cameraState.value = CameraManager.CameraState.Error("Activity not initialized")
            }
        }
    }

    override fun stopPreview() {
        if (_cameraState.value != CameraManager.CameraState.Preview) {
            Log.d(TAG, "Preview already stopped")
            return
        }
        mCameraHelper?.apply {
            unregisterUSB()
            closeCamera()
        }
        _cameraState.value = CameraManager.CameraState.Disconnected
        Log.d(TAG, "Preview stopped")
    }

    private fun createUsbListener() = object : UVCCameraHelper.OnMyDevConnectListener {
        override fun onAttachDev(device: UsbDevice) {
            Log.d(TAG, "USB device attached: ${device.deviceName}")
            if (!isRequest) {
                isRequest = true
                mCameraHelper?.requestPermission(0)
            }
        }

        override fun onDettachDev(device: UsbDevice) {
            Log.d(TAG, "USB device detached: ${device.deviceName}")
            if (isRequest) {
                isRequest = false
                mCameraHelper?.closeCamera()
                _cameraState.value = CameraManager.CameraState.Disconnected
            }
        }

        override fun onConnectDev(device: UsbDevice, isConnected: Boolean) {
            if (!isConnected) {
                Log.d(TAG, "Failed to connect to camera")
                _cameraState.value = CameraManager.CameraState.Error("Failed to connect to camera")
            } else {
                Log.d(TAG, "Camera connected")
                _cameraState.value = CameraManager.CameraState.Connected

                Handler(Looper.getMainLooper()).postDelayed({
                    if (mCameraHelper?.isCameraOpened == true) {
                        Log.d(TAG, "Camera opened successfully")
                        val width = mCameraHelper!!.previewWidth
                        val height = mCameraHelper!!.previewHeight
                        videoRepository.initializeEncoder(width, height)
                        Log.d(TAG, "Camera opened successfully")
                        _cameraState.value = CameraManager.CameraState.Preview
                    }
                    else {
                        Log.d(TAG, "Camera not opened yet")
                    }
                }, 1500)
            }
        }

        override fun onDisConnectDev(device: UsbDevice) {
            Log.d(TAG, "Camera disconnected")
            _cameraState.value = CameraManager.CameraState.Disconnected
        }
    }

    // TODO Remove isProcessingFrame because using one thread
    private fun handleFrame(frame: ByteArray) {
        if (isProcessingFrame) {
            Log.d(TAG, "Skipping frame - still processing previous")
            return
        }
        
        frameCallback?.let { callback ->
            isProcessingFrame = true
            scope.launch {
                try {
                    callback(frame)
                } finally {
                    isProcessingFrame = false
                }
            }
        }
    }

    override fun release() {
        Log.d(TAG, "Releasing camera resources")
        stopPreview()
        mCameraHelper?.release()
        mCameraHelper = null
        activity = null
        frameCallback = null
        _cameraState.value = CameraManager.CameraState.Disconnected
    }
} 