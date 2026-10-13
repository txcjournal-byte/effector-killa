; Inno Setup script for TrapWin WIDE (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{99B57209-8729-4DC5-B62A-8C0E57F8B274}
AppName=TrapWin WIDE
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\WIDE
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-WIDE-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin WIDE.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin WIDE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\WIDE"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin WIDE.vst3"
