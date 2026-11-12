// The 2P Start icon, every size and kind, drawn from one set of shapes: the CabinetOS cabinet
// (cabinetos/docs/assets/mark.svg) with a two-player control panel, a stick and two buttons each
// side, in the mark's own colours. MMagTech's choice, 2026-11-11.
//
// In the tray and the menu bar, a small dot at the lower right shows the status, with a clear gap
// round it: none Offline, green Online, the console's amber while an invite waits (his choice,
// 2026-11-11, over a larger dot, the screen or the marquee lit).
//
//   swift icon/make.swift icon               everything the builds use, into icon/
//   swift icon/make.swift icon <folder>      the same, and a picture of them all to judge, in <folder>
//
// What it makes is tracked, so neither build runs this:
//   AppIcon.icon/              the Mac app's, in Icon Composer's form: actool makes the app's icon from
//                              it, which macOS 26 and later draw in their own shape
//   2pstart.ico, 2pstart.png   the Windows program's, 16 to 256 pixels, and its notifications' picture
//   tray-online.ico, tray-invite.ico
//                              the Windows tray's, with the dot; Offline is 2pstart.ico
//   menubar.png, menubar-cut.png, menubar-lit.png, and each @2x
//                              the Mac menu bar's: the cabinet as a template, coloured by the system;
//                              the gap cut round the dot; and the dot (tray_mac.mm)
//
// Up to 32 pixels the cabinet has a dark edge, so its cream body holds on a light taskbar.

import AppKit
import Foundation

// MARK: The shapes, in the mark's units: the cabinet fills 165 to 435 across and 150 to 480 down.

let markCentre = CGPoint(x: 300, y: 315)
let markHeight: CGFloat = 330

func rgb(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(srgbRed: CGFloat((hex >> 16) & 0xFF) / 255, green: CGFloat((hex >> 8) & 0xFF) / 255,
            blue: CGFloat(hex & 0xFF) / 255, alpha: alpha)
}

let edgeColour = rgb(0x1B1430)
let green = rgb(0x30D158)
let amber = rgb(0xFFC457)   // the console's invite dot (ui::palette::kMarqueeAmber)

func roundRect(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, _ r: CGFloat) {
    g.addPath(CGPath(roundedRect: CGRect(x: x, y: y, width: w, height: h), cornerWidth: r, cornerHeight: r,
                     transform: nil))
}

func circle(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ r: CGFloat) {
    g.addEllipse(in: CGRect(x: x - r, y: y - r, width: r * 2, height: r * 2))
}

func gradient(_ g: CGContext, _ rect: CGRect, _ stops: [(CGFloat, UInt32)], across: Bool) {
    g.saveGState()
    g.clip(to: rect)
    let colours = stops.map { rgb($0.1) } as CFArray
    var places = stops.map { $0.0 }
    let grad = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB), colors: colours, locations: &places)!
    let end = across ? CGPoint(x: rect.maxX, y: rect.minY) : CGPoint(x: rect.minX, y: rect.maxY)
    g.drawLinearGradient(grad, start: rect.origin, end: end, options: [])
    g.restoreGState()
}

// The cabinet in colour. `edge`, in the mark's units, is the dark edge around it; 0 for none.
func cabinet(_ g: CGContext, edge: CGFloat) {
    if edge > 0 {
        g.setFillColor(edgeColour)
        roundRect(g, 175 - edge, 150 - edge, 250 + edge * 2, 330 + edge * 2, 20 + edge)
        roundRect(g, 165 - edge, 390 - edge, 270 + edge * 2, 48 + edge * 2, 10 + edge)
        circle(g, 203, 392, 11 + edge)
        circle(g, 349, 392, 11 + edge)
        g.fillPath()
    }
    g.setFillColor(rgb(0xF4EFE0))
    roundRect(g, 175, 150, 250, 330, 20)
    g.fillPath()
    gradient(g, CGRect(x: 193, y: 168, width: 214, height: 34), [(0, 0xF68CB8), (1, 0xFBC27A)], across: true)
    gradient(g, CGRect(x: 193, y: 213, width: 214, height: 159), [(0, 0xFFD36A), (0.55, 0xF99A4C), (1, 0xEF5F38)],
             across: false)
    g.setFillColor(rgb(0xDAD4C3))
    roundRect(g, 165, 390, 270, 48, 10)
    g.fillPath()
    for x: CGFloat in [0, 146] {
        g.setFillColor(rgb(0x3D3548))
        roundRect(g, 200.5 + x, 392, 5, 22, 2)
        circle(g, 203 + x, 392, 11)
        g.fillPath()
        g.setFillColor(rgb(0xE8395A))
        circle(g, 231 + x, 416, 9)
        g.fillPath()
        g.setFillColor(rgb(0xF5B82E))
        circle(g, 253 + x, 416, 9)
        g.fillPath()
    }
}

