; Inno Setup script for TrapWin AUTO KILLA (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{9FC53AC3-A33E-4C0C-AEDF-CB1ADE5DA5F4}
AppName=TrapWin AUTO KILLA
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\AUTO KILLA
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-AUTO-KILLA-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin AUTO KILLA.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin AUTO KILLA.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\AUTO KILLA"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin AUTO KILLA.vst3"
