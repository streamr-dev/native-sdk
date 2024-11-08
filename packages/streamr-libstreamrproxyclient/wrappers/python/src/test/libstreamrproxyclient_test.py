from streamrproxyclient.libstreamrproxyclient import (
    Proxy,
    LibStreamrProxyClient,
    ProxyClient,
    ProxyClientException,
    ErrorCodes
)

invalid_ethereum_address = "INVALID_ETHEREUM_ADDRESS"
good_ethereum_address = "0x123456789012345678901234567890123456789a"
valid_ethereum_address = "0x1234567890123456789012345678901234567890"
valid_ethereum_address2 = "0x1234567890123456789012345678901234567892"
valid_ethereum_address3 = "0x1234567890123456789012345678901234567893"
invalid_stream_part_id = "INVALID_STREAM_PART_ID"
valid_stream_part_id = "0xa000000000000000000000000000000000000000#01"
own_ethereum_address = "0x1234567890123456789012345678901234567890"
stream_part_id = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
invalid_proxy_url = "poiejrg039utg240"
valid_proxy_url = "ws://valid.com"
non_existent_proxy_url_0 = "ws://localhost:0"
non_existent_proxy_url_1 = "ws://localhost:1"
non_existent_proxy_url_2 = "ws://localhost:2"

invalid_client_handle = 0

def test_proxyClientNew():
    with LibStreamrProxyClient() as lib:        
        with ProxyClient(lib, own_ethereum_address, stream_part_id) as client:
            assert client.clientHandle != 0

def test_invalid_ethereum_address():
    with LibStreamrProxyClient() as lib:
        try:
            with ProxyClient(lib, invalid_ethereum_address, valid_stream_part_id) as client:
                pass  # Should not reach here
        except ProxyClientException as e:
            assert e.error.code == ErrorCodes.ERROR_INVALID_ETHEREUM_ADDRESS

def test_invalid_stream_part_id():
    with LibStreamrProxyClient() as lib:
        try:
            with ProxyClient(lib, valid_ethereum_address, invalid_stream_part_id) as client:
                pass  # Should not reach here
        except ProxyClientException as e:
            assert e.error.code == ErrorCodes.ERROR_INVALID_STREAM_PART_ID

def test_no_proxies_defined():
    with LibStreamrProxyClient() as lib:
        with ProxyClient(lib, valid_ethereum_address, valid_stream_part_id) as client:
            result = client.connect([])
            assert len(result.errors) == 1
            assert result.errors[0].code == ErrorCodes.ERROR_NO_PROXIES_DEFINED

def test_invalid_proxy_url():
    with LibStreamrProxyClient() as lib:
        with ProxyClient(lib, valid_ethereum_address, valid_stream_part_id) as client:
            result = client.connect([Proxy(invalid_proxy_url, valid_ethereum_address)])
            assert len(result.errors) == 1
            assert result.errors[0].code == ErrorCodes.ERROR_INVALID_PROXY_URL
            assert result.errors[0].proxy.websocket_url == invalid_proxy_url

def test_invalid_proxy_ethereum_address():
    with LibStreamrProxyClient() as lib:
        with ProxyClient(lib, valid_ethereum_address, valid_stream_part_id) as client:
            result = client.connect([Proxy(valid_proxy_url, invalid_ethereum_address)])
            assert len(result.errors) == 1
            assert result.errors[0].code == ErrorCodes.ERROR_INVALID_ETHEREUM_ADDRESS
            assert result.errors[0].proxy.websocket_url == valid_proxy_url
            assert result.errors[0].proxy.ethereum_address == invalid_ethereum_address

def test_proxy_connection_failed():
    with LibStreamrProxyClient() as lib:
        with ProxyClient(lib, valid_ethereum_address, valid_stream_part_id) as client:
            result = client.connect([Proxy(non_existent_proxy_url_0, valid_ethereum_address)])
            assert len(result.errors) == 1
            assert result.errors[0].code == ErrorCodes.ERROR_PROXY_CONNECTION_FAILED
            assert result.errors[0].proxy.websocket_url == non_existent_proxy_url_0
            assert result.errors[0].proxy.ethereum_address == valid_ethereum_address

def test_three_proxy_connections_failed():
    with LibStreamrProxyClient() as lib:
        with ProxyClient(lib, valid_ethereum_address, valid_stream_part_id) as client:
            result = client.connect([Proxy(non_existent_proxy_url_0, valid_ethereum_address), Proxy(non_existent_proxy_url_1, valid_ethereum_address2), Proxy(non_existent_proxy_url_2, valid_ethereum_address3)])
            assert len(result.errors) == 3
            assert result.errors[0].code == ErrorCodes.ERROR_PROXY_CONNECTION_FAILED
            assert result.errors[1].code == ErrorCodes.ERROR_PROXY_CONNECTION_FAILED
            assert result.errors[2].code == ErrorCodes.ERROR_PROXY_CONNECTION_FAILED
            assert result.errors[0].proxy.websocket_url == non_existent_proxy_url_0
            assert result.errors[1].proxy.websocket_url == non_existent_proxy_url_1
            assert result.errors[2].proxy.websocket_url == non_existent_proxy_url_2
            assert result.errors[0].proxy.ethereum_address == valid_ethereum_address
            assert result.errors[1].proxy.ethereum_address == valid_ethereum_address2
            assert result.errors[2].proxy.ethereum_address == valid_ethereum_address3
