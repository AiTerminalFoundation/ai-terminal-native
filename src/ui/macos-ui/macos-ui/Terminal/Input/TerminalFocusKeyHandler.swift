import SwiftUI

struct TerminalFocusKeyHandler: ViewModifier {
    @Binding var input: String
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
                    input.append(char)
                }
                return .handled
            }

            return .ignored
        }
    }
}

extension View {
    func terminalFocusKeyHandler(input: Binding<String>, inputFocused: FocusState<Bool>.Binding) -> some View {
        modifier(TerminalFocusKeyHandler(input: input, inputFocused: inputFocused))
    }
}
