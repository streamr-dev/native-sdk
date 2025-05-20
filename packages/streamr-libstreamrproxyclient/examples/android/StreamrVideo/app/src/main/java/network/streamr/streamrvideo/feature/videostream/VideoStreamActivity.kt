package network.streamr.streamrvideo.feature.videostream

import network.streamr.streamrvideo.feature.videostream.VideoStreamScreen
import android.content.Context
import android.os.Bundle
import android.os.PowerManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.viewinterop.AndroidView
import com.serenegiant.usb.widget.CameraViewInterface
import com.serenegiant.usb.widget.UVCCameraTextureView
import network.streamr.streamrvideo.ui.theme.StreamrVideoTheme
import dagger.hilt.android.AndroidEntryPoint
import network.streamr.proxyclient.StreamrProxyClientCoro
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import network.streamr.streamrvideo.service.VideoStreamService
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import network.streamr.streamrvideo.feature.settings.ProxySettingsScreen

@AndroidEntryPoint
class VideoStreamActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        setContent {
            StreamrVideoTheme {
                VideoStreamScreen()
            }
        }
    }

}

