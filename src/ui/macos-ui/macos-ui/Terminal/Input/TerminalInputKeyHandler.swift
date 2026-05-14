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

        if modifiers.contains(.control) {
            if let controlInput = controlInput(for: keyPress) {
                return controlInput
            }
        } else if modifiers.contains(.command) {
            if let commandInput = commandInput(for: keyPress) {
                return commandInput
            }
            return nil
        } else if modifiers.contains(.option) {
            if let optionInput = optionInput(for: keyPress) {
                return optionInput
            }
        }

        // this isn't inside an else branch because if one of the modifiers doesn't recognise the combo,
        //then it will return nil, instead of falling back on normal key handling
        return normalKeyInput(for: keyPress)
    }

    private func normalKeyInput(for keyPress: KeyPress) -> String? {
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

    private func commandInput(for keyPress: KeyPress) -> String? {
        if isDelete(keyPress) {
            // Command-Delete mirrors Ctrl-U: kill from cursor to start of line.
            return "\u{15}"
        } else if keyPress.key.character.uppercased() == "V" { // cmd + V to paste
            if let pastedText = NSPasteboard.general.string(forType: .string) {
                let wrapped = "\u{1B}[200~" + pastedText + "\u{1B}[201~" //wrapping to avoid sending the pasted command directly
                  return wrapped
            }
            return nil
        }

        return nil
    }

    private func optionInput(for keyPress: KeyPress) -> String? {
        if isDelete(keyPress) {
            // Option-Delete mirrors Ctrl-W: delete the previous word.
            return "\u{17}"
        }

        return nil
    }

    private func isDelete(_ keyPress: KeyPress) -> Bool {
        keyPress.key == .delete || keyPress.characters.unicodeScalars.first?.value == 127
    }

    private func controlInput(for keyPress: KeyPress) -> String? {
        guard let scalar = keyPress.characters.lowercased().unicodeScalars.first else {
            return nil
        }

        // Map Control-modified ASCII keys to the C0 control codes terminals expect.
        switch scalar.value {
        case 64:
            // Ctrl-@ sends NUL.
            return "\u{00}"
        case 65...90:
            // Ctrl-A through Ctrl-Z send SOH through SUB.
            return String(UnicodeScalar(scalar.value - 64)!)
        case 97...122:
            // Lowercase letters produce the same control codes as uppercase letters.
            return String(UnicodeScalar(scalar.value - 96)!)
        case 91:
            // Ctrl-[ is Escape.
            return "\u{1B}"
        case 92:
            // Ctrl-\ is File Separator.
            return "\u{1C}"
        case 93:
            // Ctrl-] is Group Separator.
            return "\u{1D}"
        case 94:
            // Ctrl-^ is Record Separator.
            return "\u{1E}"
        case 95:
            // Ctrl-_ is Unit Separator.
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
