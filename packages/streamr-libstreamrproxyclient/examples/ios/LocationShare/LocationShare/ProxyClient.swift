//
//  ProxyClient.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import Foundation
import Combine
import ProxyClientAPI
import StreamrProxyClientActor
import SwiftUI

@Observable
@MainActor
class ProxyClient {
    var proxyInfo: PeerDesc = PeerDesc(peerId: "ws://127.0.0.1:44211", peerAddress: "0xce119099e2839e522b9657e40d331fe8d322375c")
    var ethereumPrivateKey = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820"
    var publishingIntervalInSeconds: TimeInterval = defaultPublishingIntervalInSeconds
    var status: Status = .stopped
    private let locationManager: LocationManager
    private let streamrProxyClientActor: StreamrProxyClientActor
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
        self.streamrProxyClientActor = try! StreamrProxyClientActor(ownEthereumAddress: "0x1234567890123456789012345678901234567890", streamPartId: "0xd7278f1e4a946fa7838b5d1e0fe50c5725fb23de/nativesdktest#01")
    }

    func startPublishing() {
        print("startPublishing start")
        Task { @MainActor in
            status = .proxySet
            _ = await streamrProxyClientActor.connect(
                proxies: [StreamrProxyAddress(websocketUrl: proxyInfo.peerId,
                ethereumAddress: proxyInfo.peerAddress)])
            timerSequence = Timer
                .publish(every: publishingIntervalInSeconds, on: .main, in: .default)
                .autoconnect()
                .values
            self.status = .publishing
            task = Task {
                for await _ in timerSequence {
                    _ = await streamrProxyClientActor.publish(
                        content: "\(locationManager.location?.latitude ?? 0) \(locationManager.location?.longitude ?? 0)",
                        ethereumPrivateKey: ethereumPrivateKey)
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
