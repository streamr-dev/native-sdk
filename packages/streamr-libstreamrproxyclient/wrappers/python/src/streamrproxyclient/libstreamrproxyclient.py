import ctypes
import platform
import os

class ErrorCodes:
    ERROR_INVALID_ETHEREUM_ADDRESS = "INVALID_ETHEREUM_ADDRESS"
    ERROR_INVALID_STREAM_PART_ID = "INVALID_STREAM_PART_ID"
    ERROR_PROXY_CLIENT_NOT_FOUND = "PROXY_CLIENT_NOT_FOUND"
    ERROR_INVALID_PROXY_URL = "INVALID_PROXY_URL"
    ERROR_NO_PROXIES_DEFINED = "NO_PROXIES_DEFINED"
    ERROR_PROXY_CONNECTION_FAILED = "PROXY_CONNECTION_FAILED"
    ERROR_PROXY_BROADCAST_FAILED = "PROXY_BROADCAST_FAILED"

class CProxy(ctypes.Structure):
    _fields_ = [("websocketUrl", ctypes.c_char_p),
                ("ethereumAddress", ctypes.c_char_p)]

class CError(ctypes.Structure):
    _fields_ = [("message", ctypes.c_char_p),
                ("code", ctypes.c_char_p),
                ("proxy", ctypes.POINTER(CProxy))]

class CProxyClientResult(ctypes.Structure):
    _fields_ = [("errors", ctypes.POINTER(CError)),
                ("numErrors", ctypes.c_uint64),
                ("successful", ctypes.POINTER(CProxy)),
                ("numSuccessful", ctypes.c_uint64)]
# Native python classes for the C structs above

class Proxy:
    def __init__(self, websocket_url: str, ethereum_address: str):
        self.websocket_url = websocket_url
        self.ethereum_address = ethereum_address
    
    @classmethod    
    def from_c_proxy(cls, c_proxy: CProxy):
        #print(c_proxy.contents.websocketUrl)
        #print(c_proxy.contents.ethereumAddress)
        #print("c_proxy.contents: ", c_proxy.contents)
        print(c_proxy.websocketUrl)
        return cls(c_proxy.websocketUrl.decode('utf-8'), c_proxy.ethereumAddress.decode('utf-8'))
    
    def __str__(self):
        return f"Proxy(websocketUrl={self.websocket_url}, ethereumAddress={self.ethereum_address})"
    
    def __repr__(self):
        return self.__str__()
    
    def __eq__(self, other):
        return self.websocket_url == other.websocket_url and self.ethereum_address == other.ethereum_address

class Error:
    def __init__(self, c_error: CError):
        self.message = c_error.message.decode('utf-8')
        self.code = c_error.code.decode('utf-8')
        #print(c_error.proxy)
        self.proxy = None if not c_error.proxy else Proxy.from_c_proxy(c_error.proxy.contents)

    def __str__(self):
        return f"Error(message={self.message}, code={self.code}, proxy={self.proxy})"
    
    def __repr__(self):
        return self.__str__()   

# The exception that wraps the C library errors
class ProxyClientException(Exception):
    def __init__(self, error: Error):
        super().__init__(str(error))
        self.error = error

class ProxyClientResult:
    def __init__(self, proxy_result_ptr: CProxyClientResult):
        self.errors = []
        self.successful = []
        
        print(proxy_result_ptr.contents.numErrors)
        for i in range(proxy_result_ptr.contents.numErrors):
            c_error = proxy_result_ptr.contents.errors[i]
            error = Error(c_error)
            self.errors.append(error)

        for i in range(proxy_result_ptr.contents.numSuccessful):
            c_proxy = proxy_result_ptr.contents.successful[i]
            proxy = Proxy.from_c_proxy(c_proxy)
            self.successful.append(proxy)

# The class that wraps the C library

class LibStreamrProxyClient:
    def __enter__(self):
        # Load dynamic library
        lib_name = 'libstreamrproxyclient.so'

        if platform.system() == "Darwin":
            lib_name = 'libstreamrproxyclient.dylib'
            
        self.lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), lib_name)) 
        
        # Define function signatures for the library functions
        
        self.lib.testRpc.restype = ctypes.c_char_p
        
        self.lib.proxyClientInitLibrary.restype = None
        
        self.lib.proxyClientCleanupLibrary.restype = None

        self.lib.proxyClientResultDelete.argtypes = [ctypes.POINTER(CProxyClientResult)]
        self.lib.proxyClientResultDelete.restype = None

        self.lib.proxyClientNew.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_char_p, ctypes.c_char_p]
        self.lib.proxyClientNew.restype = ctypes.c_uint64

        self.lib.proxyClientDelete.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_uint64]
        self.lib.proxyClientDelete.restype = None

        self.lib.proxyClientConnect.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_uint64, ctypes.POINTER(CProxy), ctypes.c_uint64]
        self.lib.proxyClientConnect.restype = ctypes.c_uint64

        self.lib.proxyClientPublish.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_uint64, ctypes.c_char_p, ctypes.c_uint64, ctypes.c_char_p]
        self.lib.proxyClientPublish.restype = ctypes.c_uint64
        
        self.lib.proxyClientInitLibrary()
        
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        # Cleanup library
        self.lib.proxyClientCleanupLibrary()

class ProxyClient:
    def __init__(self, lib: LibStreamrProxyClient, ownEthereumAddress: str, streamPartId: str):
        self.lib = lib.lib
        self.ownEthereumAddress = ownEthereumAddress
        self.streamPartId = streamPartId
        
    def __enter__(self):
        result = ctypes.POINTER(CProxyClientResult)()
        self.clientHandle = self.lib.proxyClientNew(ctypes.byref(result), self.ownEthereumAddress.encode('utf-8'), self.streamPartId.encode('utf-8'))
        if result.contents.numErrors > 0:
            raise ProxyClientException(Error(result.contents.errors[0]))
        self.lib.proxyClientResultDelete(result)
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.proxyClientDelete(ctypes.byref(result), self.clientHandle)
        assert result.contents.numErrors == 0
        self.lib.proxyClientResultDelete(result)

    def connect(self, proxies: list[Proxy]) -> ProxyClientResult:
        proxy_array = (CProxy * len(proxies))(*[CProxy(proxy.websocket_url.encode('utf-8'), proxy.ethereum_address.encode('utf-8')) for proxy in proxies])
        num_proxies = ctypes.c_uint64(len(proxies))
        print(f'num_proxies: {num_proxies.value}')
        for i in range(num_proxies.value):
            print(f'proxy_array[{i}]: websocket_url={proxy_array[i].websocketUrl.decode("utf-8")}, ethereum_address={proxy_array[i].ethereumAddress.decode("utf-8")}')
        result = ctypes.POINTER(CProxyClientResult)()
        print(result)
        self.lib.proxyClientConnect(ctypes.byref(result), self.clientHandle, proxy_array, num_proxies)
        res = ProxyClientResult(result)
        self.lib.proxyClientResultDelete(result)
        return res
    
    def publish(self, data: bytes, ethereumPrivateKey: str = None) -> ProxyClientResult:
        result = ctypes.POINTER(CProxyClientResult)()
        if ethereumPrivateKey:
            self.lib.proxyClientPublish(ctypes.byref(result), self.clientHandle, data, len(data), ethereumPrivateKey.encode('utf-8'))
        else:
            self.lib.proxyClientPublish(ctypes.byref(result), self.clientHandle, data, len(data), None)
        res = ProxyClientResult(result)
        self.lib.proxyClientResultDelete(result)
        return res
