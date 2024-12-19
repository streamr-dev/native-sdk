// The Swift Programming Language
// https://docs.swift.org/swift-book

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
    public let websocketUrl: String
    public let ethereumAddress: String
    
    public init(websocketUrl: String, ethereumAddress: String) {
        self.websocketUrl = websocketUrl
        self.ethereumAddress = ethereumAddress
    }
}

public struct StreamrProxyError {
    public let error: StreamrError
    public let proxy: StreamrProxyAddress
}

public struct StreamrProxyResult {
    public let numConnected: UInt64
    public let successful: [StreamrProxyAddress]
    public let failed: [StreamrProxyError]
}

public protocol StreamrProxyClientAPI: AnyObject {
    associatedtype CAPI: CProxyClientAPI
    
    var api: CAPI! { get set }
    var clientHandle: UInt64 { get set }
    init(ownEthereumAddress: String,
         streamPartId: String, api: CAPI?
    ) throws
    func initialize(ownEthereumAddress: String,
                    streamPartId: String
    ) throws
    func deinitialize()
    func createCProxyInstance(from swiftProxy: StreamrProxyAddress) -> CAPI.P
    func connect(proxies: [StreamrProxyAddress]) -> StreamrProxyResult
    func publish(content: String, ethereumPrivateKey: String
    ) -> StreamrProxyResult
    func convertToStreamrResult(_ proxyResult: UnsafeMutablePointer<CAPI.R>?,
                                numConnected: UInt64
    ) -> StreamrProxyResult
}

public extension StreamrProxyClientAPI {
    // Helper Functions
    private func withProxyResultPointer<T>(
        operation: (UnsafeMutablePointer<UnsafePointer<CAPI.R>?>) -> T
    ) -> (result: T, proxyResult: UnsafeMutablePointer<CAPI.R>?) {
        var mutableProxyResult: UnsafeMutablePointer<CAPI.R>?
        let proxyResultPtr = withUnsafeMutablePointer(to: &mutableProxyResult) { ptr in
            UnsafeMutablePointer<UnsafePointer<CAPI.R>?>(OpaquePointer(ptr))
        }
        let result = operation(proxyResultPtr)
        return (result, mutableProxyResult)
    }
    
    private func cleanupProxyResult(_ result: UnsafeMutablePointer<CAPI.R>?) {
        if let result = result {
            api.cProxyClientResultDelete(UnsafePointer(result))
        }
    }
    
    private func processSuccessful(_ result: UnsafeMutablePointer<CAPI.R>) -> [StreamrProxyAddress] {
        let successfulPtr = result.pointee.successful
        return (0..<Int(result.pointee.numSuccessful)).map { i in
            let proxy = successfulPtr!.advanced(by: i)
            return StreamrProxyAddress(
                websocketUrl: String(cString: proxy.pointee.websocketUrl),
                ethereumAddress: String(cString: proxy.pointee.ethereumAddress)
            )
        }
    }
    
    private func processErrors(_ result: UnsafeMutablePointer<CAPI.R>) -> [StreamrProxyError] {
        let errorsPtr = result.pointee.errors
        return (0..<Int(result.pointee.numErrors)).map { i in
            let error = errorsPtr!.advanced(by: i)
            return StreamrProxyError(
                error: createStreamrProxyError(
                    message: String(cString: error.pointee.message),
                    code: error.pointee.code
                ),
                proxy: StreamrProxyAddress(
                    websocketUrl: String(cString: error.pointee.proxy.pointee.websocketUrl),
                    ethereumAddress: String(cString: error.pointee.proxy.pointee.ethereumAddress)
                )
            )
        }
    }

    // Main Functions
    func initialize(ownEthereumAddress: String = "",
                   streamPartId: String = "") throws {
        let (handle, proxyResult) = withProxyResultPointer { proxyResultPtr in
            api.cProxyClientNew(proxyResultPtr,
                              ownEthereumAddress,
                              streamPartId)
        }
        
        defer { cleanupProxyResult(proxyResult) }
        self.clientHandle = handle
        
        if let result = proxyResult, result.pointee.numErrors > 0 {
            let error = result.pointee.errors[0]
            throw createStreamrProxyError(
                message: String(cString: error.message),
                code: error.code
            )
        }
    }
    
    func deinitialize() {
        let (_, proxyResult) = withProxyResultPointer { proxyResultPtr in
            api.cProxyClientDelete(proxyResultPtr, self.clientHandle)
        }
        cleanupProxyResult(proxyResult)
    }
    
    func createCProxyInstance(from swiftProxy: StreamrProxyAddress) -> CAPI.P {
        let websocketUrlCString = swiftProxy.websocketUrl.withCString { strdup($0) }
        let websocketUrlPointer = UnsafePointer(websocketUrlCString)
        let ethereumAddressCString = swiftProxy.ethereumAddress.withCString { strdup($0) }
        let ethereumAddressPointer = UnsafePointer(ethereumAddressCString)
        return CAPI.P(websocketUrl: websocketUrlPointer, ethereumAddress: ethereumAddressPointer)
    }
    
    func connect(proxies: [StreamrProxyAddress]) -> StreamrProxyResult {
        if proxies.isEmpty {
            return StreamrProxyResult(
                numConnected: 0,
                successful: [],
                failed: [StreamrProxyError(
                    error: .noProxiesDefined("No proxies defined"),
                    proxy: StreamrProxyAddress(websocketUrl: "", ethereumAddress: "")
                )]
            )
        }
        
        var cProxies = proxies.map { createCProxyInstance(from: $0) }
        
        let (numConnected, proxyResult) = withProxyResultPointer { proxyResultPtr in
            api.cProxyClientConnect(
                proxyResultPtr,
                self.clientHandle,
                &cProxies,
                UInt64(proxies.count)
            )
        }
        
        // Clean up C strings
        for proxy in cProxies {
            free(UnsafeMutablePointer(mutating: proxy.websocketUrl))
            free(UnsafeMutablePointer(mutating: proxy.ethereumAddress))
        }
        
        return convertToStreamrResult(proxyResult, numConnected: numConnected)
    }
    
    func publish(content: String, ethereumPrivateKey: String) -> StreamrProxyResult {
        let (numPublished, proxyResult) = withProxyResultPointer { proxyResultPtr in
            api.cProxyClientPublish(
                proxyResultPtr,
                self.clientHandle,
                content,
                UInt64(content.count),
                ethereumPrivateKey
            )
        }
        
        return convertToStreamrResult(proxyResult, numConnected: numPublished)
    }
    
    func convertToStreamrResult(
        _ proxyResult: UnsafeMutablePointer<CAPI.R>?,
        numConnected: UInt64
    ) -> StreamrProxyResult {
        defer { cleanupProxyResult(proxyResult) }
        
        var successful: [StreamrProxyAddress] = []
        var failed: [StreamrProxyError] = []
        
        if let result = proxyResult {
            successful = processSuccessful(result)
            failed = processErrors(result)
        }
        
        return StreamrProxyResult(
            numConnected: numConnected,
            successful: successful,
            failed: failed
        )
    }
}
