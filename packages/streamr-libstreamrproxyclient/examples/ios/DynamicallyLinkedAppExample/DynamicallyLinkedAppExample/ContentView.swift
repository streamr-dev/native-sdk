//
//  ContentView.swift
//  DynamicallyLinkedAppExample
//
//  Created by Petri Savolainen on 22.8.2024.
//

import SwiftUI
import streamrproxyclient

struct ContentView: View {
    var body: some View {
        VStack {
            Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            Text(String(cString: testRpc()))
        }
        .padding()
    }
}

#Preview {
    ContentView()
}
