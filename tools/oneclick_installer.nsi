; English 3000 一键安装包（Windows）
; 包含：应用本体 + llama.cpp 本地推理 + Qwen2.5 1.5B 小模型

!include "MUI2.nsh"

Name "English 3000 一键版"
OutFile "English3000-OneClick-Setup.exe"
InstallDir "$LOCALAPPDATA\English3000"
RequestExecutionLevel user
SetCompressor /SOLID zlib

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Install"
    SetOutPath "$INSTDIR"
    File /r "oneclick\*"

    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateShortcut "$DESKTOP\English3000.lnk" "$INSTDIR\english3000.exe"
    CreateDirectory "$SMPROGRAMS\English3000"
    CreateShortcut "$SMPROGRAMS\English3000\English3000.lnk" "$INSTDIR\english3000.exe"
    CreateShortcut "$SMPROGRAMS\English3000\卸载.lnk" "$INSTDIR\Uninstall.exe"

    ; 开机自启本地 AI 服务
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "English3000AI" "$\"$INSTDIR\llama\llama-server.exe$\" -m $\"$INSTDIR\llama\qwen2.5-1.5b-instruct-q4_k_m.gguf$\" --host 127.0.0.1 --port 8080 -c 4096"

    ; 写入应用默认 AI 设置（OpenAI 兼容 → 本地 llama-server）
    CreateDirectory "$APPDATA\liang\english3000"
    ExecWait '"$INSTDIR\sqlite3.exe" "$APPDATA\liang\english3000\english3000.db" "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL); INSERT OR REPLACE INTO settings(key,value) VALUES(''ai_provider'',''openai''); INSERT OR REPLACE INTO settings(key,value) VALUES(''ai_base_url'',''http://127.0.0.1:8080''); INSERT OR REPLACE INTO settings(key,value) VALUES(''ai_model'',''qwen2.5:1.5b'');"'

    ; 立即启动本地 AI 服务
    Exec '"$INSTDIR\llama\llama-server.exe" -m "$INSTDIR\llama\qwen2.5-1.5b-instruct-q4_k_m.gguf" --host 127.0.0.1 --port 8080 -c 4096'

    ; 启动应用
    Exec '"$INSTDIR\english3000.exe"'
SectionEnd

Section "Uninstall"
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "English3000AI"
    Delete "$DESKTOP\English3000.lnk"
    Delete "$SMPROGRAMS\English3000\English3000.lnk"
    Delete "$SMPROGRAMS\English3000\卸载.lnk"
    RMDir "$SMPROGRAMS\English3000"
    RMDir /r "$INSTDIR"
SectionEnd
