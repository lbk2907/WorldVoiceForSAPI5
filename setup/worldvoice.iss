; WorldVoice for SAPI 5 - one SAPI 5 voice that reads each language with the voice chosen for it.
; Built by build.cmd, after the programs (or: "C:\Program Files\Inno Setup 7\ISCC.exe" setup\worldvoice.iss).
; It installs the user's guide, setup\README.txt (README.md is the project's own), and the licence.

#define Clsid "{{47BE0B8C-0DF1-4FB0-BBE9-5EA76A365C4C}"
#define Root ".."

[Setup]
AppId=WorldVoiceSAPI5
AppName=WorldVoice for SAPI 5
AppVersion=1.0
AppVerName=WorldVoice for SAPI 5
AppPublisher=Loh Boon Keat
VersionInfoCompany=Loh Boon Keat
VersionInfoDescription=WorldVoice for SAPI 5 setup
VersionInfoVersion=1.0
UninstallDisplayName=WorldVoice for SAPI 5
UninstallDisplayIcon={app}\WorldVoiceConfig.exe
DefaultDirName={commonpf64}\WorldVoice
DisableDirPage=yes
DisableProgramGroupPage=yes
DefaultGroupName=WorldVoice
DisableWelcomePage=no
DisableReadyPage=yes
UsePreviousAppDir=no
DirExistsWarning=no
CloseApplications=no
RestartApplications=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
OutputDir=..\Output
OutputBaseFilename=WorldVoice for SAPI 5 Setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel2=This will install WorldVoice on your computer: one SAPI 5 voice that reads each language with the voice you choose for it.%n%nIt works in all SAPI 5 programs, both 64-bit and 32-bit, and speaks through the SAPI 5 voices already installed. Use the WorldVoice configuration program to choose a voice for each language.

[Files]
Source: "{#Root}\build\x64\WorldVoiceSapi.dll"; DestDir: "{app}\x64"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#Root}\build\x86\WorldVoiceSapi.dll"; DestDir: "{app}\x86"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#Root}\build\WorldVoiceConfig.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

; The finish page offers the README, unticked (a [Files] isreadme box is always ticked); the wording is Inno Setup's own
; for that box (ShowReadmeCheck).
[Run]
Filename: "{app}\README.txt"; Description: "Yes, I would like to view the README file"; Flags: postinstall shellexec skipifsilent unchecked

; The shortcut choices go straight on the tasks page (no Start Menu folder page to browse): Inno Setup's
; standard "Create a desktop shortcut" box, and a Start Menu box worded to match it (Inno has none).
[CustomMessages]
CreateStartMenuIcon=Create a &Start Menu shortcut

[Tasks]
Name: "startmenuicon"; Description: "{cm:CreateStartMenuIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Icons]
Name: "{group}\WorldVoice"; Filename: "{app}\WorldVoiceConfig.exe"; Tasks: startmenuicon
Name: "{group}\{cm:UninstallProgram,WorldVoice}"; Filename: "{uninstallexe}"; Tasks: startmenuicon
Name: "{commondesktop}\WorldVoice"; Filename: "{app}\WorldVoiceConfig.exe"; Tasks: desktopicon

; A shortcut left over from an earlier install goes when its box is unticked this time.
[InstallDelete]
Type: filesandordirs; Name: "{group}"; Tasks: not startmenuicon
Type: files; Name: "{commondesktop}\WorldVoice.lnk"; Tasks: not desktopicon

[Registry]
#sub Engine
Root: {#R}; Subkey: "SOFTWARE\Classes\CLSID\{#Clsid}"; ValueType: string; ValueName: ""; ValueData: "WorldVoice for SAPI 5"; Flags: uninsdeletekey
Root: {#R}; Subkey: "SOFTWARE\Classes\CLSID\{#Clsid}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\{#Arch}\WorldVoiceSapi.dll"
Root: {#R}; Subkey: "SOFTWARE\Classes\CLSID\{#Clsid}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Both"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice"; ValueType: string; ValueName: ""; ValueData: "WorldVoice"; Flags: uninsdeletekey
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice"; ValueType: string; ValueName: "409"; ValueData: "WorldVoice"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice"; ValueType: string; ValueName: "CLSID"; ValueData: "{#Clsid}"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice\Attributes"; ValueType: string; ValueName: "Name"; ValueData: "WorldVoice"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice\Attributes"; ValueType: string; ValueName: "Language"; ValueData: "409"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice\Attributes"; ValueType: string; ValueName: "Gender"; ValueData: "Female"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice\Attributes"; ValueType: string; ValueName: "Age"; ValueData: "Adult"
Root: {#R}; Subkey: "SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice\Attributes"; ValueType: string; ValueName: "Vendor"; ValueData: "WorldVoice"
#endsub
#define R "HKLM64"
#define Arch "x64"
#emit Engine
#define R "HKLM32"
#define Arch "x86"
#emit Engine
