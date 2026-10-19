import UIKit

enum KeyboardAction {
    case key(String), literal(String), punctuation(String, ascii: String), select(Int, revision: UInt64)
    case backspace(repeating: Bool)
    case space, enter, mode, nextPage, previousPage, dismiss
}

class MaodouKeyButton: UIButton {
    override var isHighlighted: Bool {
        didSet { backgroundColor = isHighlighted ? MaodouTheme.selection : MaodouTheme.key }
    }
}

final class KeyboardSurface: UIView {
    var onAction: ((KeyboardAction) -> Void)?
    let globeButton = MaodouKeyButton(type: .custom)
    private let mascot = MascotView()
    private let panel = UIView()
    private let preedit = UILabel()
    private let preeditScroll = UIScrollView()
    private let candidateScroll = UIScrollView()
    private let previous = MaodouKeyButton(type: .custom)
    private let nextPageButton = MaodouKeyButton(type: .custom)
    private var rows: [[(UIButton, CGFloat)]] = []
    private var keyButtons: [UIButton] = []
    private var wordButtons: [UIButton] = []
    private var spaceButton: UIButton?
    private var modeButton: UIButton?
    private var enterButton: UIButton?
    private var state = KeyboardState()
    private var wide: Bool?
    private var panelMode = 0 // letters, numbers, symbols
    private var uppercase = false
    private var idleTimer: Timer?
    var enterTitle = "换行" { didSet { enterButton?.setTitle(enterTitle, for: .normal) } }

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = MaodouTheme.background
        panel.isUserInteractionEnabled = false
        panel.backgroundColor = MaodouTheme.key
        panel.layer.cornerRadius = 10
        panel.layer.borderWidth = 0.5
        panel.layer.borderColor = MaodouTheme.selection.withAlphaComponent(0.45).cgColor
        panel.clipsToBounds = true
        addSubview(panel)
        addSubview(preeditScroll)
        preeditScroll.addSubview(preedit)
        preedit.font = .systemFont(ofSize: 15)
        preedit.textColor = MaodouTheme.muted
        preedit.accessibilityIdentifier = "preedit"
        preeditScroll.showsHorizontalScrollIndicator = false
        addSubview(mascot)
        addSubview(candidateScroll)
        candidateScroll.showsHorizontalScrollIndicator = false
        candidateScroll.delaysContentTouches = false
        for (button, symbol, action) in [(previous, "chevron.left", KeyboardAction.previousPage),
                                           (nextPageButton, "chevron.right", KeyboardAction.nextPage)] {
            decorate(button, title: nil, symbol: symbol)
            button.layer.cornerRadius = 6
            button.accessibilityLabel = symbol == "chevron.left" ? "上一页候选" : "下一页候选"
            button.addAction(UIAction { [weak self] _ in self?.emit(action) }, for: .touchUpInside)
            addSubview(button)
        }
        globeButton.accessibilityLabel = "切换系统键盘，长按显示键盘列表"
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    func preferredHeight(for width: CGFloat) -> CGFloat { width >= 600 ? 404 : 336 }

