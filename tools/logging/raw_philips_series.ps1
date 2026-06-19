[CmdletBinding()]
param(
    [string]$PortName = 'COM6',
    [int]$BaudRate = 115200,
    [int]$Count = 10,
    [string]$Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips',
    [string]$LogPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $LogPath = Join-Path (Get-Location) 'docs\raw_pcm_philips_series_2026-06-19.txt'
}

function Add-Log {
    param([Parameter(Mandatory = $true)][string]$Line)
    $stream = [System.IO.File]::Open(
        $LogPath,
        [System.IO.FileMode]::Append,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::ReadWrite
    )
    try {
        $writer = New-Object System.IO.StreamWriter($stream, [System.Text.UTF8Encoding]::new($false))
        try {
            $writer.AutoFlush = $true
            $writer.WriteLine($Line)
        } finally {
            $writer.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Get-SerialDrainSlackMs {
    param(
        [Parameter(Mandatory = $true)][int]$CapturedSamples,
        [Parameter(Mandatory = $true)][int]$BaudRateValue
    )

    $estimatedRowBytes = 40.0
    $payloadBytes = [double]$CapturedSamples * $estimatedRowBytes
    $wireMs = [int][math]::Ceiling(($payloadBytes * 10.0 * 1000.0) / [double]$BaudRateValue)
    return [int][math]::Max(1500, [math]::Min(12000, $wireMs + 500))
}

$port = New-Object System.IO.Ports.SerialPort
$port.PortName = $PortName
$port.BaudRate = $BaudRate
$port.Parity = [System.IO.Ports.Parity]::None
$port.DataBits = 8
$port.StopBits = [System.IO.Ports.StopBits]::One
$port.Handshake = [System.IO.Ports.Handshake]::None
$port.ReadTimeout = 100
$port.WriteTimeout = 100
$port.DtrEnable = $false
$port.RtsEnable = $false
$port.NewLine = "`n"

function Read-UntilOkRaw {
    param([int]$DeadlineMs)

    $deadline = [DateTime]::UtcNow.AddMilliseconds($DeadlineMs)
    $buffer = ''
    $capturedSamples = 0
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            $chunk = $port.ReadExisting()
        } catch {
            $chunk = ''
        }

        if ($chunk) {
            $buffer += $chunk
            while (($idx = $buffer.IndexOf("`n")) -ge 0) {
                $line = $buffer.Substring(0, $idx).TrimEnd("`r")
                $buffer = $buffer.Substring($idx + 1)
                if ($line.Length -gt 0) {
                    Add-Log $line
                    if ($line -match '^RAW_SUMMARY\b') {
                        if ($line -match 'captured=(\d+)') {
                            $capturedSamples = [int]$Matches[1]
                        }
                    }
                    if ($line -eq 'OK RAW') {
                        return $capturedSamples
                    }
                }
            }
        }

        Start-Sleep -Milliseconds 50
    }

    return -1
}

try {
    Remove-Item -LiteralPath $LogPath -ErrorAction SilentlyContinue
    Add-Log ('# RAW Philips series ' + (Get-Date).ToString('o'))
    $port.Open()
    Start-Sleep -Seconds 2

    for ($i = 1; $i -le $Count; $i++) {
        Add-Log ('=== CAPTURE {0} START {1} ===' -f $i, (Get-Date).ToString('o'))
        Add-Log ('CMD ' + $Command)
        $port.DiscardInBuffer()
        $port.WriteLine($Command)

        $capturedSamples = Read-UntilOkRaw -DeadlineMs 120000
        if ($capturedSamples -lt 0) {
            Add-Log ('CAPTURE ' + $i + ' TIMEOUT')
            break
        }

        $slackMs = Get-SerialDrainSlackMs -CapturedSamples $capturedSamples -BaudRateValue $BaudRate
        Add-Log ('CAPTURE ' + $i + ' captured=' + $capturedSamples + ' drain_slack_ms=' + $slackMs)
        Start-Sleep -Milliseconds $slackMs
        Start-Sleep -Milliseconds 250
    }
} finally {
    if ($port.IsOpen) {
        $port.Close()
    }
}
