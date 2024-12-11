package main

import (
	"log"

	streamrproxyclient "github.com/streamr-dev/goproxyclient"
)

func main() {
	// This is a widely-used test account
	ownEthereumAddress :=
	 "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5";
	ethereumPrivateKey :=
	 "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";

	// A Proxy server run by Streamr
	proxyUrl := "ws://95.216.15.80:44211"
	proxyEthereumAddress := "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
	
	// The stream to publish to
	streamPartId := "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
	
	// Create a new library instance
	lib := streamrproxyclient.NewLibStreamrProxyClient()
	defer lib.Close()

	
	// Create a new ProxyClient instance
	client, err := streamrproxyclient.NewProxyClient(ownEthereumAddress, streamPartId)
	if err != nil {
			log.Fatalf("Error creating ProxyClient: %v", err)
	}
	defer client.Close()

	// Create definition of the proxy server to connect to
	proxies := []streamrproxyclient.Proxy{
		*streamrproxyclient.NewProxy(proxyUrl, proxyEthereumAddress),
	}

	// Connect to the proxy server
	result := client.Connect(proxies)
	if len(result.Errors) != 0 {
		log.Fatalf("Errors during connection: %v", result.Errors)
	}
	if len(result.Successful) != 1 {
		log.Fatalf("Unexpected number of successful connections: %d", len(result.Successful))
	}

	// Publish a message to the test stream
	// You should see the result in the Streamr HUB web UI at
	// https://streamr.network/hub/streams/0xd2078dc2d780029473a39ce873fc182587be69db%2Flow-level-client/live-data
	data := []byte("Hello from Go!")
	result = client.Publish(data, ethereumPrivateKey)
	if len(result.Errors) != 0 {
		log.Fatalf("Errors during publish: %v", result.Errors)
	}
	if len(result.Successful) != 1 {
		log.Fatalf("Unexpected number of successful publishes: %d", len(result.Successful))
	}
}