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
    var tabName: String = "Terminal"
    
    private var master_fd: Int32 = 0
    private var slave_fd: Int32 = 0

    func start() {
        let res = create_pseudoterminal(&master_fd, &slave_fd)
        
        if res == 0 {
            DispatchQueue.main.async {
                self.output += "PTY OK: master_fd: \(self.master_fd), slave_fd: \(self.slave_fd)\n"
            }

            let fork_res = fork_and_exec_shell(master_fd, slave_fd)
            
            DispatchQueue.main.async {
                self.output += "Fork result: \(fork_res)\n"
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
