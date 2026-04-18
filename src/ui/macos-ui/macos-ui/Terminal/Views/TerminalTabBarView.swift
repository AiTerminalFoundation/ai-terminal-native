import SwiftUI

struct TerminalTabBarView: View {
    @ObservedObject var tabs: TerminalTabManager

    var body: some View {
        HStack(spacing: 6) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    ForEach(tabs.tabs) { tab in
                        TerminalTabItemView(
                            tab: tab,
                            isSelected: tabs.selectedTabId == tab.id,
                            onSelect: { tabs.selectTab(tab) },
                            onClose: { tabs.closeTab(tab) }
                        )
                    }
                }
                .padding(.horizontal, 8)
                .padding(.vertical, 6)
            }

            Button {
                tabs.addTab()
            } label: {
                Image(systemName: "plus")
                    .frame(width: 28, height: 24)
            }
            .buttonStyle(.plain)
            .keyboardShortcut("t", modifiers: .command)

            Button {
                tabs.closeSelectedTab()
            } label: {
                Image(systemName: "minus")
                    .frame(width: 28, height: 24)
            }
            .buttonStyle(.plain)
            .keyboardShortcut("w", modifiers: .command)
            .padding(.trailing, 8)
        }
        .background(Color(NSColor.windowBackgroundColor))
        .overlay(alignment: .bottom) {
            Divider()
        }
    }
}
