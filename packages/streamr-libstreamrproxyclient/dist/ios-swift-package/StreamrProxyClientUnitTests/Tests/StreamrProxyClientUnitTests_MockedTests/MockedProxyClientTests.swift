import Testing
@testable import ProxyClientAPI

// Test constants
enum TestConstants {
    static let proxyWebsocketUrl = "ws://95.216.15.80:44211"
    static let proxyEthereumAddress = "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"
    static let validStreamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    static let validEthereumAddress = "0x1234567890123456789012345678901234567890"
    static let validEthereumAddress2 = "0x2234567890123456789012345678901234567890"
    static let validEthereumAddress3 = "0x3234567890123456789012345678901234567890"
    static let invalidEthereumAddress = "invalid_address"
    static let invalidStreamPartId = "invalid_stream_id"
    static let invalidProxyUrl = "invalid_url"
    static let nonExistentProxyUrl0 = "ws://non.existent.proxy0.url"
    static let nonExistentProxyUrl1 = "ws://non.existent.proxy1.url"
    static let nonExistentProxyUrl2 = "ws://non.existent.proxy2.url"
    static let ethereumPrivateKey = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
    static let testMessage = "test message"
}

// Mock implementation
class TestStreamrProxyClient: StreamrProxyClientAPI {    
    var api: MockCProxyClientAPI!
    var clientHandle: UInt64 = 0
    
    required init(ownEthereumAddress: String, streamPartId: String, api: MockCProxyClientAPI?) throws {
        self.api = api
        self.clientHandle = 0
        try self.initialize(
            ownEthereumAddress: ownEthereumAddress,
            streamPartId: streamPartId
        )
    }
    
    deinit {
        deinitialize()
    }

}

private func createClient() throws -> TestStreamrProxyClient {
       let mockAPI = MockCProxyClientAPI()
       mockAPI.shouldSucceed = true
       return try TestStreamrProxyClient(
           ownEthereumAddress: TestConstants.validEthereumAddress,
           streamPartId: TestConstants.validStreamPartId,
           api: mockAPI
       )
       
   }

@Test("Initialize with valid parameters succeeds")
func testValidInitialization() async throws {
    let mockAPI = MockCProxyClientAPI()
    mockAPI.shouldSucceed = true
    
    let client = try TestStreamrProxyClient(
        ownEthereumAddress: TestConstants.validEthereumAddress,
        streamPartId: TestConstants.validStreamPartId,
        api: mockAPI
    )
    print("mockAPI.newClientCalled: \(mockAPI.newClientCalled)")
    #expect(mockAPI.newClientCalled)
    #expect(mockAPI.proxyClientResultDeleteCalled)
    #expect(client.clientHandle == mockAPI.mockHandle)
}

@Test("Deinitialize succeeds")
func testValidDeinitialization() async throws {
    let mockAPI = MockCProxyClientAPI()
    mockAPI.shouldSucceed = true
    var client: TestStreamrProxyClient? = try TestStreamrProxyClient(
        ownEthereumAddress: TestConstants.validEthereumAddress,
        streamPartId: TestConstants.validStreamPartId,
        api: mockAPI
    )
    #expect(mockAPI.newClientCalled)
    #expect(client?.clientHandle == mockAPI.mockHandle)
    client = nil
    #expect(mockAPI.proxyClientDeleteCalled)
    #expect(mockAPI.proxyClientResultDeleteCalled)
}

@Test("Initialize with invalid Ethereum address throws error")
func testInvalidEthereumAddress() async throws {
    let mockAPI = MockCProxyClientAPI()
    mockAPI.shouldSucceed = false
    mockAPI.mockError = "Invalid address"
    mockAPI.mockErrorCode = "INVALID_ETHEREUM_ADDRESS"
    #expect(throws: StreamrError.invalidEthereumAddress("Invalid address")) {
        _ = try TestStreamrProxyClient(
            ownEthereumAddress: TestConstants.invalidEthereumAddress,
            streamPartId: TestConstants.validStreamPartId,
            api: mockAPI
        )
    }
    #expect(mockAPI.proxyClientResultDeleteCalled)
}

@Test("Initialize with invalid stream part ID throws error")
func testInvalidStreamPartId() async throws {
    let mockAPI = MockCProxyClientAPI()
    mockAPI.shouldSucceed = false
    mockAPI.mockError = "Invalid stream ID"
    mockAPI.mockErrorCode = "INVALID_STREAM_PART_ID"
    
    #expect(throws: StreamrError.invalidStreamPartId("Invalid stream ID")) {
        _ = try TestStreamrProxyClient(
            ownEthereumAddress: TestConstants.validEthereumAddress,
            streamPartId: TestConstants.invalidStreamPartId,
            api: mockAPI
        )
    }
    #expect(mockAPI.proxyClientResultDeleteCalled)
}

@Test("Connect with no proxies returns error")
func testConnectNoProxies() async throws {
    let client = try createClient()
    let result = client.connect(proxies: [])
    #expect(result.numConnected == 0)
    #expect(result.successful.isEmpty)
    #expect(result.failed.count == 1)
    #expect(result.failed[0].error == .noProxiesDefined("No proxies defined"))
}

@Test("Connect with invalid proxy URL returns error")
func testConnectInvalidProxyUrl() async throws {
    let client = try createClient()
    let mockAPI = client.api!
    mockAPI.shouldSucceed = false
    mockAPI.mockError = "Invalid URL"
    mockAPI.mockErrorCode = "INVALID_PROXY_URL"
    
    let result = client.connect(proxies: [
        StreamrProxyAddress(
            websocketUrl: TestConstants.invalidProxyUrl,
            ethereumAddress: TestConstants.validEthereumAddress
        )
    ])
    #expect(mockAPI.connectCalled)
    #expect(result.failed[0].error == .invalidProxyUrl("Invalid URL"))
}

@Test("Publish successfully with connection")
func testSuccessfulPublish() async throws {
    let client = try createClient()
    let mockAPI = client.api!
    
    // First connect
    _ = client.connect(proxies: [
        StreamrProxyAddress(
            websocketUrl: TestConstants.proxyWebsocketUrl,
            ethereumAddress: TestConstants.proxyEthereumAddress
        )
    ])
    
    // Then publish
    let result = client.publish(
        content: TestConstants.testMessage,
        ethereumPrivateKey: TestConstants.ethereumPrivateKey
    )
    
    #expect(result.numConnected == 1)
    #expect(mockAPI.connectCalled)
    #expect(mockAPI.publishCalled)
    #expect(mockAPI.lastContent == TestConstants.testMessage)
    #expect(mockAPI.lastPrivateKey == TestConstants.ethereumPrivateKey)
}




