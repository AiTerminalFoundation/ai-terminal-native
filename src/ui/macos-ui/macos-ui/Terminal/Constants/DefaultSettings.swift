//
//  DefaultSettings.swift
//  macos-ui
//
//  Created by Michele Verriello on 19/06/2026.
//

import AppKit
import Foundation

enum DefaultSettings {
    static let windowWidth: CGFloat = 900
    static let windowHeight: CGFloat = 600
    static let minimumWindowWidth: CGFloat = 600
    static let minimumWindowHeight: CGFloat = 600
    
    static let fontSize: CGFloat = 13
    static let fontName: String = "JetBrainsMono-Regular"

    static let terminalFont: NSFont = NSFont(name: fontName, size: fontSize)
        ?? NSFont.monospacedSystemFont(ofSize: fontSize, weight: .regular)

    static let cellWidth: CGFloat = ceil(("W" as NSString).size(withAttributes: [.font: terminalFont]).width)
    static let cellHeight: CGFloat = ceil(terminalFont.ascender - terminalFont.descender + terminalFont.leading)

    static let columns: Int = max(1, Int(windowWidth / cellWidth))
    static let rows: Int = max(1, Int(windowHeight / cellHeight))
}
