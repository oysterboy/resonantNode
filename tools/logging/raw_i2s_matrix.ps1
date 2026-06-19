[CmdletBinding()]
param(
    [string]$PortName = 'COM6',
    [int]$BaudRate = 115200
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$seriesScript = Join-Path $PSScriptRoot 'raw_philips_series.ps1'
$logDir = Join-Path $root 'docs'
$warmupLog = Join-Path $logDir 'raw_i2s_matrix_2026-06-19_warmup.txt'

$matrix = @(
    @{ Name = 'wrapper_512_buf128';  Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=arduino read=512 buf=128';  Log = 'raw_i2s_matrix_2026-06-19_wrapper_512_buf128.txt' },
    @{ Name = 'wrapper_256_buf128';   Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=arduino read=256 buf=128';  Log = 'raw_i2s_matrix_2026-06-19_wrapper_256_buf128.txt' },
    @{ Name = 'wrapper_1024_buf128';  Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=arduino read=1024 buf=128'; Log = 'raw_i2s_matrix_2026-06-19_wrapper_1024_buf128.txt' },
    @{ Name = 'wrapper_512_buf64';    Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=arduino read=512 buf=64';   Log = 'raw_i2s_matrix_2026-06-19_wrapper_512_buf64.txt' },
    @{ Name = 'direct_512_buf128';    Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=direct read=512 buf=128';   Log = 'raw_i2s_matrix_2026-06-19_direct_512_buf128.txt' },
    @{ Name = 'direct_256_buf128';    Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=direct read=256 buf=128';   Log = 'raw_i2s_matrix_2026-06-19_direct_256_buf128.txt' },
    @{ Name = 'direct_1024_buf128';   Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=direct read=1024 buf=128';  Log = 'raw_i2s_matrix_2026-06-19_direct_1024_buf128.txt' },
    @{ Name = 'direct_512_buf64';     Command = 'RAW trigger f=3200 dur=100 pre=50 post=200 mode=pcm i2s=philips transport=direct read=512 buf=64';    Log = 'raw_i2s_matrix_2026-06-19_direct_512_buf64.txt' }
)

function Invoke-RawSeries {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][int]$Count
    )

    & $seriesScript -PortName $PortName -BaudRate $BaudRate -Count $Count -Command $Command -LogPath $LogPath
}

if (Test-Path -LiteralPath $warmupLog) {
    Remove-Item -LiteralPath $warmupLog -Force
}

Write-Host "Warm-up: wrapper_512_buf128 first pass"
Invoke-RawSeries -Command $matrix[0].Command -LogPath $warmupLog -Count 1

foreach ($item in $matrix) {
    $logPath = Join-Path $logDir $item.Log
    if (Test-Path -LiteralPath $logPath) {
        Remove-Item -LiteralPath $logPath -Force
    }

    Write-Host ("Capture: {0}" -f $item.Name)
    Invoke-RawSeries -Command $item.Command -LogPath $logPath -Count 1
}
