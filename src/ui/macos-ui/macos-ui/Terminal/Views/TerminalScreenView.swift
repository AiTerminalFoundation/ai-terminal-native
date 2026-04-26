import SwiftUI

struct TerminalScreenView: View {
    @ObservedObject var session: TerminalSession
    @FocusState private var terminalFocused: Bool
    @State private var cursorVisible = true

    private var renderedOutput: String {
        session.renderSnapshot.renderedText(showCursor: terminalFocused && cursorVisible)
    }

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                VStack(alignment: .leading) {
                    Text(renderedOutput)
                        .font(.system(size: 13, design: .monospaced))
                        .lineSpacing(0)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled)
                        .padding(.horizontal, 5)

                    Color.clear
                        .frame(height: 1)
                        .id("bottom")
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .contentShape(Rectangle())
            }
            .focusable()
            .focused($terminalFocused)
            .terminalInputKeyHandler(session: session)
            .onChange(of: session.renderSnapshot) {
                proxy.scrollTo("bottom")
            }
            .onAppear {
                terminalFocused = true
            }
            .onTapGesture {
                terminalFocused = true
            }
            .overlay(alignment: .topTrailing) {
                FPSCounterView()
                    .padding(8)
            }
            .task {
                while !Task.isCancelled {
                    try? await Task.sleep(for: .milliseconds(550))
                    cursorVisible.toggle()
                }
            }
        }
    }
}
