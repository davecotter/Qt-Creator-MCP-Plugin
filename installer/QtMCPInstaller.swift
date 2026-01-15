#!/usr/bin/env swift
/*
 * Qt MCP Plugin Installer
 * Native macOS installer using Swift/AppKit
 */

import Cocoa
import Foundation

// MARK: - Constants

let APP_NAME = "Qt MCP Plugin Installer"
let PLUGIN_NAME = "Qt MCP Plugin"
let PLUGIN_DYLIB_PATTERN = "libQt_MCP_Plugin"
let PLUGIN_JSON_FILE = "Qt_MCP_Plugin.json"
let PLUGIN_DISCOVERY_FILE = "Qt_MCP_Plugin_discovery.json"

// MARK: - Qt Creator Finder

class QtCreatorFinder {
    static let searchPaths = [
        "/Applications/Qt Creator.app",
        NSHomeDirectory() + "/Applications/Qt Creator.app",
        NSHomeDirectory() + "/Developer/Qt/Qt Creator.app",
        "/opt/Qt/Qt Creator.app",
        "/usr/local/Qt/Qt Creator.app"
    ]
    
    static func findAll() -> [String] {
        var found: [String] = []
        
        for path in searchPaths {
            if isValidQtCreator(path: path) && !found.contains(path) {
                found.append(path)
            }
        }
        
        // Check ~/Developer/Qt
        let qtDevDir = NSHomeDirectory() + "/Developer/Qt"
        if FileManager.default.fileExists(atPath: qtDevDir) {
            if let contents = try? FileManager.default.contentsOfDirectory(atPath: qtDevDir) {
                for entry in contents where entry == "Qt Creator.app" {
                    let fullPath = qtDevDir + "/" + entry
                    if isValidQtCreator(path: fullPath) && !found.contains(fullPath) {
                        found.append(fullPath)
                    }
                }
            }
        }
        
        // Check /Applications
        let appsDir = "/Applications"
        if let contents = try? FileManager.default.contentsOfDirectory(atPath: appsDir) {
            for entry in contents where entry.contains("Qt Creator") && entry.hasSuffix(".app") {
                let fullPath = appsDir + "/" + entry
                if isValidQtCreator(path: fullPath) && !found.contains(fullPath) {
                    found.append(fullPath)
                }
            }
        }
        
        return found
    }
    
    static func isValidQtCreator(path: String) -> Bool {
        let binaryPath = path + "/Contents/MacOS/Qt Creator"
        let pluginDir = path + "/Contents/PlugIns/qtcreator"
        return FileManager.default.fileExists(atPath: binaryPath) &&
               FileManager.default.fileExists(atPath: pluginDir)
    }
    
    static func getPluginDir(appPath: String) -> String {
        return appPath + "/Contents/PlugIns/qtcreator"
    }
    
    static func getVersion(appPath: String) -> String? {
        let plistPath = appPath + "/Contents/Info.plist"
        guard let plist = NSDictionary(contentsOfFile: plistPath) else { return nil }
        return plist["CFBundleShortVersionString"] as? String
    }
}

// MARK: - Plugin Installer

class PluginInstaller {
    let qtCreatorPath: String
    let pluginSourceDir: String
    let pluginDir: String
    
    init(qtCreatorPath: String, pluginSourceDir: String) {
        self.qtCreatorPath = qtCreatorPath
        self.pluginSourceDir = pluginSourceDir
        self.pluginDir = QtCreatorFinder.getPluginDir(appPath: qtCreatorPath)
    }
    
    func getExistingPluginFiles() -> [String] {
        var files: [String] = []
        guard let contents = try? FileManager.default.contentsOfDirectory(atPath: pluginDir) else {
            return files
        }
        
        for file in contents {
            // Match ANY file containing our plugin name patterns
            // This catches all versions: libQt_MCP_Plugin.1.dylib, libQt_MCP_Plugin.1.18.0.dylib, etc.
            let lowercaseFile = file.lowercased()
            if lowercaseFile.contains("qt_mcp_plugin") || 
               lowercaseFile.contains("qtmcpplugin") ||
               file.contains(PLUGIN_DYLIB_PATTERN) {
                // Include dylibs, json files, and any other related files
                if file.hasSuffix(".dylib") || file.hasSuffix(".json") || 
                   file.hasSuffix(".so") || file.hasSuffix(".dll") {
                    files.append(pluginDir + "/" + file)
                }
            }
        }
        return files
    }
    
