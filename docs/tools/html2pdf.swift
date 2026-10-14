// Renders an HTML file to a paginated A4 PDF with WebKit (macOS only).
// Usage: swift html2pdf.swift input.html output.pdf
import AppKit
import WebKit

final class Renderer: NSObject, WKNavigationDelegate {
    let input: URL
    let output: URL
    let window: NSWindow
    let webView: WKWebView

    init(input: URL, output: URL) {
        self.input = input
        self.output = output
        // A4 at 72 dpi minus margins: the width WebKit lays the page out with.
        let frame = NSRect(x: 0, y: 0, width: 595, height: 842)
        webView = WKWebView(frame: frame, configuration: WKWebViewConfiguration())
        window = NSWindow(contentRect: frame, styleMask: [.borderless], backing: .buffered, defer: false)
        super.init()
        window.contentView = webView
        webView.navigationDelegate = self
        // Allow the page to load images from the repository (e.g. ../resources/logo_full.png).
        webView.loadFileURL(input, allowingReadAccessTo: input.deletingLastPathComponent().deletingLastPathComponent())
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        // Give WebKit a moment to decode images and finish layout.
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { self.printToPDF() }
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        fputs("Load failed: \(error)\n", stderr)
        exit(1)
    }

    func printToPDF() {
        let info = NSPrintInfo()
        info.paperSize = NSSize(width: 595.28, height: 841.89) // A4
        info.topMargin = 42
        info.bottomMargin = 42
        info.leftMargin = 40
        info.rightMargin = 40
        info.horizontalPagination = .fit
        info.verticalPagination = .automatic
        info.isHorizontallyCentered = false
        info.isVerticallyCentered = false
        info.jobDisposition = .save
        info.dictionary()[NSPrintInfo.AttributeKey.jobSavingURL] = output

        let op = webView.printOperation(with: info)
        op.showsPrintPanel = false
        op.showsProgressPanel = false
        op.view?.frame = webView.bounds
        op.runModal(for: window, delegate: self, didRun: #selector(done(_:success:contextInfo:)), contextInfo: nil)
    }

    @objc func done(_ op: NSPrintOperation, success: Bool, contextInfo: UnsafeMutableRawPointer?) {
        if !success { fputs("Printing failed\n", stderr); exit(1) }
        print("Wrote \(output.path)")
        exit(0)
    }
}

let args = CommandLine.arguments
guard args.count == 3 else {
    fputs("Usage: html2pdf input.html output.pdf\n", stderr)
    exit(2)
}

let app = NSApplication.shared
app.setActivationPolicy(.prohibited)
let renderer = Renderer(input: URL(fileURLWithPath: args[1]).standardizedFileURL,
                        output: URL(fileURLWithPath: args[2]).standardizedFileURL)
app.run()