    func render(_ newState: KeyboardState) {
        let changedWords = state.candidates != newState.candidates || state.selected != newState.selected || state.revision != newState.revision
        let changedMode = state.ascii != newState.ascii
        state = newState
        preedit.text = newState.error ?? (newState.preedit.isEmpty ?
            (newState.ready ? (newState.ascii ? "English · 小毛豆" : "中文拼音 · 小毛豆") : "小毛豆准备中") : newState.preedit)
        previous.isEnabled = newState.previousPage
        nextPageButton.isEnabled = newState.nextPage
        previous.alpha = previous.isEnabled ? 1 : 0.3
        nextPageButton.alpha = nextPageButton.isEnabled ? 1 : 0.3
        if changedWords { rebuildCandidates() }
        if changedMode { rebuildKeys() }
        spaceButton?.setTitle(newState.ascii ? "space" : "空格", for: .normal)
        modeButton?.setTitle(newState.ascii ? "A" : "中", for: .normal)
        setNeedsLayout()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        let isWide = bounds.width >= 600
        if wide != isWide { wide = isWide; rebuildKeys() }
        let width = bounds.width
        panel.frame = CGRect(x: 6, y: 79, width: max(0, width - 12), height: 53)
        let dogWidth: CGFloat = isWide ? 137 : 119
        mascot.frame = CGRect(x: width - dogWidth - 24, y: 80 - dogWidth * 221 / 400,
                             width: dogWidth, height: dogWidth * 221 / 400)
        preeditScroll.frame = CGRect(x: 16, y: 46, width: max(1, width - dogWidth - 56), height: 24)
        let textSize = preedit.sizeThatFits(CGSize(width: CGFloat.greatestFiniteMagnitude, height: 24))
        preedit.frame = CGRect(x: 0, y: 0, width: max(textSize.width, preeditScroll.bounds.width), height: 24)
        preeditScroll.contentSize = preedit.bounds.size
        preeditScroll.contentOffset.x = max(0, preedit.bounds.width - preeditScroll.bounds.width)
        candidateScroll.frame = CGRect(x: 14, y: 86, width: max(1, width - 106), height: 39)
        previous.frame = CGRect(x: width - 84, y: 90, width: 32, height: 32)
        nextPageButton.frame = CGRect(x: width - 49, y: 90, width: 32, height: 32)
        var wordX: CGFloat = 0
        for button in wordButtons {
            let size = button.sizeThatFits(CGSize(width: CGFloat.greatestFiniteMagnitude, height: 39))
            button.frame = CGRect(x: wordX, y: 0, width: max(44, size.width + 4), height: 39)
            wordX += button.frame.width + 9
        }
        candidateScroll.contentSize = CGSize(width: wordX, height: 39)
        let gap: CGFloat = isWide ? 8 : 4
        let margin: CGFloat = isWide ? 12 : 5
        let top: CGFloat = 140
        let height = max(0, (bounds.height - top - 8 - CGFloat(rows.count - 1) * gap) / CGFloat(max(1, rows.count)))
        for (rowIndex, row) in rows.enumerated() {
            let visible = row.filter { !$0.0.isHidden }
            let indent: CGFloat = panelMode == 0 && rowIndex == (isWide ? 2 : 1) ? (isWide ? 24 : 13) : 0
            let available = width - 2 * (margin + indent) - CGFloat(max(0, visible.count - 1)) * gap
            let total = visible.reduce(CGFloat(0)) { $0 + $1.1 }
            var x = margin + indent
            for (button, weight) in visible {
                let keyWidth = max(0, available * weight / max(1, total))
                button.frame = CGRect(x: x, y: top + CGFloat(rowIndex) * (height + gap), width: keyWidth, height: height)
                x += keyWidth + gap
            }
        }
    }

