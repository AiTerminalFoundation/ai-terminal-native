//
//  TerminalSizeObserver.swift
//  macos-ui
//
//  Created by Michele Verriello on 19/06/2026.
//
import SwiftUI

struct WindowSizeDetector: ViewModifier {
    let onChange: (CGSize) -> Void

    func body(content: Content) -> some View {
        content
            .background {
                GeometryReader { proxy in
                    Color.clear
                        .onAppear {
                            // startup size
                            onChange(proxy.size)
                        }
                        .onChange(of: proxy.size) { _, newSize in
                            // resize events
                            onChange(newSize)
                        }
                }
            }
    }
}

extension View {
    func detectWindowSize(_ onChange: @escaping (CGSize) -> Void) -> some View {
        modifier(WindowSizeDetector(onChange: onChange))
    }
}