// The cabinet in one colour, for the menu bar: the body and the panel's ledge, with the marquee and
// the screen cut out of it, and the two sticks standing up into the screen.
func silhouette(_ g: CGContext) {
    g.setFillColor(rgb(0x000000))
    roundRect(g, 175, 150, 250, 330, 20)
    roundRect(g, 165, 390, 270, 48, 10)
    g.fillPath()
    g.setBlendMode(.clear)
    g.fill(CGRect(x: 193, y: 168, width: 214, height: 34))
    g.fill(CGRect(x: 193, y: 213, width: 214, height: 159))
    g.setBlendMode(.normal)
    for x: CGFloat in [0, 146] {
        roundRect(g, 193 + x, 350, 20, 50, 6)
        circle(g, 203 + x, 342, 26)
    }
    g.fillPath()
}

// MARK: Pictures

func canvas(_ w: Int, _ h: Int) -> CGContext {
    let g = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: 0,
                      space: CGColorSpace(name: CGColorSpace.sRGB)!,
                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    // Down the page, as the mark is drawn.
    g.translateBy(x: 0, y: CGFloat(h))
    g.scaleBy(x: 1, y: -1)
    return g
}

// The mark's cabinet centred in a square `size` pixels across, `fill` of its height.
func place(_ g: CGContext, size: CGFloat, fill: CGFloat) {
    let s = size * fill / markHeight
    g.translateBy(x: size * 0.5, y: size * 0.5)
    g.scaleBy(x: s, y: s)
    g.translateBy(x: -markCentre.x, y: -markCentre.y)
}

enum Status { case offline, online, invite }

// The dot in a square `size` across: at the lower right, a third of it across, and the gap round it.
struct Dot {
    var centre: CGPoint
    var radius: CGFloat
    var gap: CGFloat
}
func dotIn(_ size: CGFloat) -> Dot {
    let r = max(2.5, size * 0.16)
    return Dot(centre: CGPoint(x: size - r, y: size - r), radius: r, gap: max(1, size * 0.07))
}

// The gap cut and the dot drawn, in `status`'s colour.
func dot(_ g: CGContext, size: CGFloat, _ status: Status) {
    guard status != .offline else { return }
    let d = dotIn(size)
    g.setBlendMode(.clear)
    circle(g, d.centre.x, d.centre.y, d.radius + d.gap)
    g.fillPath()
    g.setBlendMode(.normal)
    g.setFillColor(status == .online ? green : amber)
    circle(g, d.centre.x, d.centre.y, d.radius)
    g.fillPath()
}

// The Windows icon at `size` pixels: the cabinet filling the square, with an edge when small, and the
// tray's dot for a status.
func windowsIcon(_ size: Int, _ status: Status = .offline) -> CGImage {
    let g = canvas(size, size)
    let n = CGFloat(size)
    let small = size <= 32
    let fill: CGFloat = small ? 0.94 : 0.90
    g.saveGState()
    place(g, size: n, fill: fill)
    // One pixel of edge at these sizes, in the mark's units.
    cabinet(g, edge: small ? markHeight / (n * fill) : 0)
    g.restoreGState()
    dot(g, size: n, status)
    return g.makeImage()!
}

func menuBarIcon(_ size: Int) -> CGImage {
    let g = canvas(size, size)
    place(g, size: CGFloat(size), fill: 0.92)
    silhouette(g)
    return g.makeImage()!
}

// The menu bar's dot, or the gap round it, in black where it goes: tray_mac.mm cuts the one and fills
// the other with the status's colour.
func menuBarDot(_ size: Int, gap: Bool) -> CGImage {
    let g = canvas(size, size)
    let d = dotIn(CGFloat(size))
    g.setFillColor(rgb(0x000000))
    circle(g, d.centre.x, d.centre.y, d.radius + (gap ? d.gap : 0))
    g.fillPath()
    return g.makeImage()!
}

// The layer Icon Composer puts on the purple tile: the cabinet as mac.svg had it on its tile.
func macLayer() -> CGImage {
    let g = canvas(1024, 1024)
    let s: CGFloat = 2.3
    g.translateBy(x: 512, y: 522)
    g.scaleBy(x: s, y: s)
    g.translateBy(x: -markCentre.x, y: -markCentre.y)
    cabinet(g, edge: 0)
    return g.makeImage()!
}

func png(_ image: CGImage) -> Data {
    NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:])!
}

