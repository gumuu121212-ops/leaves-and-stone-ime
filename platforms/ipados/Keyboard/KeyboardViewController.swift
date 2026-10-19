import UIKit

class KeyboardViewController: UIInputViewController {
    private let surface = KeyboardSurface()
    private let input = InputService()
    private var height: NSLayoutConstraint?
    private var observed: DocumentStamp?
    private var writing = false

    private struct DocumentStamp: Equatable {
        let identifier: UUID
        let before: String?
        let after: String?
        let selected: String?
    }

    override func loadView() {
        let keyboard = UIInputView(frame: .zero, inputViewStyle: .keyboard)
        keyboard.allowsSelfSizing = true
        view = keyboard
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = MaodouTheme.background
        view.addSubview(surface)
        surface.onAction = { [weak self] action in self?.handle(action) }
        surface.globeButton.addTarget(self, action: #selector(handleInputModeList(from:with:)), for: .allTouchEvents)
        input.onDelivery = { [weak self] delivery in self?.receive(delivery) }
        observed = stamp()
        height = view.heightAnchor.constraint(equalToConstant: surface.preferredHeight(for: 768))
        height?.priority = .init(999)
        height?.isActive = true
        input.start()
        surface.render(KeyboardState())
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        synchronizeDocument()
        surface.globeButton.isHidden = !needsInputModeSwitchKey
        updateReturnTitle()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        guard view.bounds.width > 0 else { return }
        let bottom = view.safeAreaInsets.bottom
        let desired = surface.preferredHeight(for: view.bounds.width) + bottom
        if height?.constant != desired { height?.constant = desired }
        surface.frame = CGRect(x: 0, y: 0, width: view.bounds.width, height: max(0, view.bounds.height - bottom))
    }

    override func viewWillDisappear(_ animated: Bool) {
        surface.stopInteraction()
        input.invalidateContext()
        super.viewWillDisappear(animated)
    }

    override func textDidChange(_ textInput: UITextInput?) {
        super.textDidChange(textInput)
        if !writing { synchronizeDocument() }
        updateReturnTitle()
    }

    override func selectionDidChange(_ textInput: UITextInput?) {
        super.selectionDidChange(textInput)
        if !writing { synchronizeDocument() }
    }

    private func stamp() -> DocumentStamp {
        let proxy = textDocumentProxy
        return DocumentStamp(identifier: proxy.documentIdentifier,
                             before: proxy.documentContextBeforeInput, after: proxy.documentContextAfterInput,
                             selected: proxy.selectedText)
    }

    @discardableResult private func synchronizeDocument() -> Bool {
        let current = stamp()
        defer { observed = current }
        guard let previous = observed, current != previous else { return false }
        surface.stopInteraction()
        input.invalidateContext()
        surface.render(KeyboardState())
        return true
    }

    private func receive(_ delivery: InputDelivery) {
        // UIKit may report a new field after an engine command has completed.
        // Validate the proxy again at the final insertion boundary.
        guard !synchronizeDocument() else { return }
        let originalDocument = observed?.identifier
        writing = true
        if !delivery.text.isEmpty { textDocumentProxy.insertText(delivery.text) }
        if delivery.deleteBackward { textDocumentProxy.deleteBackward() }
        let after = stamp()
        writing = false
        observed = after
        if after.identifier != originalDocument {
            surface.stopInteraction()
            input.invalidateContext()
            surface.render(KeyboardState())
            return
        }
        surface.render(delivery.state)
    }

    private func handle(_ action: KeyboardAction) {
        let changed = synchronizeDocument()
        if case .backspace(let repeating) = action, repeating && changed { return }
        switch action {
        case .key(let text): input.send(.key, text: text)
        case .literal(let text): input.send(.literal, text: text)
        case .select(let index, let revision): input.send(.select, index: index, expectedRevision: revision)
        case .punctuation(let chinese, let english): input.send(.literal, text: chinese, asciiText: english)
        case .space: input.send(.space)
        case .enter: input.send(.enter)
        case .backspace: input.send(.backspace)
        case .mode: input.send(.toggleMode)
        case .nextPage: input.send(.nextPage)
        case .previousPage: input.send(.previousPage)
        case .dismiss: surface.stopInteraction(); input.invalidateContext(); dismissKeyboard()
        }
    }

    private func updateReturnTitle() {
        switch textDocumentProxy.returnKeyType ?? .default {
        case .search, .google, .yahoo: surface.enterTitle = "搜索"
        case .go: surface.enterTitle = "前往"
        case .send: surface.enterTitle = "发送"
        case .done: surface.enterTitle = "完成"
        case .next: surface.enterTitle = "下一项"
        default: surface.enterTitle = "换行"
        }
    }
}
