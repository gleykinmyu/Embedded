# Downloads official clangd from github.com/clangd/clangd into tools/clangd-portable/
$ErrorActionPreference = 'Stop'
$Version = '22.1.0'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Dest = Join-Path $Root 'tools\clangd-portable'
$ZipName = "clangd-windows-${Version}.zip"
$Url = "https://github.com/clangd/clangd/releases/download/${Version}/${ZipName}"
$ExtractDir = Join-Path $Dest "clangd_${Version}"
$Bin = Join-Path $ExtractDir 'bin\clangd.exe'

if ((Test-Path $Bin) -and (& $Bin --version 2>$null)) {
    Write-Host "Already installed: $Bin"
    & $Bin --version
    exit 0
}

Write-Host "Downloading $Url ..."
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
$tmpZip = Join-Path ([System.IO.Path]::GetTempPath()) $ZipName
try {
    Invoke-WebRequest -Uri $Url -OutFile $tmpZip -UseBasicParsing
    if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }
    Expand-Archive -Path $tmpZip -DestinationPath $Dest -Force
} finally {
    if (Test-Path $tmpZip) { Remove-Item -Force $tmpZip }
}

if (-not (Test-Path $Bin)) {
    throw "Expected binary missing after extract: $Bin"
}

Write-Host "Done: $Bin"
& $Bin --version
