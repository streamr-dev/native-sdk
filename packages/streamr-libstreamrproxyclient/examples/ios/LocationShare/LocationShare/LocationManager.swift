//
//  LocationManager.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import SwiftUI
import CoreLocation

@Observable
@MainActor
final class LocationManager {
    
    var location: (latitude: Double, longitude: Double)? = nil
    private let locationManager = CLLocationManager()
    
    func requestUserAuthorization() async throws {
        locationManager.requestWhenInUseAuthorization()
    }
    
    func startCurrentLocationUpdates() async throws {
        for try await locationUpdate in CLLocationUpdate.liveUpdates() {
            if let coodinate = locationUpdate.location?.coordinate {
                self.location = (coodinate.latitude, coodinate.longitude)
            }
        }
    }
}
