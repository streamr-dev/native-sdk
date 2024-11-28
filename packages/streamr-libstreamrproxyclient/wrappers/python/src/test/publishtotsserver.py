from streamrproxyclient.libstreamrproxyclient import (
    Proxy,
    LibStreamrProxyClient,
    ProxyClient
)

ts_ethereum_address = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
ts_proxy_url = "ws://127.0.0.1:44211"
ts_stream_part_id = "0xa000000000000000000000000000000000000000#01"
own_ethereum_address = "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5"
ethereum_private_key = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
 
        
with LibStreamrProxyClient() as lib:
    with ProxyClient(lib, own_ethereum_address, ts_stream_part_id) as client:
        result = client.connect([Proxy(ts_proxy_url, ts_ethereum_address)])
        assert len(result.errors) == 0
        assert len(result.successful) == 1
        
        result = client.publish(b"Hello from python!", ethereum_private_key)
        assert len(result.errors) == 0
        assert len(result.successful) == 1
