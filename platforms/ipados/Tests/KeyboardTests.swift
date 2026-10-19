import XCTest
import UIKit

private extension UIView {
    func findButton(_ identifier: String) -> UIButton? {
        if let button = self as? UIButton, button.accessibilityIdentifier == identifier { return button }
        for view in subviews { if let button = view.findButton(identifier) { return button } }
        return nil
    }
    func button(label: String) -> UIButton? {
        if let button = self as? UIButton, button.accessibilityLabel == label { return button }
        for view in subviews { if let button = view.button(label: label) { return button } }
        return nil
    }
}

@MainActor final class KeyboardTests: XCTestCase {
    private static let userURL = FileManager.default.temporaryDirectory
        .appendingPathComponent("maodou-tests-\(UUID().uuidString)")

    private func service() -> InputService {
        InputService(dataURL: Bundle(for: Self.self).url(forResource: "Rime", withExtension: nil),
                     userURL: Self.userURL)
    }

    func testInitialKeysAreQueuedAndSelectionCommitsRealChinese() {
        let service = service()
        let selected = expectation(description: "real Chinese commit")
        var requestedSelection = false
        service.onDelivery = { delivery in
            if !requestedSelection, delivery.state.candidates.first == "你好" {
                requestedSelection = true
                service.send(.select, index: 0)
            }
            if delivery.text == "你好" { selected.fulfill() }
        }
        service.start()
        for key in "nihao" { service.send(.key, text: String(key)) }
        wait(for: [selected], timeout: 10)
    }

    func testChangingDocumentsDiscardsPendingCommit() {
        let service = service()
        let finished = expectation(description: "new document candidate")
        var inserted: [String] = []
        service.onDelivery = { delivery in
            if !delivery.text.isEmpty { inserted.append(delivery.text) }
            if delivery.state.candidates.first == "iPad" { finished.fulfill() }
        }
        service.start()
        for key in "nihao" { service.send(.key, text: String(key)) }
        service.send(.select, index: 0)
        service.invalidateContext()
        for key in "ipad" { service.send(.key, text: String(key)) }
        wait(for: [finished], timeout: 10)
        XCTAssertTrue(inserted.isEmpty, "The old document's commit must never reach the new proxy")
    }

    func testTouchLayoutAndCompleteCandidatesAcrossIPadWidths() {
        for width in [320.0, 600.0, 768.0, 1024.0] {
            let surface = KeyboardSurface()
            let state = KeyboardState(preedit: "shu ru wen zi de shi hou",
                candidates: ["输入文字的时候", "输入文字的", "输入"], ready: true)
            surface.render(state)
            surface.frame = CGRect(x: 0, y: 0, width: width, height: surface.preferredHeight(for: width))
            surface.layoutIfNeeded()
            let mascot = surface.subviews.first { $0 is MascotView }
            XCTAssertNotNil(mascot)
            if let mascot { XCTAssertGreaterThanOrEqual(mascot.frame.minY, 0, "The dog must fit within the keyboard") }
            let key = surface.findButton("key-n")
            XCTAssertNotNil(key)
            if let key {
                XCTAssertGreaterThanOrEqual(key.bounds.width, 20)
                XCTAssertGreaterThanOrEqual(key.bounds.height, 34)
                let bounds = key.convert(key.bounds, to: surface)
                XCTAssertGreaterThanOrEqual(bounds.minX, 0)
                XCTAssertLessThanOrEqual(bounds.maxX, width)
            }
            XCTAssertEqual(surface.findButton("candidate-0")?.title(for: .normal), "输入文字的时候")
            var tapped = false
            surface.onAction = { action in if case .select(0) = action { tapped = true } }
            surface.findButton("candidate-0")?.sendActions(for: .touchUpInside)
            XCTAssertTrue(tapped)
            let image = UIGraphicsImageRenderer(size: surface.bounds.size).image { renderer in
                surface.layer.render(in: renderer.cgContext)
            }
            let attachment = XCTAttachment(image: image)
            attachment.name = "xiaomaodou-\(Int(width))pt"
            attachment.lifetime = .keepAlways
            add(attachment)
        }
    }

