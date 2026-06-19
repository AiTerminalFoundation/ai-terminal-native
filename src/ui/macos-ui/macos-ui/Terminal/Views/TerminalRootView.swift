import SwiftUI

struct TerminalRootView: View {
    private let masterFileDescriptor: Int32?

    @State private var terminalSize = TerminalSize(
        width: DefaultSettings.windowWidth,
        height: DefaultSettings.windowHeight,
        rows: DefaultSettings.rows,
        cols: DefaultSettings.columns
    )

    init(masterFileDescriptor: Int32? = nil) {
        self.masterFileDescriptor = masterFileDescriptor
    }

    var body: some View {
        TerminalCellGrid(size: terminalSize)
            .background(Color(red: 0.06, green: 0.065, blue: 0.07))
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .detectWindowSize { size in
                updateTerminalSize(from: size)
            }
    }

    private func updateTerminalSize(from size: CGSize) {
        let newTerminalSize = makeTerminalSize(from: size)
        let gridChanged = newTerminalSize.rows != terminalSize.rows || newTerminalSize.cols != terminalSize.cols

        terminalSize = newTerminalSize

        if gridChanged {
            resizePty(to: newTerminalSize)
        }
    }

    private func makeTerminalSize(from size: CGSize) -> TerminalSize {
        TerminalSize(
            width: size.width,
            height: size.height,
            rows: max(1, Int(size.height / DefaultSettings.fontSize)),
            cols: max(1, Int(size.width / DefaultSettings.fontSize))
        )
    }

    private func resizePty(to size: TerminalSize) {
        guard let masterFileDescriptor else {
            return
        }

        _ = resize_terminal_window(
            masterFileDescriptor,
            Int32(size.rows),
            Int32(size.cols),
            Int32(size.width.rounded()),
            Int32(size.height.rounded())
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
