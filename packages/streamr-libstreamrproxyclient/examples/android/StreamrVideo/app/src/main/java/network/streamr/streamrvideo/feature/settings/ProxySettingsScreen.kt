package network.streamr.streamrvideo.feature.settings

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ProxySettingsScreen(
    viewModel: ProxySettingsViewModel = hiltViewModel(),
    onNavigateBack: () -> Unit
) {
    var ethereumAddress by remember { mutableStateOf("") }
    var streamPartId by remember { mutableStateOf("") }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Proxy Settings") },
                navigationIcon = {
                    IconButton(onClick = onNavigateBack) {
                        Icon(
                            imageVector = Icons.Default.ArrowBack,
                            contentDescription = "Back"
                        )
                    }
                }
            )
        }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(paddingValues)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            OutlinedTextField(
                value = ethereumAddress,
                onValueChange = { ethereumAddress = it },
                label = { Text("Ethereum Address") },
                modifier = Modifier.fillMaxWidth()
            )
            
            OutlinedTextField(
                value = streamPartId,
                onValueChange = { streamPartId = it },
                label = { Text("Stream Part ID") },
                modifier = Modifier.fillMaxWidth()
            )

            Button(
                onClick = { 
                    viewModel.updateSettings(ethereumAddress, streamPartId)
                    onNavigateBack()
                },
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Save Settings")
            }
        }
    }
} 