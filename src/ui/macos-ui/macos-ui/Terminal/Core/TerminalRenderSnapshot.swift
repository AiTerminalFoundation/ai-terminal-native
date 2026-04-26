import Foundation

struct TerminalRenderSnapshot: Equatable {
    static let empty = TerminalRenderSnapshot(
        lines: [String(repeating: " ", count: 80)],
        cursorRow: 0,
        cursorColumn: 0,
        terminalCursorVisible: true
    )

    let lines: [String]
    let cursorRow: Int
    let cursorColumn: Int
    let terminalCursorVisible: Bool

    func renderedText(showCursor: Bool) -> String {
        var renderedLines = lines

        if showCursor, terminalCursorVisible, renderedLines.indices.contains(cursorRow) {
            var line = renderedLines[cursorRow]

            if cursorColumn >= line.count {
                line += String(repeating: " ", count: cursorColumn - line.count + 1)
            }

            let index = line.index(line.startIndex, offsetBy: cursorColumn)
            line.replaceSubrange(index...index, with: "▏")
            renderedLines[cursorRow] = line
        }

        return renderedLines.joined(separator: "\n")
    }
}