    func hasExistingPlugin() -> Bool {
        let files = getExistingPluginFiles()
        return !files.isEmpty
    }
    
    func getExistingPluginInfo() -> String {
        let files = getExistingPluginFiles()
        if files.isEmpty {
            return "No existing plugin"
        }
        
        // Count dylibs specifically
        let dylibFiles = files.filter { $0.hasSuffix(".dylib") }
        let jsonFiles = files.filter { $0.hasSuffix(".json") }
        
        if dylibFiles.count > 1 {
            // Multiple versions detected - list them
            let names = dylibFiles.map { ($0 as NSString).lastPathComponent }
            return "\(dylibFiles.count) dylibs found: \(names.joined(separator: ", "))"
        } else if let firstDylib = dylibFiles.first {
            let filename = (firstDylib as NSString).lastPathComponent
            if let attrs = try? FileManager.default.attributesOfItem(atPath: firstDylib) {
                let size = attrs[.size] as? Int64 ?? 0
                let date = attrs[.modificationDate] as? Date ?? Date()
                let formatter = DateFormatter()
                formatter.dateStyle = .short
                formatter.timeStyle = .short
                let sizeKB = size / 1024
                return "\(filename) (\(sizeKB) KB, \(formatter.string(from: date)))"
            }
            return filename
        }
        
        return "\(files.count) plugin file(s) found"
    }
    
    func removeExistingPlugin() throws -> (removed: Int, failed: Int, failedFiles: [String]) {
        let files = getExistingPluginFiles()
        var removedCount = 0
        var failedCount = 0
        var failedFiles: [String] = []
        
        for file in files {
            let filename = (file as NSString).lastPathComponent
            do {
                // Check if it's a symlink and resolve it
                let attrs = try FileManager.default.attributesOfItem(atPath: file)
                let fileType = attrs[.type] as? FileAttributeType
                
                if fileType == .typeSymbolicLink {
                    // Remove symlink
                    try FileManager.default.removeItem(atPath: file)
                    removedCount += 1
                    print("Removed symlink: \(filename)")
                } else {
                    // Remove regular file
                    try FileManager.default.removeItem(atPath: file)
                    removedCount += 1
                    print("Removed: \(filename)")
                }
            } catch {
                print("Warning: Could not remove \(filename): \(error)")
                failedCount += 1
                failedFiles.append(filename)
            }
        }
        return (removedCount, failedCount, failedFiles)
    }
    
    func getSourcePluginFiles() -> (dylib: String?, jsons: [String]) {
        var mainDylib: String? = nil
        var jsonFiles: [String] = []
        
        guard let contents = try? FileManager.default.contentsOfDirectory(atPath: pluginSourceDir) else {
            return (nil, [])
        }
        
        for file in contents {
            let fullPath = pluginSourceDir + "/" + file
            
            if file.contains(PLUGIN_DYLIB_PATTERN) && file.hasSuffix(".dylib") {
                // Only use the versioned dylib (e.g., libQt_MCP_Plugin.1.dylib)
                if file.contains(".1.") || file.hasSuffix(".1.dylib") {
                    mainDylib = fullPath
                }
            }
            if file == PLUGIN_JSON_FILE || file == PLUGIN_DISCOVERY_FILE {
                jsonFiles.append(fullPath)
            }
        }
        
        // Fallback: if no versioned dylib found, use any dylib
        if mainDylib == nil {
            for file in contents {
                if file.contains(PLUGIN_DYLIB_PATTERN) && file.hasSuffix(".dylib") {
                    mainDylib = pluginSourceDir + "/" + file
                    break
                }
            }
        }
        
        return (mainDylib, jsonFiles)
    }
    
