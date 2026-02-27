//
//  TerminalSession.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//


import Foundation
internal import Combine

final class TerminalSession: ObservableObject {

    @Published var output: String = ""
    @Published var currentFolder: String = ""
    private var master_fd: Int32 = 0
    private var slave_fd: Int32 = 0
    
    private static let outputCallback: @convention(c) (UnsafePointer<CChar>?, Int, UnsafeMutableRawPointer?) -> Void = { buffer, nBytes, context in
                
        guard let context = context else { return }
        let instance = Unmanaged<TerminalSession>.fromOpaque(context).takeUnretainedValue()
        
        
        if let buffer = buffer, nBytes > 0 {
            let data = Data(bytes: buffer, count: nBytes)
            
            if let chunk = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .ascii) {
                DispatchQueue.main.async {
                    instance.output += chunk
                    
                    // getting index of last \n and removing the next char sequence
                    if let range = instance.output.range(of: "\n", options: .backwards) {
                        // extract the next char sequence
                        // the next char sequence will be the current folder
                        let nextCharSequence = instance.output[instance.output.index(after: range.lowerBound)...]
                        instance.currentFolder = String(nextCharSequence)
                        
                        instance.output.removeSubrange(range.lowerBound..<instance.output.endIndex)
                    }
                    
                }
            }
        }
    }

    func start() {
        let result = create_pseudoterminal(&master_fd, &slave_fd)
        
        if result == 0 {
            DispatchQueue.main.async {
                self.output += "PTY OK: master_fd: \(self.master_fd), slave_fd: \(self.slave_fd)\n"
            }

            let forkResult = fork_and_exec_shell(master_fd, slave_fd)
            
            DispatchQueue.main.async {
                self.output += "Fork result: \(forkResult)\n"
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
