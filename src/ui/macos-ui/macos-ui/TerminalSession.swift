//
//  TerminalSession.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//


import Foundation
internal import Combine

final class TerminalSession: ObservableObject {

    @Published var output: String = "Starting..."

    private var master: Int32 = 0
    private var slave: Int32 = 0

    func start() {
        let current_pid: Int32 = ProcessInfo.processInfo.processIdentifier
        let res = create_pseudoterminal(&master, &slave)
        
        DispatchQueue.main.async {
            self.output += "Current App PID: \(current_pid)\n"
        }

        if res == 0 {
            DispatchQueue.main.async {
                self.output += "PTY OK: master_fd: \(self.master), slave_fd: \(self.slave)\n"
            }

            let fork_res = fork_and_exec_shell(master, slave)
            
            DispatchQueue.main.async {
                self.output += "Fork result: \(fork_res)\n"
            }
            
        } else {
            DispatchQueue.main.async {
                self.output = "PTY FAILED"
            }
        }
    }
}
