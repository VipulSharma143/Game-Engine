; Inno Setup 6 script for FunkShot, the game built on the SORTED engine (Windows x64).
; Built by "./build.sh --package" on Windows, which runs:
;   ISCC /DAppVersion=<x.y.z> /DStageDir=<"cmake --install" prefix> /O<output dir> FunkShot.iss
; The version comes from CMakeLists.txt; do not hard-code one here.

#ifndef AppVersion
  #error Pass /DAppVersion=<x.y.z> (build.sh reads it from CMakeLists.txt)
#endif
#ifndef StageDir
  #error Pass /DStageDir=<directory written by "cmake --install --prefix">
#endif

[Setup]
; Never change AppId: Windows uses it to recognise an earlier install and upgrade it in place.
AppId={{F905FA3A-154E-45EA-B734-D6657944DE22}
AppName=FunkShot
AppVersion={#AppVersion}
AppPublisher=VipulSharma143
AppPublisherURL=https://github.com/VipulSharma143/Game-Engine
DefaultDirName={autopf}\FunkShot
DefaultGroupName=FunkShot
DisableProgramGroupPage=yes
OutputBaseFilename=FunkShot-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Installs for the current user without an admin prompt; the dialog (or /ALLUSERS) offers "all users" (Program Files).
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline dialog
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\funkshot.exe
VersionInfoVersion={#AppVersion}.0

[Files]
; The staged tree is bin\funkshot.exe, share\funkshot\assets\ and share\doc\funkshot\. The demo finds
; its assets at <exe>\..\share\funkshot\assets, so the same layout works installed anywhere.
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\FunkShot"; Filename: "{app}\bin\funkshot.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\bin\funkshot.exe"; Description: "Start FunkShot"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
