import SwiftUI

struct TerminalInputKeyHandler: ViewModifier {
    let session: TerminalSession

    func body(content: Content) -> some View {
        content.onKeyPress(phases: .down) { keyPress in
            guard let input = terminalInput(for: keyPress) else {
                return .ignored
            }

            session.send_input_string(input: input)
            return .handled
        }
    }

    private func terminalInput(for keyPress: KeyPress) -> String? {
        let modifiers = keyPress.modifiers

        // Keep app/menu shortcuts available to SwiftUI and AppKit.
        guard !modifiers.contains(.command) else { return nil }

        if modifiers.contains(.control),
           let controlInput = controlInput(for: keyPress) {
            return controlInput
        }

        switch keyPress.key {
        case .return:
            return "\r"
        case .tab:
            return "\t"
        case .delete:
            return "\u{7F}"
        case .deleteForward:
            return "\u{1B}[3~"
        case .escape:
            return "\u{1B}"
        case .upArrow:
            return "\u{1B}[A"
        case .downArrow:
            return "\u{1B}[B"
        case .rightArrow:
            return "\u{1B}[C"
        case .leftArrow:
            return "\u{1B}[D"
        case .home:
            return "\u{1B}[H"
        case .end:
            return "\u{1B}[F"
        case .pageUp:
            return "\u{1B}[5~"
        case .pageDown:
            return "\u{1B}[6~"
        default:
            return keyPress.characters.isEmpty ? nil : keyPress.characters
        }
    }

    private func controlInput(for keyPress: KeyPress) -> String? {
        guard let scalar = keyPress.characters.lowercased().unicodeScalars.first else {
            return nil
        }

        switch scalar.value {
        case 64:
            return "\u{00}"
        case 65...90:
            return String(UnicodeScalar(scalar.value - 64)!)
        case 97...122:
            return String(UnicodeScalar(scalar.value - 96)!)
        case 91:
            return "\u{1B}"
        case 92:
            return "\u{1C}"
        case 93:
            return "\u{1D}"
        case 94:
            return "\u{1E}"
        case 95:
            return "\u{1F}"
        default:
            return nil
        }
    }
}

extension View {
    func terminalInputKeyHandler(session: TerminalSession) -> some View {
        modifier(TerminalInputKeyHandler(session: session))
    }
}
