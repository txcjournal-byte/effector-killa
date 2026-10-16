; =====================================================================================
;  TrapWin BEND DEMON - Inno Setup 6 installer script
;  Built by scripts\build-win.ps1:
;     iscc /DAppVersion=1.0.0 /DSourceDir=<dir with BEND DEMON.vst3 + BEND DEMON.exe> /DOutputDir=<out> BendDemon.iss
; =====================================================================================

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\win\bundles"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist\win"
#endif

[Setup]
AppId={{6B0C9E2A-7D41-4C3B-9A7E-1F2D3B4C5D6E}
AppName=TrapWin BEND DEMON
AppVersion={#AppVersion}
AppVerName=TrapWin BEND DEMON {#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={autopf}\TrapWin\BEND DEMON
DefaultGroupName=TrapWin
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=TrapWinBendDemon-{#AppVersion}-Windows-Setup
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile=license-win.txt
UninstallDisplayName=TrapWin BEND DEMON
MinVersion=10.0

[Types]
Name: "full";   Description: "VST3 plug-in + standalone app"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in  (C:\Program Files\Common Files\VST3)"; Types: full custom; Flags: fixed
Name: "app";  Description: "Standalone app"; Types: full

[Files]
; VST3 bundle -> C:\Program Files\Common Files\VST3\BEND DEMON.vst3\...
Source: "{#SourceDir}\BEND DEMON.vst3\*"; DestDir: "{commoncf64}\VST3\BEND DEMON.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\BEND DEMON.exe";    DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\TrapWin\BEND DEMON"; Filename: "{app}\BEND DEMON.exe"; Components: app

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\BEND DEMON.vst3"

[Messages]
FinishedLabel=BEND DEMON is installed.%n%nIn FL Studio: Options > Manage plugins > Find plugins, then add BEND DEMON from the plugin database (Generators).
