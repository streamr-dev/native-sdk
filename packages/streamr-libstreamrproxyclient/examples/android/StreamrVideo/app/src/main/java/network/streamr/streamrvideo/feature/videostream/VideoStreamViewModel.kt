package network.streamr.streamrvideo.feature.videostream

import android.app.Activity
import androidx.lifecycle.ViewModel
import com.serenegiant.usb.widget.CameraViewInterface
import dagger.hilt.android.lifecycle.HiltViewModel
import network.streamr.streamrvideo.data.camera.CameraManager
import network.streamr.streamrvideo.data.repository.VideoRepository
import network.streamr.streamrvideo.data.repository.VideoRepositoryImpl
import javax.inject.Inject

@HiltViewModel
class VideoStreamViewModel @Inject constructor(
    private val cameraManager: CameraManager,
    private val videoRepository: VideoRepository
) : ViewModel() {

    fun initializeCamera(activity: Activity) {
        cameraManager.initialize(activity)
    }

    fun startPreview(textureView: CameraViewInterface) {
        cameraManager.startPreview(textureView) { frame ->
            videoRepository.encodeFrame(frame)
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