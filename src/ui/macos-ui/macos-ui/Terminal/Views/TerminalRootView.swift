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
                TerminalKeyboardInputView(applicationCursorKeys: session.applicationCursorKeys) { input in
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
                        .foregroundStyle(Self.foregroundColor(for: cell.color))

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

    private static let trueColorFlag: Int32 = 0x01000000
    private static let ansiColors: [Color] = [
        rgb(red: 0, green: 0, blue: 0),
        rgb(red: 255, green: 123, blue: 114),
        rgb(red: 86, green: 211, blue: 100),
        rgb(red: 128, green: 128, blue: 0),
        rgb(red: 0, green: 0, blue: 128),
        rgb(red: 128, green: 0, blue: 128),
        rgb(red: 0, green: 128, blue: 128),
        rgb(red: 192, green: 192, blue: 192),
        rgb(red: 128, green: 128, blue: 128),
        rgb(red: 255, green: 154, blue: 148),
        rgb(red: 125, green: 241, blue: 139),
        rgb(red: 255, green: 255, blue: 0),
        rgb(red: 0, green: 0, blue: 255),
        rgb(red: 255, green: 0, blue: 255),
        rgb(red: 0, green: 255, blue: 255),
        rgb(red: 255, green: 255, blue: 255)
    ]

    private static func foregroundColor(for colorValue: Int32) -> Color {
        if colorValue < 0 {
            return rgb(red: 220, green: 220, blue: 216)
        }

        if (colorValue & trueColorFlag) == trueColorFlag {
            let rgbValue = colorValue & 0x00FF_FFFF
            return rgb(
                red: Int((rgbValue >> 16) & 0xFF),
                green: Int((rgbValue >> 8) & 0xFF),
                blue: Int(rgbValue & 0xFF)
            )
        }

        return xtermColor(index: Int(colorValue))
    }

    private static func xtermColor(index: Int) -> Color {
        let clampedIndex = min(max(index, 0), 255)

        if clampedIndex < ansiColors.count {
            return ansiColors[clampedIndex]
        }

        if clampedIndex <= 231 {
            let colorCubeIndex = clampedIndex - 16
            let levels = [0, 95, 135, 175, 215, 255]
            return rgb(
                red: levels[colorCubeIndex / 36],
                green: levels[(colorCubeIndex / 6) % 6],
                blue: levels[colorCubeIndex % 6]
            )
        }

        let grayLevel = 8 + (clampedIndex - 232) * 10
        return rgb(red: grayLevel, green: grayLevel, blue: grayLevel)
    }

    private static func rgb(red: Int, green: Int, blue: Int) -> Color {
        Color(
            .sRGB,
            red: Double(red) / 255.0,
            green: Double(green) / 255.0,
            blue: Double(blue) / 255.0,
            opacity: 1
        )
    }
}
