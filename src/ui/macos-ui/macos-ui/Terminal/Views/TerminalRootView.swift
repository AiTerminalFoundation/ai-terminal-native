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
        TerminalCellGrid(size: terminalSize)
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
        }
    }
}
