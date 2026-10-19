import XCTest
import UIKit

private extension UIView {
    func findButton(_ identifier: String) -> UIButton? {
        if let button = self as? UIButton, button.accessibilityIdentifier == identifier { return button }
        for view in subviews { if let button = view.findButton(identifier) { return button } }
        return nil
    }
}

final class KeyboardTests: XCTestCase {
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
}