    func installPlugin() throws -> Int {
        let sourceFiles = getSourcePluginFiles()
        
        guard let dylibPath = sourceFiles.dylib else {
            throw NSError(domain: "Installer", code: 1, 
                         userInfo: [NSLocalizedDescriptionKey: "No plugin dylib found in bundle"])
        }
        
        var installedCount = 0
        
        // Install the main dylib
        let dylibFilename = (dylibPath as NSString).lastPathComponent
        let dstDylibPath = pluginDir + "/" + dylibFilename
        
        if FileManager.default.fileExists(atPath: dstDylibPath) {
            try FileManager.default.removeItem(atPath: dstDylibPath)
        }
        try FileManager.default.copyItem(atPath: dylibPath, toPath: dstDylibPath)
        installedCount += 1
        print("Installed: \(dylibFilename)")
        
        // Create symlink: libQt_MCP_Plugin.dylib -> libQt_MCP_Plugin.1.dylib
        let symlinkName = "libQt_MCP_Plugin.dylib"
        let symlinkPath = pluginDir + "/" + symlinkName
        
        if FileManager.default.fileExists(atPath: symlinkPath) {
            try FileManager.default.removeItem(atPath: symlinkPath)
        }
        try FileManager.default.createSymbolicLink(atPath: symlinkPath, withDestinationPath: dylibFilename)
        installedCount += 1
        print("Created symlink: \(symlinkName) -> \(dylibFilename)")
        
        // Install JSON files
        for jsonPath in sourceFiles.jsons {
            let filename = (jsonPath as NSString).lastPathComponent
            let dstPath = pluginDir + "/" + filename
            
            if FileManager.default.fileExists(atPath: dstPath) {
                try FileManager.default.removeItem(atPath: dstPath)
            }
            try FileManager.default.copyItem(atPath: jsonPath, toPath: dstPath)
            installedCount += 1
            print("Installed: \(filename)")
        }
        
        return installedCount
    }
    
    func verify() -> (Bool, String) {
        let contents = (try? FileManager.default.contentsOfDirectory(atPath: pluginDir)) ?? []
        
        // Check for our plugin dylib (not symlink)
        var realDylibCount = 0
        var hasSymlink = false
        
        for file in contents {
            if file.contains(PLUGIN_DYLIB_PATTERN) && file.hasSuffix(".dylib") {
                let fullPath = pluginDir + "/" + file
                let attrs = try? FileManager.default.attributesOfItem(atPath: fullPath)
                let fileType = attrs?[.type] as? FileAttributeType
                
                if fileType == .typeSymbolicLink {
                    hasSymlink = true
                } else {
                    realDylibCount += 1
                }
            }
        }
        
        // Check for JSON
        let hasJson = contents.contains { $0 == PLUGIN_JSON_FILE }
        
        if realDylibCount == 0 { return (false, "Plugin dylib not found after installation") }
        if !hasJson { return (false, "Plugin JSON not found after installation") }
        
        // Check for duplicates (multiple REAL dylib versions, not counting symlinks)
        if realDylibCount > 1 {
            return (false, "Warning: Multiple plugin versions detected (\(realDylibCount) dylibs)")
        }
        
        return (true, "Installation verified: 1 dylib" + (hasSymlink ? " + symlink" : "") + " + JSON")
    }
}

// MARK: - Installer Window Controller

class InstallerWindowController: NSWindowController, NSWindowDelegate {
    var qtCreatorPaths: [String] = []
    var selectedPath: String = ""
    var pluginSourceDir: String = ""
    var installationComplete = false
    
    // UI Elements
    var popup: NSPopUpButton!
    var versionLabel: NSTextField!
    var statusLabel: NSTextField!
    var existingPluginLabel: NSTextField!
    var progressBar: NSProgressIndicator!
    var progressLabel: NSTextField!
    var installButton: NSButton!
    var cancelButton: NSButton!
    var progressContainer: NSView!
    
