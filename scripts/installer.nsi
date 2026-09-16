; Per-user installer for the already packaged application. Requires NSIS 3.
; Runtime prerequisites (VC++ redistributable, ODBC driver) are explicit in README.
Unicode True
Name "CodeAsMetal"
OutFile "..\out\CodeAsMetal-Setup-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\CodeAsMetal"
RequestExecutionLevel user
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles
Section
 SetOutPath "$INSTDIR"
 File /r "${STAGE}\*.*"
 CreateShortcut "$DESKTOP\CodeAsMetal.lnk" "$INSTDIR\CodeAsMetal.exe"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd
Section "Uninstall"
 Delete "$DESKTOP\CodeAsMetal.lnk"
 RMDir /r "$INSTDIR"
 ; User project documents and recovery under AppLocalData are retained.
SectionEnd
