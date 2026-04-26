import Foundation

struct TerminalCell: Equatable {
    static let blank = TerminalCell(codepoint: 32)

    var codepoint: UInt32

    var stringValue: String {
        guard let scalar = UnicodeScalar(codepoint) else {
            return " "
        }

        return String(scalar)
    }
}
