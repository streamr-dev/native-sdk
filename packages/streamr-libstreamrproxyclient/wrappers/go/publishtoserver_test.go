package streamrproxyclient

import (
	"testing"
)

func TestPublishToServer(t *testing.T) {
	tsEthereumAddress := "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	tsProxyUrl := "ws://127.0.0.1:44211"
	tsStreamPartId := "0xa000000000000000000000000000000000000000#01"
	ownEthereumAddress := "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5"
	ethereumPrivateKey := "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"

	lib := NewLibStreamrProxyClient()
	defer lib.Close()

	client, err := NewProxyClient(ownEthereumAddress, tsStreamPartId)
	if err != nil {
		t.Fatalf("Error creating ProxyClient: %v", err)
	}
	defer client.Close()

	proxies := []Proxy{
		*NewProxy(tsProxyUrl, tsEthereumAddress),
	}

	result := client.Connect(proxies)
	if len(result.errors) != 0 {
		t.Fatalf("Errors during connection: %v", result.errors)
	}
	if len(result.successful) != 1 {
		t.Fatalf("Unexpected number of successful connections: %d", len(result.successful))
	}

	data := []byte("Hello from Go!")
	result = client.Publish(data, ethereumPrivateKey)
	if len(result.errors) != 0 {
		t.Fatalf("Errors during publish: %v", result.errors)
	}
	if len(result.successful) != 1 {
		t.Fatalf("Unexpected number of successful publishes: %d", len(result.successful))
	}
}