import SwiftUI

struct TerminalInputRowView: View {
    @ObservedObject var session: TerminalSession
    var inputFocused: FocusState<Bool>.Binding

    var body: some View {
        HStack(spacing: 4) {
            Text(session.currentPrompt)

            TextField("", text: $session.inputDraft)
                .textFieldStyle(.plain)
                .focused(inputFocused)
                .onSubmit {
                    session.submitInputDraft()
                }
                .terminalInputKeyHandler(session: session)
        }
    }
}
