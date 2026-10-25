; Inno Setup 6 script for the SORTED engine SDK (Windows x64).
; Built by "./build.sh --package" on Windows, which runs:
;   ISCC /DAppVersion=<x.y.z> /DStageDir=<"cmake --install --component sdk" prefix> /O<output dir> SORTED-SDK.iss
; The version comes from CMakeLists.txt; do not hard-code one here.

#ifndef AppVersion
  #error Pass /DAppVersion=<x.y.z> (build.sh reads it from CMakeLists.txt)
#endif
#ifndef StageDir
  #error Pass /DStageDir=<directory written by "cmake --install --component sdk --prefix">
#endif

[Setup]
; Never change AppId: Windows uses it to recognise an earlier install and upgrade it in place.
AppId={{2BC22EC9-A252-4BE4-9D1A-F164AB649C2A}
AppName=SORTED SDK
AppVersion={#AppVersion}
AppPublisher=VipulSharma143
AppPublisherURL=https://github.com/VipulSharma143/Game-Engine
DefaultDirName={autopf}\SORTED
DisableProgramGroupPage=yes
OutputBaseFilename=SORTED-SDK-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per user by default (no admin prompt); the dialog or /ALLUSERS installs into Program Files for everyone.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline dialog
ChangesEnvironment=yes
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
VersionInfoVersion={#AppVersion}.0

[Tasks]
; CMake finds the SDK through SORTED_ROOT without any extra option. An all-users install in Program Files is
; also found automatically, so the variable is only added for per-user installs.
Name: "setroot"; Description: "Set SORTED_ROOT so that CMake's find_package(SORTED) finds this SDK"; Check: not IsAdminInstallMode

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: string; ValueName: "SORTED_ROOT"; ValueData: "{app}"; Flags: uninsdeletevalue; Tasks: setroot
