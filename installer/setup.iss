; Inno Setup Script for GTK3 CSS225 Project - With Correct EXE Path

[Setup]
; Basic Application Information
AppName=GTK3 CSS225 Project
AppVersion=1.0
AppVerName=SERC Mini-OS version 1.0
AppPublisher=Emmanuel Nkaka
AppCopyright=Copyright © 2026 Emmanuel Nkaka

; Installation Directories
DefaultDirName={userappdata}\GTK3 CSS225 Project
DefaultGroupName=GTK3 CSS225
AllowNoIcons=yes

; Output Settings
OutputDir=Output
OutputBaseFilename=GTK3_CSS225_Setup
SetupIconFile=app_icon.ico

; Compression Settings
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

; Show directory selection page
DisableDirPage=no
DisableProgramGroupPage=no
PrivilegesRequired=lowest

; Version Information
VersionInfoVersion=1.0.0.0
VersionInfoCompany=Emmanuel Nkaka
VersionInfoDescription=SERC Mini Operating System - Emergency Response Center
VersionInfoProductName=GTK3 CSS225 Project
VersionInfoProductVersion=1.0.0.0

; Architecture
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Uninstall Settings
UninstallDisplayIcon={app}\app_icon.ico
UninstallDisplayName=GTK3 CSS225 Project

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

; Files to install
[Files]

; ============ MAIN EXECUTABLE from bin\Debug ============
Source: "bin\Debug\gtk3 css225.exe"; DestDir: "{app}"; Flags: ignoreversion

; ============ GTK3 DLL FILES ============
; Add GTK DLLs (adjust paths based on your GTK installation)
; Common locations for GTK DLLs:
; - C:\msys64\mingw64\bin\
; - C:\GTK\bin\
; - Or where you installed GTK

; Core GTK DLLs (UNCOMMENT and adjust paths as needed)
; Source: "C:\msys64\mingw64\bin\libgtk-3-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libgdk-3-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libglib-2.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libgobject-2.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libcairo-2.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libpango-1.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libpangocairo-1.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libintl-8.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libharfbuzz-0.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libpng16-16.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libgio-2.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion

; MinGW Runtime DLLs (if needed)
; Source: "C:\msys64\mingw64\bin\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion

; ============ PROJECT FILES ============
; Main program files
Source: "gtk3 css225.cbp"; DestDir: "{app}"; Flags: ignoreversion
Source: "gtk3 css225.depend"; DestDir: "{app}"; Flags: ignoreversion
Source: "gtk3 css225.layout"; DestDir: "{app}"; Flags: ignoreversion

; Source code files
Source: "main.c"; DestDir: "{app}\src"; Flags: ignoreversion
Source: "Makfile.c"; DestDir: "{app}\src"; Flags: ignoreversion

; Documentation files - in documentation folder
Source: "Group 42 report.pdf"; DestDir: "{app}\documentation"; Flags: ignoreversion

; Icon and License
Source: "app_icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "license.txt"; DestDir: "{app}"; Flags: ignoreversion


; Installer script
Source: "setup.iss"; DestDir: "{app}\installer"; Flags: ignoreversion

; Icons
[Icons]
Name: "{group}\GTK3 CSS225"; Filename: "{app}\gtk3 css225.exe"; IconFilename: "{app}\app_icon.ico"; Comment: "Launch SERC Emergency Response Center"
Name: "{group}\Documentation"; Filename: "{app}\documentation"
Name: "{group}\Source Code"; Filename: "{app}\src"
Name: "{group}\Event Logs"; Filename: "{app}\logs"
Name: "{group}\{cm:UninstallProgram,GTK3 CSS225 Project}"; Filename: "{uninstallexe}"

; Desktop shortcut
Name: "{autodesktop}\GTK3 CSS225"; Filename: "{app}\gtk3 css225.exe"; IconFilename: "{app}\app_icon.ico"; Tasks: desktopicon

; Post-installation
[Run]
Filename: "{app}\gtk3 css225.exe"; Description: "Launch GTK3 CSS225 Project"; Flags: postinstall nowait skipifsilent

Filename: "{app}\documentation\Group 42 report.pdf"; Description: "View Group 42 Report"; Flags: postinstall shellexec skipifsilent

; Create directories
[Dirs]
Name: "{app}\src"
Name: "{app}\documentation"
Name: "{app}\logs"
Name: "{app}\installer"

; Uninstall cleanup
[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"
Type: dirifempty; Name: "{app}"