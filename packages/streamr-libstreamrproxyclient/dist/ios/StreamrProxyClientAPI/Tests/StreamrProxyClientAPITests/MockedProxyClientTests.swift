import Testing
@testable import StreamrProxyClientAPI

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
    var api: CAPI!
    var clientHandle: UInt64 = 0
    
    required init(ownEthereumAddress: String, streamPartId: String, api: MockCProxyClientAPI) throws {
        self.clientHandle = 0
        try self.initialize(
            ownEthereumAddress: ownEthereumAddress,
            streamPartId: streamPartId,
            api: api
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




/*
@Suite("StreamrProxyClient Tests")
struct StreamrProxyClientTests {
    // Test constants
  //  let testConstants = TestConstants()
    
    // Helper functions
    func verifyFailed(result: StreamrProxyResult, expectedError: StreamrError) {
        #expect(result.numConnected == 0)
        #expect(result.successful.isEmpty)
        #expect(result.failed.count == 1)
        #expect(result.failed[0].error == expectedError)
    }
    
    func createMockClient(
        mockAPI: MockCProxyClientAPI = MockCProxyClientAPI(),
        shouldSucceed: Bool = true,
        errorCode: String? = nil
    ) throws -> (TestStreamrProxyClient, MockCProxyClientAPI) {
        let client = try TestStreamrProxyClient(ownEthereumAddress: TestConstants.validEthereumAddress, streamPartId: TestConstants.validStreamPartId, api: mockAPI)
        mockAPI.shouldSucceed = shouldSucceed
        if let code = errorCode {
            mockAPI.mockError = "Error"
            mockAPI.mockErrorCode = code
        }
        return (client, mockAPI)
    }
    
    @Test("Success")
    func testInvalidEthereumAddress() async throws {
        let mockAPI = MockCProxyClientAPI()
        mockAPI.shouldSucceed = false
        mockAPI.mockError = "Invalid address"
        mockAPI.mockErrorCode = "INVALID_ETHEREUM_ADDRESS"
        
        #expect(throws: StreamrError.invalidEthereumAddress()) {
            try TestStreamrProxyClient(ownEthereumAddress: TestConstants.invalidEthereumAddress, streamPartId: TestConstants.validStreamPartId, api: mockAPI)
        }
    }
    
    
    @Test("Invalid Ethereum address throws error")
    func testInvalidEthereumAddress() async throws {
        let mockAPI = MockCProxyClientAPI()
        mockAPI.shouldSucceed = false
        mockAPI.mockError = "Invalid address"
        mockAPI.mockErrorCode = "INVALID_ETHEREUM_ADDRESS"
        
        #expect(throws: StreamrError.invalidEthereumAddress()) {
            try TestStreamrProxyClient(ownEthereumAddress: TestConstants.invalidEthereumAddress, streamPartId: TestConstants.validStreamPartId, api: mockAPI)
        }
    }
 */
    /*
    @Test("Invalid stream part ID throws error")
    func testInvalidStreamPartId() async throws {
        let mockAPI = MockCProxyClientAPI()
        mockAPI.shouldSucceed = false
        mockAPI.mockError = "Invalid stream ID"
        mockAPI.mockErrorCode = "INVALID_STREAM_PART_ID"
        
        let client = TestStreamrProxyClient()
        
        await #expect(throws: StreamrError.invalidStreamPartId()) {
            try client.initialize(
                ownEthereumAddress: testConstants.validEthereumAddress,
                streamPartId: testConstants.invalidStreamPartId,
                api: mockAPI
            )
        }
    }
    
    @Test("Invalid proxy URL returns error")
    func testInvalidProxyUrl() async throws {
        let (client, _) = try createMockClient(
            shouldSucceed: false,
            errorCode: "INVALID_PROXY_URL"
        )
        
        let result = client.connect(proxies: [
            StreamrProxyAddress(
                websocketUrl: testConstants.invalidProxyUrl,
                ethereumAddress: testConstants.validEthereumAddress
            )
        ])
        
        verifyFailed(result: result, expectedError: .invalidProxyUrl())
    }
    
    @Test("No proxies defined returns error")
    func testNoProxiesDefined() async throws {
        let (client, _) = try createMockClient()
        let result = client.connect(proxies: [])
        verifyFailed(result: result, expectedError: .noProxiesDefined())
    }
    
    @Test("Connect and publish successfully")
    func testConnectAndPublishSuccessfully() async throws {
        // Given
        let mockAPI = MockCProxyClientAPI()
        let (client, _) = try createMockClient(mockAPI: mockAPI)
        
        // When - Connect
        let connectResult = client.connect(proxies: [
            StreamrProxyAddress(
                websocketUrl: testConstants.proxyWebsocketUrl,
                ethereumAddress: testConstants.proxyEthereumAddress
            )
        ])
        
        // Then - Connect
        #expect(connectResult.numConnected == 1)
        #expect(!connectResult.successful.isEmpty)
        #expect(connectResult.failed.isEmpty)
        
        // When - Publish
        let publishResult = client.publish(
            content: "test message",
            ethereumPrivateKey: testConstants.ethereumPrivateKey
        )
        
        
        // Then - Publish
        #expect(publishResult.failed.isEmpty)
        #expect(publishResult.numConnected > 0)
    }
    
    // ... more tests ...
}

// Mock implementation
class TestStreamrProxyClient: StreamrProxyClient {
    required init(ownEthereumAddress: String, streamPartId: String, api: MockCProxyClientAPI) throws {
        self.clientHandle = 0
        try self.initialize(
            ownEthereumAddress: ownEthereumAddress,
            streamPartId: streamPartId,
            api: api
        )
    }
    
    typealias CAPI = MockCProxyClientAPI
    var api: CAPI!
    var clientHandle: UInt64 = 0
}




*/


//
//  MockedProxyClientTests.swift
//  StreamrProxyClient
//
//  Created by Santtu Rantanen on 10.11.2024.
//
//import Testing
//@testable import ProxyClientAPI
/*
import Testing
@testable import ProxyClientAPI

@Suite("StreamrProxyClient Tests")
struct StreamrProxyClientTests {
    @Test("Initialize client successfully")
    func testInitialize() async throws {
        // Given
        let mockAPI = MockCProxyClientAPI()
        mockAPI.shouldSucceed = true
        let client = try TestStreamrProxyClient(ownEthereumAddress: "ddd", streamPartId: "ddd", api: mockAPI)
      //  #expect(mockAPI.newClientCalled)
       // #expect(client.clientHandle == mockAPI.mockHandle)
 */
        /*
        
        // When
        try client.initialize(
            ownEthereumAddress: "0x123",
            streamPartId: "stream/1",
            api: mockAPI
        )
        
        // Then
        #expect(mockAPI.newClientCalled)
        #expect(client.clientHandle == mockAPI.mockHandle)
        
        // Cleanup
        client.deinitialize()
         */
  //  }
    /*
    @Test("Initialize client with invalid address throws error")
    func testInitializeFailure() async throws {
        // Given
        let mockAPI = MockCProxyClientAPI()
        let client = TestStreamrProxyClient()
        mockAPI.shouldSucceed = false
        mockAPI.mockError = "Invalid address"
        mockAPI.mockErrorCode = "INVALID_ETHEREUM_ADDRESS"
        
        // When/Then
        await #expect(throws: StreamrError.invalidEthereumAddress("Invalid address")) {
            try client.initialize(
                ownEthereumAddress: "invalid",
                streamPartId: "stream/1",
                api: mockAPI
            )
        }
        
        // Cleanup
        client.deinitialize()
    }
    
    @Test("Connect to proxies successfully")
    func testConnect() async throws {
        // Given
        let mockAPI = MockCProxyClientAPI()
        let client = TestStreamrProxyClient()
        try client.initialize(
            ownEthereumAddress: "0x123",
            streamPartId: "stream/1",
            api: mockAPI
        )
        
        let proxies = [
            StreamrProxyAddress(websocketUrl: "ws://1", ethereumAddress: "0x1"),
            StreamrProxyAddress(websocketUrl: "ws://2", ethereumAddress: "0x2")
        ]
        mockAPI.shouldSucceed = true
        
        // When
        let result = client.connect(proxies: proxies)
        
        // Then
        #expect(mockAPI.connectCalled)
        #expect(result.numConnected == 2)
        #expect(result.successful.isEmpty)
        #expect(result.failed.isEmpty)
        
        // Cleanup
        client.deinitialize()
    }
    
    @Test("Publish message successfully")
    func testPublish() async throws {
        // Given
        let mockAPI = MockCProxyClientAPI()
        let client = TestStreamrProxyClient()
        try client.initialize(
            ownEthereumAddress: "0x123",
            streamPartId: "stream/1",
            api: mockAPI
        )
        
        let content = "test message"
        let privateKey = "private_key"
        mockAPI.shouldSucceed = true
        
        // When
        let result = client.publish(
            content: content,
            ethereumPrivateKey: privateKey
        )
        
        // Then
        #expect(mockAPI.publishCalled)
        #expect(mockAPI.lastContent == content)
        #expect(mockAPI.lastPrivateKey == privateKey)
        #expect(result.numConnected == 1)
        
        // Cleanup
        client.deinitialize()
    }
    
    @Test("Connect fails with no proxies")
    func testConnectWithNoProxies() async throws {
        // Given
        let mockAPI = MockCProxyClientAPI()
        let client = TestStreamrProxyClient()
        try client.initialize(
            ownEthereumAddress: "0x123",
            streamPartId: "stream/1",
            api: mockAPI
        )
        
        // When
        let result = client.connect(proxies: [])
        
        // Then
        #expect(result.numConnected == 0)
        #expect(result.failed.count == 1)
        #expect(result.failed[0].error == .noProxiesDefined("No proxies defined"))
        
        // Cleanup
        client.deinitialize()
    }
     */
//}
/*
// Test helper types remain the same
class TestStreamrProxyClient: StreamrProxyClient {
    
    var api: CAPI!
    var clientHandle: UInt64 = 0
  
    required init(ownEthereumAddress: String, streamPartId: String, api: MockCProxyClientAPI) throws {
        try initialize(ownEthereumAddress: ownEthereumAddress, streamPartId: streamPartId, api: api)
    }
    
    deinit {
        deinitialize()
    }
 */
  /*
    func createCProxyInstance(from swiftProxy: ProxyClientAPI.StreamrProxyAddress) -> MockProxy {
        
    }
    
    func convertToStreamrResult(_ proxyResult: UnsafeMutablePointer<MockResult>?, numConnected: UInt64) -> ProxyClientAPI.StreamrProxyResult {
        
    }
    */
//}


/*
import Testing
@testable import ProxyClientAPI

struct MockProxy {
    var websocketUrl: UnsafePointer<CChar>
    var ethereumAddress: UnsafePointer<CChar>
}

struct MockError {
    var message: UnsafePointer<CChar>
    var code: UnsafePointer<CChar>
    var proxy: UnsafePointer<MockProxy>?
}

struct MockResult {
    var errors: UnsafePointer<MockError>?
    var numErrors: UInt64
    var successful: UnsafePointer<MockProxy>?
    var numSuccessful: UInt64
}
*/
/*
class DummyProxyData: ProxyData {
    var websocketUrl: UnsafePointer<CChar>
    var ethereumAddress: UnsafePointer<CChar>
    
    
    required init(websocketUrl: UnsafePointer<CChar>!, ethereumAddress: UnsafePointer<CChar>!) {
        self.websocketUrl = websocketUrl
        self.ethereumAddress = ethereumAddress
    }
     
}

class DummyErrorData: ErrorData {
    var proxy: UnsafePointer<DummyProxyData>
    var message: UnsafePointer<CChar>
    var code: UnsafePointer<CChar>
    
    required init(message: UnsafePointer<CChar>!, code: UnsafePointer<CChar>!, proxy: UnsafePointer<DummyProxyData>!) {
        self.message = message
        self.code = code
        self.proxy = proxy
    }
    
}

class DummyResultData: ResultData {
    var errors: UnsafeMutablePointer<DummyErrorData>
    var successful: UnsafeMutablePointer<DummyProxyData>
    var numErrors: UInt64
    var numSuccessful: UInt64
    
    required init(errors: UnsafeMutablePointer<DummyErrorData>!, numErrors: UInt64, successful: UnsafeMutablePointer<DummyProxyData>!, numSuccessful: UInt64) {
        self.errors = errors
        self.successful = successful
        self.numErrors = numErrors
        self.numSuccessful = numSuccessful
    }
}
 */
/*
class DummyAPI: CProxyClientAPI {
    typealias P = DummyProxyData
    typealias E = DummyErrorData
    typealias R = DummyResultData
    
    func proxyClientNew(_ result: UnsafeMutablePointer<UnsafePointer<DummyResultData>?>, _ ownEthereumAddress: String, _ streamPartId: String) -> UInt64 {
        
    }
    
    func proxyClientDelete(_ result: UnsafeMutablePointer<UnsafePointer<DummyResultData>?>, _ handle: UInt64) {
        
    }
    
    func proxyClientConnect(_ result: UnsafeMutablePointer<UnsafePointer<DummyResultData>?>, _ handle: UInt64, _ proxies: UnsafeMutablePointer<DummyProxyData>, _ numProxies: UInt64) -> UInt64 {
        
    }
    
    func proxyClientPublish(_ result: UnsafeMutablePointer<UnsafePointer<DummyResultData>?>, _ handle: UInt64, _ content: String, _ contentLength: UInt64, _ ethereumPrivateKey: String?) -> UInt64 {
        
    }
    
    func proxyClientResultDelete(_ result: UnsafePointer<DummyResultData>) {
        
    }
   
    func initLibrary() {
        
    }
    
    func cleanupLibrary() {
        
    }
    
    
}



class StreamrProxyClientTest: StreamrProxyClient {
    func createCProxyInstance(from swiftProxy: ProxyClientAPI.StreamrProxyAddress) -> DummyProxyData {
        
    }
    
    func initialize(ownEthereumAddress: String, streamPartId: String, api: DummyAPI) throws {
        
    }
    
    func deinitialize() {
        
    }
    
    func connect(proxies: [StreamrProxyAddress]) -> StreamrProxyResult {
        
    }
    
    func publish(content: String, ethereumPrivateKey: String) -> StreamrProxyResult {
        
    }
    
    func convertToStreamrResult(_ proxyResult: UnsafeMutablePointer<DummyResultData>?, numConnected: UInt64) -> ProxyClientAPI.StreamrProxyResult {
        
    }
    
    var api: DummyAPI!
    
    typealias CAPI = DummyAPI
    
    var clientHandle: UInt64 = 0
    
    

}

*/
/*

@Test func example() async throws {
    
    let v = DummyProxyData(websocketUrl: "", ethereumAddress: "")
    
    
}
*/
/*
import XCTest
@testable import ProxyClientAPI

final class ProxyClientAPITests: XCTestCase {
    
    
    func testInitLibrary() {
       
    }
    
    func testNewClientSuccess() {
       // mockAPI.shouldSucceed = true
       // mockAPI.mockHandle = 42
        
    }
 */
//import XCTest
//@testable import ProxyClientAPI

//final class MockedProxyClientTests: XCTestCase {
    
    
    
    /*
    var mockAPI: MockProxyClientAPI!
    var client: StreamrProxyClient?
    
    // Keep the same constants as in your original tests
    let validEthereumAddress = "0x1234567890123456789012345678901234567890"
    let validStreamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    let invalidEthereumAddress = "invalid_address"
    let invalidStreamPartId = "invalid_stream_id"
    // ... other constants ...
    
    override func setUp() {
        super.setUp()
        mockAPI = MockProxyClientAPI()
    }
*/

/*
 import XCTest
 @testable import StreamrProxyClient
 
 final class MockedProxyClientTests: XCTestCase {
 var mockAPI: MockProxyClientAPI!
 var client: StreamrProxyClient?
 
 // Keep the same constants as in your original tests
 let validEthereumAddress = "0x1234567890123456789012345678901234567890"
 let validStreamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
 let invalidEthereumAddress = "invalid_address"
 let invalidStreamPartId = "invalid_stream_id"
 // ... other constants ...
 
 override func setUp() {
 super.setUp()
 mockAPI = MockProxyClientAPI()
 }
 
 func testInvalidEthereumAddress() throws {
 mockAPI.shouldSucceedNew = false
 mockAPI.errorToReturn = .invalidEthereumAddress()
 
 XCTAssertThrowsError(try StreamrProxyClient(
 ownEthereumAddress: invalidEthereumAddress,
 streamPartId: validStreamPartId,
 api: mockAPI
 )) { error in
 guard let streamrError = error as? StreamrError else {
 XCTFail("Expected StreamrError")
 return
 }
 XCTAssertEqual(streamrError, .invalidEthereumAddress())
 }
 
 XCTAssertEqual(mockAPI.newCallCount, 1)
 }
 
 func testConnectAndPublishSuccessfully() throws {
 mockAPI.shouldSucceedNew = true
 mockAPI.shouldSucceedConnect = true
 mockAPI.shouldSucceedPublish = true
 
 client = try StreamrProxyClient(
 ownEthereumAddress: validEthereumAddress,
 streamPartId: validStreamPartId,
 api: mockAPI
 )
 
 let proxies = [
 StreamrProxyAddress(
 websocketUrl: "ws://valid.proxy.url",
 ethereumAddress: validEthereumAddress
 )
 ]
 
 let result = client!.connect(proxies: proxies)
 
 XCTAssertEqual(result.numConnected, 1)
 XCTAssertTrue(result.failed.isEmpty)
 XCTAssertEqual(mockAPI.connectCallCount, 1)
 
 let publishResult = client!.publish(
 content: "test message",
 ethereumPrivateKey: "privateKey"
 )
 
 XCTAssertTrue(publishResult.failed.isEmpty)
 XCTAssertEqual(mockAPI.publishCallCount, 1)
 }
 
 func testProxyConnectionFailed() throws {
 mockAPI.shouldSucceedNew = true
 mockAPI.shouldSucceedConnect = false
 mockAPI.errorToReturn = .proxyConnectionFailed()
 
 client = try StreamrProxyClient(
 ownEthereumAddress: validEthereumAddress,
 streamPartId: validStreamPartId,
 api: mockAPI
 )
 
 let proxies = [
 StreamrProxyAddress(
 websocketUrl: "ws://invalid.proxy.url",
 ethereumAddress: validEthereumAddress
 )
 ]
 let result = client!.connect(proxies: proxies)
 
 XCTAssertEqual(result.numConnected, 0)
 XCTAssertEqual(result.failed.count, 1)
 XCTAssertEqual(result.failed[0].error, .proxyConnectionFailed())
 XCTAssertEqual(mockAPI.connectCallCount, 1)
 }
 
 // Add more test cases following the same pattern...
 }
 
 */

//import Testing
//@testable import StreamrProxyClient

/*
@Suite("MockedProxyClientTests")
struct MockedProxyClientTests {
    @Test("Invalid ethereum address should throw appropriate error")
    func testInvalidEthereumAddress() async throws {
        let mockAPI = MockProxyClientAPI()
        mockAPI.shouldSucceedNew = false
        mockAPI.errorToReturn = .invalidEthereumAddress()
        
        do {
            _ = try StreamrProxyClient(
                ownEthereumAddress: "invalid_address",
                streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0",
                api: mockAPI
            )
          //  fail("Expected error was not thrown")
        } catch let error as StreamrError {
            #expect(error == .invalidEthereumAddress())
        }
        
        #expect(mockAPI.newCallCount == 1)
    }
    
    @Test("Successfully connect and publish")
    func testConnectAndPublishSuccessfully() async throws {
        let mockAPI = MockProxyClientAPI()
        mockAPI.shouldSucceedNew = true
        mockAPI.shouldSucceedConnect = true
        mockAPI.shouldSucceedPublish = true
        
        let client = try StreamrProxyClient(
            ownEthereumAddress: "0x1234567890123456789012345678901234567890",
            streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0",
            api: mockAPI
        )
        
        let proxies = [
            StreamrProxyAddress(
                websocketUrl: "ws://valid.proxy.url",
                ethereumAddress: "0x1234567890123456789012345678901234567890"
            )
        ]
        
        let result = client.connect(proxies: proxies)
        
        #expect(result.numConnected == 1)
        #expect(result.failed.isEmpty)
        #expect(mockAPI.connectCallCount == 1)
        
        let publishResult = client.publish(
            content: "test message",
            ethereumPrivateKey: "privateKey"
        )
        
        #expect(publishResult.failed.isEmpty)
        #expect(mockAPI.publishCallCount == 1)
    }
    
    @Test("Proxy connection should fail with appropriate error")
    func testProxyConnectionFailed() async throws {
        let mockAPI = MockProxyClientAPI()
        mockAPI.shouldSucceedNew = true
        mockAPI.shouldSucceedConnect = false
        mockAPI.errorToReturn = .proxyConnectionFailed()
        
        let client = try StreamrProxyClient(
            ownEthereumAddress: "0x1234567890123456789012345678901234567890",
            streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0",
            api: mockAPI
        )
        
        let proxies = [
            StreamrProxyAddress(
                websocketUrl: "ws://invalid.proxy.url",
                ethereumAddress: "0x1234567890123456789012345678901234567890"
            )
        ]
        
        let result = client.connect(proxies: proxies)
        
        #expect(result.numConnected == 0)
        #expect(result.failed.count == 1)
        #expect(result.failed[0].error == .proxyConnectionFailed())
        #expect(mockAPI.connectCallCount == 1)
    }
    
    @Test("Multiple proxy connections should fail appropriately")
    func testMultipleProxyConnectionsFailed() async throws {
        let mockAPI = MockProxyClientAPI()
        mockAPI.shouldSucceedNew = true
        mockAPI.shouldSucceedConnect = false
        mockAPI.errorToReturn = .proxyConnectionFailed()
        
        let client = try StreamrProxyClient(
            ownEthereumAddress: "0x1234567890123456789012345678901234567890",
            streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0",
            api: mockAPI
        )
        
        let proxies = [
            StreamrProxyAddress(
                websocketUrl: "ws://non.existent.proxy0.url",
                ethereumAddress: "0x1234567890123456789012345678901234567890"
            ),
            StreamrProxyAddress(
                websocketUrl: "ws://non.existent.proxy1.url",
                ethereumAddress: "0x2234567890123456789012345678901234567890"
            ),
            StreamrProxyAddress(
                websocketUrl: "ws://non.existent.proxy2.url",
                ethereumAddress: "0x3234567890123456789012345678901234567890"
            )
        ]
        
        let result = client.connect(proxies: proxies)
        
        #expect(result.numConnected == 0)
        #expect(result.successful.isEmpty)
        #expect(result.failed.count == 3)
        #expect(mockAPI.connectCallCount == 1)
        
        // Verify that errors match the original proxies
        for (index, failure) in result.failed.enumerated() {
            #expect(failure.proxy.websocketUrl == proxies[index].websocketUrl + ":80")
            #expect(failure.error == .proxyConnectionFailed())
        }
    }
    
    @Test("Publish without connection should fail")
    func testPublishWithoutConnection() async throws {
        let mockAPI = MockProxyClientAPI()
        mockAPI.shouldSucceedNew = true
        
        let client = try StreamrProxyClient(
            ownEthereumAddress: "0x1234567890123456789012345678901234567890",
            streamPartId: "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0",
            api: mockAPI
        )
        
        let publishResult = client.publish(
            content: "test message",
            ethereumPrivateKey: "privateKey"
        )
        
        #expect(publishResult.numConnected == 0)
    }
}

*/
