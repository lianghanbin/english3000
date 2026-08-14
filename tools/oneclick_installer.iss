#define AppName "English 3000"
#define AppVersion "1.1.0"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=English 3000
DefaultDirName={localappdata}\English3000
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..
OutputBaseFilename=English3000-OneClick-Setup
Compression=none
SolidCompression=no
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\english3000.exe
ShowLanguageDialog=no

[Dirs]
Name: "{userappdata}\liang\english3000"

[Files]
Source: "..\oneclick\English3000\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "English3000AI"; ValueData: "powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File ""{app}\start-llama.ps1"""; Flags: uninsdeletevalue

[Icons]
Name: "{autodesktop}\{#AppName}.lnk"; Filename: "{app}\english3000.exe"
Name: "{group}\{#AppName}.lnk"; Filename: "{app}\english3000.exe"

[Run]
Filename: "{app}\sqlite3.exe"; Parameters: """{userappdata}\liang\english3000\english3000.db"" ""CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL)"""; Flags: runhidden
Filename: "{app}\sqlite3.exe"; Parameters: """{userappdata}\liang\english3000\english3000.db"" ""INSERT OR REPLACE INTO settings(key,value) VALUES('ai_provider','openai')"""; Flags: runhidden
Filename: "{app}\sqlite3.exe"; Parameters: """{userappdata}\liang\english3000\english3000.db"" ""INSERT OR REPLACE INTO settings(key,value) VALUES('ai_base_url','http://127.0.0.1:8080')"""; Flags: runhidden
Filename: "{app}\sqlite3.exe"; Parameters: """{userappdata}\liang\english3000\english3000.db"" ""INSERT OR REPLACE INTO settings(key,value) VALUES('ai_model','qwen2.5:1.5b')"""; Flags: runhidden
Filename: "powershell.exe"; Parameters: "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File ""{app}\start-llama.ps1"""; Flags: runhidden nowait
Filename: "{app}\english3000.exe"; Description: "启动 English 3000"; Flags: nowait
