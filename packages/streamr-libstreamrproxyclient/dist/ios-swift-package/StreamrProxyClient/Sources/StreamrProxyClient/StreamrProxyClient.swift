//
//  StreamrProxyClient.swift
//  StreamrProxyClient
//
//  Created by Santtu Rantanen on 10.11.2024.
//
import ProxyClientAPI

public class StreamrProxyClient: StreamrProxyClientAPI {
    public var api: DefaultProxyClientAPI!
    public var clientHandle: UInt64 = 0
  
    public required init(ownEthereumAddress: String, streamPartId: String, api: DefaultProxyClientAPI? = nil) throws {
        self.api = DefaultProxyClientAPI()
        try self.initialize(
            ownEthereumAddress: ownEthereumAddress,
            streamPartId: streamPartId
        )
    }
    
    deinit {
        deinitialize()
    }

}
