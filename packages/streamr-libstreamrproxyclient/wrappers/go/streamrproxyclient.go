package streamrproxyclient

// #include <stdlib.h>
// #include "shim.h"
// #include "streamrproxyclient.h"
import "C"
import (
	"fmt"
	"unsafe"
)

func openLibrary() (error) { 
	fileName, err := SaveLibToTempFile()
	if err != nil {
		return err
	}
	C.loadSharedLibrary(C.CString(fileName))
	return nil
}

func closeLibrary() {
	C.closeSharedLibrary()
}

/*
func openEmbeddedLib(embedFS embed.FS, libName, funcName string) (unsafe.Pointer, error) {
	
	log.Println("creating temp dir to host .so")
	dirName, err := os.MkdirTemp("", "*")
	if err != nil {
		return nil, err
	}

	log.Println("opening a .so from embed")
	f, err := greet.LibFS.Open(libName)
	if err != nil {
		return nil, err
	}

	log.Println("read .so content into memory buffer")
	b := make([]byte, 1_000_000)
	_, err = f.Read(b)
	if err != nil {
		return nil, err
	}

	fPath := filepath.Join(dirName, libName)

	log.Println("writing embedded .so in:", fPath)
	err = ioutil.WriteFile(fPath, b, 0o600)
	if err != nil {
		return nil, err
	}
	
	fileName, err := SaveLibToTempFile()
	if err != nil {
		return nil, err
	}
	log.Println("dlopen the lib apps from temp file")
	lName := C.CString(fileName)
	handle := C.dlopen(lName, C.RTLD_LAZY)
	if handle == nil {
		return nil, errors.New("dlopen: fail")
	}

	log.Println("dlsym the func from the lib")
	symbol := C.CString(funcName)
	greetFn := C.dlsym(handle, symbol)
	if greetFn == nil {
		return nil, errors.New("dlsym: fail")
	}
	return greetFn, err
}
*/

// Error codes from the C library.
const (
	ERROR_INVALID_ETHEREUM_ADDRESS = "INVALID_ETHEREUM_ADDRESS"
	ERROR_INVALID_STREAM_PART_ID   = "INVALID_STREAM_PART_ID"
	ERROR_PROXY_CLIENT_NOT_FOUND   = "PROXY_CLIENT_NOT_FOUND"
	ERROR_INVALID_PROXY_URL        = "INVALID_PROXY_URL"
	ERROR_NO_PROXIES_DEFINED       = "NO_PROXIES_DEFINED"
	ERROR_PROXY_CONNECTION_FAILED  = "PROXY_CONNECTION_FAILED"
	ERROR_PROXY_BROADCAST_FAILED   = "PROXY_BROADCAST_FAILED"
)

// Proxy struct represents a proxy with a websocket URL and an Ethereum address.
type Proxy struct {
	websocketUrl    string
	ethereumAddress string
}

// NewProxy creates a new Proxy instance.
func NewProxy(websocketUrl, ethereumAddress string) *Proxy {
	return &Proxy{
		websocketUrl:    websocketUrl,
		ethereumAddress: ethereumAddress,
	}
}

// FromCProxy converts a C Proxy to a Go Proxy.
func (p *Proxy) FromCProxy(cProxy *C.Proxy) *Proxy {
	return &Proxy{
		websocketUrl:    C.GoString(cProxy.websocketUrl),
		ethereumAddress: C.GoString(cProxy.ethereumAddress),
	}
}

// String returns a string representation of the Proxy.
func (p *Proxy) String() string {
	return fmt.Sprintf("Proxy(websocketUrl=%s, ethereumAddress=%s)", p.websocketUrl, p.ethereumAddress)
}

// Equals checks if two Proxy instances are equal.
func (p *Proxy) Equals(other *Proxy) bool {
	return p.websocketUrl == other.websocketUrl && p.ethereumAddress == other.ethereumAddress
}

// ProxyClientError struct represents an error in the ProxyClient.
type ProxyClientError struct {
	message string
	code    string
	proxy   *Proxy
}

// Error implements the error interface for ProxyClientError.
func (e *ProxyClientError) Error() string {
	return e.String()
}

// NewProxyClientError creates a new ProxyClientError from a C Error.
func NewProxyClientError(cError *C.Error) *ProxyClientError {
	var proxy *Proxy
	if cError.proxy != nil {
		proxy = NewProxy(C.GoString(cError.proxy.websocketUrl), C.GoString(cError.proxy.ethereumAddress))
	}
	return &ProxyClientError{
		message: C.GoString(cError.message),
		code:    C.GoString(cError.code),
		proxy:   proxy,
	}
}

// String returns a string representation of the ProxyClientError.
func (e *ProxyClientError) String() string {
	return fmt.Sprintf("Error(message=%s, code=%s, proxy=%v)", e.message, e.code, e.proxy)
}

// ProxyClientResult struct represents the result of a ProxyClient operation.
type ProxyClientResult struct {
	errors     []*ProxyClientError
	successful []*Proxy
}

