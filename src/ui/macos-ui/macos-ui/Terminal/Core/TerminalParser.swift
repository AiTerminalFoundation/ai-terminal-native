import Foundation

struct TerminalParser {
    private enum State {
        case ground
        case escape
        case characterSet
        case osc
        case oscEscape
        case csi(String)
    }

    private var state: State = .ground

    mutating func parse(_ data: Data, into screen: inout TerminalScreenBuffer) {
        let chunk = String(decoding: data, as: UTF8.self)

        for scalar in chunk.unicodeScalars {
            parse(scalar, into: &screen)
        }
    }

    mutating func reset() {
        state = .ground
    }

    private mutating func parse(_ scalar: Unicode.Scalar, into screen: inout TerminalScreenBuffer) {
        switch state {
        case .ground:
            parseGround(scalar, into: &screen)
        case .escape:
            parseEscape(scalar, into: &screen)
        case .characterSet:
            state = .ground
        case .osc:
            parseOSC(scalar)
        case .oscEscape:
            state = scalar == "\\" ? .ground : .osc
        case .csi(let sequence):
            parseCSI(scalar, sequence: sequence, into: &screen)
        }
    }

    private mutating func parseGround(_ scalar: Unicode.Scalar, into screen: inout TerminalScreenBuffer) {
        switch scalar.value {
        case 0x08, 0x7F:
            screen.backspace()
        case 0x09:
            screen.tab()
        case 0x0A, 0x0B, 0x0C:
            screen.lineFeed()
        case 0x0D:
            screen.carriageReturn()
        case 0x1B:
            state = .escape
        case 0x00...0x1F:
            break
        default:
            screen.put(scalar)
        }
    }

    private mutating func parseEscape(_ scalar: Unicode.Scalar, into screen: inout TerminalScreenBuffer) {
        switch scalar {
        case "[":
            state = .csi("")
        case "]":
            state = .osc
        case "7":
            screen.saveCursor()
            state = .ground
        case "8":
            screen.restoreCursor()
            state = .ground
        case "c":
            screen.reset()
            state = .ground
        case "M":
            screen.moveCursorBy(rowDelta: -1, columnDelta: 0)
            state = .ground
        case "(", ")":
            state = .characterSet
        default:
            state = .ground
        }
    }

    private mutating func parseOSC(_ scalar: Unicode.Scalar) {
        switch scalar.value {
        case 0x07:
            state = .ground
        case 0x1B:
            state = .oscEscape
        default:
            break
        }
    }

    private mutating func parseCSI(
        _ scalar: Unicode.Scalar,
        sequence: String,
        into screen: inout TerminalScreenBuffer
    ) {
        if scalar.value >= 0x40 && scalar.value <= 0x7E {
            dispatchCSI(sequence: sequence, final: scalar, into: &screen)
            state = .ground
        } else {
            state = .csi(sequence + String(scalar))
        }
    }

    private func dispatchCSI(sequence: String, final: Unicode.Scalar, into screen: inout TerminalScreenBuffer) {
        let isPrivate = sequence.hasPrefix("?")
        let cleanSequence = isPrivate ? String(sequence.dropFirst()) : sequence
        let params = parseParameters(cleanSequence)

        switch final {
        case "A":
            screen.moveCursorBy(rowDelta: -parameter(params, at: 0, defaultValue: 1), columnDelta: 0)
        case "B":
            screen.moveCursorBy(rowDelta: parameter(params, at: 0, defaultValue: 1), columnDelta: 0)
        case "C":
            screen.moveCursorBy(rowDelta: 0, columnDelta: parameter(params, at: 0, defaultValue: 1))
        case "D":
            screen.moveCursorBy(rowDelta: 0, columnDelta: -parameter(params, at: 0, defaultValue: 1))
        case "E":
            screen.moveCursorBy(rowDelta: parameter(params, at: 0, defaultValue: 1), columnDelta: 0)
            screen.carriageReturn()
        case "F":
            screen.moveCursorBy(rowDelta: -parameter(params, at: 0, defaultValue: 1), columnDelta: 0)
            screen.carriageReturn()
        case "G":
            screen.setCursorColumn(parameter(params, at: 0, defaultValue: 1) - 1)
        case "H", "f":
            let row = parameter(params, at: 0, defaultValue: 1) - 1
            let column = parameter(params, at: 1, defaultValue: 1) - 1
            screen.moveCursor(row: row, column: column)
        case "J":
            screen.clearScreen(mode: parameter(params, at: 0, defaultValue: 0))
        case "K":
            screen.clearLine(mode: parameter(params, at: 0, defaultValue: 0))
        case "h":
            setMode(params, enabled: true, isPrivate: isPrivate, screen: &screen)
        case "l":
            setMode(params, enabled: false, isPrivate: isPrivate, screen: &screen)
        case "m":
            break
        case "r":
            screen.setScrollRegion(top: params.first, bottom: params.dropFirst().first)
        case "s":
            screen.saveCursor()
        case "u":
            screen.restoreCursor()
        default:
            break
        }
    }

    private func parseParameters(_ sequence: String) -> [Int] {
        sequence
            .split(separator: ";", omittingEmptySubsequences: false)
            .map { Int($0) ?? 0 }
    }

    private func parameter(_ params: [Int], at index: Int, defaultValue: Int) -> Int {
        guard params.indices.contains(index), params[index] != 0 else {
            return defaultValue
        }

        return params[index]
    }

    private func setMode(
        _ params: [Int],
        enabled: Bool,
        isPrivate: Bool,
        screen: inout TerminalScreenBuffer
    ) {
        guard isPrivate else { return }

        for parameter in params {
            switch parameter {
            case 25:
                screen.setCursorVisible(enabled)
            case 47, 1047, 1049:
                screen.useAlternateScreen(enabled, clear: true)
            default:
                break
            }
        }
    }
}
