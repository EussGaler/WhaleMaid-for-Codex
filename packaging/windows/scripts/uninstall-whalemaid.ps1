[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\WhaleMaid'),
    [string]$CodexHome = (Join-Path $env:USERPROFILE '.codex'),
    [string]$StartupFolder = (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Startup')
)

$ErrorActionPreference = 'Stop'
$logDirectory = Join-Path $env:LOCALAPPDATA 'WhaleMaid\logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$logPath = Join-Path $logDirectory 'uninstaller.log'
Start-Transcript -LiteralPath $logPath -Append | Out-Null
trap {
    Write-Host ''
    Write-Host ("卸载失败：{0}" -f $_.Exception.Message) -ForegroundColor Red
    Write-Host "卸载日志：$logPath" -ForegroundColor Yellow
    try { Stop-Transcript | Out-Null } catch {}
    exit 1
}
$hooksPath = Join-Path $CodexHome 'hooks.json'
$shortcutPath = Join-Path $StartupFolder 'WhaleMaid.lnk'
$installedHook = Join-Path $InstallRoot 'app\WhaleMaidHook.exe'
$manualExitFlag = Join-Path $env:LOCALAPPDATA 'WhaleMaid\manual-exit.flag'
$settingsRegistryPath = 'HKCU:\Software\WhaleMaid'
$runRegistryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'WhaleMaid'

function Invoke-WhaleMaidHook([string]$path, [string[]]$arguments) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return }
    $process = Start-Process -FilePath $path -ArgumentList $arguments -PassThru
    [void]$process.WaitForExit(5000)
}

function Get-InstalledPetProcesses {
    $expectedPath = [IO.Path]::GetFullPath((Join-Path $InstallRoot 'app\WhaleMaidPet.exe'))
    return @(Get-Process -Name 'WhaleMaidPet' -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and ([IO.Path]::GetFullPath($_.Path) -eq $expectedPath) }
        catch { $false }
    })
}

function Stop-InstalledPet {
    $deadline = (Get-Date).AddSeconds(5)
    while ((Get-Date) -lt $deadline) {
        if ((Get-InstalledPetProcesses).Count -eq 0) { return }
        Start-Sleep -Milliseconds 200
    }
    foreach ($process in @(Get-InstalledPetProcesses)) {
        Stop-Process -Id $process.Id -Force -ErrorAction Stop
    }
    Start-Sleep -Milliseconds 500
}

function Remove-WhaleMaidFromHooks {
    if (-not (Test-Path -LiteralPath $hooksPath)) { return }
    $raw = Get-Content -LiteralPath $hooksPath -Raw -Encoding UTF8
    if ([string]::IsNullOrWhiteSpace($raw)) { return }
    try { $config = $raw | ConvertFrom-Json }
    catch { throw "无法读取 Codex Hooks 配置：$hooksPath。卸载已停止，原文件没有被修改。" }
    if ($null -eq $config.hooks) { return }

    foreach ($eventProperty in @($config.hooks.PSObject.Properties)) {
        $keptGroups = @()
        foreach ($group in @($eventProperty.Value)) {
            if ($null -eq $group) { continue }
            $keptHandlers = @()
            foreach ($handler in @($group.hooks)) {
                $commandText = "{0} {1}" -f $handler.command, $handler.commandWindows
                if ($commandText -notmatch '(?i)WhaleMaidHook\.exe') {
                    $keptHandlers += $handler
                }
            }
            if ($keptHandlers.Count -gt 0) {
                $group.hooks = @($keptHandlers)
                $keptGroups += $group
            }
        }
        $eventProperty.Value = @($keptGroups)
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    Copy-Item -LiteralPath $hooksPath -Destination "$hooksPath.whalemaid-backup-$stamp" -Force
    $json = $config | ConvertTo-Json -Depth 20
    [IO.File]::WriteAllText($hooksPath, $json, [Text.UTF8Encoding]::new($false))
}

Write-Host '正在断开 WhaleMaid 与 Codex 的状态连接……'
Remove-WhaleMaidFromHooks

if (Test-Path -LiteralPath $installedHook) {
    Invoke-WhaleMaidHook $installedHook @('--status', 'shutdown', '--message', '正在卸载 WhaleMaid')
    Stop-InstalledPet
}

if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}
Remove-ItemProperty -Path $runRegistryPath -Name $runValueName -ErrorAction SilentlyContinue
Remove-Item -Path $settingsRegistryPath -Recurse -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $manualExitFlag) {
    Remove-Item -LiteralPath $manualExitFlag -Force
}

$resolvedTarget = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if ((Split-Path -Leaf $resolvedTarget) -ne 'WhaleMaid' -or $resolvedTarget.Length -lt 12) {
    throw "为保护文件，拒绝删除异常安装路径：$resolvedTarget"
}
if (Test-Path -LiteralPath $resolvedTarget) {
    Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
}

Write-Host ''
Write-Host 'WhaleMaid 已卸载。其他 Codex Hooks 配置均已保留。' -ForegroundColor Green
Write-Host '卸载前的 hooks.json 备份仍保存在 .codex 文件夹中，可用于恢复。'
Write-Host "卸载日志：$logPath"
Stop-Transcript | Out-Null
exit 0
