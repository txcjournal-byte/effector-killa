; Inno Setup script for TrapWin VOX DEMON (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{9E3B6D21-4C7A-4B85-B1F2-6A0D8C7E4F19}
AppName=TrapWin VOX DEMON
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\VOX DEMON
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-VOX-DEMON-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin VOX DEMON.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin VOX DEMON.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\VOX DEMON"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin VOX DEMON.vst3"
