import Testing
@testable import StreamrProxyClientAPI
import Foundation


// First, create mock types that match the C API structure
public class MockProxy: ProxyData {
    public var websocketUrl: UnsafePointer<CChar>!
    public var ethereumAddress: UnsafePointer<CChar>!
    
    required public init(websocketUrl: UnsafePointer<CChar>!, ethereumAddress: UnsafePointer<CChar>!) {
        self.websocketUrl = websocketUrl
        self.ethereumAddress = ethereumAddress
    }
}

public class MockError: ErrorData {
    public var proxy: UnsafePointer<MockProxy>!
    public var message: UnsafePointer<CChar>!
    public var code: UnsafePointer<CChar>!
    
    required public init(message: UnsafePointer<CChar>!, code: UnsafePointer<CChar>!, proxy: UnsafePointer<MockProxy>!) {
        self.message = message
        self.code = code
        self.proxy = proxy
    }
    
}

public class MockResult: ResultData {
    public var errors: UnsafeMutablePointer<MockError>!
    public var successful: UnsafeMutablePointer<MockProxy>!
    public var numErrors: UInt64
    public var numSuccessful: UInt64
    
    required public init(errors: UnsafeMutablePointer<MockError>!, numErrors: UInt64, successful: UnsafeMutablePointer<MockProxy>!, numSuccessful: UInt64) {
        self.errors = errors
        self.successful = successful
        self.numErrors = numErrors
        self.numSuccessful = numSuccessful
    }
}

class MockCProxyClientAPI: CProxyClientAPI {
    var shouldSucceed = true
    var mockHandle: UInt64 = 123
    var mockError: String?
    var mockErrorCode: String?
    var mockConnectedCount: UInt64 = 1
    // Call tracking
    var newClientCalled = false
    var connectCalled = false
    var publishCalled = false
    var proxyClientResultDeleteCalled = false
    var proxyClientDeleteCalled = false
    var lastContent: String?
    var lastPrivateKey: String?
    public typealias P = MockProxy
    public typealias E = MockError
    public typealias R = MockResult
    
    func cProxyClientNew(
        _ result: UnsafeMutablePointer<UnsafePointer<R>?>,
        _ ownEthereumAddress: String,
        _ streamPartId: String
    ) -> UInt64 {
        newClientCalled = true
        createResult(result)
        return shouldSucceed ? mockHandle : 0
    }
    
    func cProxyClientDelete(_ result: UnsafeMutablePointer<UnsafePointer<MockResult>?>, _ handle: UInt64) {
        proxyClientDeleteCalled = true
    }
    
    func cProxyClientConnect(_ result: UnsafeMutablePointer<UnsafePointer<MockResult>?>, _ handle: UInt64, _ proxies: UnsafeMutablePointer<MockProxy>, _ numProxies: UInt64) -> UInt64 {
        connectCalled = true
        createResult(result)
        return 0
    }
    
    func cProxyClientPublish(_ result: UnsafeMutablePointer<UnsafePointer<MockResult>?>, _ handle: UInt64, _ content: String, _ contentLength: UInt64, _ ethereumPrivateKey: String?) -> UInt64 {
        publishCalled = true
        lastContent = content
        lastPrivateKey = ethereumPrivateKey
        createResult(result)
        return 1
    }
    
    func cProxyClientResultDelete(_ result: UnsafePointer<MockResult>) {
        proxyClientResultDeleteCalled = true
    }
    
    private func createEmptyProxyArray() -> UnsafeMutablePointer<MockProxy> {
        let emptyProxy = UnsafeMutablePointer<MockProxy>.allocate(capacity: 1)
        emptyProxy.initialize(to: MockProxy(
            websocketUrl: strdup(""),
            ethereumAddress: strdup("")
        ))
        return UnsafeMutablePointer(emptyProxy)
    }
    
    private func createEmptyErrorArray() -> UnsafeMutablePointer<MockError> {
        let emptyError = UnsafeMutablePointer<MockError>.allocate(capacity: 1)
        emptyError.initialize(to: MockError(
            message: strdup(""),
            code: strdup(""),
            proxy: createEmptyProxyArray()
        ))
        return UnsafeMutablePointer(emptyError)
    }
    
    private func createResult(_ result: UnsafeMutablePointer<UnsafePointer<R>?>) {
        if !shouldSucceed, let errorMessage = mockError, let errorCode = mockErrorCode {
            
            let error = MockError(
                message: strdup(errorMessage),
                code: strdup(errorCode),
                proxy: createEmptyProxyArray()
            )
            let errorsPtr = UnsafeMutablePointer<MockError>.allocate(capacity: 1)
            errorsPtr.initialize(to: error)
            
            let mockResult = MockResult(
                errors: UnsafeMutablePointer(errorsPtr),
                numErrors: 1,
                successful: createEmptyProxyArray(),
                numSuccessful: 0
            )
            let resultPtr = UnsafeMutablePointer<R>.allocate(capacity: 1)
            resultPtr.initialize(to: mockResult)
            result.pointee = UnsafePointer(resultPtr)
        } else {
            let mockResult = MockResult(
                errors: createEmptyErrorArray(),
                numErrors: 0,
                successful: createEmptyProxyArray(),
                numSuccessful: mockConnectedCount
            )
            print("FFF")
            let resultPtr = UnsafeMutablePointer<R>.allocate(capacity: 1)
            resultPtr.initialize(to: mockResult)
            result.pointee = UnsafePointer(resultPtr)
        }
        
    }
}
