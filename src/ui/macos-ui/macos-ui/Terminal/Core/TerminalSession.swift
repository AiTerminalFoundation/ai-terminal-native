import Foundation
import Darwin
internal import Combine

final class TerminalSession: ObservableObject {

    private var master_fd: Int32 = -1
    private var slave_fd: Int32 = -1
    private var session_id_c_string: UnsafeMutablePointer<CChar>? = nil
    @Published private(set) var session_id: String = ""
    @Published private(set) var cells: [TerminalCell] = []
    @Published private(set) var screenRows: Int = 0
    @Published private(set) var screenColumns: Int = 0
    @Published private(set) var cursorRow: Int = 0
    @Published private(set) var cursorColumn: Int = 0
    private var isRunning = false
    private var isClosed = false
    
    private struct ScreenUpdate {
        let cells: [TerminalCell]
        let rows: Int
        let columns: Int
        let cursorRow: Int
        let cursorColumn: Int
    }

    private static let outputCallback: @convention(c) (UnsafePointer<TerminalScreenSnapshot>?, UnsafeMutableRawPointer?) -> Void = { snapshot, context in
        guard let context = context else { return }
        let instance = Unmanaged<TerminalSession>.fromOpaque(context).takeUnretainedValue()

        if let snapshot {
            let update = TerminalSession.makeScreenUpdate(from: snapshot.pointee)
            DispatchQueue.main.async {
                instance.apply(update: update)
            }
        }
    }

    func start(size: TerminalSize? = nil) {
        guard !isRunning else { return }

        let result = create_pseudoterminal(&master_fd, &slave_fd, &session_id_c_string)

        guard result == 0 else {
            failStartup()
            return
        }

        guard let newSessionId = convertCStringToSwiftString(&session_id_c_string) else {
            failStartup()
            return
        }

        session_id = newSessionId
        isRunning = true
        isClosed = false

        let forkResult = fork_and_exec_shell(master_fd, slave_fd)

        if forkResult < 0 {
            failStartup()
            return
        }

        // fork_and_exec_shell closes the slave fd in the parent process.
        slave_fd = -1

        if let size {
            resize(to: size)
        }

        // Keep the session alive until the read loop exits so closing a tab cannot
        // leave C callbacks pointing at a deallocated Swift object.
        let context = Unmanaged.passRetained(self).toOpaque()
        DispatchQueue.global(qos: .userInitiated).async { [master = self.master_fd] in
            read_loop(master, TerminalSession.outputCallback, context)
            let session = Unmanaged<TerminalSession>.fromOpaque(context).takeUnretainedValue()
            DispatchQueue.main.async {
                session.isRunning = false
            }
            Unmanaged<TerminalSession>.fromOpaque(context).release()
        }
    }

    func resize(to size: TerminalSize) {
        guard isRunning, !isClosed, master_fd >= 0 else { return }

        if resize_terminal_screen(Int32(size.rows), Int32(size.cols)) == 0 {
            apply(update: TerminalSession.makeScreenUpdate(from: get_terminal_screen_snapshot()))
        }

        _ = resize_terminal_window(
            master_fd,
            Int32(size.rows),
            Int32(size.cols),
            Int32(size.width.rounded()),
            Int32(size.height.rounded())
        )
    }
    
    func send_input_string(input: String) {
        guard isRunning, !isClosed else { return }

        let inputData = Array(input.utf8CString)
        let inputLength = inputData.count - 1

        guard inputLength > 0 else { return }

        inputData.withUnsafeBufferPointer { buffer in
            guard let baseAddress = buffer.baseAddress else { return }

            var bytesSent = 0

            while bytesSent < inputLength {
                let result = write_bytes(
                    UnsafeMutablePointer(mutating: baseAddress.advanced(by: bytesSent)),
                    master_fd,
                    inputLength - bytesSent
                )

                if result <= 0 {
                    DispatchQueue.main.async {
                        self.isRunning = false
                    }
                    return
                }

                bytesSent += result
            }
        }
    }

    func stop() {
        guard !isClosed else { return }

        isClosed = true
        isRunning = false
        closeCurrentDescriptors()
    }

    deinit {
        stop()
    }

    private func resetDescriptors() {
        master_fd = -1
        slave_fd = -1
    }

    private static func makeScreenUpdate(from snapshot: TerminalScreenSnapshot) -> ScreenUpdate {
        let rows = Int(snapshot.rows)
        let columns = Int(snapshot.columns)
        let cellsCount = max(0, rows * columns)
        let cellPointer = snapshot.cells

        var cells: [TerminalCell] = []
        cells.reserveCapacity(cellsCount)

        if let cellPointer {
            for index in 0..<cellsCount {
                let cell = cellPointer[index]
                cells.append(
                    TerminalCell(
                        id: index,
                        codepoint: cell.codepoint,
                        isEmpty: cell.is_empty != 0,
                        isBold: cell.is_bold != 0,
                        color: cell.color
                    )
                )
            }
        }

        return ScreenUpdate(
            cells: cells,
            rows: rows,
            columns: columns,
            cursorRow: Int(snapshot.cursor_row),
            cursorColumn: Int(snapshot.cursor_column)
        )
    }

    private func apply(update: ScreenUpdate) {
        cells = update.cells
        screenRows = update.rows
        screenColumns = update.columns
        cursorRow = update.cursorRow
        cursorColumn = update.cursorColumn
    }

    private func failStartup() {
        isRunning = false
        closeCurrentDescriptors()
        isClosed = true
    }

    private func closeCurrentDescriptors() {
        let master = master_fd
        let slave = slave_fd
        resetDescriptors()

        if slave >= 0 {
            close(slave)
        }

        if master >= 0 {
            close_terminal_session(master)
        }
    }
}
