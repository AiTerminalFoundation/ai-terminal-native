import SwiftUI

struct TerminalRootView: View {
    @StateObject private var session = TerminalSession()

    @State private var terminalSize = TerminalSize(
        width: DefaultSettings.windowWidth,
        height: DefaultSettings.windowHeight,
        rows: DefaultSettings.rows,
        cols: DefaultSettings.columns
    )
    @State private var didStartSession = false

    var body: some View {
        TerminalCellGrid(
            size: terminalSize,
            cells: session.cells,
            cursorRow: session.cursorRow,
            cursorColumn: session.cursorColumn
        )
            .frame(
                minWidth: DefaultSettings.minimumWindowWidth,
                maxWidth: .infinity,
                minHeight: DefaultSettings.minimumWindowHeight,
                maxHeight: .infinity
            )
            .detectWindowSize { size in
                updateTerminalSize(from: size)
            }
            .overlay {
                TerminalKeyboardInputView { input in
                    session.send_input_string(input: input)
                }
            }
    }

    private func updateTerminalSize(from size: CGSize) {
        guard size.width > 0, size.height > 0 else { return }

        let newTerminalSize = makeTerminalSize(from: size)
        let gridChanged = newTerminalSize.rows != terminalSize.rows || newTerminalSize.cols != terminalSize.cols

        terminalSize = newTerminalSize

        if !didStartSession {
            didStartSession = true
            session.start(size: newTerminalSize)
            return
        }

        if gridChanged {
            session.resize(to: newTerminalSize)
        }
    }

    private func makeTerminalSize(from size: CGSize) -> TerminalSize {
        TerminalSize(
            width: size.width,
            height: size.height,
            rows: max(1, Int(size.height / DefaultSettings.cellHeight)),
            cols: max(1, Int(size.width / DefaultSettings.cellWidth))
        )
    }
}

private struct TerminalCellGrid: View {
    let size: TerminalSize
    let cells: [TerminalCell]
    let cursorRow: Int
    let cursorColumn: Int

    var body: some View {
        Canvas { context, canvasSize in
            let rows = max(size.rows, 1)
            let cols = max(size.cols, 1)
            let cellWidth = canvasSize.width / CGFloat(cols)
            let cellHeight = canvasSize.height / CGFloat(rows)
            
            if cursorRow >= 0, cursorRow < rows, cursorColumn >= 0, cursorColumn < cols {
                let cursorRect = CGRect(
                    x: CGFloat(cursorColumn) * cellWidth,
                    y: CGFloat(cursorRow) * cellHeight,
                    width: cellWidth,
                    height: cellHeight
                )
                context.fill(Path(cursorRect), with: .color(.white.opacity(0.25)))
            }

            for row in 0..<rows {
                for col in 0..<cols {
                    let index = row * cols + col
                    guard index < cells.count else { continue }

                    let cell = cells[index]
                    guard !cell.isEmpty else { continue }

                    var text = Text(cell.character)
                        .font(.custom(DefaultSettings.fontName, size: DefaultSettings.fontSize))

                    if cell.isBold {
                        text = text.fontWeight(.bold)
                    }

                    context.draw(
                        text,
                        at: CGPoint(
                            x: CGFloat(col) * cellWidth + cellWidth / 2,
                            y: CGFloat(row) * cellHeight + cellHeight / 2
                        ),
                        anchor: .center
                    )
                }
            }
        }
    }
}
