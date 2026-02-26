//
//  ContentView.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//

import SwiftUI

struct ContentView: View {

    @StateObject private var terminal = TerminalSession()
    @State private var input = ""

    var body: some View {
        VStack {
            ScrollView {
                Text(terminal.output)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
            }

            HStack {
                TextField("Insert command", text: $input)
                    .textFieldStyle(.plain)
            }
            .padding()
        }
        .onAppear {
            terminal.start()
        }
    }
}
