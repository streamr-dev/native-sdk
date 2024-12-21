//
//  StreamrProxyClientActor.swift
//  StreamrProxyClient
//
//  Created by Santtu Rantanen on 21.12.2024.
//
import ProxyClientAPI
import StreamrProxyClient

public actor StreamrProxyClientActor {
    let client: StreamrProxyClient
    
    public init(ownEthereumAddress: String, streamPartId: String) throws {
        self.client = try StreamrProxyClient(
            ownEthereumAddress: ownEthereumAddress,
            streamPartId: streamPartId
        )
    }
    
    public init(client: StreamrProxyClient) {
        self.client = client
    }
    
    public func connect(proxies: [StreamrProxyAddress]) -> StreamrProxyResult {
        client.connect(proxies: proxies)
    }
    
    public func publish(content: String, ethereumPrivateKey: String) -> StreamrProxyResult {
        client.publish(content: content, ethereumPrivateKey: ethereumPrivateKey)
    }
}
