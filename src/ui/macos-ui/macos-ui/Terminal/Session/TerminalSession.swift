//
//  TerminalSession.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//


import Foundation
internal import Combine

final class TerminalSession: ObservableObject {

    // Matches \x01__PROMPT__:user@/path\x02
    // - "__PROMPT__:" is a unique literal prefix, unlikely to appear in normal terminal output
    // - "([^>]+)" captures one or more characters that are not ">", which is the current folder path
    // - ">" is the closing delimiter of the sentinel
    // Capture group 1 contains the folder path (e.g. /Users/you/project)
    // \x01 (SOH) and \x02 (STX) are control characters used as unique delimiters
    // that can never appear in normal shell output, preventing false matches
    private static let promptPattern = #"\x01__PROMPT__:([^\x02]+)\x02"#
    private static let ansiPattern = #"\x1B(\[[0-9;?]*[A-Za-z]|\][^\x07]*\x07|[()][AB])"#
    private static let promptRegex = try? NSRegularExpression(pattern: promptPattern)

    @Published var output: String = ""
    @Published var currentPrompt: String = ""
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
            
            if let chunk = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .ascii) {
                DispatchQueue.main.async {
                    let cleaned = chunk
                        .replacingOccurrences(of: ansiPattern, with: "", options: .regularExpression)

                    instance.appendOutputChunk(cleaned)
                }
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
                output = "SHELL FAILED TO START"
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
                self.output = "PTY FAILED"
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

    private func appendOutputChunk(_ cleaned: String) {
        guard let regex = TerminalSession.promptRegex else {
            output += cleaned
            return
        }

        let nsRange = NSRange(cleaned.startIndex..., in: cleaned)
        let matches = regex.matches(in: cleaned, range: nsRange)

        if matches.isEmpty {
            output += cleaned
            return
        }

        var visibleStart = cleaned.startIndex

        for match in matches {
            guard
                let fullMatchRange = Range(match.range(at: 0), in: cleaned),
                let folderRange = Range(match.range(at: 1), in: cleaned)
            else {
                continue
            }

            output += String(cleaned[visibleStart..<fullMatchRange.lowerBound])
            currentPrompt = String(cleaned[folderRange])
            visibleStart = fullMatchRange.upperBound
        }

        output += String(cleaned[visibleStart...])
    }

    private func resetSessionState() {
        resetDescriptors()
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
