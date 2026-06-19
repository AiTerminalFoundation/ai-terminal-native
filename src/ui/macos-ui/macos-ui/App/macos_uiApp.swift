//
//  macos_uiApp.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//

import SwiftUI

@main
struct macos_uiApp: App {
    var body: some Scene {
        WindowGroup {
            TerminalRootView()
                .environment(\.font, .custom(DefaultSettings.fontName, size: DefaultSettings.fontSize))
        }
        .defaultSize(width: DefaultSettings.windowWidth, height: DefaultSettings.windowHeight)
    }
}
