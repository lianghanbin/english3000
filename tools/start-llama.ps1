# English 3000 本地 AI 启动器：GPU(Vulkan) 优先，失败自动回退 CPU
$ErrorActionPreference = "SilentlyContinue"

$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$model = Join-Path $dir "llama\qwen2.5-1.5b-instruct-q4_k_m.gguf"
$commonArgs = @("-m", $model, "--host", "127.0.0.1", "--port", "8080", "-c", "2048")

# 已经在运行就直接退出
if (Get-NetTCPConnection -LocalPort 8080 -State Listen) {
    exit
}

$vulkan = Join-Path $dir "llama\vulkan\llama-server.exe"
$cpu = Join-Path $dir "llama\cpu\llama-server.exe"

$proc = $null
if (Test-Path $vulkan) {
    $proc = Start-Process -FilePath $vulkan -ArgumentList $commonArgs -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 5
    if (Get-NetTCPConnection -LocalPort 8080 -State Listen) {
        exit
    }
    Stop-Process -Id $proc.Id -Force
}

if (Test-Path $cpu) {
    Start-Process -FilePath $cpu -ArgumentList $commonArgs -WindowStyle Hidden
}
