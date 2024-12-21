//
//  StreamrProxyClientActorTests.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 21.12.2024.
//

import XCTest
@testable import StreamrProxyClientActor
@testable import ProxyClientAPI

final class StreamrProxyClientActorTests: XCTestCase {
    let proxyWebsocketUrl =
    "ws://95.216.15.80:44211";
    let proxyEthereumAddress =
    "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f";
    let validStreamPartId2 =
    "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    let validEthereumAddress = "0x1234567890123456789012345678901234567890"
    let validStreamPartId =
    "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0"
    let invalidEthereumAddress = "invalid_address"
    let invalidStreamPartId = "invalid_stream_id"
    let invalidProxyUrl = "invalid_url"
    let validProxyUrl = "ws://valid.proxy.url"
    let nonExistentProxyUrl0 = "ws://non.existent.proxy0.url"
    let nonExistentProxyUrl1 = "ws://non.existent.proxy1.url"
    let nonExistentProxyUrl2 = "ws://non.existent.proxy2.url"
    let validEthereumAddress2 = "0x2234567890123456789012345678901234567890"
    let validEthereumAddress3 = "0x3234567890123456789012345678901234567890"
    let ethereumPrivateKey = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
    var client: StreamrProxyClientActor?
    
    override func setUpWithError() throws {
        // Put setup code here. This method is called before the invocation of each test method in the class.
    }
    
    override func tearDownWithError() throws {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
    }
    
    private func verifyFailed(
        result: StreamrProxyResult,
        expectedError: StreamrError
    ) {
        // Basic result validation
        XCTAssertEqual(result.numConnected, 0)
        XCTAssertTrue(result.successful.isEmpty)
        XCTAssertEqual(result.failed.count, 1)
        
        // Error validation
        let actualError = result.failed[0]
        XCTAssertEqual(actualError.error, expectedError)
    }
    
    private func createProxyArrayFromProxy(
        websocketUrl: String,
        ethereumAddress: String
    ) -> [StreamrProxyAddress] {
        [
            StreamrProxyAddress(
                websocketUrl: websocketUrl,
                ethereumAddress: ethereumAddress
            )
        ]
    }
    
    private func createClientAndConnect(
        websocketUrl: String? = nil,
        ethereumAddress: String? = nil
    ) async throws -> StreamrProxyResult {
        // Create client
        self.client = try StreamrProxyClientActor(
            ownEthereumAddress: validEthereumAddress,
            streamPartId: validStreamPartId
        )
        
        // Create and connect proxies
        let proxies = websocketUrl == nil ? [] : createProxyArrayFromProxy(
            websocketUrl: websocketUrl!,
            ethereumAddress: ethereumAddress!
        )
        
        return await self.client!.connect(proxies: proxies)
    }
        
    private func createClientConnectAndVerify(
        websocketUrl: String? = nil,
        ethereumAddress: String? = nil,
        expectedError: StreamrError
    ) async throws {
        let result = try await createClientAndConnect(
            websocketUrl: websocketUrl,
            ethereumAddress: ethereumAddress
        )
        
        verifyFailed(
            result: result,
            expectedError: expectedError
        )
    }
    
    private func tryToCreateClientWhichFails(
        ownEthereumAddress: String,
        streamPartId: String,
        expectedError: StreamrError
    ) {
        XCTAssertThrowsError(
            try StreamrProxyClientActor(
                ownEthereumAddress: ownEthereumAddress,
                streamPartId: streamPartId
            )
        ) { error in
            guard let streamrError = error as? StreamrError else {
                XCTFail("Expected StreamrError, got \(type(of: error))")
                return
            }
            XCTAssertEqual(streamrError, expectedError)
        }
    }
    
    func testInvalidEthereumAddress() throws {
        tryToCreateClientWhichFails(
            ownEthereumAddress: invalidEthereumAddress,
            streamPartId: validStreamPartId,
            expectedError: .invalidEthereumAddress()
        )
    }

