# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['/Volumes/Developer/depot/Qt_MCP_Plugin/installer/qt_mcp_installer.py'],
    pathex=[],
    binaries=[],
    datas=[],
    hiddenimports=['tkinter', 'tkinter.ttk', 'plistlib'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='Qt MCP Plugin Installer',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['/Volumes/Developer/depot/Qt_MCP_Plugin/installer/installer_icon.icns'],
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='Qt MCP Plugin Installer',
)
app = BUNDLE(
    coll,
    name='Qt MCP Plugin Installer.app',
    icon='/Volumes/Developer/depot/Qt_MCP_Plugin/installer/installer_icon.icns',
    bundle_identifier='com.qtmcp.installer',
)
