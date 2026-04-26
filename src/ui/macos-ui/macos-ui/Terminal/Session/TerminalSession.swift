import Foundation
internal import Combine

final class TerminalSession: ObservableObject {
    @Published private(set) var renderSnapshot: TerminalRenderSnapshot = .empty

    private var screenBuffer = TerminalScreenBuffer()
    private var parser = TerminalParser()
    private var master_fd: Int32 = -1
    private var slave_fd: Int32 = -1
    private var session_id_c_string: UnsafeMutablePointer<CChar>? = nil
    @Published var session_id: String = ""
    private var isRunning = false
    private var isClosed = false
    
    private static let outputCallback: @convention(c) (UnsafePointer<CChar>?, Int, UnsafeMutableRawPointer?) -> Void = { buffer, nBytes, context in
                
        guard let context = context else { return }
        let instance = Unmanaged<TerminalSession>.fromOpaque(context).takeUnretainedValue()
        
        
        if let buffer = buffer, nBytes > 0 {
            let data = Data(bytes: buffer, count: nBytes)

            DispatchQueue.main.async {
                instance.processOutput(data)
            }
        }
    }

    func start() {
        guard !isRunning else { return }

        resetSessionState()

        let result = create_pseudoterminal(&master_fd, &slave_fd, &session_id_c_string)
        
        // convert to Swift String
        if let session_id_c_string {
            session_id = String(cString: session_id_c_string)
            free(session_id_c_string)  // free the malloc'd C string
            self.session_id_c_string = nil
        }

        
        if result == 0 {
            isRunning = true
            isClosed = false

            let forkResult = fork_and_exec_shell(master_fd, slave_fd)
            
            if forkResult < 0 {
                processOutput(Data("SHELL FAILED TO START".utf8))
                stop()
                return
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
            
            
        } else {
            DispatchQueue.main.async {
                self.processOutput(Data("PTY FAILED".utf8))
            }
        }
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
                let result = send_input(
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

        let master = master_fd
        isClosed = true
        isRunning = false
        resetDescriptors()

        guard master >= 0 else { return }
        close_terminal_session(master)
    }

    deinit {
        stop()
    }

    private func processOutput(_ data: Data) {
        parser.parse(data, into: &screenBuffer)
        renderSnapshot = screenBuffer.snapshot
    }

    private func resetSessionState() {
        resetDescriptors()
        screenBuffer.reset()
        parser.reset()
        renderSnapshot = screenBuffer.snapshot
        session_id_c_string = nil
        session_id = ""
        isRunning = false
        isClosed = false
    }

    private func resetDescriptors() {
        master_fd = -1
        slave_fd = -1
    }
}
