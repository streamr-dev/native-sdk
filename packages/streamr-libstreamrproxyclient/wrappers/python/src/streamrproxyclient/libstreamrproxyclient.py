import ctypes
import platform
import os

class ErrorCodes:
    """
    Error codes from the C library.
    """
    ERROR_INVALID_ETHEREUM_ADDRESS = "INVALID_ETHEREUM_ADDRESS"
    ERROR_INVALID_STREAM_PART_ID = "INVALID_STREAM_PART_ID"
    ERROR_PROXY_CLIENT_NOT_FOUND = "PROXY_CLIENT_NOT_FOUND"
    ERROR_INVALID_PROXY_URL = "INVALID_PROXY_URL"
    ERROR_NO_PROXIES_DEFINED = "NO_PROXIES_DEFINED"
    ERROR_PROXY_CONNECTION_FAILED = "PROXY_CONNECTION_FAILED"
    ERROR_PROXY_BROADCAST_FAILED = "PROXY_BROADCAST_FAILED"

class CProxy(ctypes.Structure):
    """
    C struct for Proxy.
    """
    _fields_ = [("websocketUrl", ctypes.c_char_p),
                ("ethereumAddress", ctypes.c_char_p)]

class CError(ctypes.Structure):
    """
    C struct for Error.
    """
    _fields_ = [("message", ctypes.c_char_p),
                ("code", ctypes.c_char_p),
                ("proxy", ctypes.POINTER(CProxy))]

class CProxyClientResult(ctypes.Structure):
    """
    C struct for ProxyClientResult.
    """
    _fields_ = [("errors", ctypes.POINTER(CError)),
                ("numErrors", ctypes.c_uint64),
                ("successful", ctypes.POINTER(CProxy)),
                ("numSuccessful", ctypes.c_uint64)]

class Proxy:
    """
    Native Python class for Proxy.
    """
    def __init__(self, websocket_url: str, ethereum_address: str):
        """
        Initialize a Proxy instance.
        
        :param websocket_url: The websocket URL of the proxy.
        :param ethereum_address: The Ethereum address of the proxy.
        """
        self.websocket_url = websocket_url
        self.ethereum_address = ethereum_address
    
    @classmethod    
    def from_c_proxy(cls, c_proxy: CProxy):
        """
        Create a Proxy instance from a CProxy instance.
        
        :param c_proxy: The CProxy instance.
        :return: A Proxy instance.
        """
        return cls(c_proxy.websocketUrl.decode('utf-8'), c_proxy.ethereumAddress.decode('utf-8'))
    
    def __str__(self):
        """
        Return a string representation of the Proxy.
        
        :return: A string representation of the Proxy.
        """
        return f"Proxy(websocketUrl={self.websocket_url}, ethereumAddress={self.ethereum_address})"
    
    def __repr__(self):
        """
        Return a string representation of the Proxy.
        
        :return: A string representation of the Proxy.
        """
        return self.__str__()
    
    def __eq__(self, other):
        """
        Check if two Proxy instances are equal.
        
        :param other: The other Proxy instance.
        :return: True if the instances are equal, False otherwise.
        """
        return self.websocket_url == other.websocket_url and self.ethereum_address == other.ethereum_address

class Error:
    """
    Native Python class for Error.
    """
    def __init__(self, c_error: CError):
        """
        Initialize an Error instance from a CError instance.
        
        :param c_error: The CError instance.
        """
        self.message = c_error.message.decode('utf-8')
        self.code = c_error.code.decode('utf-8')
        self.proxy = None if not c_error.proxy else Proxy.from_c_proxy(c_error.proxy.contents)

    def __str__(self):
        """
        Return a string representation of the Error.
        
        :return: A string representation of the Error.
        """
        return f"Error(message={self.message}, code={self.code}, proxy={self.proxy})"
    
    def __repr__(self):
        """
        Return a string representation of the Error.
        
        :return: A string representation of the Error.
        """
        return self.__str__()   

