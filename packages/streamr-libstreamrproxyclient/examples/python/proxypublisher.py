from streamrproxyclient.libstreamrproxyclient import (
    Proxy,
    LibStreamrProxyClient,
    ProxyClient
)

proxy_ethereum_address = "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
proxy_url = "ws://95.216.15.80:44211"
stream_part_id = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
own_ethereum_address = "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5"
ethereum_private_key = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
 
        
with LibStreamrProxyClient() as lib:
    with ProxyClient(lib, own_ethereum_address, stream_part_id) as client:
        result = client.connect([Proxy(proxy_url, proxy_ethereum_address)])
        assert len(result.errors) == 0
        assert len(result.successful) == 1
        
        result = client.publish(b"Hello from python!", ethereum_private_key)
        assert len(result.errors) == 0
        assert len(result.successful) == 1