// An .ico holding PNGs, which Windows reads at every size since Vista.
func ico(_ images: [(Int, Data)]) -> Data {
    var d = Data()
    func u16(_ v: Int) { withUnsafeBytes(of: UInt16(v).littleEndian) { d.append(contentsOf: $0) } }
    func u32(_ v: Int) { withUnsafeBytes(of: UInt32(v).littleEndian) { d.append(contentsOf: $0) } }
    u16(0)
    u16(1)
    u16(images.count)
    var offset = 6 + 16 * images.count
    for (size, data) in images {
        d.append(UInt8(size >= 256 ? 0 : size))
        d.append(UInt8(size >= 256 ? 0 : size))
        d.append(0)
        d.append(0)
        u16(1)
        u16(32)
        u32(data.count)
        u32(offset)
        offset += data.count
    }
    for (_, data) in images { d.append(data) }
    return d
}

func write(_ data: Data, _ path: String) {
    try! data.write(to: URL(fileURLWithPath: path))
}

// MARK: The files

let args = CommandLine.arguments
guard args.count >= 2 else {
    FileHandle.standardError.write("usage: swift icon/make.swift <icon folder> [<folder for the picture>]\n".data(using: .utf8)!)
    exit(2)
}
let out = args[1]

let appSizes = [16, 20, 24, 32, 40, 48, 64, 256]
let traySizes = [16, 20, 24, 32, 40, 48]
write(ico(appSizes.map { ($0, png(windowsIcon($0))) }), out + "/2pstart.ico")
write(png(windowsIcon(256)), out + "/2pstart.png")
write(ico(traySizes.map { ($0, png(windowsIcon($0, .online))) }), out + "/tray-online.ico")
write(ico(traySizes.map { ($0, png(windowsIcon($0, .invite))) }), out + "/tray-invite.ico")
// The menu bar's icons are 18 points.
for (suffix, size) in [("", 18), ("@2x", 36)] {
    write(png(menuBarIcon(size)), out + "/menubar\(suffix).png")
    write(png(menuBarDot(size, gap: true)), out + "/menubar-cut\(suffix).png")
    write(png(menuBarDot(size, gap: false)), out + "/menubar-lit\(suffix).png")
}

let iconDoc = out + "/AppIcon.icon"
try! FileManager.default.createDirectory(atPath: iconDoc + "/Assets", withIntermediateDirectories: true)
write(png(macLayer()), iconDoc + "/Assets/cabinet.png")
// The tile is Purple's top, mid and bottom (look.cpp), its gradient as mac.svg had it: flat, with no
// glass, shadow or shine, so it looks as drawn.
let iconJSON = """
{
  "fill" : {
    "linear-gradient" : [
      "srgb:0.22745,0.13333,0.40784,1.00000",
      "srgb:0.03529,0.02353,0.07843,1.00000"
    ],
    "orientation" : {
      "start" : { "x" : 0.5, "y" : 0 },
      "stop" : { "x" : 0.5, "y" : 0.7 }
    }
  },
  "groups" : [
    {
      "layers" : [
        {
          "glass" : false,
          "image-name" : "cabinet.png",
          "name" : "cabinet"
        }
      ],
      "shadow" : { "kind" : "none", "opacity" : 0 },
      "specular" : false,
      "translucency" : { "enabled" : false, "value" : 0 }
    }
  ],
  "supported-platforms" : {
    "squares" : [ "macOS" ]
  }
}

"""
write(iconJSON.data(using: .utf8)!, iconDoc + "/icon.json")

guard args.count >= 3 else { exit(0) }

// MARK: The picture to judge: each where it is seen, on light and dark, and larger.

let pictureFolder = args[2]
let sheet = canvas(1920, 1080)
sheet.setFillColor(rgb(0x3A3A3E))
sheet.fill(CGRect(x: 0, y: 0, width: 1920, height: 1080))

func label(_ s: String, _ x: CGFloat, _ y: CGFloat, size: CGFloat = 22, colour: NSColor = .white, centred: Bool = false,
           bold: Bool = false) {
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(cgContext: sheet, flipped: true)
    let a: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: size, weight: bold ? .bold : .medium),
                                            .foregroundColor: colour]
    let w = centred ? (s as NSString).size(withAttributes: a).width : 0
    (s as NSString).draw(at: NSPoint(x: x - w / 2, y: y), withAttributes: a)
    NSGraphicsContext.restoreGraphicsState()
}

func put(_ image: CGImage, _ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, sharp: Bool = false) {
    sheet.saveGState()
    sheet.interpolationQuality = sharp ? .none : .high
    sheet.translateBy(x: x, y: y + h)
    sheet.scaleBy(x: 1, y: -1)
    sheet.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
    sheet.restoreGState()
}

