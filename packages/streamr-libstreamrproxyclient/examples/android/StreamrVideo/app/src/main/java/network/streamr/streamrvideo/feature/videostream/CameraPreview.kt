package network.streamr.streamrvideo.feature.videostream

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.serenegiant.usb.widget.UVCCameraTextureView

@Composable
fun CameraPreview(
    modifier: Modifier = Modifier,
    onViewCreated: (UVCCameraTextureView) -> Unit
) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            UVCCameraTextureView(context).apply {
                onViewCreated(this)
            }
        }
    )
} 