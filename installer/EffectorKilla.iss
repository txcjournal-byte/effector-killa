; TrapWin Effector – Windows installer (Inno Setup 6)
; Built by CI:  iscc /DAppVersion=1.0.0 /DSourceDir=<dist> installer\EffectorKilla.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{7AFD1EC1-11A0-4D1A-B5F4-B62083E26212}
AppName=TrapWin Effector
AppVersion={#AppVersion}
AppVerName=TrapWin Effector {#AppVersion}
AppPublisher=TrapWin
DefaultDirName={autopf}\TrapWin\TrapWin Effector
DefaultGroupName=TrapWin
DisableProgramGroupPage=yes
LicenseFile=..\EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWinEffector-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=TrapWin Effector

[Types]
Name: "full"; Description: "VST3 + Standalone"
Name: "vst3"; Description: "VST3 only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (C:\Program Files\Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#SourceDir}\TrapWin Effector.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin Effector.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\TrapWin Effector.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\MANUAL.md"; DestDir: "{app}"; DestName: "Manual.txt"; Flags: ignoreversion
Source: "..\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resources\Fonts\*.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\TrapWin Effector"; Filename: "{app}\TrapWin Effector.exe"; Components: standalone
Name: "{group}\TrapWin Effector Manual"; Filename: "{app}\Manual.txt"

[Run]
Filename: "{app}\Manual.txt"; Description: "Open the manual"; Flags: postinstall shellexec skipifsilent unchecked
