# Локальная база для clangd: заголовки mega2560 не входят в compile_commands проектов,
# и clangd подставляет чужую команду (STM32). Путь к avr-g++ находится на этой машине.
# Запуск из любого каталога: powershell -File gen-compile-commands.ps1
$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
# toolchain: %USERPROFILE%\.platformio или C:\.platformio (и PLATFORMIO_CORE_DIR)
$SearchRoots = @()
if ($env:PLATFORMIO_CORE_DIR) { $SearchRoots += $env:PLATFORMIO_CORE_DIR }
$SearchRoots += (Join-Path $env:USERPROFILE '.platformio')
$SearchRoots += 'C:\.platformio'

$Gxx = $null
$Toolchain = $null
foreach ($base in $SearchRoots) {
    $candidate = Join-Path $base 'packages\toolchain-atmelavr\bin\avr-g++.exe'
    if (Test-Path -LiteralPath $candidate) {
        $Gxx = (Resolve-Path -LiteralPath $candidate).Path
        $Toolchain = (Resolve-Path -LiteralPath (Join-Path $base 'packages\toolchain-atmelavr')).Path
        break
    }
}
if (-not $Gxx) {
    throw "avr-g++.exe не найден. Ожидался packages\toolchain-atmelavr под PLATFORMIO_CORE_DIR, $env:USERPROFILE\.platformio или C:\.platformio"
}

$GccDirs = @(Get-ChildItem -LiteralPath (Join-Path $Toolchain 'lib\gcc\avr') -Directory | Sort-Object Name -Descending)
if ($GccDirs.Count -lt 1) {
    throw "Нет lib\gcc\avr под $Toolchain"
}
$GccInc = $GccDirs[0].FullName
$SysIncludes = @(
    (Join-Path $GccInc 'include'),
    (Join-Path $GccInc 'include-fixed'),
    (Join-Path $Toolchain 'avr\include')
) | ForEach-Object { $_.Replace('\', '/') }

$Headers = Get-ChildItem -LiteralPath $Root -Filter '*.hpp' -File | Sort-Object Name
if ($Headers.Count -lt 1) { throw "В $Root нет .hpp" }

$Dir = $Root.Replace('\', '/')
$GxxArg = $Gxx.Replace('\', '/')
$entries = foreach ($header in $Headers) {
    $file = $header.FullName.Replace('\', '/')
    $args = @(
        $GxxArg,
        '-x', 'c++-header',
        '-std=gnu++17',
        '-mmcu=atmega2560',
        '-D__AVR_ATmega2560__',
        '-DF_CPU=16000000L',
        '-Icxx',
        '-I.',
        '-I../../Interfaces'
    )
    foreach ($inc in $SysIncludes) {
        $args += '-isystem'
        $args += $inc
    }
    $args += $file
    [ordered]@{
        directory = $Dir
        file      = $file
        arguments = $args
    }
}

$json = $entries | ConvertTo-Json -Depth 5
$out = Join-Path $Root 'compile_commands.json'
[System.IO.File]::WriteAllText($out, $json)
Write-Host "avr-g++: $GxxArg"
Write-Host "Wrote $($Headers.Count) entries: $out"
