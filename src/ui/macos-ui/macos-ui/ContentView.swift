//
//  ContentView.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//

import SwiftUI
internal import Combine

struct ContentView: View {
    @StateObject private var terminal = TerminalSession()
    @State private var input = ""
    @FocusState private var inputFocused: Bool

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                VStack(alignment: .leading) {
                    Text(terminal.output)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled)
                        .padding(.horizontal, 5)
                    
                    HStack(spacing: 4) {
                        Text(terminal.currentPrompt)

                        TextField("", text: $input)
                            .textFieldStyle(.plain)
                            .focused($inputFocused)
                            .onSubmit {
                                terminal.send_input_string(input: input + "\n")
                                input = ""
                            }
                            .onKeyPress(.tab) {
                                terminal.send_input_string(input: input + "\t")
                                input = ""
                                return .handled
                            }
                    }
                    .padding(.horizontal, 5)

                    // Invisible anchor at the bottom to scroll to
                    Color.clear.frame(height: 1).id("bottom")
                }
            }
            // Scroll to bottom whenever output changes
            .onChange(of: terminal.output) {
                withAnimation { proxy.scrollTo("bottom") }
            }
            // Auto-focus the input on appear
            .onAppear {
                terminal.start()
                inputFocused = true
            }
            // Capture any keypress that isn't a modifier and redirect to input
            .onKeyPress(phases: .down) { keyPress in
                let modifiers = keyPress.modifiers
                let isModifierOnly = modifiers.contains(.command) ||
                                     modifiers.contains(.option)  ||
                                     modifiers.contains(.control)

                if !isModifierOnly && !inputFocused {
                    inputFocused = true
                    // Append the pressed character to the input so it isn't lost
                    if let char = keyPress.characters.first, !char.isNewline {
                        input.append(char)
                    }
                    return .handled
                }
                return .ignored
            }
        }
    }
}
