import SwiftUI

struct TerminalFocusKeyHandler: ViewModifier {
    @ObservedObject var session: TerminalSession
    var inputFocused: FocusState<Bool>.Binding

    func body(content: Content) -> some View {
        content.onKeyPress(phases: .down) { keyPress in
            let modifiers = keyPress.modifiers
            let isModifierOnly = modifiers.contains(.command) ||
                                 modifiers.contains(.option) ||
                                 modifiers.contains(.control)

            if !isModifierOnly && !inputFocused.wrappedValue {
                inputFocused.wrappedValue = true

                if let char = keyPress.characters.first, !char.isNewline {
                    session.inputDraft.append(char)
                }
                return .handled
            }

            return .ignored
        }
    }
}

extension View {
    func terminalFocusKeyHandler(session: TerminalSession, inputFocused: FocusState<Bool>.Binding) -> some View {
        modifier(TerminalFocusKeyHandler(session: session, inputFocused: inputFocused))
    }
}
