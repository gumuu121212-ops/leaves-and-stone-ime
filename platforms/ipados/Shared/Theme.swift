import UIKit

enum MaodouTheme {
    static let background = UIColor(red: 0.97, green: 0.95, blue: 0.91, alpha: 1)
    static let ink = UIColor(red: 0.29, green: 0.23, blue: 0.19, alpha: 1)
    static let muted = UIColor(red: 0.57, green: 0.48, blue: 0.40, alpha: 1)
    static let selection = UIColor(red: 0.84, green: 0.75, blue: 0.65, alpha: 1)
    static let key = UIColor(red: 0.998, green: 0.985, blue: 0.955, alpha: 1)
    static func image(_ name: String) -> UIImage? {
        UIImage(named: name, in: Bundle(for: MascotView.self), compatibleWith: nil)
    }
}
