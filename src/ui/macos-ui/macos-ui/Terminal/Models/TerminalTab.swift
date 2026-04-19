import Foundation

final class TerminalTab: Identifiable {
    let baseTitle: String
    let session: TerminalSession

    var id: String {
        session.session_id
    }

    init(baseTitle: String, session: TerminalSession) {
        self.baseTitle = baseTitle
        self.session = session
    }
}
