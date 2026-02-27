import SwiftUI
internal import Combine

struct ContentView: View {
    @StateObject private var terminal = TerminalSession()
    @State private var input = ""

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                VStack() {
                    TextEditor(text: .constant(terminal.output))
                        .scrollContentBackground(.hidden)
                        .scrollDisabled(true)          // let the outer ScrollView handle it
                        .frame(maxWidth: .infinity)
                        .fixedSize(horizontal: false, vertical: true)

                    TextField(terminal.currentPrompt.isEmpty ? "Type here..." : terminal.currentPrompt, text: $input)
                            .textFieldStyle(.plain)
                            .onSubmit {
                                terminal.send_input_string(input: input + "\n")
                                input = ""
                            }
                }
            }
        }.onAppear { terminal.start() }
    }
}
