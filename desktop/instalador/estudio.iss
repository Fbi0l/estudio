; Instalador do Stumix Desktop (Inno Setup). Licença GPLv3.
#ifndef Versao
  #define Versao "0.0.0"
#endif
#ifndef Fonte
  #define Fonte "..\..\build\EstudioDesktop_artefacts\Release"
#endif
[Setup]
AppId={{FA86CBEC-9E78-406C-8C70-E1777177F877}
AppName=Stumix
AppVersion={#Versao}
CloseApplications=yes
RestartApplications=no
AppPublisher=Fbiol
AppPublisherURL=https://github.com/Fbi0l/estudio
DefaultDirName={autopf}\Stumix
DefaultGroupName=Stumix
UsePreviousGroup=no
UninstallDisplayIcon={app}\Stumix.exe
OutputBaseFilename=Stumix-Desktop-Instalador
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=..\..\LICENSE
WizardStyle=modern
[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
[Tasks]
Name: "atalho"; Description: "Criar atalho na área de trabalho"; Flags: unchecked
[InstallDelete]
; restos do nome antigo (Estúdio) em quem atualiza
Type: files; Name: "{app}\Estudio.exe"
Type: filesandordirs; Name: "{autoprograms}\Estúdio"
Type: files; Name: "{autodesktop}\Estúdio.lnk"
[Files]
Source: "{#Fonte}\Stumix.exe"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\Stumix"; Filename: "{app}\Stumix.exe"
Name: "{autodesktop}\Stumix"; Filename: "{app}\Stumix.exe"; Tasks: atalho
[Run]
Filename: "{app}\Stumix.exe"; Description: "Abrir o Stumix"; Flags: nowait postinstall skipifsilent
Filename: "{app}\Stumix.exe"; Flags: nowait; Check: WizardSilent
