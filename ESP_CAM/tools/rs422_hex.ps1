# RS-422 hex send/recv (Waveshare Port C)
#   .\tools\rs422_hex.ps1 -Port COM13
#   .\tools\rs422_hex.ps1 -Port COM13 -Send "23 4F 32 0D"
#   .\tools\rs422_hex.ps1 -Port COM13 -ListenOnly
param(
    [string]$Port = "COM13",
    [int]$Baud = 9600,
    [string]$Send = "",
    [switch]$Ascii,
    [switch]$ListenOnly,
    [int]$AfterSendMs = 300
)

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, One
$sp.ReadTimeout = 20
$sp.WriteTimeout = 500
$sp.DtrEnable = $true
$sp.RtsEnable = $true
$sp.Open()
Write-Host "Opened $Port @ $Baud. Ctrl+C to quit."

function Format-Hex([byte[]]$bytes) {
    if (-not $bytes -or $bytes.Length -eq 0) { return "" }
    ($bytes | ForEach-Object { "{0:X2}" -f $_ }) -join " "
}

function Read-Available {
    $n = $sp.BytesToRead
    if ($n -le 0) { return $null }
    if ($n -gt 512) { $n = 512 }
    $buf = New-Object byte[] $n
    $got = $sp.Read($buf, 0, $n)
    if ($got -le 0) { return $null }
    if ($got -lt $n) { return $buf[0..($got - 1)] }
    return $buf
}

function Show-Rx([byte[]]$rx) {
    if (-not $rx) { return }
    $text = -join ($rx | ForEach-Object {
        if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { "." }
    })
    Write-Host ("RX  " + (Format-Hex $rx) + "  | " + $text)
}

function Send-Payload([string]$payload, [bool]$asAscii) {
    if ([string]::IsNullOrWhiteSpace($payload)) { return }
    # сбросить накопившийся мусор перед отправкой
    while ($sp.BytesToRead -gt 0) { [void](Read-Available) }

    [byte[]]$tx = @()
    if ($asAscii) {
        $tx = [System.Text.Encoding]::ASCII.GetBytes($payload)
        if ($tx[-1] -ne 0x0D) { $tx = $tx + [byte]0x0D }
    } else {
        $parts = $payload -split '[\s,]+' | Where-Object { $_ -ne "" }
        $tx = foreach ($p in $parts) { [Convert]::ToByte($p, 16) }
    }
    $sp.Write($tx, 0, $tx.Length)
    Write-Host ("TX  " + (Format-Hex $tx) + "  (" + $tx.Length + " B)")
}

# разовый send + короткий dump, потом сразу в listen-loop
if ($Send -and -not $ListenOnly) {
    Send-Payload $Send $Ascii.IsPresent
    $deadline = [Environment]::TickCount + $AfterSendMs
    while ([Environment]::TickCount -lt $deadline) {
        Show-Rx (Read-Available)
        Start-Sleep -Milliseconds 10
    }
    Write-Host "---- listening (Ctrl+C to stop) ----"
}

if (-not $Send) {
    Write-Host "Commands:"
    Write-Host "  23 4F 32 0D   hex send"
    Write-Host "  a:#O2         ASCII + CR"
    Write-Host "  #P99          ASCII + CR"
    Write-Host "  /clear        flush RX"
    Write-Host "  q             quit"
    Write-Host "---- listening ----"
}

try {
    while ($true) {
        Show-Rx (Read-Available)

        if (-not $ListenOnly -and [Console]::KeyAvailable) {
            # не блокировать приём: читаем строку только если уже нажали Enter-путь через Read-Host
            $line = Read-Host "TX"
            if ($line -eq "q" -or $line -eq "quit") { break }
            if ($line -eq "/clear") {
                while ($sp.BytesToRead -gt 0) { [void](Read-Available) }
                Write-Host "RX flushed"
                continue
            }
            if ($line.StartsWith("a:") -or $line.StartsWith("#")) {
                $payload = if ($line.StartsWith("a:")) { $line.Substring(2) } else { $line }
                Send-Payload $payload $true
            } else {
                Send-Payload $line $false
            }
        } else {
            Start-Sleep -Milliseconds 15
        }
    }
} finally {
    if ($sp.IsOpen) { $sp.Close() }
}
