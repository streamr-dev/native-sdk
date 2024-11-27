package streamrproxyclient

import (
	"fmt"
	"testing"
)

const (
	invalidEthereumAddress     = "INVALID_ETHEREUM_ADDRESS"
	goodEthereumAddress        = "0x123456789012345678901234567890123456789a"
	validEthereumAddress       = "0x1234567890123456789012345678901234567890"
	validEthereumAddress2      = "0x1234567890123456789012345678901234567892"
	validEthereumAddress3      = "0x1234567890123456789012345678901234567893"
	invalidStreamPartId        = "INVALID_STREAM_PART_ID"
	validStreamPartId          = "0xa000000000000000000000000000000000000000#01"
	ownEthereumAddress         = "0x1234567890123456789012345678901234567890"
	streamPartId               = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
	invalidProxyUrl            = "poiejrg039utg240"
	validProxyUrl              = "ws://valid.com"
	nonExistentProxyUrl0       = "ws://localhost:0"
	nonExistentProxyUrl1       = "ws://localhost:1"
	nonExistentProxyUrl2       = "ws://localhost:2"
	invalidClientHandle uint64 = 0
)
func TestLibStreamrProxyClientCreation(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    if lib == nil {
        t.Fatalf("Failed to create LibStreamrProxyClient")
    }
    defer lib.Close()
    client, err := NewProxyClient(ownEthereumAddress, streamPartId)
    if err != nil {
        t.Fatalf("Failed to create ProxyClient: %v", err)
    }
    defer client.Close()
}

func TestInvalidEthereumAddress(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(invalidEthereumAddress, validStreamPartId)
    if err == nil {
        t.Fatalf("Expected error for invalid Ethereum address")
    } else if err.code != ERROR_INVALID_ETHEREUM_ADDRESS {
        t.Fatalf("Expected error code %s, got %s", ERROR_INVALID_ETHEREUM_ADDRESS, err.code)
    }
    if client != nil {
        defer client.Close()
        t.Fatalf("Expected nil client for invalid Ethereum address")
    }
}

func TestInvalidStreamPartId(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(validEthereumAddress, invalidStreamPartId)
    if err == nil {
        t.Fatalf("Expected error for invalid stream part id")
    } else if err.code != ERROR_INVALID_STREAM_PART_ID {
        t.Fatalf("Expected error code %s, got %s", ERROR_INVALID_STREAM_PART_ID, err.code)
    }
    if client != nil {
        defer client.Close()
        t.Fatalf("Expected nil client for invalid stream part id")
    }
}

func TestNoProxiesDefined(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(validEthereumAddress, validStreamPartId)
    if err != nil {
        t.Fatalf("Failed to create ProxyClient: %v", err)
    }
    defer client.Close()
    result := client.Connect([]Proxy {})
    if result.errors == nil {
        t.Fatalf("Expected error code %s, got nil", ERROR_NO_PROXIES_DEFINED)
    } else if result.errors[0].code != ERROR_NO_PROXIES_DEFINED {
        t.Fatalf("Expected error code %s, got %s", ERROR_NO_PROXIES_DEFINED, result.errors[0].code)
    }
    if len(result.successful) != 0 {
        t.Fatalf("Expected nil successful proxies for no proxies defined")
    }
}

func TestInvalidProxyUrl(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(validEthereumAddress, validStreamPartId)
    if err != nil {
        t.Fatalf("Failed to create ProxyClient: %v", err)
    }
    defer client.Close()
    proxies := []Proxy{
        Proxy{websocketUrl: invalidProxyUrl, ethereumAddress: validEthereumAddress},
    }
    fmt.Println("calling connect")
    result := client.Connect(proxies)
    fmt.Println("Testing invalid proxy URL")
    if len(result.errors) != 1 {
        t.Fatalf("Expected 1 error for invalid proxy url, got %d", len(result.errors))
    } else if result.errors[0].code != ERROR_INVALID_PROXY_URL {
        t.Fatalf("Expected error code %s, got %s", ERROR_INVALID_PROXY_URL, result.errors[0].code)
    }
    if len(result.successful) != 0 {
        t.Fatalf("Expected nil successful proxies for invalid proxy url")
    }
}

