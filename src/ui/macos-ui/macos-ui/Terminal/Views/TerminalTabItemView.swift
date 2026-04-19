import SwiftUI

struct TerminalTabItemView: View {
    let tab: TerminalTab
    let isSelected: Bool
    let onSelect: () -> Void
    let onClose: () -> Void
    @ObservedObject private var session: TerminalSession

    init(tab: TerminalTab, isSelected: Bool, onSelect: @escaping () -> Void, onClose: @escaping () -> Void) {
        self.tab = tab
        self.isSelected = isSelected
        self.onSelect = onSelect
        self.onClose = onClose
        _session = ObservedObject(wrappedValue: tab.session)
    }

    private var title: String {
        let prompt = session.currentPrompt.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !prompt.isEmpty else { return tab.baseTitle }

        let lastPathComponent = URL(fileURLWithPath: prompt).lastPathComponent
        return lastPathComponent.isEmpty ? prompt : lastPathComponent
    }

    var body: some View {
        HStack(spacing: 8) {
            Text(title)
                .lineLimit(1)
                .foregroundStyle(isSelected ? Color.primary : Color.secondary)

            Button {
                onClose()
            } label: {
                Image(systemName: "xmark")
                    .font(.system(size: 10, weight: .medium))
                    .frame(width: 12, height: 12)
            }
            .buttonStyle(.plain)
            .foregroundStyle(isSelected ? Color.primary : Color.secondary)
            .opacity(isSelected ? 0.9 : 0.55)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(isSelected ? Color.accentColor.opacity(0.18) : Color.clear)
        .overlay {
            RoundedRectangle(cornerRadius: 6)
                .stroke(isSelected ? Color.accentColor.opacity(0.55) : Color.clear, lineWidth: 1)
        }
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .contentShape(RoundedRectangle(cornerRadius: 6))
        .onTapGesture(perform: onSelect)
    }
}
