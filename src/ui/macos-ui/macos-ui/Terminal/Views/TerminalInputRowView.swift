import SwiftUI

struct TerminalInputRowView: View {
    @ObservedObject var session: TerminalSession
    @Binding var input: String
    var inputFocused: FocusState<Bool>.Binding

    var body: some View {
        HStack(spacing: 4) {
            Text(session.currentPrompt)

            TextField("", text: $input)
                .textFieldStyle(.plain)
                .focused(inputFocused)
                .onSubmit {
                    session.send_input_string(input: input + "\n")
                    input = ""
                }
                .terminalInputKeyHandler(session: session, input: $input)
        }
    }
}
