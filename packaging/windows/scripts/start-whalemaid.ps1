[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\WhaleMaid')
)

$ErrorActionPreference = 'Stop'
$logDirectory = Join-Path $env:LOCALAPPDATA 'WhaleMaid\logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$logPath = Join-Path $logDirectory 'launcher.log'
$petPath = Join-Path $InstallRoot 'app\WhaleMaidPet.exe'
$manualExitFlag = Join-Path $env:LOCALAPPDATA 'WhaleMaid\manual-exit.flag'

function Write-ProgressLine([string]$message) {
    $line = '[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $message
    Write-Host $line
    Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
}

trap {
    $detail = $_.Exception.Message
    Write-Host ''
    Write-Host "启动失败：$detail" -ForegroundColor Red
    Write-Host "日志文件：$logPath" -ForegroundColor Yellow
    Add-Content -LiteralPath $logPath -Value ("ERROR: {0}" -f $detail) -Encoding UTF8
    exit 1
}

Write-ProgressLine '正在检查 WhaleMaidPet.exe……'
if (-not (Test-Path -LiteralPath $petPath -PathType Leaf)) {
    throw ("没有找到已安装的桌宠：{0}。请先运行安装WhaleMaid.cmd。" -f $petPath)
}

Write-ProgressLine '正在启动或唤醒 WhaleMaid……'
if (Test-Path -LiteralPath $manualExitFlag) {
    Remove-Item -LiteralPath $manualExitFlag -Force
}
Start-Process -FilePath $petPath -ArgumentList @('--manual-start') -WorkingDirectory (Split-Path -Parent $petPath)
Start-Sleep -Milliseconds 1200

$expectedPath = [IO.Path]::GetFullPath($petPath)
$running = Get-Process -Name 'WhaleMaidPet' -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and ([IO.Path]::GetFullPath($_.Path) -eq $expectedPath)
}
if (-not $running) {
    throw 'WhaleMaidPet.exe 已退出，桌宠没有成功保持运行。'
}

Write-ProgressLine '启动成功。桌宠已经在桌面上运行。'
Write-Host "日志文件：$logPath"
exit 0

