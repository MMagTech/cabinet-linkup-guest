; 2P Start's Setup for Windows, made by Inno Setup 6 from the folder cmake --install makes.
; It installs for the person alone, with no administrator: into their own programs folder, with a
; Start menu entry that carries the app's name for notifications (tray_win.cpp), a desktop one if
; they want it, and an uninstaller in Settings, Apps. A newer Setup installs over an older one,
; closing 2P Start first. Not signed, as the Mac app is not.
;
;   ISCC /DAppVersion=<version> /DSource=<the folder> /DOutput=<folder for the Setup> ci\windows-setup.iss
;
; The uninstaller takes away what 2P Start itself put on the PC as well: its Startup entry and the
; name its notifications show (both app/), and asks before it takes the friend's pairing and settings.

#ifndef AppVersion
  #error AppVersion is wanted: /DAppVersion=<version>
#endif
#ifndef Source
  #error Source is wanted: /DSource=<the folder cmake --install made>
#endif
#ifndef Output
  #define Output "."
#endif

; The same as the app's own (tray_win.cpp, startup.cpp, guest/paths.cpp).
#define AppId "io.github.mmagtech.2pstart"
#define RunName "CabinetOS 2P Start"
#define DataFolder "CabinetOS 2P Start"

[Setup]
AppId={#AppId}
AppName=2P Start
AppVersion={#AppVersion}
AppVerName=2P Start {#AppVersion}
AppPublisher=CabinetOS
AppPublisherURL=https://github.com/MMagTech/cabinet-linkup-guest
AppSupportURL=https://github.com/MMagTech/cabinet-linkup-guest
AppUpdatesURL=https://github.com/MMagTech/cabinet-linkup-guest/releases
VersionInfoVersion={#AppVersion}
VersionInfoProductName=CabinetOS 2P Start
VersionInfoDescription=2P Start Setup
PrivilegesRequired=lowest
DefaultDirName={autopf}\2P Start
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SetupIconFile=..\icon\2pstart.ico
UninstallDisplayIcon={app}\2P Start.exe
UninstallDisplayName=2P Start
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
Compression=lzma2/max
SolidCompression=yes
OutputDir={#Output}
OutputBaseFilename=2P Start Setup

[Tasks]
Name: desktopicon; Description: "A shortcut on the desktop"; Flags: unchecked

[Files]
Source: "{#Source}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; What an older version had and this one does not, so nothing stale is left among GStreamer's parts.
Type: filesandordirs; Name: "{app}\lib"

[Icons]
Name: "{autoprograms}\2P Start"; Filename: "{app}\2P Start.exe"; AppUserModelID: "{#AppId}"
Name: "{autodesktop}\2P Start"; Filename: "{app}\2P Start.exe"; AppUserModelID: "{#AppId}"; Tasks: desktopicon

[Run]
Filename: "{app}\2P Start.exe"; Description: "Start 2P Start"; Flags: nowait postinstall skipifsilent

[Code]
// Startup's entry only when it is this install's: a copy unpacked elsewhere keeps its own.
procedure TakeStartup();
var
  Value: String;
begin
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', '{#RunName}', Value) and
     (Pos(Lowercase(ExpandConstant('{app}')), Lowercase(Value)) > 0) then
    RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', '{#RunName}');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Data: String;
begin
  if CurUninstallStep = usUninstall then begin
    TakeStartup();
    RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Classes\AppUserModelId\{#AppId}');
  end;
  if CurUninstallStep = usPostUninstall then begin
    Data := ExpandConstant('{localappdata}\{#DataFolder}');
    if DirExists(Data) and not UninstallSilent and
       (MsgBox('Remove your pairing and settings too?' + #13#10#13#10 +
               'With No, 2P Start keeps them, and knows your friend if you install it again.',
               mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES) then
      DelTree(Data, True, True, True);
  end;
end;
