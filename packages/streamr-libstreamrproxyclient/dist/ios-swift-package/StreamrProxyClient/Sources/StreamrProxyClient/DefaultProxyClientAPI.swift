//
//  DefaultProxyClientAPI.swift
//  StreamrProxyClient
//
//  Created by Santtu Rantanen on 10.11.2024.
//

import streamr
import ProxyClientAPI

extension streamr.Proxy: ProxyData {}
extension streamr.Error: ErrorData {}
extension streamr.ProxyResult: ResultData {}

public class DefaultProxyClientAPI: CProxyClientAPI {
    public typealias P = streamr.Proxy
    public typealias E = streamr.Error
    public typealias R = streamr.ProxyResult
     
    public func cProxyClientNew(_ result: UnsafeMutablePointer<UnsafePointer<ProxyResult>?>, _ ownEthereumAddress: String, _ streamPartId: String) -> UInt64 {
        proxyClientNew(result, ownEthereumAddress, streamPartId)
    }
    
    public func cProxyClientDelete(_ result: UnsafeMutablePointer<UnsafePointer<ProxyResult>?>, _ handle: UInt64) {
        proxyClientDelete(result, handle)
    }
    
    public func cProxyClientConnect(_ result: UnsafeMutablePointer<UnsafePointer<ProxyResult>?>, _ handle: UInt64, _ proxies: UnsafeMutablePointer<Proxy>, _ numProxies: UInt64) -> UInt64 {
        proxyClientConnect(result, handle, proxies, numProxies)
    }
    
    public func cProxyClientPublish(_ result: UnsafeMutablePointer<UnsafePointer<ProxyResult>?>, _ handle: UInt64, _ content: String, _ contentLength: UInt64, _ ethereumPrivateKey: String?) -> UInt64 {
        proxyClientPublish(result, handle, content, contentLength, ethereumPrivateKey)
    }
    
    public func cProxyClientResultDelete(_ result: UnsafePointer<ProxyResult>) {
        proxyClientResultDelete(result)
    }
    
    public func initLibrary() {
        
    }
    
    public func cleanupLibrary() {
        
    }
    
}
