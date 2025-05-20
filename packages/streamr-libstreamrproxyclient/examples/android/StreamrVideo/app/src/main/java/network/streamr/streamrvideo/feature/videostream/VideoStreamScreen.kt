package network.streamr.streamrvideo.feature.videostream

import android.app.Activity
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.hilt.navigation.compose.hiltViewModel
import com.serenegiant.usb.widget.CameraViewInterface
import androidx.activity.ComponentActivity
import androidx.compose.material.icons.filled.Close
import network.streamr.streamrvideo.feature.settings.SettingsDialog
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import network.streamr.streamrvideo.service.VideoStreamService

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun VideoStreamScreen(
    viewModel: VideoStreamViewModel = hiltViewModel()
) {
    var showSettings by remember { mutableStateOf(false) }
    var isStreaming by remember { mutableStateOf(false) }
    val context = LocalContext.current as ComponentActivity

    if (showSettings) {
        SettingsDialog(
            onDismiss = { showSettings = false }
        )
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Video Stream") },
                actions = {
                    IconButton(
                        onClick = {
                            if (isStreaming) {
                                VideoStreamService.stopService(context)
                            } else {
                                VideoStreamService.startService(context)
                            }
                            isStreaming = !isStreaming
                        }
                    ) {
                        if (isStreaming) {
                            Icon(
                                imageVector = Icons.Filled.Close,
                                contentDescription = "Stop Streaming"
                            )
                        } else {
                            Icon(
                                imageVector = Icons.Filled.PlayArrow,
                                contentDescription = "Start Streaming"
                            )
                        }
                    }
                    IconButton(onClick = { showSettings = true }) {
                        Icon(
                            imageVector = Icons.Default.Settings,
                            contentDescription = "Settings"
                        )
                    }
                }
            )
        }
    ) { paddingValues ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
        ) {
            CameraPreview(
                onViewCreated = { textureView ->
                    viewModel.initializeCamera(context as Activity)
                    viewModel.startPreview(textureView as CameraViewInterface)
                }
            )
        }
    }
} 