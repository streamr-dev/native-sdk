package network.streamr.streamrvideo.data.camera

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.SurfaceTexture
import android.hardware.camera2.*
import android.media.Image
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import network.streamr.streamrvideo.data.repository.VideoRepository

@Singleton
class AndroidCameraManagerImpl @Inject constructor(
    private val context: Context,
    private val videoRepository: VideoRepository
) : CameraManager {
    private var cameraDevice: CameraDevice? = null
    private var captureSession: CameraCaptureSession? = null
    private var backgroundHandler: Handler? = null
    private var backgroundThread: HandlerThread? = null
    private var currentSurface: Surface? = null
    private var imageReader: ImageReader? = null
    private var frameCallback: (suspend (ByteArray) -> Unit)? = null
    
    private val _cameraState = MutableStateFlow<CameraManager.CameraState>(CameraManager.CameraState.Disconnected)
    override val cameraState: StateFlow<CameraManager.CameraState> = _cameraState.asStateFlow()
    
    private var previewWidth = 1280  // Default width
    private var previewHeight = 720  // Default height
    private var frameRate = 30       // Default FPS

    private val cameraManager: android.hardware.camera2.CameraManager by lazy {
        context.getSystemService(Context.CAMERA_SERVICE) as android.hardware.camera2.CameraManager
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    // Reuse buffer to avoid allocating new memory for each frame
    private var imageBuffer: ByteArray? = null

    override fun initialize(activity: Activity) {
        // Check and request camera permissions if needed
        if (!hasCameraPermission(activity)) {
            requestCameraPermission(activity)
            _cameraState.value = CameraManager.CameraState.Error("Camera permission not granted")
            return
        }

        try {
            // Verify camera is available
            val cameraId = cameraManager.cameraIdList.firstOrNull() ?: run {
                _cameraState.value = CameraManager.CameraState.Error("No camera available")
                return
            }

            // Get camera characteristics
            val characteristics = cameraManager.getCameraCharacteristics(cameraId)
            
            // Configure default preview size and frame rate based on camera capabilities
            val streamConfigMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
            streamConfigMap?.let { configMap ->
                val sizes = configMap.getOutputSizes(SurfaceTexture::class.java)
                sizes.firstOrNull { it.width >= 1280 && it.height >= 720 }?.let {
                    previewWidth = it.width
                    previewHeight = it.height
                }
            }

            // Initialize encoder with the selected dimensions
            videoRepository.initializeEncoder(previewWidth, previewHeight)

            _cameraState.value = CameraManager.CameraState.Connected
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize camera", e)
            _cameraState.value = CameraManager.CameraState.Error("Failed to initialize camera: ${e.message}")
        }
    }

    private fun hasCameraPermission(activity: Activity): Boolean {
        return activity.checkSelfPermission(android.Manifest.permission.CAMERA) == 
            android.content.pm.PackageManager.PERMISSION_GRANTED
    }

    private fun requestCameraPermission(activity: Activity) {
        activity.requestPermissions(
            arrayOf(android.Manifest.permission.CAMERA),
            CAMERA_PERMISSION_REQUEST_CODE
        )
    }

    @SuppressLint("MissingPermission")
    override fun startPreview(surface: Surface, onFrame: suspend (ByteArray) -> Unit) {
        frameCallback = onFrame
        currentSurface = surface
        startBackgroundThread()

        // Create ImageReader for getting frames
        imageReader = ImageReader.newInstance(
            previewWidth, previewHeight,
            ImageFormat.YUV_420_888, 2
        ).apply {
            setOnImageAvailableListener({ reader ->
                val image = reader.acquireLatestImage()
                try {
                    image?.let { processImage(it) }
                } finally {
                    image?.close()
                }
            }, backgroundHandler)
        }

        try {
            cameraManager.openCamera(CAMERA_ID, object : CameraDevice.StateCallback() {
                override fun onOpened(camera: CameraDevice) {
                    cameraDevice = camera
                    createCaptureSession(surface)
                }

                override fun onDisconnected(camera: CameraDevice) {
                    camera.close()
                    cameraDevice = null
                    _cameraState.value = CameraManager.CameraState.Disconnected
                }

                override fun onError(camera: CameraDevice, error: Int) {
                    camera.close()
                    cameraDevice = null
                    _cameraState.value = CameraManager.CameraState.Error("Camera error: $error")
                }
            }, backgroundHandler)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open camera", e)
            _cameraState.value = CameraManager.CameraState.Error("Failed to open camera: ${e.message}")
        }
    }

    private fun createCaptureSession(surface: Surface) {
        val previewRequest = cameraDevice?.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)?.apply {
            addTarget(surface)
            addTarget(imageReader!!.surface)
            set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
            set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, android.util.Range(frameRate, frameRate))
        }

        try {
            cameraDevice?.createCaptureSession(
                listOf(surface, imageReader!!.surface),
                object : CameraCaptureSession.StateCallback() {
                    override fun onConfigured(session: CameraCaptureSession) {
                        captureSession = session
                        previewRequest?.build()?.let { request ->
                            try {
                                session.setRepeatingRequest(request, null, backgroundHandler)
                                _cameraState.value = CameraManager.CameraState.Preview
                            } catch (e: Exception) {
                                Log.e(TAG, "Failed to start preview", e)
                                _cameraState.value = CameraManager.CameraState.Error("Failed to start preview: ${e.message}")
                            }
                        }
                    }

                    override fun onConfigureFailed(session: CameraCaptureSession) {
                        Log.e(TAG, "Failed to configure camera session")
                        _cameraState.value = CameraManager.CameraState.Error("Failed to configure camera session")
                    }
                },
                backgroundHandler
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create capture session", e)
            _cameraState.value = CameraManager.CameraState.Error("Failed to create capture session: ${e.message}")
        }
    }

    private fun processImage(image: Image) {
        val yBuffer = image.planes[0].buffer
        val uBuffer = image.planes[1].buffer
        val vBuffer = image.planes[2].buffer

        val ySize = yBuffer.remaining()
        val uSize = uBuffer.remaining()
        val vSize = vBuffer.remaining()
        
        val totalSize = ySize + uSize + vSize
        
        // Reuse or create buffer
        if (imageBuffer == null || imageBuffer!!.size < totalSize) {
            imageBuffer = ByteArray(totalSize)
        }

        // Use the reused buffer
        val nv21 = imageBuffer!!
        
        try {
            yBuffer.get(nv21, 0, ySize)
            vBuffer.get(nv21, ySize, vSize)
            uBuffer.get(nv21, ySize + vSize, uSize)

            frameCallback?.let { callback ->
                scope.launch {
                    callback(nv21)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error processing image", e)
        }
    }

    override fun stopPreview() {
        try {
            captureSession?.stopRepeating()
            captureSession?.close()
            captureSession = null
            currentSurface = null
            _cameraState.value = CameraManager.CameraState.Connected
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping preview", e)
            _cameraState.value = CameraManager.CameraState.Error("Error stopping preview: ${e.message}")
        }
    }

    override fun release() {
        stopPreview()
        cameraDevice?.close()
        cameraDevice = null
        stopBackgroundThread()
        imageBuffer = null  // Clear the buffer
        _cameraState.value = CameraManager.CameraState.Disconnected
    }

    override fun setPreviewSize(width: Int, height: Int) {
        previewWidth = width
        previewHeight = height
        // Re-create the preview if active
        frameCallback?.let { callback ->
            stopPreview()
            currentSurface?.let { surface ->
                // We need the original textureView here
                // This method might need to store the textureView reference
                // or be redesigned to handle size changes differently
                Log.w(TAG, "setPreviewSize not fully implemented for Camera2 API")
            }
        }
    }

    override fun setFrameRate(fps: Int) {
        frameRate = fps
        // Update frame rate if preview is active
        captureSession?.let { session ->
            try {
                currentSurface?.let { surface ->
                    val request = cameraDevice?.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)?.apply {
                        addTarget(surface)
                        set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, android.util.Range(fps, fps))
                    }
                    request?.build()?.let { session.setRepeatingRequest(it, null, backgroundHandler) }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to update frame rate", e)
            }
        }
    }

    private fun startBackgroundThread() {
        backgroundThread = HandlerThread("CameraBackground").also { it.start() }
        backgroundHandler = backgroundThread?.looper?.let { Handler(it) }
    }

    private fun stopBackgroundThread() {
        backgroundThread?.quitSafely()
        try {
            backgroundThread?.join()
            backgroundThread = null
            backgroundHandler = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Error stopping background thread", e)
        }
    }

    companion object {
        private const val TAG = "AndroidCameraManager"
        private const val CAMERA_ID = "0"  // Usually the back camera
        private const val CAMERA_PERMISSION_REQUEST_CODE = 100
    }
} 