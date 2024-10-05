//
//  StreamrProxyClient.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import Foundation
import Combine

@Observable
class StreamrProxyClient {
    @MainActor var proxyInfo: PeerDesc = PeerDesc(peerId: "ws://127.0.0.1:44211", peerAddress: "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
    @MainActor var publishingIntervalInSeconds: TimeInterval = defaultPublishingIntervalInSeconds
    var status: Status = .stopped
    private let locationManager: LocationManager
    private let proxyClient = ProxyClient()
    static let defaultPublishingIntervalInSeconds: TimeInterval = 5
    @ObservationIgnored private var proxyClientHandle: UInt64 = 0
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
        proxyClientHandle = self.proxyClient.newClient("0x1234567890123456789012345678901234567890", "0xa000000000000000000000000000000000000000#01")
    }
    
    @MainActor
    private func setProxy() async -> UInt64 {
        print("Set Proxy start")
        let peerId = std.string(proxyInfo.peerId)
        let peerAddress = std.string(proxyInfo.peerAddress)
        
        let result = await Task.detached {
            self.proxyClient.connect(self.proxyClientHandle, peerId, peerAddress)
        }.value
        print("Set Proxy end")
        return result
    }
 
    @MainActor
    private func publish() async -> Result {
        print("Publish start")
        let latitude = locationManager.location?.latitude ?? 0
        let longitude = locationManager.location?.longitude ?? 0
        self.status = .publishing
        let result = await Task.detached {
            self.proxyClient.publish(self.proxyClientHandle, std.string("\(latitude) \(longitude)"))
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


