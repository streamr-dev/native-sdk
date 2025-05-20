package network.streamr.streamrvideo.data.camera

import android.app.Activity
import android.hardware.usb.UsbDevice
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.jiangdg.usbcamera.UVCCameraHelper
import com.serenegiant.usb.widget.CameraViewInterface
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import network.streamr.streamrvideo.data.repository.VideoRepository
import javax.inject.Inject
import javax.inject.Singleton

interface CameraManager {
    val cameraState: StateFlow<CameraState>
    
    fun initialize(activity: Activity)
    fun startPreview(textureView: CameraViewInterface, onFrame: suspend (ByteArray) -> Unit)
    fun stopPreview()
    fun release()

    sealed class CameraState {
        object Disconnected : CameraState()
        object Connected : CameraState()
        object Preview : CameraState()
        data class Error(val message: String) : CameraState()
    }
}
