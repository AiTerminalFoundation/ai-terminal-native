//
//  TerminalSession.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//


import Foundation
internal import Combine

final class TerminalSession: ObservableObject {

    // Matches the sentinel prompt format: __PROMPT__:/some/path>
    // - "__PROMPT__:" is a unique literal prefix, unlikely to appear in normal terminal output
    // - "([^>]+)" captures one or more characters that are not ">", which is the current folder path
    // - ">" is the closing delimiter of the sentinel
    // Capture group 1 contains the folder path (e.g. /Users/you/project)
    private static let promptPattern = #"__PROMPT__:([^>]+)>"#
    private static let ansiPattern = #"\x1B(\[[0-9;?]*[A-Za-z]|\][^\x07]*\x07|[()][AB])"#

    @Published var output: String = ""
    @Published var currentPrompt: String = ""
    private var master_fd: Int32 = 0
    private var slave_fd: Int32 = 0
    
    private static let outputCallback: @convention(c) (UnsafePointer<CChar>?, Int, UnsafeMutableRawPointer?) -> Void = { buffer, nBytes, context in
                
        guard let context = context else { return }
        let instance = Unmanaged<TerminalSession>.fromOpaque(context).takeUnretainedValue()
        
        
        if let buffer = buffer, nBytes > 0 {
            let data = Data(bytes: buffer, count: nBytes)
            
            if let chunk = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .ascii) {
                DispatchQueue.main.async {
                    let cleaned = chunk.replacingOccurrences(of: ansiPattern, with: "", options: .regularExpression)
                    instance.output += cleaned
                    
                    // Search for the last occurrence of the sentinel prompt using regex.
                    // We use the last match in case multiple prompts accumulated in the buffer.
                    if let regex = try? NSRegularExpression(pattern: TerminalSession.promptPattern),
                       let match = regex.matches(
                           in: instance.output,
                           range: NSRange(instance.output.startIndex..., in: instance.output)
                       ).last,
                       let folderRange = Range(match.range(at: 1), in: instance.output),
                       let fullMatchRange = Range(match.range(at: 0), in: instance.output) {

                        // Group 1 contains the expanded path, e.g. /Users/you/project
                        instance.currentPrompt = String(instance.output[folderRange])

                        // Strip the sentinel and everything after it from the visible output
                        instance.output.removeSubrange(fullMatchRange.lowerBound..<instance.output.endIndex)
                    }
                }
            }
        }
    }

    func start() {
        let result = create_pseudoterminal(&master_fd, &slave_fd)
        
        if result == 0 {

            let forkResult = fork_and_exec_shell(master_fd, slave_fd)
            
            if forkResult < 0 {
                fatalError("Couldn't fork and execute shell")
            }
            
            // converts self into a raw void* pointer so it can be passed to C.
            // passUnretained doesn't increment the reference count, given that C doesn't own it
            let context = Unmanaged.passUnretained(self).toOpaque()
            DispatchQueue.global(qos: .userInitiated).async { [master = self.master_fd] in
                read_loop(master, TerminalSession.outputCallback, context)
            }
        } else {
            DispatchQueue.main.async {
                self.output = "PTY FAILED"
            }
        }
    }
    
    func send_input_string(input: String) {
        // Ensure we send a null-terminated UTF-8 buffer to the C API expecting `char *`.
        input.withCString { cString in
            // Compute the length excluding the null terminator
            let length = strlen(cString)
            // `send_input` expects an UnsafeMutablePointer<CChar>. We can safely cast away mutability
            // here because we don't expect `send_input` to modify the buffer; if it does, we should copy.
            send_input(UnsafeMutablePointer(mutating: cString), master_fd, Int(length))
        }
    }
}
