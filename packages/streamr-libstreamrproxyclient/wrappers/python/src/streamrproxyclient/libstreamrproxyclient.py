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
        
        self.lib.streamrInitLibrary.restype = None
        
        self.lib.streamrCleanupLibrary.restype = None

        self.lib.streamrResultDelete.argtypes = [ctypes.POINTER(CProxyClientResult)]
        self.lib.streamrResultDelete.restype = None

        self.lib.proxyClientNew.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_char_p, ctypes.c_char_p]
        self.lib.proxyClientNew.restype = ctypes.c_uint64

        self.lib.proxyClientDelete.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_uint64]
        self.lib.proxyClientDelete.restype = None

        self.lib.proxyClientConnect.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_uint64, ctypes.POINTER(CProxy), ctypes.c_uint64]
        self.lib.proxyClientConnect.restype = ctypes.c_uint64

        self.lib.proxyClientPublish.argtypes = [ctypes.POINTER(ctypes.POINTER(CProxyClientResult)), ctypes.c_uint64, ctypes.c_char_p, ctypes.c_uint64, ctypes.c_char_p]
        self.lib.proxyClientPublish.restype = ctypes.c_uint64
        
        self.lib.streamrInitLibrary()
        
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Cleanup the library.
        """
        self.lib.streamrCleanupLibrary()

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
        self.lib.streamrResultDelete(result)
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Delete the ProxyClient instance.
        """
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.proxyClientDelete(ctypes.byref(result), self.clientHandle)
        assert result.contents.numErrors == 0
        self.lib.streamrResultDelete(result)

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
        self.lib.streamrResultDelete(result)
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
        self.lib.streamrResultDelete(result)
        return res


# ---------------------------------------------------------------------------
# Full-node API (streamrnode.h) — phase D3b.
# ---------------------------------------------------------------------------

class NodeErrorCodes:
    """
    Error codes of the full-node API (streamrnode.h).
    """
    ERROR_NODE_NOT_FOUND = "NODE_NOT_FOUND"
    ERROR_NODE_NOT_STARTED = "NODE_NOT_STARTED"
    ERROR_NODE_ALREADY_STARTED = "NODE_ALREADY_STARTED"
    ERROR_NODE_STOPPED = "NODE_STOPPED"
    ERROR_INVALID_ENTRY_POINT_URL = "INVALID_ENTRY_POINT_URL"
    ERROR_SUBSCRIPTION_NOT_FOUND = "SUBSCRIPTION_NOT_FOUND"
    ERROR_NODE_OPERATION_FAILED = "NODE_OPERATION_FAILED"


class CStreamrNodeConfig(ctypes.Structure):
    """
    C struct for StreamrNodeConfig.
    """
    _fields_ = [("entryPoints", ctypes.POINTER(CProxy)),
                ("numEntryPoints", ctypes.c_uint64),
                ("websocketPort", ctypes.c_uint16),
                ("websocketHost", ctypes.c_char_p),
                ("acceptProxyConnections", ctypes.c_bool)]


# void (*StreamrNodeMessageCallback)(uint64_t nodeHandle,
#     const char* streamPartId, const char* content,
#     uint64_t contentLength, void* userData)
# content is opaque bytes (may contain NULs), so it is bound as a raw
# pointer and sliced with contentLength.
CMessageCallback = ctypes.CFUNCTYPE(
    None, ctypes.c_uint64, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char),
    ctypes.c_uint64, ctypes.c_void_p)


def _register_node_functions(lib):
    """
    Define the ctypes signatures of the streamrNode* functions.
    """
    result_out = ctypes.POINTER(ctypes.POINTER(CProxyClientResult))
    lib.streamrNodeNew.argtypes = [
        result_out, ctypes.c_char_p, ctypes.POINTER(CStreamrNodeConfig)]
    lib.streamrNodeNew.restype = ctypes.c_uint64
    lib.streamrNodeDelete.argtypes = [result_out, ctypes.c_uint64]
    lib.streamrNodeDelete.restype = None
    lib.streamrNodeStart.argtypes = [result_out, ctypes.c_uint64]
    lib.streamrNodeStart.restype = None
    lib.streamrNodeStop.argtypes = [result_out, ctypes.c_uint64]
    lib.streamrNodeStop.restype = None
    lib.streamrNodeJoinStreamPart.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_char_p,
        ctypes.POINTER(CProxy), ctypes.c_uint64]
    lib.streamrNodeJoinStreamPart.restype = None
    lib.streamrNodeLeaveStreamPart.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_char_p]
    lib.streamrNodeLeaveStreamPart.restype = None
    lib.streamrNodePublish.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.c_uint64, ctypes.c_char_p]
    lib.streamrNodePublish.restype = None
    lib.streamrNodeSubscribe.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_char_p, CMessageCallback,
        ctypes.c_void_p]
    lib.streamrNodeSubscribe.restype = ctypes.c_uint64
    lib.streamrNodeUnsubscribe.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_uint64]
    lib.streamrNodeUnsubscribe.restype = None
    lib.streamrNodeGetNeighborCount.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_char_p]
    lib.streamrNodeGetNeighborCount.restype = ctypes.c_uint64
    lib.streamrNodeSetProxies.argtypes = [
        result_out, ctypes.c_uint64, ctypes.c_char_p,
        ctypes.POINTER(CProxy), ctypes.c_uint64, ctypes.c_int,
        ctypes.c_uint64]
    lib.streamrNodeSetProxies.restype = None


