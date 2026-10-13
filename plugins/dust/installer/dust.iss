; Inno Setup script for TrapWin DUST TAPE (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{9D3E5A71-2C4B-4E8F-9B1A-5D7C0D0D0571}
AppName=TrapWin DUST TAPE
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\DUST
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-DUST-TAPE-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin DUST TAPE.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin DUST TAPE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\DUST"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin DUST TAPE.vst3"
