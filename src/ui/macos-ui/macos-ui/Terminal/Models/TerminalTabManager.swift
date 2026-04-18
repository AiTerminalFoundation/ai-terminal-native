import Foundation
internal import Combine

final class TerminalTabManager: ObservableObject {
    @Published private(set) var tabs: [TerminalTab] = []
    @Published var selectedTabId: String?

    init() {
        addTab()
    }

    var selectedTab: TerminalTab? {
        guard let selectedTabId else { return tabs.first }
        return tabs.first(where: { $0.id == selectedTabId }) ?? tabs.first
    }

    func addTab() {
        let session = TerminalSession()
        let title = "New Tab"

        session.start()

        let tab = TerminalTab(baseTitle: title, session: session)
        tabs.append(tab)
        selectedTabId = tab.id
    }

    func closeSelectedTab() {
        guard let selectedTab else { return }
        closeTab(selectedTab)
    }

    func closeTab(_ tab: TerminalTab) {
        guard tabs.count > 1 else { return }
        guard let index = tabs.firstIndex(where: { $0.id == tab.id }) else { return }
        
        tab.session.stop()
        tabs.remove(at: index)

        if selectedTabId == tab.id {
            let nextIndex = min(index, tabs.count - 1)
            selectedTabId = tabs[nextIndex].id
        }
    }

    func selectTab(_ tab: TerminalTab) {
        selectedTabId = tab.id
    }
}