    func testBackspaceRepeatStopsAfterCancellation() {
        let button = RepeatKeyButton()
        var count = 0
        button.onRepeat = { count += 1 }
        button.sendActions(for: .touchDown)
        XCTAssertEqual(count, 1)
        button.sendActions(for: .touchCancel)
        RunLoop.main.run(until: Date().addingTimeInterval(0.5))
        XCTAssertEqual(count, 1)
    }

    func testControllerTouchSelectionWritesThroughDocumentProxy() {
        let controller = ProxyKeyboardController()
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.view.setNeedsLayout()
        controller.view.layoutIfNeeded()
        for key in "nihao" { controller.view.findButton("key-\(key)")?.sendActions(for: .touchUpInside) }
        let candidate = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in
            controller.view.findButton("candidate-0")?.title(for: .normal) == "你好"
        }, object: nil)
        wait(for: [candidate], timeout: 10)
        controller.view.findButton("candidate-0")?.sendActions(for: .touchUpInside)
        let commit = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in controller.proxy.text == "你好" }, object: nil)
        wait(for: [commit], timeout: 10)
        controller.viewWillDisappear(false)
        XCTAssertEqual(controller.proxy.text, "你好")
    }

    func testTouchOnOldCandidatesCannotSelectNewPageWords() {
        let controller = ProxyKeyboardController()
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.viewDidLayoutSubviews()
        controller.view.layoutIfNeeded()
        for key in "shi" { controller.view.findButton("key-\(key)")?.sendActions(for: .touchUpInside) }
        let ready = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in
            controller.view.findButton("candidate-0")?.title(for: .normal) == "是"
        }, object: nil)
        wait(for: [ready], timeout: 10)
        let oldButton = controller.view.findButton("candidate-0")
        let next = controller.view.button(label: "下一页候选")
        XCTAssertEqual(next?.isEnabled, true)
        next?.sendActions(for: .touchUpInside)
        oldButton?.sendActions(for: .touchUpInside)
        RunLoop.main.run(until: Date().addingTimeInterval(0.7))
        XCTAssertEqual(controller.proxy.text, "", "A stale candidate must never commit a different page's word")
        controller.viewWillDisappear(false)
    }

    func testChangingDocumentCancelsHeldBackspace() {
        let controller = ProxyKeyboardController()
        controller.proxy.text = "OLD"
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.viewDidLayoutSubviews()
        controller.view.layoutIfNeeded()
        controller.view.findButton("backspace")?.sendActions(for: .touchDown)
        controller.proxy.documentIdentifier = UUID()
        controller.proxy.text = "SAFE"
        controller.textDidChange(nil)
        RunLoop.main.run(until: Date().addingTimeInterval(0.7))
        XCTAssertEqual(controller.proxy.text, "SAFE", "A held key from the old field must not delete in the new field")
        controller.viewWillDisappear(false)
    }

    func testQueuedModeSwitchUsesEnglishCaseAndPunctuation() {
        let controller = ProxyKeyboardController()
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.viewDidLayoutSubviews()
        controller.view.layoutIfNeeded()
        for id in ["mode", "shift", "key-a", "comma", "period"] {
            controller.view.findButton(id)?.sendActions(for: .touchUpInside)
        }
        let complete = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in controller.proxy.text.count >= 3 }, object: nil)
        wait(for: [complete], timeout: 10)
        XCTAssertEqual(controller.proxy.text, "A,.")
        controller.viewWillDisappear(false)
    }

    func testUpdatedSelectionRejectsAnOldPendingCommit() {
        let controller = ProxyKeyboardController()
        controller.proxy.text = "OLD"; controller.proxy.selection = "OLD"
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.viewDidLayoutSubviews()
        controller.view.layoutIfNeeded()
        for key in "nihao" { controller.view.findButton("key-\(key)")?.sendActions(for: .touchUpInside) }
        let ready = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in
            controller.view.findButton("candidate-0")?.title(for: .normal) == "你好"
        }, object: nil)
        wait(for: [ready], timeout: 10)
        controller.view.findButton("candidate-0")?.sendActions(for: .touchUpInside)
        controller.proxy.text = "NEW"; controller.proxy.selection = "NEW"
        controller.textDidChange(nil)
        RunLoop.main.run(until: Date().addingTimeInterval(0.7))
        XCTAssertEqual(controller.proxy.text, "NEW")
        controller.viewWillDisappear(false)
    }

    func testHostAppPresentsASetupWindow() {
        let windows = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.flatMap { $0.windows }
        guard let window = windows.first(where: { !$0.isHidden && $0.rootViewController != nil }) else {
            XCTFail("The SwiftUI setup app must create a visible scene window"); return
        }
        let image = UIGraphicsImageRenderer(size: window.bounds.size).image { renderer in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        let attachment = XCTAttachment(image: image)
        attachment.name = "xiaomaodou-setup-app"; attachment.lifetime = .keepAlways
        add(attachment)
    }

    func testSilentDocumentChangeStopsRepeatAtItsNextTick() {
        let controller = ProxyKeyboardController()
        controller.proxy.text = "OLD"
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.viewDidLayoutSubviews()
        controller.view.layoutIfNeeded()
        controller.view.findButton("backspace")?.sendActions(for: .touchDown)
        let deleted = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in controller.proxy.text != "OLD" }, object: nil)
        wait(for: [deleted], timeout: 10)
        controller.proxy.documentIdentifier = UUID(); controller.proxy.text = "SAFE"
        // No textDidChange notification: the repeat must detect this itself.
        RunLoop.main.run(until: Date().addingTimeInterval(0.7))
        XCTAssertEqual(controller.proxy.text, "SAFE")
        controller.viewWillDisappear(false)
    }

    func testHostFocusChangeWhileInsertingDiscardsAlreadyQueuedText() {
        let controller = ProxyKeyboardController()
        controller.loadViewIfNeeded()
        controller.view.frame = CGRect(x: 0, y: 0, width: 768, height: 388)
        controller.viewWillAppear(false)
        controller.viewDidLayoutSubviews()
        controller.view.layoutIfNeeded()
        let moved = expectation(description: "host automatically moved to another field")
        controller.proxy.onInsert = {
            controller.proxy.onInsert = nil
            controller.proxy.documentIdentifier = UUID(); controller.proxy.text = "SAFE"
            moved.fulfill()
        }
        for id in ["mode", "key-a", "key-b"] { controller.view.findButton(id)?.sendActions(for: .touchUpInside) }
        wait(for: [moved], timeout: 10)
        RunLoop.main.run(until: Date().addingTimeInterval(0.5))
        XCTAssertEqual(controller.proxy.text, "SAFE")
        controller.viewWillDisappear(false)
    }
}

private final class ProxyKeyboardController: KeyboardViewController {
    let proxy = TestDocumentProxy()
    override var textDocumentProxy: UITextDocumentProxy { proxy }
}

private final class TestDocumentProxy: NSObject, UITextDocumentProxy {
    var text = ""
    var selection: String?
    var onInsert: (() -> Void)?
    var documentIdentifier = UUID()
    var documentContextBeforeInput: String? { selection == nil ? text : "" }
    var documentContextAfterInput: String? { "" }
    var documentInputMode: UITextInputMode? { nil }
    var selectedText: String? { selection }
    var hasText: Bool { !text.isEmpty }
    func insertText(_ text: String) {
        if selection != nil { self.text = text; selection = nil } else { self.text += text }
        onInsert?()
    }
    func deleteBackward() { if !text.isEmpty { text.removeLast() } }
    func adjustTextPosition(byCharacterOffset offset: Int) {}
    func setMarkedText(_ markedText: String, selectedRange: NSRange) {}
    func unmarkText() {}
}
