; Effector Killa – Windows installer (Inno Setup 6)
; Built by CI:  iscc /DAppVersion=1.0.0 /DSourceDir=<dist> installer\EffectorKilla.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{A3901BEE-76CB-4476-B75B-BE9288D3DF60}
AppName=Effector Killa
AppVersion={#AppVersion}
AppVerName=Effector Killa {#AppVersion}
AppPublisher=Killa
DefaultDirName={autopf}\Killa\Effector Killa
DefaultGroupName=Killa
DisableProgramGroupPage=yes
LicenseFile=..\EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=EffectorKilla-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Effector Killa

[Types]
Name: "full"; Description: "VST3 + Standalone"
Name: "vst3"; Description: "VST3 only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (C:\Program Files\Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#SourceDir}\Effector Killa.vst3\*"; DestDir: "{commoncf64}\VST3\Effector Killa.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Effector Killa.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\MANUAL.md"; DestDir: "{app}"; DestName: "Manual.txt"; Flags: ignoreversion
Source: "..\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Effector Killa"; Filename: "{app}\Effector Killa.exe"; Components: standalone
Name: "{group}\Effector Killa Manual"; Filename: "{app}\Manual.txt"

[Run]
Filename: "{app}\Manual.txt"; Description: "Open the manual"; Flags: postinstall shellexec skipifsilent unchecked
