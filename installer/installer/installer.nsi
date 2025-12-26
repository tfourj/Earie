;================================
; Qt Installer Template - NSIS Script
;================================
; CUSTOMIZE: Replace all placeholders marked with "YOUR_*" below
;================================

;--------------------------------
; Use Modern UI 2
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "nsProcess.nsh"

;--------------------------------
; Include version information (generated during build)
!include "version.nsh"

;--------------------------------
; ============================================
; CONFIGURATION - CUSTOMIZE THESE VALUES
; ============================================
!define APPNAME "Earie"              ; YOUR_APP_NAME
!define COMPANYNAME "TfourJ"           ; YOUR_COMPANY_NAME
!define DESCRIPTION "Qt-based volume mixer" ; YOUR_APP_DESCRIPTION
!define ABOUTURL "https://github.com/tfourj/Earie"    ; YOUR_ABOUT_URL
; ============================================

Name "${APPNAME}"
OutFile "Earie-Setup.exe"            ; YOUR_OUTPUT_FILENAME
InstallDir "$PROGRAMFILES\${APPNAME}"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallLocation"
RequestExecutionLevel admin

;--------------------------------
; Start Menu page settings
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\${APPNAME}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

Var StartMenuFolder

;--------------------------------
; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES

; Finish page with "Run" checkbox and "Create Desktop Shortcut"
Function finishpageaction
  ; Create desktop shortcut
  CreateShortcut "$desktop\${APPNAME}.lnk" "$instdir\${APPNAME}.exe"  ; YOUR_EXE_NAME
FunctionEnd

!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPNAME}.exe"  ; YOUR_EXE_NAME
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Desktop Shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION finishpageaction
!insertmacro MUI_PAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Kill process if running before install starts
Function .onInit
  ; Kill app if running - UPDATE PROCESS NAME HERE
  nsProcess::_FindProcess "Earie.exe"  ; YOUR_EXE_NAME
  Pop $0
  ${If} $0 == 0
    nsProcess::_KillProcess "Earie.exe"  ; YOUR_EXE_NAME
    Pop $1
    Sleep 1000
  ${EndIf}
  
  ; Check for existing installation
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString"
  ${If} $0 != ""
    MessageBox MB_YESNO|MB_ICONQUESTION \
      "${APPNAME} is already installed. $\n$\nDo you want to uninstall the previous version before installing?" \
      /SD IDYES IDNO +2
      ExecWait '$0 _?=$INSTDIR' ; Do not copy the uninstaller to a temp file
  ${EndIf}
FunctionEnd

;--------------------------------
; Sections

; Main Application (always installed)
Section "Main Application" SecMain
  SectionIn RO ; Required section

  SetOutPath "$INSTDIR"
  File /r "Earie\*.*"  ; YOUR_APP_FOLDER_NAME - Must match folder name in temp_installer directory

  ; Save install path to registry (legacy)
  WriteRegStr HKCU "Software\${APPNAME}" "" $INSTDIR

  ; Create Start Menu shortcut
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortcut "$SMPROGRAMS\$StartMenuFolder\${APPNAME}.lnk" "$INSTDIR\${APPNAME}.exe"  ; YOUR_EXE_NAME
  !insertmacro MUI_STARTMENU_WRITE_END

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  ; Write Windows uninstall registry entries
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "QuietUninstallString" "$\"$INSTDIR\Uninstall.exe$\" /S"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\${APPNAME}.exe"  ; YOUR_EXE_NAME
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${COMPANYNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "URLInfoAbout" "${ABOUTURL}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${FULLVERSION}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "VersionMajor" ${VERSIONMAJOR}
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "VersionMinor" ${VERSIONMINOR}
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "EstimatedSize" ${INSTALLSIZE}
  
  ; Add install date
  System::Call 'kernel32::GetTickCount()i.r0'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallDate" $0
SectionEnd

; Optional VC++ Redistributable Section (unchecked by default)
Section /o "Microsoft Visual C++ Redistributable" SecVCRedist
  DetailPrint "Downloading Microsoft Visual C++ Redistributable..."
  
  ; Download the VC++ redistributable to temp directory
  NSISdl::download "https://aka.ms/vs/17/release/vc_redist.x64.exe" "$TEMP\vc_redist.x64.exe"
  Pop $0 ; get the return value
  
  ; Check if download was successful
  StrCmp $0 success download_success
    MessageBox MB_ICONSTOP "Download failed: $0"
    Goto download_end
    
  download_success:
    DetailPrint "Installing Microsoft Visual C++ Redistributable..."
    
    ; Run the redistributable installer silently
    ExecWait '"$TEMP\vc_redist.x64.exe" /install /passive /norestart' $0
    
    ; Check installation result
    ${If} $0 == 0
      DetailPrint "Microsoft Visual C++ Redistributable installed successfully"
    ${ElseIf} $0 == 1638
      DetailPrint "Microsoft Visual C++ Redistributable is already installed (newer or same version)"
    ${Else}
      DetailPrint "Microsoft Visual C++ Redistributable installation failed (Exit code: $0)"
    ${EndIf}
    
    ; Clean up downloaded file
    Delete "$TEMP\vc_redist.x64.exe"
    
  download_end:
SectionEnd

;--------------------------------
; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "Main ${APPNAME} application (required)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVCRedist} "Download and install Microsoft Visual C++ Redistributable (recommended for compatibility)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; Uninstaller
Section "Uninstall"
  ; Kill app if running - UPDATE PROCESS NAME HERE
  nsProcess::_FindProcess "Earie.exe"  ; YOUR_EXE_NAME
  Pop $0
  ${If} $0 == 0
    nsProcess::_KillProcess "Earie.exe"  ; YOUR_EXE_NAME
    Pop $1
    Sleep 1000
    nsProcess::_FindProcess "Earie.exe"  ; YOUR_EXE_NAME
    Pop $2
    ${If} $2 == 0
      MessageBox MB_OK|MB_ICONEXCLAMATION \
        "${APPNAME} is still running and could not be closed. Please close it and retry."
      Quit
    ${EndIf}
  ${EndIf}

  ; Remove installed files
  RMDir /r "$INSTDIR"

  ; Remove Start Menu folder
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
  RMDir /r "$SMPROGRAMS\$StartMenuFolder"

  ; Remove desktop shortcut
  Delete "$DESKTOP\${APPNAME}.lnk"

  ; Remove registry entries
  DeleteRegKey HKCU "Software\${APPNAME}"
  
  ; Remove Windows uninstall registry entries
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
SectionEnd

