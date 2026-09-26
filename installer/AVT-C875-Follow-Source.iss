#define MyAppName "AVT-C875 Follow Source"
#define MyAppVersion "0.3.0-alpha"
#define MyAppPublisher "YurKLYK"

[Setup]
AppId={{64575FE5-0D14-48D4-9CB9-EF01C8DB0E49}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\AVT-C875 Follow Source
DefaultGroupName=AVT-C875 Follow Source
OutputDir=output
OutputBaseFilename=AVT-C875-Follow-Source-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\recentral_share_injector.exe
WizardStyle=modern
CloseApplications=force
CloseApplicationsFilter=obs64.exe,RECentral.exe,AVerRECentral.exe
LicenseFile=..\LICENSE

[Files]
Source: "..\dist\obs-plugin\bin\64bit\c875-follow-source.dll"; DestDir: "{commonappdata}\obs-studio\plugins\c875-follow-source\bin\64bit"; Flags: ignoreversion
Source: "..\dist\obs-plugin\data\locale\*.ini"; DestDir: "{commonappdata}\obs-studio\plugins\c875-follow-source\data\locale"; Flags: ignoreversion
Source: "..\dist\hook\recentral_share_hook.dll"; DestDir: "{commonappdata}\obs-studio\plugins\c875-follow-source\data"; Flags: ignoreversion
Source: "..\dist\hook\recentral_share_injector.exe"; DestDir: "{commonappdata}\obs-studio\plugins\c875-follow-source\data"; Flags: ignoreversion
Source: "..\dist\hook\recentral_share_hook.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\hook\recentral_share_injector.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Enable-RECentral-TS-Sharing.cmd"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\troubleshooting.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Enable RECentral TS sharing"; Filename: "{app}\Enable-RECentral-TS-Sharing.cmd"; WorkingDir: "{app}"
Name: "{group}\README"; Filename: "{app}\README.md"
Name: "{group}\Uninstall AVT-C875 Follow Source"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\README.md"; Description: "READMEを開く"; Flags: postinstall shellexec skipifsilent unchecked
