import AppKit
import SwiftUI

struct TerminalKeyboardInputView: NSViewRepresentable {
    let onInput: (String) -> Void

    func makeNSView(context: Context) -> KeyboardView {
        let view = KeyboardView()
        view.onInput = onInput

        DispatchQueue.main.async {
            view.window?.makeFirstResponder(view)
        }

        return view
    }

    func updateNSView(_ nsView: KeyboardView, context: Context) {
        nsView.onInput = onInput

        DispatchQueue.main.async {
            nsView.window?.makeFirstResponder(nsView)
        }
    }
}

final class KeyboardView: NSView {
    var onInput: ((String) -> Void)?

    override var acceptsFirstResponder: Bool {
        true
    }

    override func keyDown(with event: NSEvent) {
        switch event.specialKey {
            case .upArrow:
                onInput?("\u{1b}[A")
            case .downArrow:
                onInput?("\u{1b}[B")
            case .rightArrow:
                onInput?("\u{1b}[C")
            case .leftArrow:
                onInput?("\u{1b}[D")
            default:
                handleTextInput(event)
        }
    }

    private func handleTextInput(_ event: NSEvent) {
        switch event.keyCode {
            case 36:
                onInput?("\n")
            case 48:
                onInput?("\t")
            case 51:
                onInput?("\u{7f}")
            case 53:
                onInput?("\u{1b}")
            default:
                guard let characters = event.characters, !characters.isEmpty else {
                    return
                }

                onInput?(characters)
        }
    }
}
