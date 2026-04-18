import SwiftUI

struct TerminalInputKeyHandler: ViewModifier {
    let session: TerminalSession
    @Binding var input: String

    func body(content: Content) -> some View {
        content
            .onKeyPress(.tab) {
                session.send_input_string(input: input + "\t")
                return .handled
            }
            .onKeyPress(.upArrow) {
                session.send_input_string(input: "\u{1B}[A")
                return .handled
            }
            .onKeyPress(.downArrow) {
                session.send_input_string(input: "\u{1B}[B")
                return .handled
            }
    }
}

extension View {
    func terminalInputKeyHandler(session: TerminalSession, input: Binding<String>) -> some View {
        modifier(TerminalInputKeyHandler(session: session, input: input))
    }
}
