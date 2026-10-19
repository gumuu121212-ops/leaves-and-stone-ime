import UIKit

enum KeyboardAction { case select(Int) }

// The first simulator run exercises these public touch/layout contracts before
// the production layout is added.
final class KeyboardSurface: UIView {
    var onAction: ((KeyboardAction) -> Void)?
    func render(_ state: KeyboardState) {}
    func preferredHeight(for width: CGFloat) -> CGFloat { 320 }
}

final class RepeatKeyButton: UIButton {
    var onRepeat: (() -> Void)?
}
