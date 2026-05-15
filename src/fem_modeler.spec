# PyInstaller spec for the Qt modeler (onedir — faster and more reliable than onefile for PySide6).
from PyInstaller.utils.hooks import collect_data_files, collect_dynamic_libs

block_cipher = None

# Qt platform plugin + DLLs only (avoid --collect-all PySide6, which is very slow).
_qt_datas = collect_data_files(
    "PySide6",
    includes=[
        "plugins/platforms/qwindows.dll",
        "plugins/styles/qmodernwindowsstyle.dll",
    ],
)
_qt_binaries = collect_dynamic_libs("PySide6")

a = Analysis(
    ["Modeler/main.py"],
    pathex=["Modeler"],
    binaries=_qt_binaries,
    datas=_qt_datas,
    hiddenimports=[
        "PySide6.QtCore",
        "PySide6.QtGui",
        "PySide6.QtWidgets",
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="fem_modeler",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=False,
    upx_exclude=[],
    name="fem_modeler",
)