class ProxyClientException(Exception):
    """
    The exception that wraps the C library errors.
    """
    def __init__(self, error: Error):
        """
        Initialize a ProxyClientException instance.
        
        :param error: The Error instance.
        """
        super().__init__(str(error))
        self.error = error

class ProxyClientResult:
    """
    Native Python class for ProxyClientResult.
    """
    def __init__(self, proxy_result_ptr: CProxyClientResult):
        """
        Initialize a ProxyClientResult instance from a CProxyClientResult instance.
        
        :param proxy_result_ptr: The CProxyClientResult instance.
        """
        self.errors = []
        self.successful = []
        
        for i in range(proxy_result_ptr.contents.numErrors):
            c_error = proxy_result_ptr.contents.errors[i]
            error = Error(c_error)
            self.errors.append(error)

        for i in range(proxy_result_ptr.contents.numSuccessful):
            c_proxy = proxy_result_ptr.contents.successful[i]
            proxy = Proxy.from_c_proxy(c_proxy)
            self.successful.append(proxy)

class LibStreamrProxyClient:
    """
    The class that wraps the C library.
    """
    def __enter__(self):
        """
        Load the dynamic library and initialize the library.
        
        :return: The LibStreamrProxyClient instance.
        """
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
        """
        Cleanup the library.
        """
        self.lib.proxyClientCleanupLibrary()

class ProxyClient:
    """
    The class that represents a ProxyClient.
    """
    def __init__(self, lib: LibStreamrProxyClient, ownEthereumAddress: str, streamPartId: str):
        """
        Initialize a ProxyClient instance.
        
        :param lib: The LibStreamrProxyClient instance.
        :param ownEthereumAddress: The Ethereum address of the client.
        :param streamPartId: The stream part ID.
        """
        self.lib = lib.lib
        self.ownEthereumAddress = ownEthereumAddress
        self.streamPartId = streamPartId
        
    def __enter__(self):
        """
        Create a new ProxyClient instance.
        
        :return: The ProxyClient instance.
        """
        result = ctypes.POINTER(CProxyClientResult)()
        self.clientHandle = self.lib.proxyClientNew(ctypes.byref(result), self.ownEthereumAddress.encode('utf-8'), self.streamPartId.encode('utf-8'))
        if result.contents.numErrors > 0:
            raise ProxyClientException(Error(result.contents.errors[0]))
        self.lib.proxyClientResultDelete(result)
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Delete the ProxyClient instance.
        """
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.proxyClientDelete(ctypes.byref(result), self.clientHandle)
        assert result.contents.numErrors == 0
        self.lib.proxyClientResultDelete(result)

    def connect(self, proxies: list[Proxy]) -> ProxyClientResult:
        """
        Connect the ProxyClient to the given proxies.
        
        :param proxies: The list of Proxy instances.
        :return: The ProxyClientResult instance.
        """
        proxy_array = (CProxy * len(proxies))(*[CProxy(proxy.websocket_url.encode('utf-8'), proxy.ethereum_address.encode('utf-8')) for proxy in proxies])
        num_proxies = ctypes.c_uint64(len(proxies))
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.proxyClientConnect(ctypes.byref(result), self.clientHandle, proxy_array, num_proxies)
        res = ProxyClientResult(result)
        self.lib.proxyClientResultDelete(result)
        return res
    
    def publish(self, data: bytes, ethereumPrivateKey: str = None) -> ProxyClientResult:
        """
        Publish data using the ProxyClient.
        
        :param data: The data to be published.
        :param ethereumPrivateKey: The Ethereum private key.
        :return: The ProxyClientResult instance.
        """
        result = ctypes.POINTER(CProxyClientResult)()
        if ethereumPrivateKey:
            self.lib.proxyClientPublish(ctypes.byref(result), self.clientHandle, data, len(data), ethereumPrivateKey.encode('utf-8'))
        else:
            self.lib.proxyClientPublish(ctypes.byref(result), self.clientHandle, data, len(data), None)
        res = ProxyClientResult(result)
        self.lib.proxyClientResultDelete(result)
        return res
