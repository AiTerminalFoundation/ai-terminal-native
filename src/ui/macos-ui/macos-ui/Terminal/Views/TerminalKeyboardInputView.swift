import AppKit
import SwiftUI

struct TerminalKeyboardInputView: NSViewRepresentable {
    let applicationCursorKeys: Bool
    let onInput: (String) -> Void

    func makeNSView(context: Context) -> KeyboardView {
        let view = KeyboardView()
        view.applicationCursorKeys = applicationCursorKeys
        view.onInput = onInput

        DispatchQueue.main.async {
            view.window?.makeFirstResponder(view)
        }

        return view
    }

    func updateNSView(_ nsView: KeyboardView, context: Context) {
        nsView.applicationCursorKeys = applicationCursorKeys
        nsView.onInput = onInput

        DispatchQueue.main.async {
            nsView.window?.makeFirstResponder(nsView)
        }
    }
}

final class KeyboardView: NSView {
    var applicationCursorKeys = false
    var onInput: ((String) -> Void)?

    override var acceptsFirstResponder: Bool {
        true
    }

    override func keyDown(with event: NSEvent) {
        switch event.specialKey {
            case .upArrow:
                onInput?(arrowSequence(normal: "A"))
            case .downArrow:
                onInput?(arrowSequence(normal: "B"))
            case .rightArrow:
                onInput?(arrowSequence(normal: "C"))
            case .leftArrow:
                onInput?(arrowSequence(normal: "D"))
            default:
                handleTextInput(event)
        }
    }

    private func arrowSequence(normal finalByte: Character) -> String {
        applicationCursorKeys ? "\u{1b}O\(finalByte)" : "\u{1b}[\(finalByte)"
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
