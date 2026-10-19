import AppKit
import Foundation

let root = URL(fileURLWithPath: CommandLine.arguments[1])
let source = NSImage(contentsOf: root.appendingPathComponent("Resources/mascot-awake.png"))!
var proposed = CGRect.zero
let raw = source.cgImage(forProposedRect: &proposed, context: nil, hints: nil)!
let dog = raw.cropping(to: CGRect(x: 248, y: 96, width: 400, height: 221))!
let space = CGColorSpaceCreateDeviceRGB()
let context = CGContext(data: nil, width: 1024, height: 1024, bitsPerComponent: 8, bytesPerRow: 0,
                        space: space, bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
context.setFillColor(CGColor(red: 0.97, green: 0.94, blue: 0.88, alpha: 1))
context.fill(CGRect(x: 0, y: 0, width: 1024, height: 1024))
context.interpolationQuality = .high
context.draw(dog, in: CGRect(x: 62, y: 255, width: 900, height: 497.25))
let output = root.appendingPathComponent("Resources/Assets.xcassets/AppIcon.appiconset")
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let image = NSBitmapImageRep(cgImage: context.makeImage()!)
try image.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent("icon.png"))
let manifest = """
{"images":[{"filename":"icon.png","idiom":"universal","platform":"ios","size":"1024x1024"}],"info":{"author":"xcode","version":1}}
"""
try manifest.write(to: output.appendingPathComponent("Contents.json"), atomically: true, encoding: .utf8)
