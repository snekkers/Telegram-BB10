// Native UI backgrounds, rasterized on macOS. Re-run after changing the palette.
import AppKit

guard CommandLine.arguments.count == 2 else { exit(64) }
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let colors: [UInt32] = [0x6ba5d0, 0x81b979, 0xd99d66, 0xa590c7, 0xcc8298, 0x62b8b6]
for i in 0..<9 {
    let avatar = i < 6
    let name = avatar ? "avatar-\(i)" : i == 6 ? "bubble-in" : i == 7 ? "bubble-out" : "badge"
    let size = avatar ? 96 : 64
    let color: UInt32 = avatar ? colors[i] : i == 6 ? 0xffffff : i == 7 ? 0xdcedc8 : 0x5288b7
    let image = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: size * 4, bitsPerPixel: 32)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: image)
    NSColor(srgbRed: CGFloat((color >> 16) & 255) / 255,
        green: CGFloat((color >> 8) & 255) / 255, blue: CGFloat(color & 255) / 255, alpha: 1).setFill()
    let rect = NSRect(x: 0, y: 0, width: size, height: size)
    if avatar { NSBezierPath(ovalIn: rect).fill() }
    else { NSBezierPath(roundedRect: rect, xRadius: i == 8 ? 30 : 14, yRadius: i == 8 ? 30 : 14).fill() }
    NSGraphicsContext.restoreGraphicsState()
    try image.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent(name + ".png"))
    if !avatar {
        let margins = i == 8 ? "31 31 31 31" : "16 16 16 16"
        try "#RimCascadesAssetMetaData version=1.0\nsource: \"\(name).png\"\nsliceMargins: \(margins)\n".write(
            to: output.appendingPathComponent(name + ".amd"), atomically: true, encoding: .utf8)
    }
}
