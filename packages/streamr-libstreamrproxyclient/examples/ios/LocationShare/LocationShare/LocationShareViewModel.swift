//
//  LocationShareViewModel.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import Foundation

struct LocationShareViewModel {
    private var locationManager: LocationManager
    var proxyClient: ProxyClient
   
    init() {
        self.locationManager = LocationManager()
        self.proxyClient = ProxyClient(locationManager: locationManager)
    }
       
    @MainActor
    var status: String {
        "Status: \(proxyClient.status)"
    }
    
    @MainActor
    var buttonText: String {
        proxyClient.status == .stopped ? "Start Sharing" : "Stop Sharing"
    }
    
    @MainActor
    var proxyAddress: String {
        get {
            proxyClient.proxyInfo.peerAddress
        }
        set(newProxyAddress) {
            proxyClient.proxyInfo.peerAddress = newProxyAddress
        }
    }
    
    @MainActor
    var ethereumPrivateKey: String {
        get {
            proxyClient.ethereumPrivateKey
        }
        set(newEthereumPrivateKey) {
            proxyClient.ethereumPrivateKey = newEthereumPrivateKey
        }
    }
    
    @MainActor
    var proxyId: String {
        get {
            proxyClient.proxyInfo.peerId
        }
        set(newProxyId) {
            proxyClient.proxyInfo.peerId = newProxyId
        }
    }
    
    @MainActor
    var publishingIntervalInSeconds: String {
        get {
            print("Thread: publishingIntervalInSeconds: \(Thread.current)")
        
            return String(proxyClient.publishingIntervalInSeconds)
        }
        set(newProxyId) {
            proxyClient.publishingIntervalInSeconds =
                TimeInterval(newProxyId) ?? ProxyClient.defaultPublishingIntervalInSeconds
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
        switch(proxyClient.status) {
        case .stopped:
            proxyClient.startPublishing()
        case .proxySet:
            proxyClient.startPublishing()
        case .publishing:
            proxyClient.stopPublishing()
        }
    }
    
    func initialize() async {
        try? await locationManager.requestUserAuthorization()
        try? await locationManager.startCurrentLocationUpdates()
    }
    
    func setProxyAndStartPublishing() {
        proxyClient.startPublishing()
    }
    
   
}

