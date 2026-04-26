import SwiftUI

struct FPSCounterView: View {
    @State private var frameCount = 0
    @State private var lastSample = Date()
    @State private var fps = 0

    var body: some View {
        TimelineView(.animation) { context in
            Text("\(fps) FPS")
                .font(.system(size: 11, weight: .semibold, design: .monospaced))
                .foregroundStyle(.white)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background {
                    RoundedRectangle(cornerRadius: 5)
                        .fill(Color.black.opacity(0.72))
                }
                .overlay {
                    RoundedRectangle(cornerRadius: 5)
                        .stroke(Color.white.opacity(0.18), lineWidth: 1)
                }
                .onChange(of: context.date) {
                    sampleFrame(at: context.date)
                }
        }
    }

    private func sampleFrame(at date: Date) {
        frameCount += 1

        let elapsed = date.timeIntervalSince(lastSample)
        guard elapsed >= 1 else { return }

        fps = Int(Double(frameCount) / elapsed)
        frameCount = 0
        lastSample = date
    }
}