    func testInvalidStreamPartId() throws {
        tryToCreateClientWhichFails(
            ownEthereumAddress: validEthereumAddress,
            streamPartId: invalidStreamPartId,
            expectedError: .invalidStreamPartId()
        )
    }
  
    func testInvalidProxyUrl() async throws {
        try await createClientConnectAndVerify(
            websocketUrl: invalidProxyUrl,
            ethereumAddress: validEthereumAddress,
            expectedError: .invalidProxyUrl()
        )
    }
    
    func testNoProxiesDefined() async throws {
        try await createClientConnectAndVerify(expectedError: .noProxiesDefined())
    }
    
    func testInvalidProxyEthereumAddress() async throws {
        try await createClientConnectAndVerify(
            websocketUrl: validProxyUrl,
            ethereumAddress: invalidEthereumAddress,
            expectedError: .invalidEthereumAddress()
        )
    }
    
    func testProxyConnectionFailed() async throws {
        try await createClientConnectAndVerify(
            websocketUrl: nonExistentProxyUrl0,
            ethereumAddress: validEthereumAddress,
            expectedError: .proxyConnectionFailed()
        )
    }
    
    func testThreeProxyConnectionsFailed() async throws {
        let client = try StreamrProxyClientActor(
            ownEthereumAddress: validEthereumAddress,
            streamPartId: validStreamPartId
        )
        
        let proxies = [
            StreamrProxyAddress(
                websocketUrl: nonExistentProxyUrl0,
                ethereumAddress: validEthereumAddress
            ),
            StreamrProxyAddress(
                websocketUrl: nonExistentProxyUrl1,
                ethereumAddress: validEthereumAddress2
            ),
            StreamrProxyAddress(
                websocketUrl: nonExistentProxyUrl2,
                ethereumAddress: validEthereumAddress3
            )
        ]
        
        let result = await client.connect(proxies: proxies)
        
        XCTAssertEqual(result.numConnected, 0)
        XCTAssertTrue(result.successful.isEmpty)
        XCTAssertEqual(result.failed.count, 3)
        
        // Verify that errors match the original proxies
        XCTAssertEqual(result.failed[0].proxy.websocketUrl, proxies[0].websocketUrl + ":80")
        XCTAssertEqual(result.failed[1].proxy.websocketUrl, proxies[1].websocketUrl + ":80")
        XCTAssertEqual(result.failed[2].proxy.websocketUrl, proxies[2].websocketUrl + ":80")
        
        let actualError = result.failed[0]
        XCTAssertEqual(actualError.error, .proxyConnectionFailed())
    }
     
    func testConnectAndPublishSuccessfully() async throws {
        let result = try await createClientAndConnect(
            websocketUrl: proxyWebsocketUrl,
            ethereumAddress: proxyEthereumAddress
        )
        
        // Verify results
        XCTAssertEqual(result.numConnected, 1)
        XCTAssertFalse(result.successful.isEmpty)
        XCTAssertEqual(result.failed.count, 0)
        
        // Verify proxy details
        guard let successfulProxy = result.successful.first else {
            XCTFail("Expected successful proxy not found")
            return
        }
        XCTAssertEqual(successfulProxy.websocketUrl, proxyWebsocketUrl)
        XCTAssertEqual(successfulProxy.ethereumAddress, proxyEthereumAddress)
        
        let testMessage = "test message"
        
        // Add this to your test function after the connection test
        let publishResult = await client!.publish(
            content: testMessage,
            ethereumPrivateKey: ethereumPrivateKey
        )

        // Verify publish results
        XCTAssertTrue(publishResult.failed.isEmpty)
        XCTAssertTrue(publishResult.numConnected != 0)
    }
    
    func testPublishWithoutConnection() async throws {
        self.client = try StreamrProxyClientActor(
            ownEthereumAddress: validEthereumAddress,
            streamPartId: validStreamPartId
        )
        
        let testMessage = "test message"

        // Add this to your test function after the connection test
        let publishResult = await client!.publish(
            content: testMessage,
            ethereumPrivateKey: ethereumPrivateKey
        )

        // Verify publish results
        XCTAssertTrue(publishResult.numConnected == 0)
    }
}
