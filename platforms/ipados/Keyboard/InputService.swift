import Foundation

struct KeyboardState {
    var preedit = ""
    var candidates: [String] = []
    var selected = 0
    var ascii = false
    var previousPage = false
    var nextPage = false
    var ready = false
    var error: String? = nil
    var revision: UInt64 = 0

    init(preedit: String = "", candidates: [String] = [], selected: Int = 0,
         ascii: Bool = false, previousPage: Bool = false, nextPage: Bool = false,
         ready: Bool = false, error: String? = nil) {
        self.preedit = preedit; self.candidates = candidates; self.selected = selected
        self.ascii = ascii; self.previousPage = previousPage; self.nextPage = nextPage
        self.ready = ready; self.error = error
    }
    init(_ snapshot: MDInputSnapshot) {
        preedit = snapshot.preedit; candidates = snapshot.candidates
        selected = snapshot.selectedIndex; ascii = snapshot.asciiMode
        previousPage = snapshot.hasPreviousPage; nextPage = snapshot.hasNextPage
        ready = true
    }
}

struct InputDelivery {
    var state: KeyboardState
    var text = ""
    var deleteBackward = false
}

final class InputService {
    // Rime's process-wide runtime and every session share one queue.
    private static let queue = DispatchQueue(label: "org.leavesandstone.ipados.input", qos: .userInteractive)
    private var bridge: MDRimeBridge?
    private var started = false
    private var epoch: UInt64 = 0 // main-thread only
    private var revision: UInt64 = 0 // input queue only
    private var asciiMode = false // input queue only
    private let dataURL: URL?
    private let userURL: URL
    var onDelivery: ((InputDelivery) -> Void)?

    init(dataURL: URL? = Bundle(for: MDRimeBridge.self).url(forResource: "Rime", withExtension: nil),
         userURL: URL = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Rime", isDirectory: true)) {
        self.dataURL = dataURL
        self.userURL = userURL
    }

    func start() {
        dispatchPrecondition(condition: .onQueue(.main))
        guard !started else { return }
        started = true
        let token = epoch
        Self.queue.async { [weak self] in
            guard let self else { return }
            do {
                guard let dataURL = self.dataURL else {
                    throw NSError(domain: "XiaomaodouInput", code: 2)
                }
                self.bridge = try MDRimeBridge(dataPath: dataURL.path, userPath: self.userURL.path)
                self.apply(.clear, text: "", index: 0, token: token)
            } catch {
                self.emitFailure(token: token)
            }
        }
    }

    func send(_ action: MDInputAction, text: String = "", index: Int = 0,
              expectedRevision: UInt64? = nil, asciiText: String? = nil) {
        dispatchPrecondition(condition: .onQueue(.main))
        let token = epoch
        Self.queue.async { [weak self] in
            guard let self else { return }
            guard action != .select || expectedRevision == self.revision else { return }
            let actualText = action == .key && !self.asciiMode ? text.lowercased() :
                (self.asciiMode ? (asciiText ?? text) : text)
            self.apply(action, text: actualText, index: index, token: token)
        }
    }

    func invalidateContext() {
        dispatchPrecondition(condition: .onQueue(.main))
        epoch &+= 1
        send(.clear)
    }

    private func apply(_ action: MDInputAction, text: String, index: Int, token: UInt64) {
        guard let bridge else { emitFailure(token: token); return }
        let update = bridge.apply(action, text: text, index: index)
        asciiMode = update.snapshot.asciiMode
        revision &+= 1
        emit(update, token: token, revision: revision)
    }

    private func emit(_ update: MDInputUpdate, token: UInt64, revision: UInt64) {
        var state = KeyboardState(update.snapshot)
        state.revision = revision
        let delivery = InputDelivery(state: state,
                                     text: update.committedText,
                                     deleteBackward: update.shouldDeleteBackward)
        DispatchQueue.main.async { [weak self] in
            guard let self, token == self.epoch else { return }
            self.onDelivery?(delivery)
        }
    }

    private func emitFailure(token: UInt64) {
        DispatchQueue.main.async { [weak self] in
            guard let self, token == self.epoch else { return }
            self.onDelivery?(InputDelivery(state: KeyboardState(error: "词库暂未就绪，请重新打开键盘")))
        }
    }

    deinit {
        // Destroying a session also calls the process-wide Rime API.
        let retainedBridge = bridge
        Self.queue.async { withExtendedLifetime(retainedBridge) {} }
    }
}
