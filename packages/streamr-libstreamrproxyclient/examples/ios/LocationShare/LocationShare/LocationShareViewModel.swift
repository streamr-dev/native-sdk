//
//  LocationShareViewModel.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import Foundation

struct LocationShareViewModel {
    private var locationManager: LocationManager
    private var streamrProxyClient: StreamrProxyClient
   
    init() {
        self.locationManager = LocationManager()
        self.streamrProxyClient = StreamrProxyClient(locationManager: locationManager)
    }
    
    @MainActor
    var buttonText: String {
        streamrProxyClient.status == .stopped ? "Start Sharing" : "Stop Sharing"
    }
    
    @MainActor
    var proxyAddress: String {
        get {
            streamrProxyClient.proxyInfo.peerAddress
        }
        set(newProxyAddress) {
            streamrProxyClient.proxyInfo.peerAddress = newProxyAddress
        }
    }
    
    @MainActor
    var proxyId: String {
        get {
            streamrProxyClient.proxyInfo.peerId
        }
        set(newProxyId) {
            streamrProxyClient.proxyInfo.peerId = newProxyId
        }
    }
    
    @MainActor
    var publishingIntervalInSeconds: String {
        get {
            print("Thread: publishingIntervalInSeconds: \(Thread.current)")
        
            return String(streamrProxyClient.publishingIntervalInSeconds)
        }
        set(newProxyId) {
            streamrProxyClient.publishingIntervalInSeconds = 
                TimeInterval(newProxyId) ?? StreamrProxyClient.defaultPublishingIntervalInSeconds
        }
    }
    
    @MainActor
    private var locationCoordinates: (latitude: String, longitude: String) {
        guard let location = locationManager.location else {
            return (latitude: "Unknown", longitude: "Unknown")
        }
        return (latitude: String(location.latitude), longitude: String(location.longitude))
    }
    
    @MainActor
    var latitude: String {
        locationCoordinates.latitude
    }
      
    @MainActor
    var longitude: String {
        locationCoordinates.longitude
    }
    
    @MainActor
    func buttonClicked() {
        switch(streamrProxyClient.status) {
        case .stopped:
            streamrProxyClient.startPublishing()
        case .proxySet:
            streamrProxyClient.startPublishing()
        case .publishing:
            streamrProxyClient.stopPublishing()
        }
    }
    
    func initialize() async {
        try? await locationManager.requestUserAuthorization()
        try? await locationManager.startCurrentLocationUpdates()
    }
    
    func setProxyAndStartPublishing() {
        streamrProxyClient.startPublishing()
    }
    
   
}

