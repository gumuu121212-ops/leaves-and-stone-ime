import UIKit

final class MascotView: UIView {
    private let awake = MaodouTheme.image("mascot-awake")
    private let sleeping = MaodouTheme.image("mascot-sleep")
    var isSleeping = false { didSet { showFrame() } }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isAccessibilityElement = false
        isUserInteractionEnabled = false
        layer.contentsRect = CGRect(x: 248.0 / 896, y: 96.0 / 480,
                                    width: 400.0 / 896, height: 221.0 / 480)
        layer.contentsGravity = .resize
        layer.magnificationFilter = .linear
        showFrame()
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    private func showFrame() { layer.contents = (isSleeping ? sleeping : awake)?.cgImage }
}
