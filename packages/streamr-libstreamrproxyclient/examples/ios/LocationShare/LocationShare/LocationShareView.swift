//
//  ContentView.swift
//  LocationShare
//
//  Created by Santtu Rantanen on 23.8.2024.
//

import SwiftUI

struct LocationShareView: View {
    @State var viewModel = LocationShareViewModel()
    
    var body: some View {
        ZStack {
            List {
                Section(header: Text("Proxy Information")) {
                    TextField(
                        "Enter proxy address",
                        text: $viewModel.proxyAddress
                    )
                    TextField(
                        "Enter proxy ID",
                        text: $viewModel.proxyId
                    )
                    TextField(
                        "Enter sending interval in seconds",
                        text: $viewModel.publishingIntervalInSeconds
                    )
                
                }
                Section(header: Text("GPS Location")) {
                    Text("\(viewModel.latitude), \(viewModel.longitude)")
                }
            }
            VStack {
                Spacer()
                Button {
                    viewModel.buttonClicked()
                } label: {
                    Text(viewModel.buttonText)
                        .frame(maxWidth: .infinity)
                }.buttonStyle(.borderedProminent)
                    .controlSize(.large)
                    .padding([.leading, .trailing, .top, .bottom])
                    .disabled(viewModel.proxyAddress == "" ||
                              viewModel.proxyId == "" ||
                              viewModel.publishingIntervalInSeconds == "")
            }
            
        }
        .task {
            await viewModel.initialize()
        }
    }
}

/*
#Preview {
    ContentView()
}
*/
