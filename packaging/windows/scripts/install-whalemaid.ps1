[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\WhaleMaid'),
    [string]$CodexHome = (Join-Path $env:USERPROFILE '.codex'),
    [string]$StartupFolder = (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Startup'),
    [switch]$NoLaunch
)

$ErrorActionPreference = 'Stop'
$logDirectory = Join-Path $env:LOCALAPPDATA 'WhaleMaid\logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$logPath = Join-Path $logDirectory 'installer.log'
Start-Transcript -LiteralPath $logPath -Append | Out-Null
trap {
    Write-Host ''
    Write-Host ("安装失败：{0}" -f $_.Exception.Message) -ForegroundColor Red
    Write-Host "安装日志：$logPath" -ForegroundColor Yellow
    try { Stop-Transcript | Out-Null } catch {}
    exit 1
}
$packageRoot = Split-Path -Parent $PSScriptRoot
$sourceApp = Join-Path $packageRoot 'app'
$installedApp = Join-Path $InstallRoot 'app'
$installedPet = Join-Path $installedApp 'WhaleMaidPet.exe'
$installedHook = Join-Path $installedApp 'WhaleMaidHook.exe'
$hooksPath = Join-Path $CodexHome 'hooks.json'
$shortcutPath = Join-Path $StartupFolder 'WhaleMaid.lnk'
$manualExitFlag = Join-Path $env:LOCALAPPDATA 'WhaleMaid\manual-exit.flag'
$settingsRegistryPath = 'HKCU:\Software\WhaleMaid'
$runRegistryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'WhaleMaid'

function Assert-Package {
    $required = @(
        (Join-Path $sourceApp 'WhaleMaidPet.exe'),
        (Join-Path $sourceApp 'WhaleMaidHook.exe'),
        (Join-Path $sourceApp 'Resources\WhaleMaid\WhaleMaid-runtime-v1.model3.json')
    )
    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "安装包不完整，缺少文件：$path。请重新完整解压安装包。"
        }
    }
}

function Invoke-WhaleMaidHook([string]$path, [string[]]$arguments) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return }
    $process = Start-Process -FilePath $path -ArgumentList $arguments -PassThru
    [void]$process.WaitForExit(5000)
}

function Get-InstalledPetProcesses {
    $expectedPath = [IO.Path]::GetFullPath($installedPet)
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

function Read-HooksConfig {
    if (-not (Test-Path -LiteralPath $hooksPath)) {
        return [pscustomobject]@{ hooks = [pscustomobject]@{} }
    }

    $raw = Get-Content -LiteralPath $hooksPath -Raw -Encoding UTF8
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return [pscustomobject]@{ hooks = [pscustomobject]@{} }
    }

    try {
        $config = $raw | ConvertFrom-Json
    }
    catch {
        throw "无法读取现有 Codex Hooks 配置：$hooksPath。原文件没有被修改。"
    }

    if ($null -eq $config.hooks) {
        $config | Add-Member -NotePropertyName hooks -NotePropertyValue ([pscustomobject]@{})
    }
    return $config
}

function Remove-WhaleMaidHandlers([object]$config) {
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
}

function Add-HookGroup([object]$config, [string]$eventName, [string]$matcher = '') {
    $handler = [pscustomobject]@{
        type = 'command'
        command = 'true'
        commandWindows = ('"{0}"' -f $installedHook)
        timeout = 6
    }
    if ([string]::IsNullOrEmpty($matcher)) {
        $group = [pscustomobject]@{ hooks = @($handler) }
    }
    else {
        $group = [pscustomobject]@{ matcher = $matcher; hooks = @($handler) }
    }

    $property = $config.hooks.PSObject.Properties[$eventName]
    if ($null -eq $property) {
        $config.hooks | Add-Member -NotePropertyName $eventName -NotePropertyValue @($group)
    }
    else {
        $property.Value = @($property.Value) + @($group)
    }
}

function Write-HooksConfig([object]$config) {
    New-Item -ItemType Directory -Force -Path $CodexHome | Out-Null
    if (Test-Path -LiteralPath $hooksPath) {
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
        Copy-Item -LiteralPath $hooksPath -Destination "$hooksPath.whalemaid-backup-$stamp" -Force
    }
    $json = $config | ConvertTo-Json -Depth 20
    [IO.File]::WriteAllText($hooksPath, $json, [Text.UTF8Encoding]::new($false))
}

Write-Host '正在检查 WhaleMaid 安装包……'
Assert-Package

# Ask a running copy to exit before replacing its files.
$sourceHook = Join-Path $sourceApp 'WhaleMaidHook.exe'
Invoke-WhaleMaidHook $sourceHook @('--status', 'shutdown', '--message', '正在更新 WhaleMaid')
Stop-InstalledPet

Write-Host '正在安装桌宠程序与 Live2D 资源……'
New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
if (Test-Path -LiteralPath $installedApp) {
    Remove-Item -LiteralPath $installedApp -Recurse -Force
}
Copy-Item -LiteralPath $sourceApp -Destination $installedApp -Recurse -Force
if (Test-Path -LiteralPath $manualExitFlag) {
    Remove-Item -LiteralPath $manualExitFlag -Force
}

Write-Host '正在接入 Codex 工作状态……'
$config = Read-HooksConfig
Remove-WhaleMaidHandlers $config
Add-HookGroup $config 'SessionStart' 'startup|resume|clear'
Add-HookGroup $config 'UserPromptSubmit'
Add-HookGroup $config 'PreToolUse' '*'
Add-HookGroup $config 'PermissionRequest' '*'
Add-HookGroup $config 'PostToolUse' '*'
Add-HookGroup $config 'SubagentStart' '*'
Add-HookGroup $config 'SubagentStop' '*'
Add-HookGroup $config 'Stop'
Add-HookGroup $config 'SessionEnd'
Write-HooksConfig $config

Write-Host '正在应用开机启动设置……'
if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}
New-Item -Path $settingsRegistryPath -Force | Out-Null
$savedStartup = Get-ItemProperty -Path $settingsRegistryPath -Name 'StartWithWindows' -ErrorAction SilentlyContinue
if ($null -eq $savedStartup) {
    $startupEnabled = 1
    New-ItemProperty -Path $settingsRegistryPath -Name 'StartWithWindows' -PropertyType DWord -Value 1 -Force | Out-Null
}
else {
    $startupEnabled = [int]$savedStartup.StartWithWindows
}
New-Item -Path $runRegistryPath -Force | Out-Null
if ($startupEnabled -ne 0) {
    $startupCommand = '"{0}" --manual-start' -f $installedPet
    New-ItemProperty -Path $runRegistryPath -Name $runValueName -PropertyType String -Value $startupCommand -Force | Out-Null
}
else {
    Remove-ItemProperty -Path $runRegistryPath -Name $runValueName -ErrorAction SilentlyContinue
}

if (-not $NoLaunch) {
    Start-Process -FilePath $installedPet -WorkingDirectory $installedApp
    Start-Sleep -Milliseconds 800
    Invoke-WhaleMaidHook $installedHook @('--status', 'completed', '--message', 'WhaleMaid 安装完成')
}

Write-Host ''
Write-Host 'WhaleMaid 安装完成。' -ForegroundColor Green
Write-Host '请直接回到 Codex 正常使用；无需重启，也无需建立专用对话。'
Write-Host '如果 Codex 出现 Hooks 信任提示，可以允许；本地状态监听不依赖该提示。'
Write-Host "安装日志：$logPath"
Stop-Transcript | Out-Null
exit 0
