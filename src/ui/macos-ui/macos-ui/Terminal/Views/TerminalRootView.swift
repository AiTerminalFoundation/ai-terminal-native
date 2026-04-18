//
//  TerminalRootView.swift
//  macos-ui
//
//  Created by Michele Verriello on 22/02/26.
//

import SwiftUI
internal import Combine

struct TerminalRootView: View {
    @StateObject private var tabs = TerminalTabManager()

    var body: some View {
        VStack(spacing: 0) {
            TerminalTabBarView(tabs: tabs)

            if let selectedTab = tabs.selectedTab {
                TerminalScreenView(session: selectedTab.session)
                    .id(selectedTab.id)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                Color.clear
            }
        }
        .frame(minWidth: 720, minHeight: 480)
    }
}
