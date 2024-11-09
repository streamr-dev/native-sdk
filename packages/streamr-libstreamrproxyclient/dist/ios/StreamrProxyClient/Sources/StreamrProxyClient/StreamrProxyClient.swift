// The Swift Programming Language
// https://docs.swift.org/swift-book

import streamr
import Foundation

public enum StreamrError: LocalizedError, Equatable {
    case invalidEthereumAddress(String = "")
    case invalidStreamPartId(String = "")
    case invalidProxyUrl(String = "")
    case noProxiesDefined(String = "")
    case proxyConnectionFailed(String = "")
    case unknownError(String = "")
    
    public var errorDescription: String? {
        switch self {
        case .invalidEthereumAddress(let message):
            return "Invalid Ethereum address: \(message)"
        case .invalidStreamPartId(let message):
            return "Invalid stream part ID: \(message)"
        case .invalidProxyUrl(let message):
            return "Invalid proxy URL: \(message)"
        case .noProxiesDefined(let message):
            return "No proxies defined: \(message)"
        case .proxyConnectionFailed(let message):
            return "Proxy connection failed: \(message)"
        case .unknownError(let message):
            return "Unknown error: \(message)"
        }
    }
    
    public static func == (
        lhs: StreamrError,
        rhs: StreamrError
    ) -> Bool {
        switch (lhs, rhs) {
        case (.invalidEthereumAddress, .invalidEthereumAddress):
            return true
        case (.invalidStreamPartId, .invalidStreamPartId):
            return true
        case (.invalidProxyUrl, .invalidProxyUrl):
            return true
        case (.noProxiesDefined, .noProxiesDefined):
            return true
        case (.proxyConnectionFailed, .proxyConnectionFailed):
            return true
        case (.unknownError, .unknownError):
            return true
        default:
            return false
        }
    }
    
}

private func createStreamrProxyError(
    message: String,
    code: UnsafePointer<CChar>?
) -> StreamrError {
    guard let code = code else {
        return .unknownError(message)
    }
    
    let errorCode = String(cString: code)
    
    switch errorCode {
    case "INVALID_ETHEREUM_ADDRESS":
        return .invalidEthereumAddress(message)
    case "INVALID_STREAM_PART_ID":
        return .invalidStreamPartId(message)
    case "INVALID_PROXY_URL":
        return .invalidProxyUrl(message)
    case "NO_PROXIES_DEFINED":
        return .noProxiesDefined(message)
    case "PROXY_CONNECTION_FAILED":
        return .proxyConnectionFailed(message)
    default:
        return .unknownError(message)
    }
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
    let error: StreamrError
    let proxy: StreamrProxyAddress
}

public struct StreamrProxyResult {
    let numConnected: UInt64
    let successful: [StreamrProxyAddress]
    let failed: [StreamrProxyError]
}

public class StreamrProxyClient {
    private let clientHandle: UInt64
    
    public init(
        ownEthereumAddress: String,
        streamPartId: String
    ) throws {
        // Prepare proxy result pointer
        var proxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &proxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(
                OpaquePointer(ptr)
            )
        }
        
        // Initialize client
        self.clientHandle = proxyClientNew(
            proxyResultPtr,
            ownEthereumAddress,
            streamPartId
        )
        
        
        // Ensure cleanup
        defer {
            if let result = proxyResult {
                proxyClientResultDelete(UnsafePointer(result))
            }
        }
        
        // Handle errors if any
        if let result = proxyResult, result.pointee.numErrors > 0 {
            let error = result.pointee.errors[0]
            let errorMessage = String(cString: error.message)
            let errorCode = String(cString: error.code)
            
            throw createStreamrProxyError(
                message: errorMessage,
                code: errorCode
            )
        }
    }
    
    deinit {
        // Prepare proxy result pointer
        var mutableProxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(
                OpaquePointer(ptr)
            )
        }
        
        // Delete client
        proxyClientDelete(
            proxyResultPtr,
            self.clientHandle
        )
        
        // Cleanup result if needed
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
                    error: StreamrError.noProxiesDefined("No proxies defined"), proxy: StreamrProxyAddress(websocketUrl: "", ethereumAddress: "")
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
    
    public func publish(
        content: String,
        ethereumPrivateKey: String
    ) -> StreamrProxyResult {
        // Prepare proxy result pointer
        var mutableProxyResult: UnsafeMutablePointer<streamr.ProxyResult>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<streamr.ProxyResult>?>(
                OpaquePointer(ptr)
            )
        }
        
        // Publish content
        let numPublished = proxyClientPublish(
            proxyResultPtr,
            self.clientHandle,
            content,
            UInt64(content.count),
            ethereumPrivateKey
        )
        
        // Convert and return result
        return convertToStreamrResult(
            mutableProxyResult,
            numConnected: numPublished
        )
    }
    
    private func convertToStreamrResult(
        _ proxyResult: UnsafeMutablePointer<streamr.ProxyResult>?,
        numConnected: UInt64
    ) -> StreamrProxyResult {
        defer {
            if let result = proxyResult {
                proxyClientResultDelete(UnsafePointer(result))
            }
        }
        
        var successful: [StreamrProxyAddress] = []
        var failed: [StreamrProxyError] = []
        
        if let result = proxyResult {
            // Handle successful operations
            if let successfulPtr = result.pointee.successful {
                for i in 0..<Int(result.pointee.numSuccessful) {
                    let proxy = successfulPtr.advanced(by: i)
                    successful.append(
                        StreamrProxyAddress(
                            websocketUrl: String(cString: proxy.pointee.websocketUrl),
                            ethereumAddress: String(cString: proxy.pointee.ethereumAddress)
                        )
                    )
                }
            }
            
            // Handle errors
            if let errorsPtr = result.pointee.errors {
                for i in 0..<Int(result.pointee.numErrors) {
                    let error = errorsPtr.advanced(by: i)
                    let errorMessage = String(cString: error.pointee.message)
                    let errorCode = String(cString: error.pointee.code)
                    
                    failed.append(
                        StreamrProxyError(
                            error: createStreamrProxyError(
                                message: errorMessage,
                                code: errorCode
                            ),
                            proxy: StreamrProxyAddress(
                                websocketUrl: String(cString: error.pointee.proxy.pointee.websocketUrl),
                                ethereumAddress: String(cString: error.pointee.proxy.pointee.ethereumAddress)
                            )
                        )
                    )
                }
            }
        }
        
        return StreamrProxyResult(
            numConnected: numConnected,
            successful: successful,
            failed: failed
        )
    }
}
