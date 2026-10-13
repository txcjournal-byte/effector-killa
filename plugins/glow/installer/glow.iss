; Inno Setup script for TrapWin GLOW (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{7BBEF193-DFB3-4D61-B300-3CD7E0D6D489}
AppName=TrapWin GLOW
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\GLOW
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-GLOW-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin GLOW.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin GLOW.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\GLOW"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin GLOW.vst3"
