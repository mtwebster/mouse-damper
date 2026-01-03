# Mouse Damper for Windows

Windows-native implementation of Mouse Damper.

## System Requirements

- Windows 7 or newer (32-bit or 64-bit)
- No external dependencies required

## Building from Source

### Prerequisites

1. MSYS2 - Download from https://www.msys2.org/
2. Install MSYS2 and open **MSYS2 MINGW64** terminal
3. Install build tools:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-meson mingw-w64-x86_64-ninja
   ```

### Build Steps

```bash
cd /d/dev/mouse-damper
meson setup build
ninja -C build
```

### Build Output

Three executables are created in `build/src/`:
- `mousedamper.exe` (~290KB) - Main daemon
- `mousedamper-launch.exe` (~293KB) - Launcher with configuration support
- `mousedamper-config.exe` (~308KB) - GUI configuration tool

## Installation

### Option 1: NSIS Installer (Recommended)

If you have NSIS installed (https://nsis.sourceforge.io/):

```bash
makensis installer.nsi
```

This creates `mousedamper-setup.exe` which:
- Installs to `C:\Program Files\MouseDamper\`
- Creates Start Menu shortcuts
- Registers in Programs and Features
- Creates uninstaller

### Option 2: Manual Installation

1. **Create installation directory:**
   ```
   C:\Program Files\MouseDamper\
   ```

2. **Copy executables:**
   ```
   build\src\mousedamper.exe         → C:\Program Files\MouseDamper\
   build\src\mousedamper-launch.exe  → C:\Program Files\MouseDamper\
   build\src\mousedamper-config.exe  → C:\Program Files\MouseDamper\
   ```

3. **Create Start Menu shortcut (optional):**
   - Right-click on `mousedamper-config.exe`
   - Send to → Desktop (create shortcut)
   - Move shortcut to: `%APPDATA%\Microsoft\Windows\Start Menu\Programs\`

Settings are stored in: `%APPDATA%\mousedamper\config.ini`

Example:
```ini
[MouseDamper]
Enabled=1
DeltaThreshold=100
OverrideDoubleClickTime=0
DoubleClickTimeOverride=400
```

### If installed with NSIS installer:
1. Use "Uninstall Mouse Damper" from Start Menu, or
2. Use Programs and Features in Control Panel

### If installed manually:
1. Stop the daemon: `taskkill /F /IM mousedamper.exe`
2. Remove autostart (if enabled):
   - Open Registry Editor (regedit)
   - Navigate to: `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`
   - Delete value: `MouseDamper`
3. Delete installation directory: `C:\Program Files\MouseDamper\`
4. Delete configuration (optional): `%APPDATA%\mousedamper\`

## License

GPL-3.0 - See COPYING file for details

Copyright 2020 Michael Webster <miketwebster@gmail.com>
