import Foundation

struct TerminalCell: Identifiable {
    let id: Int
    let codepoint: UInt32
    let isEmpty: Bool
    let isBold: Bool
    let color: Int32

    var character: String {
        guard !isEmpty, let scalar = UnicodeScalar(codepoint) else {
            return " "
        }

        return String(Character(scalar))
    }
}
