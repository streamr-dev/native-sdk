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
    @MainActor var proxyInfo: PeerDesc = PeerDesc(peerId: "", peerAddress: "")
    @MainActor var publishingIntervalInSeconds: TimeInterval = defaultPublishingIntervalInSeconds
    @MainActor private(set) var status: Status = .stopped
    private var cancellable: AnyCancellable?
    private var locationManager: LocationManager
    private var proxyClient = ProxyClient()
    static var defaultPublishingIntervalInSeconds: TimeInterval = 5
    private var proxyClientHandle: Int32 = 0
    
    enum Status {
        case stopped
        case proxySet
        case publishing
    }
    
    init(locationManager: LocationManager) {
        self.locationManager = locationManager
        proxyClientHandle = self.proxyClient.newClient()
    }
    
    @MainActor
    private func setProxy() async -> Result {
        print("Set Proxy start")
        let peerDescriptor = PeerDescriptor(peerId: "9f8wunfaiwuhfwe9a8", peerAddress: "ws://127.0.0.1:8080")
        let result = await Task.detached {
            self.proxyClient.setProxy(self.proxyClientHandle, peerDescriptor)
        }.value
        print("Set Proxy end")
        return result
    }
 
    private func publish() {
        print("Set Proxy start")
        Task { @MainActor in
            print("Thread: publish: \(Thread.current)")
            let latitude = locationManager.location?.latitude ?? 0
            let longitude = locationManager.location?.longitude ?? 0
            print("Publish: \(latitude), \(longitude)")
            self.status = .publishing
        }
    }
    
    func startPublishing() {
        Task { @MainActor in
            print("Thread: startPublishing: \(Thread.current)")
            print("startPublishing with interval: \(publishingIntervalInSeconds)")
            let result = await setProxy()
            if result.code == 1 {
                return
            }
            status = .proxySet
            let task = Timer.publish(every: publishingIntervalInSeconds, on: .main, in: .default).values
            cancellable = Timer.publish(every: publishingIntervalInSeconds, on: .main, in: .default)
                .autoconnect()
                .sink { [self] _ in
                    publish()
                }
        }
    }
    
    func stopPublishing() {
        print("stopPublishing")
        cancellable?.cancel()
        cancellable = nil
        Task { @MainActor in
            status = .stopped
        }
    }
    

}