func TestInvalidProxyEthereumAddress(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(validEthereumAddress, validStreamPartId)
    if err != nil {
        t.Fatalf("Failed to create ProxyClient: %v", err)
    }
    defer client.Close()
    proxies := []Proxy{
        Proxy{websocketUrl: validProxyUrl, ethereumAddress: invalidEthereumAddress},
    }
    result := client.Connect(proxies)
    if len(result.errors) != 1 {
        t.Fatalf("Expected 1 error for invalid proxy ethereum address, got %d", len(result.errors))
    } else if result.errors[0].code != ERROR_INVALID_ETHEREUM_ADDRESS {
        t.Fatalf("Expected error code %s, got %s", ERROR_INVALID_ETHEREUM_ADDRESS, result.errors[0].code)
    }
    if len(result.successful) != 0 {
        t.Fatalf("Expected nil successful proxies for invalid proxy ethereum address")
    }
}

func TestProxyConnectionFailed(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(validEthereumAddress, validStreamPartId)
    if err != nil {
        t.Fatalf("Failed to create ProxyClient: %v", err)
    }
    defer client.Close()
    proxies := []Proxy{
        Proxy{websocketUrl: nonExistentProxyUrl0, ethereumAddress: validEthereumAddress},
    }
    result := client.Connect(proxies)
    if len(result.errors) != 1 {
        t.Fatalf("Expected 1 error for proxy connection failed, got %d", len(result.errors))
    } else if result.errors[0].code != ERROR_PROXY_CONNECTION_FAILED {
        t.Fatalf("Expected error code %s, got %s", ERROR_PROXY_CONNECTION_FAILED, result.errors[0].code)
    }
    if len(result.successful) != 0 {    
        t.Fatalf("Expected nil successful proxies for proxy connection failed")
    }
}

func TestThreeProxyConnectionsFailed(t *testing.T) {
    lib := NewLibStreamrProxyClient()
    defer lib.Close()
    client, err := NewProxyClient(validEthereumAddress, validStreamPartId)
    if err != nil {
        t.Fatalf("Failed to create ProxyClient: %v", err)
    }
    defer client.Close()
    proxies := []Proxy{
        Proxy{websocketUrl: nonExistentProxyUrl0, ethereumAddress: validEthereumAddress},
        Proxy{websocketUrl: nonExistentProxyUrl1, ethereumAddress: validEthereumAddress2},
        Proxy{websocketUrl: nonExistentProxyUrl2, ethereumAddress: validEthereumAddress3},
    }
    result := client.Connect(proxies)
    if len(result.errors) != 3 {
        t.Fatalf("Expected 3 errors for three proxy connections failed, got %d", len(result.errors))
    } else {
        for i, err := range result.errors {
            if err.code != ERROR_PROXY_CONNECTION_FAILED {
                t.Fatalf("Expected error code %s for error %d, got %s", ERROR_PROXY_CONNECTION_FAILED, i, err.code)
            }
            expectedWebsocketUrl := ""
            expectedEthereumAddress := ""
            switch i {
            case 0:
                expectedWebsocketUrl = nonExistentProxyUrl0
                expectedEthereumAddress = validEthereumAddress
            case 1:
                expectedWebsocketUrl = nonExistentProxyUrl1
                expectedEthereumAddress = validEthereumAddress2
            case 2:
                expectedWebsocketUrl = nonExistentProxyUrl2
                expectedEthereumAddress = validEthereumAddress3
            }
            if err.proxy.websocketUrl != expectedWebsocketUrl {
                t.Fatalf("Expected websocket URL %s for error %d, got %s", expectedWebsocketUrl, i, err.proxy.websocketUrl)
            }
            if err.proxy.ethereumAddress != expectedEthereumAddress {
                t.Fatalf("Expected ethereum address %s for error %d, got %s", expectedEthereumAddress, i, err.proxy.ethereumAddress)
            }
        }
    }
    if len(result.successful) != 0 {
        t.Fatalf("Expected nil successful proxies for three proxy connections failed")
    }
}