; Mouse Damper Windows Installer
; NSIS Script for creating Windows installer

!include "MUI2.nsh"

; Installer configuration
Name "Mouse Damper"
OutFile "mousedamper-setup.exe"
InstallDir "$PROGRAMFILES64\MouseDamper"
RequestExecutionLevel admin

; Modern UI configuration
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_RUN "$INSTDIR\mousedamper-launch.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start Mouse Damper now"
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "COPYING"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; Version information
VIProductVersion "0.9.0.0"
VIAddVersionKey "ProductName" "Mouse Damper"
VIAddVersionKey "FileDescription" "Mouse Damper Installer"
VIAddVersionKey "FileVersion" "0.9.0.0"
VIAddVersionKey "ProductVersion" "0.9.0.0"
VIAddVersionKey "LegalCopyright" "GPL-3.0"
VIAddVersionKey "CompanyName" "Michael Webster"

; Installer sections
Section "Mouse Damper" SecMain
  SectionIn RO  ; Required section

  ; Stop any running mousedamper processes before installing
  ; Kill launcher FIRST so it doesn't respawn mousedamper.exe
  DetailPrint "Stopping any running Mouse Damper processes..."
  nsExec::Exec "taskkill /F /IM mousedamper-launch.exe"
  Pop $0
  nsExec::Exec "taskkill /F /IM mousedamper.exe"
  Pop $0
  nsExec::Exec "taskkill /F /IM mousedamper-config.exe"
  Pop $0
  ; Ignore errors if processes not running
  Sleep 500

  ; Set output path
  SetOutPath "$INSTDIR"

  ; Install executables
  File "build\src\mousedamper.exe"
  File "build\src\platform\windows\mousedamper-launch.exe"
  File "build\src\platform\windows\mousedamper-config.exe"

  ; Create Start Menu shortcuts
  CreateDirectory "$SMPROGRAMS\Mouse Damper"
  CreateShortcut "$SMPROGRAMS\Mouse Damper\Configure Mouse Damper.lnk" \
                 "$INSTDIR\mousedamper-config.exe" "" "$INSTDIR\mousedamper-config.exe" 0
  CreateShortcut "$SMPROGRAMS\Mouse Damper\Uninstall Mouse Damper.lnk" \
                 "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Add to Programs and Features
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
              "DisplayName" "Mouse Damper"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
              "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
              "DisplayIcon" "$INSTDIR\mousedamper-config.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
              "Publisher" "Michael Webster"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
              "DisplayVersion" "0.9.0"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
                "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper" \
                "NoRepair" 1

  ; Add autostart registry entry (manager always autostarts)
  DetailPrint "Adding to startup..."
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" \
              "MouseDamper" "$INSTDIR\mousedamper-launch.exe"

  ; Display completion message
  DetailPrint "Installation complete!"
  DetailPrint ""
  DetailPrint "To configure Mouse Damper, use the Start Menu shortcut or run:"
  DetailPrint "  $INSTDIR\mousedamper-config.exe"
SectionEnd

; Uninstaller section
Section "Uninstall"
  ; Stop any running mousedamper processes
  ; Kill launcher FIRST so it doesn't respawn mousedamper.exe
  DetailPrint "Stopping Mouse Damper processes..."
  nsExec::Exec "taskkill /F /IM mousedamper-launch.exe"
  Pop $0
  nsExec::Exec "taskkill /F /IM mousedamper.exe"
  Pop $0
  nsExec::Exec "taskkill /F /IM mousedamper-config.exe"
  Pop $0
  ; Ignore errors if processes not running

  ; Remove autostart registry entry
  DetailPrint "Removing autostart entry..."
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "MouseDamper"

  ; Remove executables
  Delete "$INSTDIR\mousedamper.exe"
  Delete "$INSTDIR\mousedamper-launch.exe"
  Delete "$INSTDIR\mousedamper-config.exe"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  ; Remove Start Menu shortcuts
  Delete "$SMPROGRAMS\Mouse Damper\Configure Mouse Damper.lnk"
  Delete "$SMPROGRAMS\Mouse Damper\Uninstall Mouse Damper.lnk"
  RMDir "$SMPROGRAMS\Mouse Damper"

  ; Remove from Programs and Features
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MouseDamper"

  ; Ask if user wants to remove configuration
  MessageBox MB_YESNO|MB_ICONQUESTION "Remove user configuration file?$\n$\nThis will delete your Mouse Damper settings.$\nLocation: %APPDATA%\mousedamper\config.ini" IDNO skip_config

  ; Remove user config
  Delete "$APPDATA\mousedamper\config.ini"
  RMDir "$APPDATA\mousedamper"

  skip_config:

  DetailPrint "Uninstallation complete!"
SectionEnd
