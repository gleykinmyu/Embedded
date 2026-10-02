# Обновляет compile_commands.json для clangd: pio run -t compiledb -e tester
# Из корня проекта:  powershell -File tools/update-compile-commands.ps1
# Без перезапуска clangd:  ... -NoRestart
param(
    [switch]$NoRestart
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $Root

function Find-Pio {
    $cmd = Get-Command pio -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $cmd = Get-Command platformio -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        'C:\.platformio\penv\Scripts\pio.exe',
        (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'),
        (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'),
        'C:\.platformio\penv\Scripts\platformio.exe'
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }

    throw @"
PlatformIO CLI не найден (pio / platformio).
Установите PlatformIO IDE или добавьте pio в PATH.
Ожидаемые пути: C:\.platformio\penv\Scripts\pio.exe, %USERPROFILE%\.platformio\penv\Scripts\pio.exe
"@
}

$pio = Find-Pio
Write-Host "pio: $pio"
Write-Host "cwd: $Root"
& $pio run -t compiledb -e tester
if ($LASTEXITCODE -ne 0) {
    throw "pio run -t compiledb failed (exit $LASTEXITCODE)"
}

$db = Join-Path $Root 'compile_commands.json'
if (-not (Test-Path -LiteralPath $db)) {
    throw "compile_commands.json не появился в $Root"
}

if ($NoRestart) {
    Write-Host "Готово: $db"
    Write-Host "Перезапустите clangd: Command Palette -> Clangd: Restart language server"
    exit 0
}

$clangd = Get-CimInstance Win32_Process -Filter "Name = 'clangd.exe'" -ErrorAction SilentlyContinue
if ($clangd) {
    foreach ($p in @($clangd)) {
        Write-Host "Restart clangd PID $($p.ProcessId)"
        Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
    }
    Write-Host "Готово: $db  (clangd перезапущен)"
} else {
    Write-Host "Готово: $db"
    Write-Host "clangd не запущен - откройте C/C++ файл или: Clangd: Restart language server"
}
