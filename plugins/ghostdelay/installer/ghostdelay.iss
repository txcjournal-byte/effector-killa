; Inno Setup script for TrapWin GHOST DELAY (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{B1F44D36-36EC-4DFC-A945-01AB64953ADF}
AppName=TrapWin GHOST DELAY
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\GHOST DELAY
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-GHOST-DELAY-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin GHOST DELAY.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin GHOST DELAY.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\GHOST DELAY"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin GHOST DELAY.vst3"
