import AppKit

let size = NSSize(width: 1024, height: 1024)
let image = NSImage(size: size)
image.lockFocus()

let background = NSBezierPath(roundedRect: NSRect(x: 42, y: 42, width: 940, height: 940), xRadius: 210, yRadius: 210)
NSColor(calibratedRed: 0.957, green: 0.941, blue: 0.902, alpha: 1).setFill()
background.fill()

let paper = NSBezierPath()
paper.move(to: NSPoint(x: 238, y: 260))
paper.line(to: NSPoint(x: 786, y: 260))
paper.line(to: NSPoint(x: 786, y: 764))
paper.line(to: NSPoint(x: 238, y: 764))
paper.close()
NSColor(calibratedRed: 0.996, green: 0.988, blue: 0.965, alpha: 1).setFill()
paper.fill()

let lowerFold = NSBezierPath()
lowerFold.move(to: NSPoint(x: 238, y: 260))
lowerFold.line(to: NSPoint(x: 512, y: 512))
lowerFold.line(to: NSPoint(x: 786, y: 260))
lowerFold.close()
NSColor(calibratedRed: 0.859, green: 0.918, blue: 0.902, alpha: 1).setFill()
lowerFold.fill()

let crease = NSBezierPath()
crease.move(to: NSPoint(x: 238, y: 764))
crease.line(to: NSPoint(x: 512, y: 512))
crease.line(to: NSPoint(x: 786, y: 764))
crease.move(to: NSPoint(x: 512, y: 512))
crease.line(to: NSPoint(x: 512, y: 260))
crease.lineWidth = 22
crease.lineJoinStyle = .round
crease.lineCapStyle = .round
NSColor(calibratedRed: 0.106, green: 0.431, blue: 0.408, alpha: 1).setStroke()
crease.stroke()

let outline = NSBezierPath(rect: NSRect(x: 238, y: 260, width: 548, height: 504))
outline.lineWidth = 18
NSColor(calibratedRed: 0.114, green: 0.165, blue: 0.188, alpha: 1).setStroke()
outline.stroke()

let accent = NSBezierPath()
accent.move(to: NSPoint(x: 238, y: 260))
accent.line(to: NSPoint(x: 512, y: 512))
accent.lineWidth = 22
accent.lineCapStyle = .round
NSColor(calibratedRed: 0.882, green: 0.373, blue: 0.259, alpha: 1).setStroke()
accent.stroke()

image.unlockFocus()

guard let tiff = image.tiffRepresentation,
      let bitmap = NSBitmapImageRep(data: tiff),
      let png = bitmap.representation(using: .png, properties: [:]) else {
    fatalError("Unable to render app icon")
}

let output = URL(fileURLWithPath: CommandLine.arguments[1])
try png.write(to: output)
