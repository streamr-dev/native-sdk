//
//  ContentView.swift
//  StreamrProtoRpcExampleApp
//
//  Created by Santtu Rantanen on 21.8.2024.
//

import SwiftUI

struct ContentView: View {
    var body: some View {
        VStack {
            Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            Text("\(Wrapper().greetingWrapper())")
        }
        .padding()
    }
}
