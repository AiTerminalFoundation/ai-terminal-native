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
            .background(Color(red: 0.06, green: 0.065, blue: 0.07))
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

            var grid = Path()

            for col in 0...cols {
                let x = CGFloat(col) * cellWidth
                grid.move(to: CGPoint(x: x, y: 0))
                grid.addLine(to: CGPoint(x: x, y: canvasSize.height))
            }

            for row in 0...rows {
                let y = CGFloat(row) * cellHeight
                grid.move(to: CGPoint(x: 0, y: y))
                grid.addLine(to: CGPoint(x: canvasSize.width, y: y))
            }

            context.stroke(grid, with: .color(.white.opacity(0.18)), lineWidth: 0.5)

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
                        .foregroundStyle(Self.color(for: cell.color))

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

    private static func color(for ansiColor: UInt8) -> Color {
        switch ansiColor {
            case 1: return Color(red: 0.12, green: 0.12, blue: 0.12)
            case 2: return Color(red: 0.80, green: 0.20, blue: 0.20)
            case 3: return Color(red: 0.25, green: 0.75, blue: 0.35)
            case 4: return Color(red: 0.85, green: 0.70, blue: 0.25)
            case 5: return Color(red: 0.35, green: 0.55, blue: 0.95)
            case 6: return Color(red: 0.75, green: 0.40, blue: 0.85)
            case 7: return Color(red: 0.35, green: 0.80, blue: 0.85)
            case 8: return Color(red: 0.88, green: 0.88, blue: 0.84)
            default: return Color(red: 0.88, green: 0.88, blue: 0.84)
        }
    }
}
