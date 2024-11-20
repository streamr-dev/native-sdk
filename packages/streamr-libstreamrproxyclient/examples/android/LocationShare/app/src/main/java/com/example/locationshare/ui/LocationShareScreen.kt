package com.example.locationshare.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme.colorScheme
import androidx.compose.material3.MaterialTheme.shapes
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.locationshare.R
import com.example.locationshare.Status

@Composable
fun LocationShareScreen(
    locationShareViewModel: LocationShareViewModel = viewModel(),
    locationViewModel: LocationViewModel = viewModel()
) {
    val locationState by locationViewModel.state.collectAsState()
    val mediumPadding = dimensionResource(R.dimen.padding_medium)

    Column(
        modifier = Modifier
            .statusBarsPadding()
            .verticalScroll(rememberScrollState())
            .safeDrawingPadding()
            .padding(mediumPadding),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.Start
    ) {
        ProxyFieldsLayout(
            proxyAddress = locationShareViewModel.streamrProxyClient.proxyAddress,
            proxyId = locationShareViewModel.streamrProxyClient.proxyId,
            ethereumPrivateKey = locationShareViewModel.streamrProxyClient.ethereumPrivateKey,
            status = locationShareViewModel.streamrProxyClient.status,
            onProxyAddressChanged = {
                locationShareViewModel.streamrProxyClient.updateProxyAddress(
                    it
                )
            },
            onProxyIdChanged = { locationShareViewModel.streamrProxyClient.updateProxyId(it) },
            onEthereumPrivateKeyChanged = { locationShareViewModel.streamrProxyClient.updateEthereumPrivateKey(it) },
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight()
                .padding(mediumPadding)
        )

        Card(
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight()
                .padding(mediumPadding),
            elevation = CardDefaults.cardElevation(defaultElevation = 5.dp)
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(mediumPadding),
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.padding(mediumPadding)
            ) {

                OutlinedTextField(
                    value = locationShareViewModel.streamrProxyClient.intervalSeconds,
                    onValueChange = {
                        locationShareViewModel.streamrProxyClient.updateIntervalSeconds(
                            it
                        )
                    },
                    label = {
                        Text("Publishing interval")
                    },
                    singleLine = true,
                    shape = shapes.large,
                    modifier = Modifier.fillMaxWidth(),
                    colors = TextFieldDefaults.colors(
                        focusedContainerColor = colorScheme.surface,
                        unfocusedContainerColor = colorScheme.surface,
                        disabledContainerColor = colorScheme.surface,
                    )
                )

            }
        }

        Card(
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight()
                .padding(mediumPadding),
            elevation = CardDefaults.cardElevation(defaultElevation = 5.dp)
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(mediumPadding),
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.padding(mediumPadding)
            ) {
                Text(text = "GPS: ${locationState.latitude},${locationState.longitude}")

            }
        }
        Button(
            enabled = if (locationShareViewModel.streamrProxyClient.proxyId != "" &&
                locationShareViewModel.streamrProxyClient.proxyAddress != "" &&
                locationShareViewModel.streamrProxyClient.intervalSeconds != ""
            ) {
                true
            } else {
                false
            },
            modifier = Modifier.fillMaxWidth(),
            onClick = { locationShareViewModel.buttonClicked() }
        ) {
            if (locationShareViewModel.streamrProxyClient.status == Status.stopped) {
                Text("Start Sharing")
            } else {
                Text("Stop Sharing")
            }

        }
    }
}

@Composable
fun ProxyFieldsLayout(
    proxyAddress: String,
    proxyId: String,
    ethereumPrivateKey: String,
    status: Status,
    onProxyAddressChanged: (String) -> Unit,
    onProxyIdChanged: (String) -> Unit,
    onEthereumPrivateKeyChanged: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val mediumPadding = dimensionResource(R.dimen.padding_medium)

    Card(
        modifier = modifier,
        elevation = CardDefaults.cardElevation(defaultElevation = 5.dp)
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(mediumPadding),
            horizontalAlignment = Alignment.Start,
            modifier = Modifier.padding(mediumPadding)
        ) {

            OutlinedTextField(
                value = proxyAddress,
                onValueChange = onProxyAddressChanged,
                label = {
                    Text("Proxy Address")
                },
                singleLine = true,
                shape = shapes.large,
                modifier = Modifier.fillMaxWidth(),
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = colorScheme.surface,
                    unfocusedContainerColor = colorScheme.surface,
                    disabledContainerColor = colorScheme.surface,
                )
            )

            OutlinedTextField(
                value = proxyId,
                onValueChange = onProxyIdChanged,
                label = {
                    Text("Proxy Id")
                },
                singleLine = true,
                shape = shapes.large,
                modifier = Modifier.fillMaxWidth(),
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = colorScheme.surface,
                    unfocusedContainerColor = colorScheme.surface,
                    disabledContainerColor = colorScheme.surface,
                )
            )

            OutlinedTextField(
                value = ethereumPrivateKey,
                onValueChange = onEthereumPrivateKeyChanged,
                label = {
                    Text("Ethereum Private Key")
                },
                singleLine = true,
                shape = shapes.large,
                modifier = Modifier.fillMaxWidth(),
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = colorScheme.surface,
                    unfocusedContainerColor = colorScheme.surface,
                    disabledContainerColor = colorScheme.surface,
                )
            )
            Text(text = "Status: ${status}")
        }
    }
}