// The menu bar's picture as tray_mac.mm draws it: the template in the bar's ink, the gap cut, and the
// dot in the status's colour.
func menuBar(_ size: Int, _ ink: CGColor, _ status: Status) -> CGImage {
    let all = CGRect(x: 0, y: 0, width: size, height: size)
    let g = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0,
                      space: CGColorSpace(name: CGColorSpace.sRGB)!,
                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    g.saveGState()
    g.clip(to: all, mask: menuBarIcon(size))
    g.setFillColor(ink)
    g.fill(all)
    g.restoreGState()
    if status != .offline {
        g.saveGState()
        g.clip(to: all, mask: menuBarDot(size, gap: true))
        g.clear(all)
        g.restoreGState()
        g.clip(to: all, mask: menuBarDot(size, gap: false))
        g.setFillColor(status == .online ? green : amber)
        g.fill(all)
    }
    return g.makeImage()!
}

let statuses: [Status] = [.offline, .online, .invite]
let words = ["Offline", "Online", "Invite"]

label("WINDOWS: the tray, beside the clock (dark and light taskbars)", 40, 24, size: 28, bold: true)
for (row, (ground, ink)) in [(0x202020, NSColor.white), (0xEEEEEE, NSColor.black)].enumerated() {
    let y = CGFloat(90 + row * 110)
    sheet.setFillColor(rgb(UInt32(ground)))
    sheet.fill(CGRect(x: 40, y: y, width: 900, height: 90))
    for (j, s) in statuses.enumerated() {
        // 100%, then 150% and 200% display scaling.
        for (k, size) in [16, 24, 32].enumerated() {
            let cx = 110 + CGFloat(j) * 290 + CGFloat(k) * 60
            put(windowsIcon(size, s), cx - CGFloat(size) / 2, y + 38 - CGFloat(size) / 2, CGFloat(size), CGFloat(size))
        }
        label(words[j], 170 + CGFloat(j) * 290, y + 60, size: 15, colour: ink, centred: true)
    }
}
label("Each at 100%, 150% and 200% display scaling. Below: 16 pixels, eight times larger.", 40, 310, size: 16)
for (j, s) in statuses.enumerated() {
    put(windowsIcon(16, s), 40 + CGFloat(j) * 180, 340, 128, 128, sharp: true)
    label(words[j], 104 + CGFloat(j) * 180, 474, size: 15, centred: true)
}

sheet.setFillColor(rgb(0x55555A))
sheet.fill(CGRect(x: 980, y: 20, width: 2, height: 520))
label("MAC: the menu bar, top right (light and dark)", 1010, 24, size: 28, bold: true)
for (row, (ground, ink, word)) in [(0xF2F2F2, rgb(0x000000, 0.85), NSColor.black), (0x2B2B2D, rgb(0xFFFFFF), NSColor.white)]
    .enumerated() {
    let y = CGFloat(90 + row * 110)
    sheet.setFillColor(rgb(UInt32(ground)))
    sheet.fill(CGRect(x: 1010, y: y, width: 870, height: 90))
    for (j, s) in statuses.enumerated() {
        let cx = 1100 + CGFloat(j) * 130
        put(menuBar(36, ink, s), cx - 18, y + 12, 36, 36)
        label(words[j], cx, y + 54, size: 15, colour: word, centred: true)
    }
}
label("The cabinet in the menu bar's own black or white, and the dot in colour. Three times larger:", 1010, 310, size: 16)
for (j, s) in statuses.enumerated() {
    put(menuBar(36, rgb(0xFFFFFF), s), 1010 + CGFloat(j) * 145, 340, 108, 108)
    label(words[j], 1064 + CGFloat(j) * 145, 474, size: 15, centred: true)
}

sheet.setFillColor(rgb(0x55555A))
sheet.fill(CGRect(x: 40, y: 560, width: 1840, height: 2))
label("The program's own icon, which the status leaves alone: Windows, and the Mac's Dock as macOS draws it", 40, 580,
      size: 22)
put(windowsIcon(256), 60, 640, 256, 256)
label("Windows", 188, 910, size: 16, centred: true)
let ictool = "/Applications/Xcode.app/Contents/Applications/Icon Composer.app/Contents/Executables/ictool"
let dockFile = pictureFolder + "/dock.png"
let p = Process()
p.executableURL = URL(fileURLWithPath: ictool)
p.arguments = [iconDoc, "--export-image", "--output-file", dockFile, "--platform", "macOS", "--rendition", "Default",
               "--width", "256", "--height", "256", "--scale", "2"]
p.standardOutput = FileHandle.nullDevice
try? p.run()
p.waitUntilExit()
if let dock = NSImage(contentsOfFile: dockFile)?.cgImage(forProposedRect: nil, context: nil, hints: nil) {
    put(dock, 420, 640, 256, 256)
    label("Mac", 548, 910, size: 16, centred: true)
}
write(png(sheet.makeImage()!), pictureFolder + "/icons.png")
