
public protocol ProxyData {
    var websocketUrl: UnsafePointer<CChar>! { get set }
    var ethereumAddress: UnsafePointer<CChar>! { get set }
  
    init(websocketUrl: UnsafePointer<CChar>!, ethereumAddress: UnsafePointer<CChar>!)
}

public protocol ErrorData {
    associatedtype P: ProxyData
    var message: UnsafePointer<CChar>! { get set }
    var code: UnsafePointer<CChar>! { get set }
    var proxy: UnsafePointer<P>! { get set }
    init(message: UnsafePointer<CChar>!, code: UnsafePointer<CChar>!, proxy: UnsafePointer<P>!)
}

public protocol ResultData {
    associatedtype P: ProxyData
    associatedtype E: ErrorData
    
    init(errors: UnsafeMutablePointer<E>!, numErrors: UInt64, successful: UnsafeMutablePointer<P>!, numSuccessful: UInt64)
    var errors: UnsafeMutablePointer<E>! { get }
    var numErrors: UInt64 { get }
    var successful: UnsafeMutablePointer<P>! { get }
    var numSuccessful: UInt64 { get }
}

public protocol CProxyClientAPI {
    associatedtype P: ProxyData
    associatedtype E: ErrorData
    associatedtype R: ResultData
    
    func cProxyClientNew(
        _ result: UnsafeMutablePointer<UnsafePointer<R>?>,
        _ ownEthereumAddress: String,
        _ streamPartId: String
    ) -> UInt64
    
    func cProxyClientDelete(
        _ result: UnsafeMutablePointer<UnsafePointer<R>?>,
        _ handle: UInt64
    )
    
    func cProxyClientConnect(
        _ result: UnsafeMutablePointer<UnsafePointer<R>?>,
        _ handle: UInt64,
        _ proxies: UnsafeMutablePointer<P>,
        _ numProxies: UInt64
    ) -> UInt64
    
    func cProxyClientPublish(
        _ result: UnsafeMutablePointer<UnsafePointer<R>?>,
        _ handle: UInt64,
        _ content: String,
        _ contentLength: UInt64,
        _ ethereumPrivateKey: String?
    ) -> UInt64
    
    func cProxyClientResultDelete(_ result: UnsafePointer<R>)
}
