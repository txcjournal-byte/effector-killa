; Inno Setup script for 808 KILLA (Windows, VST3)
; Build: ISCC.exe /DAppVersion=0.2.0 installer\win\808killa.iss
#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif

[Setup]
AppId={{6F8B2C41-808A-4B1D-9C7E-3D2A1B0C0808}
AppName=808 KILLA
AppVersion={#AppVersion}
AppPublisher=808 KILLA
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\808 KILLA
LicenseFile=..\EULA.txt
OutputDir=..\..\build\installer
OutputBaseFilename=808-KILLA-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "..\..\build\K808_artefacts\Release\VST3\808 KILLA.vst3\*"; DestDir: "{commoncf64}\VST3\808 KILLA.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\EULA.txt"; DestDir: "{commonpf64}\808 KILLA"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\808 KILLA.vst3"