    func stopInteraction() {
        keyButtons.compactMap { $0 as? RepeatKeyButton }.forEach { $0.cancelRepeat() }
        idleTimer?.invalidate()
    }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        if window == nil { stopInteraction() } else { wake() }
    }

    private func emit(_ action: KeyboardAction) { wake(); onAction?(action) }
    private func wake() {
        if mascot.isSleeping { mascot.isSleeping = false }
        idleTimer?.invalidate()
        idleTimer = Timer.scheduledTimer(withTimeInterval: 8, repeats: false) { [weak self] _ in
            self?.mascot.isSleeping = true
        }
    }

    private func decorate(_ button: UIButton, title: String?, symbol: String? = nil) {
        button.setTitle(title, for: .normal)
        if let symbol { button.setImage(UIImage(systemName: symbol), for: .normal) }
        button.tintColor = MaodouTheme.muted
        button.setTitleColor(MaodouTheme.ink, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 20, weight: .regular)
        button.backgroundColor = MaodouTheme.key
        button.layer.cornerRadius = 7
        button.layer.borderWidth = 0.5
        button.layer.borderColor = MaodouTheme.selection.withAlphaComponent(0.45).cgColor
        button.isExclusiveTouch = true
    }

    private func rebuildCandidates() {
        wordButtons.forEach { $0.removeFromSuperview() }
        let revision = state.revision
        wordButtons = state.candidates.enumerated().map { index, word in
            let button = UIButton(type: .custom)
            button.setTitle(word, for: .normal)
            button.setTitleColor(MaodouTheme.ink, for: .normal)
            button.titleLabel?.font = .systemFont(ofSize: 20)
            button.titleLabel?.lineBreakMode = .byClipping
            button.contentEdgeInsets = UIEdgeInsets(top: 0, left: 12, bottom: 0, right: 12)
            button.backgroundColor = index == state.selected ? MaodouTheme.selection : .clear
            button.layer.cornerRadius = 10
            button.accessibilityIdentifier = "candidate-\(index)"
            button.accessibilityLabel = word
            button.addAction(UIAction { [weak self] _ in self?.emit(.select(index, revision: revision)) }, for: .touchUpInside)
            candidateScroll.addSubview(button)
            return button
        }
        candidateScroll.contentOffset = .zero
    }

    private func rebuildKeys() {
        keyButtons.forEach { ($0 as? RepeatKeyButton)?.cancelRepeat(); $0.removeFromSuperview() }
        keyButtons = []; rows = []
        let isWide = bounds.width >= 600
        func button(_ title: String, id: String, symbol: String? = nil, action: @escaping () -> Void) -> UIButton {
            let key = MaodouKeyButton(type: .custom)
            decorate(key, title: symbol == nil ? title : nil, symbol: symbol)
            key.accessibilityIdentifier = id
            key.accessibilityLabel = title
            key.addAction(UIAction { [weak self] _ in self?.wake(); action() }, for: .touchUpInside)
            addSubview(key); keyButtons.append(key)
            return key
        }
        func textRow(_ values: [String], letters: Bool = false) -> [(UIButton, CGFloat)] {
            values.map { text in
                let key = button(letters ? (uppercase ? text.uppercased() : text) : text,
                                 id: "key-" + text) { [weak self] in
                    guard let self else { return }
                    if letters {
                        self.emit(.key(self.uppercase ? text.uppercased() : text))
                        if self.uppercase { self.uppercase = false; self.rebuildKeys(); self.setNeedsLayout() }
                    } else { self.emit(.literal(text)) }
                }
                return (key, 1)
            }
        }
        let digits = Array("1234567890").map(String.init)
        if panelMode == 0 {
            if isWide { rows.append(textRow(digits)) }
            rows.append(textRow(Array("qwertyuiop").map(String.init), letters: true))
            rows.append(textRow(Array("asdfghjkl").map(String.init), letters: true))
        } else {
            rows.append(textRow(panelMode == 1 ? digits : ["[", "]", "{", "}", "#", "%", "^", "*", "+", "="]))
            rows.append(textRow(panelMode == 1 ? ["-", "/", ":", ";", "(", ")", "¥", "&", "@", "\""] :
                ["_", "\\", "|", "~", "<", ">", "€", "£", "$", "·"]))
        }
        var third: [(UIButton, CGFloat)] = []
        if panelMode == 0 {
            let shift = button("大写", id: "shift", symbol: uppercase ? "shift.fill" : "shift") { [weak self] in
                guard let self else { return }; self.uppercase.toggle(); self.rebuildKeys(); self.setNeedsLayout()
            }
            third.append((shift, 1.35))
            third += textRow(Array("zxcvbnm").map(String.init), letters: true)
        } else {
            third.append((button(panelMode == 1 ? "#+=" : "123", id: "symbols") { [weak self] in
                guard let self else { return }; self.panelMode = self.panelMode == 1 ? 2 : 1
                self.rebuildKeys(); self.setNeedsLayout()
            }, 1.35))
            third += textRow([".", ",", "?", "!", "'", "…"])
        }
        let erase = RepeatKeyButton(type: .custom)
        decorate(erase, title: nil, symbol: "delete.left")
        erase.accessibilityIdentifier = "backspace"; erase.accessibilityLabel = "退格，按住连续删除"
        erase.onRepeat = { [weak self] repeating in self?.emit(.backspace(repeating: repeating)) }
        addSubview(erase); keyButtons.append(erase); third.append((erase, 1.35)); rows.append(third)
        let panelSwitch = button(panelMode == 0 ? "123" : "ABC", id: "panel") { [weak self] in
            guard let self else { return }; self.panelMode = self.panelMode == 0 ? 1 : 0
            self.rebuildKeys(); self.setNeedsLayout()
        }
        panelSwitch.titleLabel?.font = .systemFont(ofSize: 16)
        panelSwitch.titleLabel?.adjustsFontSizeToFitWidth = true
        panelSwitch.titleLabel?.minimumScaleFactor = 0.75
        decorate(globeButton, title: nil, symbol: "globe")
        addSubview(globeButton); keyButtons.append(globeButton)
        let mode = button(state.ascii ? "A" : "中", id: "mode") { [weak self] in self?.emit(.mode) }
        let comma = button(state.ascii ? "," : "，", id: "comma") { [weak self] in
            self?.emit(.punctuation("，", ascii: ","))
        }
        let space = button(state.ascii ? "space" : "空格", id: "space") { [weak self] in self?.emit(.space) }
        space.titleLabel?.font = .systemFont(ofSize: 16)
        let period = button(state.ascii ? "." : "。", id: "period") { [weak self] in
            self?.emit(.punctuation("。", ascii: "."))
        }
        let enter = button(enterTitle, id: "enter") { [weak self] in self?.emit(.enter) }
        enter.titleLabel?.font = .systemFont(ofSize: 16)
        let dismiss = button("收起键盘", id: "dismiss", symbol: "keyboard.chevron.compact.down") { [weak self] in self?.emit(.dismiss) }
        rows.append([(panelSwitch, 1.2), (globeButton, 1), (mode, 1), (comma, 0.85),
                     (space, 4), (period, 0.85), (enter, 1.7), (dismiss, 1)])
        spaceButton = space; modeButton = mode; enterButton = enter
    }
}

