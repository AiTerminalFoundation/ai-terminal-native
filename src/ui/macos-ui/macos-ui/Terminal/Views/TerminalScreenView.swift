import SwiftUI

struct TerminalScreenView: View {
    @ObservedObject var session: TerminalSession
    @FocusState private var inputFocused: Bool

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                VStack(alignment: .leading) {
                    Text(session.output)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled)
                        .padding(.horizontal, 5)

                    TerminalInputRowView(session: session, inputFocused: $inputFocused)
                        .padding(.horizontal, 5)

                    Color.clear
                        .frame(height: 1)
                        .id("bottom")
                }
            }
            .onChange(of: session.output) {
                proxy.scrollTo("bottom")
            }
            .onAppear {
                inputFocused = true
            }
            .terminalFocusKeyHandler(session: session, inputFocused: $inputFocused)
        }
    }
}
