; Inno Setup script for TrapWin SUB KILLA (Windows, VST3)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{FACD6D0A-6ED4-43AD-9539-292A80C21E2F}
AppName=TrapWin SUB KILLA
AppVersion={#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonpf64}\TrapWin\SUB KILLA
LicenseFile=EULA.txt
OutputDir=..\installer-out
OutputBaseFilename=TrapWin-SUB-KILLA-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\TrapWin SUB KILLA.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin SUB KILLA.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{commonpf64}\TrapWin\SUB KILLA"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin SUB KILLA.vst3"
