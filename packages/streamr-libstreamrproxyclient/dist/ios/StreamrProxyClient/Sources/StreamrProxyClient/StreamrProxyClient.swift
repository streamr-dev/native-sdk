// The Swift Programming Language
// https://docs.swift.org/swift-book

import streamr
import Foundation

public enum StreamrError: LocalizedError {
    case initializationError(String)
    case connectionError(String)
    case publishError(String)
    case conversionError(String)
    
    public var errorDescription: String? {
        switch self {
        case .initializationError(let message):
            return "Initialization error: \(message)"
        case .connectionError(let message):
            return "Connection error: \(message)"
        case .publishError(let message):
            return "Publish error: \(message)"
        case .conversionError(let message):
            return "Conversion error: \(message)"
        }
    }
}

public enum StreamrProxyErrorCode {
    case invalidEthereumAddress
    case invalidStreamPartId
    case invalidProxyUrl
    case proxyClientNotFound
    case noProxiesDefined
    case proxyConnectionFailed
    case unknownError
}

public struct StreamrProxyAddress {
    let websocketUrl: String
    let ethereumAddress: String
    
    public init(websocketUrl: String, ethereumAddress: String) {
        self.websocketUrl = websocketUrl
        self.ethereumAddress = ethereumAddress
    }
}

public struct StreamrProxyError {
    let message: String
    let code: StreamrProxyErrorCode
    let proxy: StreamrProxyAddress
}

public struct StreamrProxyResult {
    let numConnected: UInt64
    let successful: [StreamrProxyAddress]
    let failed: [StreamrProxyError]
}

public class StreamrProxyClient {
    private let clientHandle: UInt64
    
    public init(ownEthereumAddress: String, streamPartId: String) throws {
        // Create a mutable pointer that we'll cast appropriately
        var mutableProxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(OpaquePointer(ptr))
        }
        
        self.clientHandle = proxyClientNew(proxyResultPtr,
                                           ownEthereumAddress,
                                           streamPartId)
        
        // Handle initialization errors
        if let result = mutableProxyResult, result.pointee.numErrors > 0 {
            let errorMessage = "Failed to initialize StreamrProxyClient"
            proxyClientResultDelete(UnsafePointer(mutableProxyResult))
            throw StreamrError.initializationError(errorMessage)
        }
        proxyClientResultDelete(UnsafePointer(mutableProxyResult))
    }
    
    deinit {
        var mutableProxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(OpaquePointer(ptr))
        }
        proxyClientDelete(proxyResultPtr, self.clientHandle)
        if let result = mutableProxyResult {
            proxyClientResultDelete(UnsafePointer(result))
        }
    }
    
    public func connect(proxies: [StreamrProxyAddress]) -> StreamrProxyResult {
        // Handle empty proxy list case first
        if proxies.isEmpty {
            return StreamrProxyResult(
                numConnected: 0,
                successful: [],
                failed: [StreamrProxyError(
                    message: "No proxies defined",
                    code: .noProxiesDefined,
                    proxy: StreamrProxyAddress(websocketUrl: "", ethereumAddress: "")
                )]
            )
        }
        
        // Convert Swift proxies to C structure
        var cProxies = proxies.map { proxy -> streamr.Proxy in
            var cProxy = streamr.Proxy()
            // Create C strings and keep them alive for the duration of the call
            let websocketUrlCString = proxy.websocketUrl.withCString { strdup($0) }
            let ethereumAddressCString = proxy.ethereumAddress.withCString { strdup($0) }
            
            // Convert the mutable pointers to immutable ones
            cProxy.websocketUrl = UnsafePointer(websocketUrlCString)
            cProxy.ethereumAddress = UnsafePointer(ethereumAddressCString)
            
            return cProxy
        }
        
        // Set up the proxy result pointer
        var mutableProxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(OpaquePointer(ptr))
        }
        
        // Make the connection call
        let numConnected = proxyClientConnect(
            proxyResultPtr,
            self.clientHandle,
            &cProxies,
            UInt64(proxies.count)
        )
        
        // Clean up C strings
        for proxy in cProxies {
            free(UnsafeMutablePointer(mutating: proxy.websocketUrl))
            free(UnsafeMutablePointer(mutating: proxy.ethereumAddress))
        }
        
        // Convert the result
        let result = convertToStreamrResult(mutableProxyResult, numConnected: numConnected)
        
        return result
    }
    
    public func publish(content: String, ethereumPrivateKey: String) -> StreamrProxyResult {
        var mutableProxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(OpaquePointer(ptr))
        }
        
        let numPublished = proxyClientPublish(proxyResultPtr,
                                              self.clientHandle,
                                              content,
                                              UInt64(content.count),
                                              ethereumPrivateKey)
        
        return convertToStreamrResult(mutableProxyResult, numConnected: numPublished)
    }
    
    private func convertToStreamrResult(_ proxyResult: UnsafeMutablePointer<streamr.ProxyResult>?,
                                        numConnected: UInt64) -> StreamrProxyResult {
        defer {
            if let result = proxyResult {
                proxyClientResultDelete(UnsafePointer(result))
            }
        }
        
        // Implementation remains the same
        return StreamrProxyResult(numConnected: numConnected,
                                  successful: [],
                                  failed: [])
    }
}
