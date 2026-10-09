# Global refresh of compile_commands.json for clangd.
# Prefers <workspace>/tools/update-compile-commands.ps1 when present.
# Otherwise: pio run -t compiledb (default_envs), or all [env:*] with -AllEnvs.
#
# powershell -File C:\GitHub\Embedded\tools\update-compile-commands.ps1 -Root <workspace>
param(
    [string]$Root = '',
    [string]$Env = '',
    [switch]$AllEnvs,
    [switch]$NoRestart
)

$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = (Get-Location).Path }
$Root = (Resolve-Path -LiteralPath $Root).Path
Set-Location -LiteralPath $Root

$local = Join-Path $Root 'tools\update-compile-commands.ps1'
if ((Test-Path -LiteralPath $local) -and ((Resolve-Path -LiteralPath $local).Path -ne $PSCommandPath)) {
    Write-Host "project script: $local"
    if ($NoRestart) { & $local -NoRestart } else { & $local }
    exit $LASTEXITCODE
}

function Find-Pio {
    $cmd = Get-Command pio -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $cmd = Get-Command platformio -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'),
        'C:\.platformio\penv\Scripts\pio.exe',
        (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'),
        'C:\.platformio\penv\Scripts\platformio.exe'
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    throw "PlatformIO CLI not found (pio). Expected %USERPROFILE%\.platformio\penv\Scripts\pio.exe"
}

function Get-PioEnvs([string]$iniPath) {
    if (-not (Test-Path -LiteralPath $iniPath)) { return @() }
    $names = [System.Collections.Generic.List[string]]::new()
    foreach ($line in Get-Content -LiteralPath $iniPath) {
        if ($line -match '^\[env:([^\]]+)\]') { $names.Add($Matches[1]) }
    }
    return $names
}

$ini = Join-Path $Root 'platformio.ini'
if (-not (Test-Path -LiteralPath $ini)) {
    throw "No platformio.ini in $Root - open a PIO project as the workspace folder."
}

$pio = Find-Pio
$pioArgs = @('run', '-t', 'compiledb')
if ($Env) {
    $pioArgs += @('-e', $Env)
    Write-Host "env: $Env"
} elseif ($AllEnvs) {
    $envs = @(Get-PioEnvs $ini)
    if ($envs.Count -eq 0) { throw "No [env:...] sections in platformio.ini" }
    foreach ($e in $envs) { $pioArgs += @('-e', $e) }
    Write-Host ("env: " + ($envs -join ', '))
} else {
    Write-Host "env: (default_envs / all)"
}

Write-Host "pio: $pio"
Write-Host "cwd: $Root"
& $pio @pioArgs
if ($LASTEXITCODE -ne 0) {
    throw "pio run -t compiledb failed (exit $LASTEXITCODE)"
}

$db = Join-Path $Root 'compile_commands.json'
if (-not (Test-Path -LiteralPath $db)) {
    throw "compile_commands.json was not created in $Root"
}

if ($NoRestart) {
    Write-Host "Done: $db"
    Write-Host "Restart clangd: Command Palette -> Clangd: Restart language server"
    exit 0
}

$clangd = Get-CimInstance Win32_Process -Filter "Name = 'clangd.exe'" -ErrorAction SilentlyContinue
if ($clangd) {
    foreach ($p in @($clangd)) {
        Write-Host "Restart clangd PID $($p.ProcessId)"
        Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
    }
    Write-Host "Done: $db  (clangd restarted)"
} else {
    Write-Host "Done: $db"
    Write-Host "clangd not running - open a C/C++ file or: Clangd: Restart language server"
}
