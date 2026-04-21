import SwiftUI

struct TerminalInputKeyHandler: ViewModifier {
    let session: TerminalSession

    func body(content: Content) -> some View {
        content
            .onKeyPress(.tab) {
                session.submitInputDraftWithSuffix("\t")
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
    func terminalInputKeyHandler(session: TerminalSession) -> some View {
        modifier(TerminalInputKeyHandler(session: session))
    }
}
