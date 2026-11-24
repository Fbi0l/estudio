; Instalador do Estúdio Desktop (Inno Setup). Licença GPLv3.
#ifndef Versao
  #define Versao "0.0.0"
#endif
#ifndef Fonte
  #define Fonte "..\..\build\EstudioDesktop_artefacts\Release"
#endif
[Setup]
AppId={{FA86CBEC-9E78-406C-8C70-E1777177F877}
AppName=Estúdio
AppVersion={#Versao}
CloseApplications=yes
RestartApplications=no
AppPublisher=Fbiol
AppPublisherURL=https://github.com/Fbi0l/estudio
DefaultDirName={autopf}\Estudio
DefaultGroupName=Estúdio
OutputBaseFilename=Estudio-Desktop-Instalador
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
[Files]
Source: "{#Fonte}\Estudio.exe"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\Estúdio"; Filename: "{app}\Estudio.exe"
Name: "{autodesktop}\Estúdio"; Filename: "{app}\Estudio.exe"; Tasks: atalho
[Run]
Filename: "{app}\Estudio.exe"; Description: "Abrir o Estúdio"; Flags: nowait postinstall skipifsilent
Filename: "{app}\Estudio.exe"; Flags: nowait; Check: WizardSilent