// NewProxyClientResult creates a new ProxyClientResult from a C ProxyResult.
func NewProxyClientResult(proxyResultPtr *C.ProxyResult) *ProxyClientResult {
	result := &ProxyClientResult{
		errors:     make([]*ProxyClientError, proxyResultPtr.numErrors),
		successful: make([]*Proxy, proxyResultPtr.numSuccessful),
	}

	for i := 0; i < int(proxyResultPtr.numErrors); i++ {
		cError := (*C.Error)(unsafe.Pointer(uintptr(unsafe.Pointer(proxyResultPtr.errors)) + uintptr(i)*unsafe.Sizeof(*proxyResultPtr.errors)))
		result.errors[i] = NewProxyClientError(cError)
	}

	for i := 0; i < int(proxyResultPtr.numSuccessful); i++ {
		cProxy := (*C.Proxy)(unsafe.Pointer(uintptr(unsafe.Pointer(proxyResultPtr.successful)) + uintptr(i)*unsafe.Sizeof(*proxyResultPtr.successful)))
		result.successful[i] = NewProxy(C.GoString(cProxy.websocketUrl), C.GoString(cProxy.ethereumAddress))
	}

	return result
}

// LibStreamrProxyClient struct represents the Streamr Proxy Client library.
type LibStreamrProxyClient struct {
}

// NewLibStreamrProxyClient creates a new LibStreamrProxyClient instance.
func NewLibStreamrProxyClient() *LibStreamrProxyClient {
	err := openLibrary()
	if err != nil {
		panic(err)
	}
	lib := &LibStreamrProxyClient{}
	C.proxyClientInitLibraryWrapper()
	return lib
}

// Close cleans up the Streamr Proxy Client library.
func (l *LibStreamrProxyClient) Close() {
	C.proxyClientCleanupLibraryWrapper()
	closeLibrary()
}

// ProxyClient struct represents a client that connects to proxies.
type ProxyClient struct {
	ownEthereumAddress string
	streamPartId       string
	clientHandle       C.uint64_t
}

// NewProxyClient creates a new ProxyClient instance.
func NewProxyClient(ownEthereumAddress, streamPartId string) (*ProxyClient, *ProxyClientError) {
	client := &ProxyClient{
		ownEthereumAddress: ownEthereumAddress,
		streamPartId:       streamPartId,
	}
	var result *C.ProxyResult
	client.clientHandle = C.proxyClientNewWrapper(&result, C.CString(client.ownEthereumAddress), C.CString(client.streamPartId))
	if result.numErrors > 0 {
		firstError := (*C.Error)(unsafe.Pointer(uintptr(unsafe.Pointer(result.errors)) + uintptr(0)*unsafe.Sizeof(*result.errors)))
		return nil, NewProxyClientError(firstError)
	}
	C.proxyClientResultDeleteWrapper(result)
	return client, nil
}

// Close deletes the ProxyClient instance.
func (p *ProxyClient) Close() *ProxyClientError {
	var result *C.ProxyResult
	C.proxyClientDeleteWrapper(&result, p.clientHandle)
	if result.numErrors > 0 {
		firstError := (*C.Error)(unsafe.Pointer(uintptr(unsafe.Pointer(result.errors)) + uintptr(0)*unsafe.Sizeof(*result.errors)))
		return NewProxyClientError(firstError)
	}
	C.proxyClientResultDeleteWrapper(result)
	return nil
}

// Connect connects the ProxyClient to the given proxies.
func (p *ProxyClient) Connect(proxies []Proxy) *ProxyClientResult {
	numProxies := C.uint64_t(len(proxies))
	var proxyArray []C.Proxy
	if numProxies > 0 {
		proxyArray = make([]C.Proxy, numProxies)
		for i, proxy := range proxies {
			proxyArray[i] = C.Proxy{
				websocketUrl:    C.CString(proxy.websocketUrl),
				ethereumAddress: C.CString(proxy.ethereumAddress),
			}
		}
	}
	var result *C.ProxyResult

	if numProxies > 0 {
		C.proxyClientConnectWrapper(&result, p.clientHandle, &proxyArray[0], numProxies)
	} else {
		C.proxyClientConnectWrapper(&result, p.clientHandle, nil, numProxies)
	}
	res := NewProxyClientResult(result)
	C.proxyClientResultDeleteWrapper(result)
	return res
}

// Publish publishes data using the ProxyClient.
func (p *ProxyClient) Publish(data []byte, ethereumPrivateKey string) *ProxyClientResult {
	var result *C.ProxyResult
	if ethereumPrivateKey != "" {
		C.proxyClientPublishWrapper(&result, p.clientHandle, (*C.char)(unsafe.Pointer(&data[0])), C.uint64_t(len(data)), C.CString(ethereumPrivateKey))
	} else {
		C.proxyClientPublishWrapper(&result, p.clientHandle, (*C.char)(unsafe.Pointer(&data[0])), C.uint64_t(len(data)), nil)
	}
	res := NewProxyClientResult(result)
	C.proxyClientResultDeleteWrapper(result)
	return res
}
