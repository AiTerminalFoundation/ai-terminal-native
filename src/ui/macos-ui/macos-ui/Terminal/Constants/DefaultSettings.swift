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
    
    static let fontSize: CGFloat = 13
    static let fontName: String = "JetBrainsMono-Regular"
    
    static let columns: Int = Int(DefaultSettings.windowWidth / DefaultSettings.fontSize)
    static let rows: Int = Int(DefaultSettings.windowHeight / DefaultSettings.fontSize)
}
