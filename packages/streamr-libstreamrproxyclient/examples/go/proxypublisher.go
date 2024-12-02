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

	proxyUrl := "ws://95.216.15.80:44211"
	proxyEthereumAddress := "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
	streamPartId := "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
	
	lib := streamrproxyclient.NewLibStreamrProxyClient()
	defer lib.Close()

	client, err := streamrproxyclient.NewProxyClient(ownEthereumAddress, streamPartId)
	if err != nil {
			log.Fatalf("Error creating ProxyClient: %v", err)
	}
	defer client.Close()

	proxies := []streamrproxyclient.Proxy{
		*streamrproxyclient.NewProxy(proxyUrl, proxyEthereumAddress),
	}

	result := client.Connect(proxies)
	if len(result.Errors) != 0 {
		log.Fatalf("Errors during connection: %v", result.Errors)
	}
	if len(result.Successful) != 1 {
		log.Fatalf("Unexpected number of successful connections: %d", len(result.Successful))
	}

	data := []byte("Hello from Go!")
	result = client.Publish(data, ethereumPrivateKey)
	if len(result.Errors) != 0 {
		log.Fatalf("Errors during publish: %v", result.Errors)
	}
	if len(result.Successful) != 1 {
		log.Fatalf("Unexpected number of successful publishes: %d", len(result.Successful))
	}
}