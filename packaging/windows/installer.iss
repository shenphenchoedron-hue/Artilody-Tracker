; Inno Setup script for Artilody Tracker
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\windows"
#endif

[Setup]
AppId={{6F1E7C0B-2D3A-4C1E-9A57-7D4B8E1A2C55}
AppName=Artilody Tracker
AppVersion={#AppVersion}
AppPublisher=Artilody
AppPublisherURL=https://github.com/shenphenchoedron-hue/Artilody-Tracker
DefaultDirName={autopf}\Artilody Tracker
DefaultGroupName=Artilody Tracker
UninstallDisplayIcon={app}\Artilody Tracker.exe
OutputBaseFilename=ArtilodyTracker-{#AppVersion}-Windows-x64-Setup
OutputDir=..\..\dist
SetupIconFile=icon.ico
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
LicenseFile=..\..\LICENSE
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "danish"; MessagesFile: "compiler:Languages\Danish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Artilody Tracker"; Filename: "{app}\Artilody Tracker.exe"
Name: "{group}\Uninstall Artilody Tracker"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Artilody Tracker"; Filename: "{app}\Artilody Tracker.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.artixml"; ValueType: string; ValueName: ""; ValueData: "ArtilodyTracker.Project"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.artilody"; ValueType: string; ValueName: ""; ValueData: "ArtilodyTracker.Project"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\ArtilodyTracker.Project"; ValueType: string; ValueName: ""; ValueData: "Artilody Tracker Project"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\ArtilodyTracker.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Artilody Tracker.exe,0"
Root: HKA; Subkey: "Software\Classes\ArtilodyTracker.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Artilody Tracker.exe"" ""%1"""

[Run]
Filename: "{app}\Artilody Tracker.exe"; Description: "{cm:LaunchProgram,Artilody Tracker}"; Flags: nowait postinstall skipifsilent
