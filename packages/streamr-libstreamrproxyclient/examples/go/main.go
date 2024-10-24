package main

/*
#cgo LDFLAGS: -lstreamrproxyclient
#include "streamrproxyclient.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"os"
	"time"
	"unsafe"
)

func generateRandomEthereumAddress() string {
    const hexChars = "0123456789abcdef"
    address := make([]byte, 42)
    address[0] = '0'
    address[1] = 'x'
    for i := 2; i < 42; i++ {
        address[i] = hexChars[C.rand()%16]
    }
    return string(address)
}

func main() {
    if len(os.Args) != 4 {
        fmt.Fprintf(os.Stderr, "Usage: %s <proxy_server_url> <proxy_server_ethereum_address> <stream_part_id>\n", os.Args[0])
        os.Exit(1)
    }
    proxyUrl := os.Args[1]
    proxyServerEthereumAddress := os.Args[2]
    streamPartId := os.Args[3]

    ownEthereumAddress := C.CString("0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5")
    ethereumPrivateKey := C.CString("23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820")
    defer C.free(unsafe.Pointer(ownEthereumAddress))
    defer C.free(unsafe.Pointer(ethereumPrivateKey))

    var errors *C.Error
    var numErrors C.uint64_t

    clientHandle := C.proxyClientNew(&errors, &numErrors, ownEthereumAddress, C.CString(streamPartId))
    if numErrors != 0 || errors != nil {
        fmt.Fprintf(os.Stderr, "Error creating proxy client\n")
        os.Exit(1)
    }

    proxy := C.Proxy{
        websocketUrl:     C.CString(proxyUrl),
        ethereumAddress:  C.CString(proxyServerEthereumAddress),
    }
    defer C.free(unsafe.Pointer(proxy.websocketUrl))
    defer C.free(unsafe.Pointer(proxy.ethereumAddress))

    C.proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1)
    if numErrors != 0 || errors != nil {
        fmt.Fprintf(os.Stderr, "Error connecting to proxy\n")
        os.Exit(1)
    }

    message := "Hello from libstreamrproxyclient!"

    for {
        fmt.Println("Publishing message")
        numProxiesPublishedTo := C.proxyClientPublish(
            &errors,
            &numErrors,
            clientHandle,
            C.CString(message),
            C.uint64_t(len(message)),
            ethereumPrivateKey)

        if numErrors != 0 || errors != nil {
            fmt.Fprintf(os.Stderr, "Error publishing message\n")
            os.Exit(1)
        }

        fmt.Printf("%s published message \"%s\" to %d proxies\n", C.GoString(ownEthereumAddress), message, numProxiesPublishedTo)
        fmt.Println("Sleeping for 15 seconds")
        time.Sleep(15 * time.Second)
        fmt.Println("Sleeping done")
    }

    C.proxyClientDelete(&errors, &numErrors, clientHandle)
    if numErrors != 0 || errors != nil {
        fmt.Fprintf(os.Stderr, "Error deleting proxy client\n")
        os.Exit(1)
    }
}
