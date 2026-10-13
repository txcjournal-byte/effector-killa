; Inno Setup script for TrapWin SWEEP (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{E513FAC7-2581-4674-9ED9-B89D3B53E45D}
AppName=TrapWin SWEEP
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\SWEEP
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-SWEEP-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin SWEEP.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin SWEEP.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\SWEEP"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin SWEEP.vst3"
