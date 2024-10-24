import ctypes
import time

# Load the shared library
import platform

if platform.system() == "Darwin":
    lib = ctypes.CDLL('libstreamrproxyclient.dylib')
else:
    lib = ctypes.CDLL('libstreamrproxyclient.so')

# Define the necessary ctypes
class Error(ctypes.Structure):
    pass

class Proxy(ctypes.Structure):
    _fields_ = [("websocketUrl", ctypes.c_char_p),
                ("ethereumAddress", ctypes.c_char_p)]

# Function prototypes
lib.proxyClientNew.restype = ctypes.c_uint64
lib.proxyClientNew.argtypes = [ctypes.POINTER(ctypes.POINTER(Error)), ctypes.POINTER(ctypes.c_uint64), ctypes.c_char_p, ctypes.c_char_p]

lib.proxyClientConnect.argtypes = [ctypes.POINTER(ctypes.POINTER(Error)), ctypes.POINTER(ctypes.c_uint64), ctypes.c_uint64, ctypes.POINTER(Proxy), ctypes.c_uint64]

lib.proxyClientPublish.restype = ctypes.c_uint64
lib.proxyClientPublish.argtypes = [ctypes.POINTER(ctypes.POINTER(Error)), ctypes.POINTER(ctypes.c_uint64), ctypes.c_uint64, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]

lib.proxyClientDelete.argtypes = [ctypes.POINTER(ctypes.POINTER(Error)), ctypes.POINTER(ctypes.c_uint64), ctypes.c_uint64]

# Constants
proxyUrl = b"ws://95.216.15.80:44211"
proxyServerEthereumAddress = b"0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
streamPartId = b"0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
ownEthereumAddress = b"0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5"
ethereumPrivateKey = b"23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"

# Initialize error and numErrors
errors = ctypes.POINTER(Error)()
numErrors = ctypes.c_uint64(0)

# Create client handle
clientHandle = lib.proxyClientNew(ctypes.byref(errors), ctypes.byref(numErrors), ownEthereumAddress, streamPartId)
assert numErrors.value == 0
assert not errors

# Create proxy
proxy = Proxy(websocketUrl=proxyUrl, ethereumAddress=proxyServerEthereumAddress)

# Connect to proxy
lib.proxyClientConnect(ctypes.byref(errors), ctypes.byref(numErrors), clientHandle, ctypes.byref(proxy), 1)
assert numErrors.value == 0
assert not errors

message = b"Hello from libstreamrproxyclient!"

while True:
    print("Publishing message")
    numProxiesPublishedTo = lib.proxyClientPublish(ctypes.byref(errors), ctypes.byref(numErrors), clientHandle, message, len(message), ethereumPrivateKey)
    assert numErrors.value == 0
    assert not errors

    print(f"{ownEthereumAddress.decode()} published message \"{message.decode()}\" to {numProxiesPublishedTo} proxies")
    print("Sleeping for 15 seconds")
    time.sleep(15)
    print("Sleeping done")

# Delete client handle
lib.proxyClientDelete(ctypes.byref(errors), ctypes.byref(numErrors), clientHandle)
assert numErrors.value == 0
assert not errors
