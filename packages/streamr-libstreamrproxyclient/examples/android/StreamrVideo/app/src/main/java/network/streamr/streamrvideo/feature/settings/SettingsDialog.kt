package network.streamr.streamrvideo.feature.settings

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel

@Composable
fun SettingsDialog(
    viewModel: SettingsViewModel = hiltViewModel(),
    onDismiss: () -> Unit
) {
    val proxyId by viewModel.proxyId.collectAsState()
    val proxyAddress by viewModel.proxyAddress.collectAsState()
    val privateKey by viewModel.privateKey.collectAsState()
    val localAddress by viewModel.localAddress.collectAsState()
    val streamPartId by viewModel.streamPartId.collectAsState()

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Proxy Settings") },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 8.dp)
            ) {
                OutlinedTextField(
                    value = proxyId,
                    onValueChange = { viewModel.updateProxyId(it) },
                    label = { Text("Proxy ID") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                )
                
                OutlinedTextField(
                    value = proxyAddress,
                    onValueChange = { viewModel.updateProxyAddress(it) },
                    label = { Text("Proxy Address") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                )
                
                OutlinedTextField(
                    value = privateKey,
                    onValueChange = { viewModel.updatePrivateKey(it) },
                    label = { Text("Private Key") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                )

                OutlinedTextField(
                    value = localAddress,
                    onValueChange = { viewModel.updateLocalAddress(it) },
                    label = { Text("Local Address") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                )

                OutlinedTextField(
                    value = streamPartId,
                    onValueChange = { viewModel.updateStreamPartId(it) },
                    label = { Text("Stream Part ID") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                )
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    viewModel.saveSettings()
                    onDismiss()
                }
            ) {
                Text("Save")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        }
    )
} 