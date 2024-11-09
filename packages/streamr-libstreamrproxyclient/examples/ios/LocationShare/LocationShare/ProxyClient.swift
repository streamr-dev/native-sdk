//
//  ProxyClient.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import Foundation
import Combine
import StreamrProxyClient

@Observable
class ProxyClient {
    @MainActor var proxyInfo: PeerDesc = PeerDesc(peerId: "ws://127.0.0.1:44211", peerAddress: "0xca487535a280b0b2c47e99334372a373f963a8c4")
    @MainActor var ethereumPrivateKey = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
    @MainActor var publishingIntervalInSeconds: TimeInterval = defaultPublishingIntervalInSeconds
    var status: Status = .stopped
    private let locationManager: LocationManager
    private let streamrProxyClient = try! StreamrProxyClient(ownEthereumAddress: "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5", streamPartId: "0xd7278f1e4a946fa7838b5d1e0fe50c5725fb23de/nativesdktest#01")
        //"0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0")
    static let defaultPublishingIntervalInSeconds: TimeInterval = 5
    @ObservationIgnored private var pCallroxyClientHandle: UInt64 = 0
    @ObservationIgnored private var task: Task<Void, Never>?
    @ObservationIgnored private var timerSequence = Timer
        .publish(every: 1, on: .main, in: .default)
        .autoconnect()
        .values

    enum Status {
        case stopped
        case proxySet
        case publishing
    }
    
    init(locationManager: LocationManager) {
        self.locationManager = locationManager
    }
    
    @MainActor
    private func setProxy() async -> StreamrProxyResult {
        print("Set Proxy start")
        let streamrProxyAddess = StreamrProxyAddress(websocketUrl: proxyInfo.peerId, ethereumAddress: proxyInfo.peerAddress)
        let result = await Task.detached {
            self.streamrProxyClient.connect(proxies: [streamrProxyAddess])
        }.value
        print("Set Proxy end")
        return result
    }
 
    @MainActor
    private func publish() async -> StreamrProxyResult {
        print("Publish start")
        let latitude = locationManager.location?.latitude ?? 0
        let longitude = locationManager.location?.longitude ?? 0
        let privateKey = ethereumPrivateKey
        self.status = .publishing
        let result = await Task.detached {
            self.streamrProxyClient.publish(content: "\(latitude) \(longitude)", ethereumPrivateKey: privateKey)
        }.value
        print("Publish end")
        return result
    }
    
    func startPublishing() {
        print("startPublishing start")
        Task { @MainActor in
            status = .proxySet
            await setProxy()
            /*
            if result.code == 0 {
                status = .stopped
                return
            }
             */
            timerSequence = Timer
                  .publish(every: publishingIntervalInSeconds, on: .main, in: .default)
                  .autoconnect()
                  .values
            task = Task {
                for await _ in timerSequence {
                    await publish()
                    if Task.isCancelled { return }
                }
            }
        }
        print("startPublishing end")
    }
    
    func stopPublishing() {
        print("stopPublishing start")
        task?.cancel()
        task = nil
        Task { @MainActor in
            status = .stopped
        }
        print("stopPublishing stop")
    }
    

}

