# English 3000 本地 AI 启动器：CUDA -> Vulkan -> CPU 自动回退
$ErrorActionPreference = "SilentlyContinue"

$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$model = Join-Path $dir "llama\qwen2.5-1.5b-instruct-q4_k_m.gguf"
$commonArgs = @("-m", $model, "--host", "127.0.0.1", "--port", "8080", "-c", "8192")

# 已经在运行就直接退出
if (Get-NetTCPConnection -LocalPort 8080 -State Listen) {
    exit
}

$cuda = Join-Path $dir "llama\cuda\llama-server.exe"
$vulkan = Join-Path $dir "llama\vulkan\llama-server.exe"
$cpu = Join-Path $dir "llama\cpu\llama-server.exe"

function Try-Start($exePath) {
    $p = Start-Process -FilePath $exePath -ArgumentList $commonArgs -WindowStyle Hidden -PassThru
    for ($i = 0; $i -lt 10; $i++) {
        Start-Sleep -Milliseconds 500
        if ($p.HasExited) {
            return $false
        }
        if (Get-NetTCPConnection -LocalPort 8080 -State Listen) {
            return $true
        }
    }
    Stop-Process -Id $p.Id -Force
    return $false
}

if (Test-Path $cuda) {
    if (Try-Start $cuda) { exit }
}
if (Test-Path $vulkan) {
    if (Try-Start $vulkan) { exit }
}
if (Test-Path $cpu) {
    Start-Process -FilePath $cpu -ArgumentList $commonArgs -WindowStyle Hidden
}
