package network.streamr.streamrvideo.feature.videostream

import android.app.Activity
import android.view.Surface
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.serenegiant.usb.widget.CameraViewInterface
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import network.streamr.streamrvideo.data.camera.CameraManager
import network.streamr.streamrvideo.data.repository.VideoRepository
import network.streamr.streamrvideo.data.repository.VideoRepositoryImpl
import javax.inject.Inject
import android.view.TextureView

@HiltViewModel
class VideoStreamViewModel @Inject constructor(
    private val cameraManager: CameraManager,
    private val videoRepository: VideoRepository
) : ViewModel() {

    val cameraState: StateFlow<CameraManager.CameraState> = cameraManager.cameraState

    fun initialize(activity: Activity) {
        cameraManager.initialize(activity)
    }

    fun startPreview(textureView: TextureView) {
        val surface = Surface(textureView.surfaceTexture)
        cameraManager.startPreview(surface) { frame ->
            viewModelScope.launch {
                videoRepository.encodeFrame(frame)
            }
        }
    }

    fun stopPreview() {
        cameraManager.stopPreview()
    }

    override fun onCleared() {
        super.onCleared()
        cameraManager.release()
        (videoRepository as? VideoRepositoryImpl)?.release()
    }
}

sealed class MainUiState {
    object Initial : MainUiState()
    // Add other states as needed
} 