    convenience init() {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 550, height: 450),
            styleMask: [.titled, .closable, .miniaturizable],
            backing: .buffered,
            defer: false
        )
        window.title = APP_NAME
        window.center()
        
        self.init(window: window)
        window.delegate = self
        
        findPluginSource()
        setupUI()
        scanForQtCreator()
    }
    
    func findPluginSource() {
        // Find plugins directory in app bundle Resources
        if let resourcePath = Bundle.main.resourcePath {
            let pluginsPath = resourcePath + "/plugins"
            if FileManager.default.fileExists(atPath: pluginsPath) {
                pluginSourceDir = pluginsPath
                return
            }
        }
        
        // Fallback: look relative to executable
        let execPath = Bundle.main.executablePath ?? ""
        let appDir = (execPath as NSString).deletingLastPathComponent
        let resourcesDir = (appDir as NSString).deletingLastPathComponent + "/Resources/plugins"
        if FileManager.default.fileExists(atPath: resourcesDir) {
            pluginSourceDir = resourcesDir
        }
    }
    
    func setupUI() {
        guard let contentView = window?.contentView else { return }
        contentView.wantsLayer = true
        contentView.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
        
        // Header icon
        let iconLabel = NSTextField(labelWithString: "⚙️")
        iconLabel.font = NSFont.systemFont(ofSize: 48)
        iconLabel.frame = NSRect(x: 0, y: 370, width: 550, height: 60)
        iconLabel.alignment = .center
        contentView.addSubview(iconLabel)
        
        // Title
        let titleLabel = NSTextField(labelWithString: APP_NAME)
        titleLabel.font = NSFont.boldSystemFont(ofSize: 22)
        titleLabel.frame = NSRect(x: 0, y: 340, width: 550, height: 30)
        titleLabel.alignment = .center
        titleLabel.isBezeled = false
        titleLabel.drawsBackground = false
        titleLabel.isEditable = false
        contentView.addSubview(titleLabel)
        
        // Subtitle
        let subtitleLabel = NSTextField(labelWithString: "Install the MCP Plugin for AI-powered Qt Creator control")
        subtitleLabel.font = NSFont.systemFont(ofSize: 13)
        subtitleLabel.textColor = .secondaryLabelColor
        subtitleLabel.frame = NSRect(x: 0, y: 315, width: 550, height: 20)
        subtitleLabel.alignment = .center
        subtitleLabel.isBezeled = false
        subtitleLabel.drawsBackground = false
        subtitleLabel.isEditable = false
        contentView.addSubview(subtitleLabel)
        
        // System info box
        let sysBox = NSBox(frame: NSRect(x: 30, y: 250, width: 490, height: 55))
        sysBox.title = "System Information"
        sysBox.titleFont = NSFont.boldSystemFont(ofSize: 12)
        contentView.addSubview(sysBox)
        
        let macosVersion = ProcessInfo.processInfo.operatingSystemVersionString
        let arch = getMachineArch()
        let sysInfoLabel = NSTextField(labelWithString: "✓ macOS \(macosVersion)\n• \(arch)")
        sysInfoLabel.font = NSFont.systemFont(ofSize: 11)
        sysInfoLabel.frame = NSRect(x: 10, y: 5, width: 470, height: 30)
        sysInfoLabel.isBezeled = false
        sysInfoLabel.drawsBackground = false
        sysInfoLabel.isEditable = false
        sysBox.contentView?.addSubview(sysInfoLabel)
        
        // Qt Creator selection box
        let qtBox = NSBox(frame: NSRect(x: 30, y: 120, width: 490, height: 120))
        qtBox.title = "Qt Creator Installation"
        qtBox.titleFont = NSFont.boldSystemFont(ofSize: 12)
        contentView.addSubview(qtBox)
        
        popup = NSPopUpButton(frame: NSRect(x: 10, y: 70, width: 420, height: 25))
        popup.target = self
        popup.action = #selector(qtCreatorSelected(_:))
        qtBox.contentView?.addSubview(popup)
        
        let refreshBtn = NSButton(frame: NSRect(x: 435, y: 70, width: 35, height: 25))
        refreshBtn.title = "🔄"
        refreshBtn.bezelStyle = .rounded
        refreshBtn.target = self
        refreshBtn.action = #selector(refreshClicked(_:))
        qtBox.contentView?.addSubview(refreshBtn)
        
        versionLabel = NSTextField(labelWithString: "")
        versionLabel.font = NSFont.systemFont(ofSize: 11)
        versionLabel.textColor = .secondaryLabelColor
        versionLabel.frame = NSRect(x: 15, y: 48, width: 460, height: 18)
        versionLabel.isBezeled = false
        versionLabel.drawsBackground = false
        versionLabel.isEditable = false
        qtBox.contentView?.addSubview(versionLabel)
        
        existingPluginLabel = NSTextField(labelWithString: "")
        existingPluginLabel.font = NSFont.systemFont(ofSize: 11)
        existingPluginLabel.textColor = .secondaryLabelColor
        existingPluginLabel.frame = NSRect(x: 15, y: 28, width: 460, height: 18)
        existingPluginLabel.isBezeled = false
        existingPluginLabel.drawsBackground = false
        existingPluginLabel.isEditable = false
        qtBox.contentView?.addSubview(existingPluginLabel)
        
        statusLabel = NSTextField(labelWithString: "")
        statusLabel.font = NSFont.systemFont(ofSize: 11)
        statusLabel.textColor = .secondaryLabelColor
        statusLabel.frame = NSRect(x: 15, y: 8, width: 460, height: 18)
        statusLabel.isBezeled = false
        statusLabel.drawsBackground = false
        statusLabel.isEditable = false
        qtBox.contentView?.addSubview(statusLabel)
        
        // Progress container (hidden initially)
        progressContainer = NSView(frame: NSRect(x: 30, y: 60, width: 490, height: 50))
        progressContainer.isHidden = true
        contentView.addSubview(progressContainer)
        
        progressLabel = NSTextField(labelWithString: "Installing...")
        progressLabel.font = NSFont.systemFont(ofSize: 11)
        progressLabel.frame = NSRect(x: 0, y: 30, width: 490, height: 18)
        progressLabel.isBezeled = false
        progressLabel.drawsBackground = false
        progressLabel.isEditable = false
        progressContainer.addSubview(progressLabel)
        
        progressBar = NSProgressIndicator(frame: NSRect(x: 0, y: 5, width: 490, height: 20))
        progressBar.style = .bar
        progressBar.isIndeterminate = false
        progressBar.minValue = 0
        progressBar.maxValue = 100
        progressContainer.addSubview(progressBar)
        
        // Buttons
        cancelButton = NSButton(frame: NSRect(x: 30, y: 15, width: 100, height: 32))
        cancelButton.title = "Cancel"
        cancelButton.bezelStyle = .rounded
        cancelButton.target = self
        cancelButton.action = #selector(cancelClicked(_:))
        contentView.addSubview(cancelButton)
        
        installButton = NSButton(frame: NSRect(x: 400, y: 15, width: 120, height: 32))
        installButton.title = "Install"
        installButton.bezelStyle = .rounded
        installButton.keyEquivalent = "\r"
        installButton.target = self
        installButton.action = #selector(installClicked(_:))
        contentView.addSubview(installButton)
    }
    
    func getMachineArch() -> String {
        var sysinfo = utsname()
        uname(&sysinfo)
        let machine = withUnsafePointer(to: &sysinfo.machine) {
            $0.withMemoryRebound(to: CChar.self, capacity: 1) {
                String(cString: $0)
            }
        }
        if machine.contains("arm64") {
            return "Apple Silicon (arm64)"
        } else if machine.contains("x86_64") {
            return "Intel (x86_64)"
        }
        return machine
    }
    
    func scanForQtCreator() {
        qtCreatorPaths = QtCreatorFinder.findAll()
        
        popup.removeAllItems()
        
        if qtCreatorPaths.isEmpty {
            popup.addItem(withTitle: "No Qt Creator found")
            versionLabel.stringValue = "⚠️ Please install Qt Creator"
            existingPluginLabel.stringValue = ""
            statusLabel.stringValue = ""
            installButton.isEnabled = false
        } else {
            for path in qtCreatorPaths {
                popup.addItem(withTitle: path)
            }
            selectedPath = qtCreatorPaths[0]
            updateSelection()
        }
    }
    
    func updateSelection() {
        guard !selectedPath.isEmpty else { return }
        
        // Show Qt Creator version
        if let version = QtCreatorFinder.getVersion(appPath: selectedPath) {
            versionLabel.stringValue = "✓ Qt Creator \(version)"
            versionLabel.textColor = .systemGreen
        } else {
            versionLabel.stringValue = "• Version unknown"
            versionLabel.textColor = .secondaryLabelColor
        }
        
        // Check for existing plugin in the ACTUAL Qt Creator plugin directory
        let installer = PluginInstaller(qtCreatorPath: selectedPath, pluginSourceDir: pluginSourceDir)
        
        if installer.hasExistingPlugin() {
            let info = installer.getExistingPluginInfo()
            existingPluginLabel.stringValue = "⚠️ Existing plugin: \(info)"
            existingPluginLabel.textColor = .systemOrange
            statusLabel.stringValue = "• Will be removed and replaced"
            statusLabel.textColor = .secondaryLabelColor
        } else {
            existingPluginLabel.stringValue = "• No existing plugin installed"
            existingPluginLabel.textColor = .secondaryLabelColor
            statusLabel.stringValue = "• Ready to install"
            statusLabel.textColor = .secondaryLabelColor
        }
        
        // Check if we have plugin files to install
        if pluginSourceDir.isEmpty {
            statusLabel.stringValue = "⚠️ Plugin files not found in app bundle"
            statusLabel.textColor = .systemRed
            installButton.isEnabled = false
        } else {
            installButton.isEnabled = true
        }
        
        // Reset button state if we switched selection
        if installationComplete {
            installationComplete = false
            installButton.title = "Install"
            installButton.action = #selector(installClicked(_:))
            cancelButton.title = "Cancel"
        }
    }
    
    @objc func qtCreatorSelected(_ sender: NSPopUpButton) {
        if let selected = sender.selectedItem?.title, qtCreatorPaths.contains(selected) {
            selectedPath = selected
            updateSelection()
        }
    }
    
    @objc func refreshClicked(_ sender: NSButton) {
        scanForQtCreator()
    }
    
    @objc func cancelClicked(_ sender: NSButton) {
        NSApp.terminate(nil)
    }
    
    @objc func installClicked(_ sender: NSButton) {
        // Check if Qt Creator is running
        if isQtCreatorRunning() {
            let alert = NSAlert()
            alert.messageText = "Qt Creator Running"
            alert.informativeText = "Qt Creator must be closed before installing the plugin.\n\nWould you like to quit Qt Creator now?"
            alert.addButton(withTitle: "Quit Qt Creator")
            alert.addButton(withTitle: "Cancel")
            
            if alert.runModal() == .alertFirstButtonReturn {
                quitQtCreator()
                // Wait for Qt Creator to quit
                for _ in 0..<30 {
                    Thread.sleep(forTimeInterval: 0.5)
                    if !isQtCreatorRunning() {
                        break
                    }
                }
                
                if isQtCreatorRunning() {
                    let alert2 = NSAlert()
                    alert2.messageText = "Qt Creator Still Running"
                    alert2.informativeText = "Qt Creator could not be closed. Please close it manually and try again."
                    alert2.alertStyle = .warning
                    alert2.runModal()
                    return
                }
            } else {
                return
            }
        }
        
        // Start installation
        installButton.isEnabled = false
        cancelButton.isEnabled = false
        progressContainer.isHidden = false
        
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            self?.runInstallation()
        }
    }
    
    @objc func launchClicked(_ sender: NSButton) {
        // Quit Qt Creator if running
        if isQtCreatorRunning() {
            updateProgress(text: "Quitting Qt Creator...", value: 20)
            quitQtCreator()
            
            // Wait for it to quit
            for _ in 0..<30 {
                Thread.sleep(forTimeInterval: 0.5)
                if !isQtCreatorRunning() {
                    break
                }
            }
            Thread.sleep(forTimeInterval: 1) // Extra wait to ensure clean quit
        }
        
        // Launch Qt Creator
        updateProgress(text: "Launching Qt Creator...", value: 60)
        
        let task = Process()
        task.launchPath = "/usr/bin/open"
        task.arguments = [selectedPath]
        task.launch()
        
        updateProgress(text: "Qt Creator launched!", value: 100)
        
        // Wait a moment then quit installer
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
            NSApp.terminate(nil)
        }
    }
    
    func isQtCreatorRunning() -> Bool {
        let task = Process()
        task.launchPath = "/usr/bin/pgrep"
        task.arguments = ["-f", "Qt Creator"]
        task.standardOutput = FileHandle.nullDevice
        task.standardError = FileHandle.nullDevice
        task.launch()
        task.waitUntilExit()
        return task.terminationStatus == 0
    }
    
    func quitQtCreator() {
        let task = Process()
        task.launchPath = "/usr/bin/pkill"
        task.arguments = ["-f", "Qt Creator"]
        task.standardOutput = FileHandle.nullDevice
        task.standardError = FileHandle.nullDevice
        task.launch()
        task.waitUntilExit()
    }
    
    func runInstallation() {
        let installer = PluginInstaller(qtCreatorPath: selectedPath, pluginSourceDir: pluginSourceDir)
        
        // Step 1: Remove existing plugin
        updateProgress(text: "Checking for existing plugin...", value: 10)
        
        let existingFiles = installer.getExistingPluginFiles()
        if !existingFiles.isEmpty {
            updateProgress(text: "Removing \(existingFiles.count) existing plugin file(s)...", value: 20)
            
            do {
                let result = try installer.removeExistingPlugin()
                if result.failed > 0 {
                    updateProgress(text: "Removed \(result.removed), failed \(result.failed): \(result.failedFiles.joined(separator: ", "))", value: 35)
                } else {
                    updateProgress(text: "Removed \(result.removed) existing file(s)", value: 40)
                }
            } catch {
                showError("Failed to remove existing plugin: \(error.localizedDescription)")
                resetUI()
                return
            }
        } else {
            updateProgress(text: "No existing plugin to remove", value: 30)
        }
        
        // Step 2: Install new plugin
        updateProgress(text: "Installing plugin files...", value: 50)
        
        do {
            let installed = try installer.installPlugin()
            updateProgress(text: "Installed \(installed) file(s)", value: 80)
        } catch {
            showError("Failed to install plugin: \(error.localizedDescription)")
            resetUI()
            return
        }
        
        // Step 3: Verify installation
        updateProgress(text: "Verifying installation...", value: 90)
        
        let (success, message) = installer.verify()
        
        if success {
            updateProgress(text: "✅ Installation complete!", value: 100)
            installationComplete = true
            
            DispatchQueue.main.async { [weak self] in
                // Change Install button to Launch button
                self?.installButton.title = "Launch Qt Creator"
                self?.installButton.action = #selector(self?.launchClicked(_:))
                self?.installButton.isEnabled = true
                self?.cancelButton.title = "Close"
                self?.cancelButton.isEnabled = true
                
                // Update status
                self?.statusLabel.stringValue = "✅ Plugin installed successfully"
                self?.statusLabel.textColor = .systemGreen
                self?.existingPluginLabel.stringValue = "• Click 'Launch Qt Creator' to start"
                self?.existingPluginLabel.textColor = .secondaryLabelColor
            }
        } else {
            showError("Verification failed: \(message)")
            resetUI()
        }
    }
    
    func resetUI() {
        DispatchQueue.main.async { [weak self] in
            self?.installButton.isEnabled = true
            self?.cancelButton.isEnabled = true
            self?.progressContainer.isHidden = true
        }
    }
    
    func updateProgress(text: String, value: Double) {
        DispatchQueue.main.async { [weak self] in
            self?.progressLabel.stringValue = text
            self?.progressBar.doubleValue = value
        }
    }
    
    func showError(_ message: String) {
        DispatchQueue.main.async {
            let alert = NSAlert()
            alert.messageText = "Installation Failed"
            alert.informativeText = message
            alert.alertStyle = .critical
            alert.addButton(withTitle: "OK")
            alert.runModal()
        }
    }
    
    func windowWillClose(_ notification: Notification) {
        NSApp.terminate(nil)
    }
}

// MARK: - App Delegate

class AppDelegate: NSObject, NSApplicationDelegate {
    var windowController: InstallerWindowController?
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        windowController = InstallerWindowController()
        windowController?.showWindow(nil)
        windowController?.window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
    
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }
}

// MARK: - Main

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
