import Foundation

struct TerminalScreenBuffer {
    private(set) var rows: Int
    private(set) var columns: Int
    private(set) var cursorRow = 0
    private(set) var cursorColumn = 0
    private(set) var cursorVisible = true

    private var primaryCells: [TerminalCell]
    private var alternateCells: [TerminalCell]
    private var isUsingAlternateScreen = false
    private var savedCursorRow = 0
    private var savedCursorColumn = 0
    private var scrollTop = 0
    private var scrollBottom: Int
    private var scrollback: [String] = []
    private let maxScrollback = 1_000

    init(rows: Int = 24, columns: Int = 80) {
        self.rows = rows
        self.columns = columns
        self.scrollBottom = rows - 1
        self.primaryCells = Array(repeating: .blank, count: rows * columns)
        self.alternateCells = Array(repeating: .blank, count: rows * columns)
    }

    var snapshot: TerminalRenderSnapshot {
        let visibleLines = (0..<rows).map { rowString($0) }
        let lines = isUsingAlternateScreen ? visibleLines : scrollback + visibleLines
        let rowOffset = isUsingAlternateScreen ? 0 : scrollback.count

        return TerminalRenderSnapshot(
            lines: lines,
            cursorRow: rowOffset + cursorRow,
            cursorColumn: cursorColumn,
            terminalCursorVisible: cursorVisible
        )
    }

    mutating func reset() {
        primaryCells = Array(repeating: .blank, count: rows * columns)
        alternateCells = Array(repeating: .blank, count: rows * columns)
        isUsingAlternateScreen = false
        cursorRow = 0
        cursorColumn = 0
        cursorVisible = true
        savedCursorRow = 0
        savedCursorColumn = 0
        scrollTop = 0
        scrollBottom = rows - 1
        scrollback.removeAll(keepingCapacity: true)
    }

    mutating func put(_ scalar: Unicode.Scalar) {
        if cursorColumn >= columns {
            carriageReturn()
            lineFeed()
        }

        setCell(row: cursorRow, column: cursorColumn, TerminalCell(codepoint: scalar.value))

        if cursorColumn == columns - 1 {
            carriageReturn()
            lineFeed()
        } else {
            cursorColumn += 1
        }
    }

    mutating func carriageReturn() {
        cursorColumn = 0
    }

    mutating func lineFeed() {
        if cursorRow == scrollBottom {
            scrollUp(top: scrollTop, bottom: scrollBottom)
        } else {
            cursorRow = min(cursorRow + 1, rows - 1)
        }
    }

    mutating func backspace() {
        cursorColumn = max(cursorColumn - 1, 0)
    }

    mutating func tab() {
        cursorColumn = min(((cursorColumn / 8) + 1) * 8, columns - 1)
    }

    mutating func moveCursor(row: Int, column: Int) {
        cursorRow = min(max(row, 0), rows - 1)
        cursorColumn = min(max(column, 0), columns - 1)
    }

    mutating func moveCursorBy(rowDelta: Int, columnDelta: Int) {
        moveCursor(row: cursorRow + rowDelta, column: cursorColumn + columnDelta)
    }

    mutating func setCursorColumn(_ column: Int) {
        moveCursor(row: cursorRow, column: column)
    }

    mutating func clearScreen(mode: Int) {
        switch mode {
        case 0:
            clearLineFromCursor()
            if cursorRow + 1 < rows {
                clearRows((cursorRow + 1)..<rows)
            }
        case 1:
            if cursorRow > 0 {
                clearRows(0..<cursorRow)
            }
            clearLineToCursor()
        case 2, 3:
            clearRows(0..<rows)
            moveCursor(row: 0, column: 0)
        default:
            break
        }
    }

    mutating func clearLine(mode: Int) {
        switch mode {
        case 0:
            clearCells(row: cursorRow, columns: cursorColumn..<columns)
        case 1:
            clearCells(row: cursorRow, columns: 0...cursorColumn)
        case 2:
            clearCells(row: cursorRow, columns: 0..<columns)
        default:
            break
        }
    }

    mutating func saveCursor() {
        savedCursorRow = cursorRow
        savedCursorColumn = cursorColumn
    }

    mutating func restoreCursor() {
        moveCursor(row: savedCursorRow, column: savedCursorColumn)
    }

    mutating func setCursorVisible(_ visible: Bool) {
        cursorVisible = visible
    }

    mutating func setScrollRegion(top: Int?, bottom: Int?) {
        let newTop = min(max((top ?? 1) - 1, 0), rows - 1)
        let newBottom = min(max((bottom ?? rows) - 1, newTop), rows - 1)

        scrollTop = newTop
        scrollBottom = newBottom
        moveCursor(row: 0, column: 0)
    }

    mutating func useAlternateScreen(_ enabled: Bool, clear: Bool) {
        guard isUsingAlternateScreen != enabled else { return }

        isUsingAlternateScreen = enabled
        scrollTop = 0
        scrollBottom = rows - 1
        moveCursor(row: 0, column: 0)

        if clear {
            clearRows(0..<rows)
        }
    }

    private var activeCells: [TerminalCell] {
        get { isUsingAlternateScreen ? alternateCells : primaryCells }
        set {
            if isUsingAlternateScreen {
                alternateCells = newValue
            } else {
                primaryCells = newValue
            }
        }
    }

    private func cellIndex(row: Int, column: Int) -> Int {
        row * columns + column
    }

    private func rowString(_ row: Int) -> String {
        let start = cellIndex(row: row, column: 0)
        let end = start + columns
        return activeCells[start..<end].map(\.stringValue).joined()
    }

    private mutating func setCell(row: Int, column: Int, _ cell: TerminalCell) {
        guard row >= 0, row < rows, column >= 0, column < columns else { return }

        var cells = activeCells
        cells[cellIndex(row: row, column: column)] = cell
        activeCells = cells
    }

    private mutating func clearRows(_ rowsToClear: Range<Int>) {
        for row in rowsToClear {
            clearCells(row: row, columns: 0..<columns)
        }
    }

    private mutating func clearLineFromCursor() {
        clearCells(row: cursorRow, columns: cursorColumn..<columns)
    }

    private mutating func clearLineToCursor() {
        clearCells(row: cursorRow, columns: 0...cursorColumn)
    }

    private mutating func clearCells<R: Sequence>(row: Int, columns columnsToClear: R) where R.Element == Int {
        guard row >= 0, row < rows else { return }

        var cells = activeCells
        for column in columnsToClear where column >= 0 && column < columns {
            cells[cellIndex(row: row, column: column)] = .blank
        }
        activeCells = cells
    }

    private mutating func scrollUp(top: Int, bottom: Int) {
        var cells = activeCells

        if !isUsingAlternateScreen, top == 0 {
            scrollback.append(rowString(top))
            if scrollback.count > maxScrollback {
                scrollback.removeFirst(scrollback.count - maxScrollback)
            }
        }

        if top < bottom {
            for row in top..<bottom {
                for column in 0..<columns {
                    cells[cellIndex(row: row, column: column)] = cells[cellIndex(row: row + 1, column: column)]
                }
            }
        }

        for column in 0..<columns {
            cells[cellIndex(row: bottom, column: column)] = .blank
        }

        activeCells = cells
    }
}