class StreamrNode:
    """
    The full network node (context manager). Mirrors the C++
    StreamrNode wrapper: create with an optional entry-point list and
    websocket port, then start/join/publish/subscribe.
    """

    PROXY_DIRECTION_PUBLISH = 0
    PROXY_DIRECTION_SUBSCRIBE = 1

    def __init__(self, lib: LibStreamrProxyClient, own_ethereum_address: str,
                 entry_points: list[Proxy] = None, websocket_port: int = 0,
                 websocket_host: str = None,
                 accept_proxy_connections: bool = False):
        self.lib = lib.lib
        _register_node_functions(self.lib)
        self.own_ethereum_address = own_ethereum_address
        self.entry_points = entry_points or []
        self.websocket_port = websocket_port
        self.websocket_host = websocket_host
        self.accept_proxy_connections = accept_proxy_connections
        self.node_handle = 0
        # ctypes callback objects must outlive their subscriptions; the C
        # API may still dispatch a message whose delivery already started
        # when unsubscribe returns, so these are kept until node deletion.
        self._callbacks = []

    def _check(self, result):
        if result and result.contents.numErrors > 0:
            error = Error(result.contents.errors[0])
            self.lib.streamrResultDelete(result)
            raise ProxyClientException(error)
        self.lib.streamrResultDelete(result)

    def __enter__(self):
        entry_point_array = (CProxy * len(self.entry_points))(
            *[CProxy(ep.websocket_url.encode('utf-8'),
                     ep.ethereum_address.encode('utf-8'))
              for ep in self.entry_points])
        config = CStreamrNodeConfig(
            entryPoints=entry_point_array if self.entry_points else None,
            numEntryPoints=len(self.entry_points),
            websocketPort=self.websocket_port,
            websocketHost=self.websocket_host.encode('utf-8')
            if self.websocket_host else None,
            acceptProxyConnections=self.accept_proxy_connections)
        result = ctypes.POINTER(CProxyClientResult)()
        self.node_handle = self.lib.streamrNodeNew(
            ctypes.byref(result),
            self.own_ethereum_address.encode('utf-8'),
            ctypes.byref(config))
        self._check(result)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeDelete(ctypes.byref(result), self.node_handle)
        self.lib.streamrResultDelete(result)
        self._callbacks.clear()

    def start(self):
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeStart(ctypes.byref(result), self.node_handle)
        self._check(result)

    def stop(self):
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeStop(ctypes.byref(result), self.node_handle)
        self._check(result)

    def join_stream_part(self, stream_part_id: str,
                         entry_points: list[Proxy] = None):
        entry_points = entry_points or []
        entry_point_array = (CProxy * len(entry_points))(
            *[CProxy(ep.websocket_url.encode('utf-8'),
                     ep.ethereum_address.encode('utf-8'))
              for ep in entry_points])
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeJoinStreamPart(
            ctypes.byref(result), self.node_handle,
            stream_part_id.encode('utf-8'),
            entry_point_array if entry_points else None, len(entry_points))
        self._check(result)

    def leave_stream_part(self, stream_part_id: str):
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeLeaveStreamPart(
            ctypes.byref(result), self.node_handle,
            stream_part_id.encode('utf-8'))
        self._check(result)

    def publish(self, stream_part_id: str, data: bytes,
                ethereum_private_key: str = None):
        result = ctypes.POINTER(CProxyClientResult)()
        key = ethereum_private_key.encode('utf-8') \
            if ethereum_private_key else None
        self.lib.streamrNodePublish(
            ctypes.byref(result), self.node_handle,
            stream_part_id.encode('utf-8'), data, len(data), key)
        self._check(result)

    def subscribe(self, stream_part_id: str, callback) -> int:
        """
        Subscribe to a stream part. callback(stream_part_id: str,
        content: bytes) is invoked on an internal network thread: return
        quickly and do not call node methods from inside it.
        """
        def trampoline(_node_handle, c_stream_part_id, c_content,
                       content_length, _user_data):
            callback(c_stream_part_id.decode('utf-8'),
                     ctypes.string_at(c_content, content_length))
        c_callback = CMessageCallback(trampoline)
        result = ctypes.POINTER(CProxyClientResult)()
        subscription_handle = self.lib.streamrNodeSubscribe(
            ctypes.byref(result), self.node_handle,
            stream_part_id.encode('utf-8'), c_callback, None)
        self._check(result)
        self._callbacks.append(c_callback)
        return subscription_handle

    def unsubscribe(self, subscription_handle: int):
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeUnsubscribe(
            ctypes.byref(result), self.node_handle, subscription_handle)
        self._check(result)

    def neighbor_count(self, stream_part_id: str) -> int:
        result = ctypes.POINTER(CProxyClientResult)()
        count = self.lib.streamrNodeGetNeighborCount(
            ctypes.byref(result), self.node_handle,
            stream_part_id.encode('utf-8'))
        self._check(result)
        return count

    def set_proxies(self, stream_part_id: str, proxies: list[Proxy],
                    direction: int, connection_count: int = 0):
        proxy_array = (CProxy * len(proxies))(
            *[CProxy(p.websocket_url.encode('utf-8'),
                     p.ethereum_address.encode('utf-8')) for p in proxies])
        result = ctypes.POINTER(CProxyClientResult)()
        self.lib.streamrNodeSetProxies(
            ctypes.byref(result), self.node_handle,
            stream_part_id.encode('utf-8'),
            proxy_array if proxies else None, len(proxies), direction,
            connection_count)
        self._check(result)