final class RepeatKeyButton: MaodouKeyButton {
    var onRepeat: ((Bool) -> Void)?
    private var delay: Timer?
    private var repeatTimer: Timer?
    private var generation: UInt64 = 0
    override init(frame: CGRect) {
        super.init(frame: frame)
        addTarget(self, action: #selector(beginRepeat), for: .touchDown)
        addTarget(self, action: #selector(cancelRepeat), for: [.touchUpInside, .touchUpOutside, .touchCancel, .touchDragExit])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    @objc private func beginRepeat() {
        cancelRepeat()
        let token = generation
        onRepeat?(false)
        guard token == generation else { return }
        delay = Timer(timeInterval: 0.35, repeats: false) { [weak self] _ in
            guard let self, token == self.generation else { return }
            self.onRepeat?(true)
            guard token == self.generation else { return }
            let timer = Timer(timeInterval: 0.07, repeats: true) { [weak self] _ in
                guard let self, token == self.generation else { return }
                self.onRepeat?(true)
            }
            self.repeatTimer = timer
            RunLoop.main.add(timer, forMode: .common)
        }
        if let delay { RunLoop.main.add(delay, forMode: .common) }
    }
    @objc func cancelRepeat() {
        generation &+= 1
        delay?.invalidate(); repeatTimer?.invalidate(); delay = nil; repeatTimer = nil
    }
    override func didMoveToWindow() { super.didMoveToWindow(); if window == nil { cancelRepeat() } }
    deinit { delay?.invalidate(); repeatTimer?.invalidate() }
}
