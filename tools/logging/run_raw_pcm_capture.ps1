[CmdletBinding()]
param(
    [string]$PortName = 'COM3',
    [int]$BaudRate = 115200,
    [int]$TotalRuns = 10,
    [int]$PreMs = 50,
    [int]$PostMs = 200,
    [ValidateSet('pcm', 'raw', 'feat', 'features', 'csv', 'both')]
    [string]$Mode = 'pcm',
    [int]$PauseBetweenRunsSec = 30,
    [int]$StartupTimeoutSec = 20,
    [int]$IdleTimeoutSec = 15,
    [int]$MaxRunSec = 180,
    [string]$Root = '',
    [string]$BatchRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:StartedAt = Get-Date

if ($TotalRuns -lt 1) {
    throw 'TotalRuns must be at least 1.'
}
if ($PreMs -lt 0) {
    throw 'PreMs must be 0 or greater.'
}
if ($PostMs -lt 0) {
    throw 'PostMs must be 0 or greater.'
}
if ($PauseBetweenRunsSec -lt 0) {
    throw 'PauseBetweenRunsSec must be 0 or greater.'
}
if ($StartupTimeoutSec -lt 1) {
    throw 'StartupTimeoutSec must be at least 1.'
}
if ($IdleTimeoutSec -lt 1) {
    throw 'IdleTimeoutSec must be at least 1.'
}
if ($MaxRunSec -lt 1) {
    throw 'MaxRunSec must be at least 1.'
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $repoRoot 'logs\raw_pcm'
}

$batchName = 'raw_pcm_batch_{0}' -f (Get-Date -Format 'yyyyMMdd_HHmmss')
$batchRoot = if ([string]::IsNullOrWhiteSpace($BatchRoot)) {
    Join-Path $Root $batchName
} else {
    $BatchRoot
}

$sessionPath = Join-Path $batchRoot 'session.log'
$heartbeatPath = Join-Path $batchRoot 'heartbeat.md'
$progressPath = Join-Path $batchRoot 'progress.md'
$statePath = Join-Path $batchRoot 'capture_state.json'
$command = 'RAW pre={0} post={1} mode={2}' -f $PreMs, $PostMs, $Mode

$currentStatus = 'starting'
$currentRun = 0
$linesSeen = 0
$bytesSeen = 0L
$sawRawBegin = $false
$sawRawSummary = $false
$latestLine = 'none'
$lastLineAt = $null
$lastHeartbeatAt = [DateTime]::MinValue
$lastRunStatus = 'none'
$port = $null

function New-TextFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string[]]$Lines
    )

    $dir = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }

    Set-Content -LiteralPath $Path -Value $Lines -Encoding utf8
}

function Append-TextLine {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Line
    )

    $attempt = 0
    while ($true) {
        try {
            $stream = [System.IO.File]::Open(
                $Path,
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
            return
        } catch [System.IO.IOException] {
            $attempt++
            if ($attempt -ge 25) {
                throw
            }
            Start-Sleep -Milliseconds 100
        }
    }
}

function Get-LineByteCount {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Line)
    return [System.Text.Encoding]::UTF8.GetByteCount($Line) + [System.Text.Encoding]::UTF8.GetByteCount([Environment]::NewLine)
}

function ConvertTo-SafeLine {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Line)
    if ($Line.Length -le 180) {
        return $Line
    }
    return $Line.Substring(0, 180) + '...'
}

function Try-ReadLine {
    param([Parameter(Mandatory = $true)][System.IO.Ports.SerialPort]$SerialPort)

    try {
        $line = $SerialPort.ReadLine()
        return $line.TrimEnd("`r")
    } catch [System.TimeoutException] {
        return $null
    }
}

function Write-CaptureStatusFiles {
    param(
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][int]$CurrentRunValue,
        [Parameter(Mandatory = $true)][string]$LastRunStatusValue
    )

    $updatedAt = (Get-Date).ToString('o')
    $lastLineAtValue = if ($null -eq $script:lastLineAt) { 'none' } else { $script:lastLineAt.ToString('o') }
    $safeLatestLine = ConvertTo-SafeLine -Line $script:latestLine

    Set-Content -LiteralPath $script:heartbeatPath -Value @(
        '# RAW PCM Capture Heartbeat',
        '',
        "- status: $Status",
        "- started_at: $($script:StartedAt.ToString('o'))",
        "- updated_at: $updatedAt",
        "- port: $script:PortName",
        "- baud: $script:BaudRate",
        "- command: $script:command",
        "- current_run: $CurrentRunValue",
        "- total_runs: $script:TotalRuns",
        "- last_run_status: $LastRunStatusValue",
        "- lines_seen: $script:linesSeen",
        "- bytes_seen: $script:bytesSeen",
        "- last_line_at: $lastLineAtValue",
        "- saw_raw_begin: $script:sawRawBegin",
        "- saw_raw_summary: $script:sawRawSummary",
        "- latest_line: $safeLatestLine",
        "- log_path: $script:sessionPath"
    ) -Encoding utf8

    Set-Content -LiteralPath $script:progressPath -Value @(
        '# RAW PCM Capture Progress',
        '',
        "Status: $Status",
        '',
        "- current_run: $CurrentRunValue",
        "- total_runs: $script:TotalRuns",
        "- last_run_status: $LastRunStatusValue",
        "- lines_seen: $script:linesSeen",
        "- bytes_seen: $script:bytesSeen",
        "- saw_raw_begin: $script:sawRawBegin",
        "- saw_raw_summary: $script:sawRawSummary",
        "- latest_line: $safeLatestLine",
        "- session_log: $script:sessionPath"
    ) -Encoding utf8

    $state = [ordered]@{
        status = $Status
        pid = $PID
        port = $script:PortName
        baud = $script:BaudRate
        command = $script:command
        batch_root = $script:batchRoot
        session_log = $script:sessionPath
        current_run = $CurrentRunValue
        total_runs = $script:TotalRuns
        last_run_status = $LastRunStatusValue
        lines_seen = $script:linesSeen
        bytes_seen = $script:bytesSeen
        saw_raw_begin = $script:sawRawBegin
        saw_raw_summary = $script:sawRawSummary
        latest_line = $script:latestLine
        last_line_at = $lastLineAtValue
        started_at = $script:StartedAt.ToString('o')
        updated_at = $updatedAt
    }
    Set-Content -LiteralPath $script:statePath -Value ($state | ConvertTo-Json -Depth 4) -Encoding utf8
    $script:lastHeartbeatAt = Get-Date
}

function Update-HeartbeatIfDue {
    param(
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][int]$CurrentRunValue,
        [Parameter(Mandatory = $true)][string]$LastRunStatusValue,
        [int]$IntervalSec = 1
    )

    if (((Get-Date) - $script:lastHeartbeatAt).TotalSeconds -ge $IntervalSec) {
        Write-CaptureStatusFiles -Status $Status -CurrentRunValue $CurrentRunValue -LastRunStatusValue $LastRunStatusValue
    }
}

function Append-SessionLine {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Line)

    Append-TextLine -Path $script:sessionPath -Line $Line
    $script:linesSeen++
    $script:bytesSeen += Get-LineByteCount -Line $Line
    $script:latestLine = $Line
    $script:lastLineAt = Get-Date
}

New-Item -ItemType Directory -Force -Path $batchRoot | Out-Null
New-TextFile -Path $sessionPath -Lines @(
    "raw_pcm_batch_start=$(Get-Date -Format o)",
    "batch_root=$batchRoot",
    "port=$PortName",
    "baud=$BaudRate",
    "total_runs=$TotalRuns",
    "pause_between_runs_sec=$PauseBetweenRunsSec",
    "startup_timeout_sec=$StartupTimeoutSec",
    "idle_timeout_sec=$IdleTimeoutSec",
    "max_run_sec=$MaxRunSec",
    "command=$command"
)
Write-CaptureStatusFiles -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus

try {
    $port = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, 'None', 8, 'One'
    $port.DtrEnable = $false
    $port.RtsEnable = $false
    $port.NewLine = "`n"
    $port.ReadTimeout = 250
    $port.WriteTimeout = 1000
    $port.Open()

    $currentStatus = 'draining_boot'
    Write-CaptureStatusFiles -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus
    Start-Sleep -Milliseconds 1200

    $bootDrainDeadline = (Get-Date).AddSeconds(2)
    while ((Get-Date) -lt $bootDrainDeadline) {
        $line = Try-ReadLine -SerialPort $port
        if ($null -ne $line) {
            Append-SessionLine -Line $line
        }
        Update-HeartbeatIfDue -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus
    }

    for ($runIndex = 1; $runIndex -le $TotalRuns; $runIndex++) {
        $currentRun = $runIndex
        $runLabel = '{0:D3}' -f $runIndex
        $script:sawRawBegin = $false
        $script:sawRawSummary = $false
        $runStartedAt = Get-Date
        $lastBytesAt = $runStartedAt
        $runStatus = 'running'
        $currentStatus = 'running'

        Append-SessionLine -Line ''
        Append-SessionLine -Line ("RUN {0} START {1}" -f $runLabel, $runStartedAt.ToString('o'))
        Append-SessionLine -Line ("RUN {0} cmd={1}" -f $runLabel, $command)
        Write-CaptureStatusFiles -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus

        $port.WriteLine($command)

        while ($true) {
            $line = Try-ReadLine -SerialPort $port
            if ($null -ne $line) {
                Append-SessionLine -Line $line
                $lastBytesAt = Get-Date

                if ($line -match '^RAW_BEGIN\b') {
                    $script:sawRawBegin = $true
                }
                if ($line -match '^RAW_SUMMARY\b') {
                    $script:sawRawSummary = $true
                    $runStatus = 'complete'
                    break
                }
            }

            $elapsedSec = ((Get-Date) - $runStartedAt).TotalSeconds
            $idleSec = ((Get-Date) - $lastBytesAt).TotalSeconds

            if (-not $script:sawRawBegin -and $elapsedSec -ge $StartupTimeoutSec) {
                $runStatus = 'startup_timeout'
                break
            }
            if ($script:sawRawBegin -and -not $script:sawRawSummary -and $idleSec -ge $IdleTimeoutSec) {
                $runStatus = 'idle_timeout'
                break
            }
            if ($elapsedSec -ge $MaxRunSec) {
                $runStatus = 'max_timeout'
                break
            }

            Update-HeartbeatIfDue -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $runStatus
        }

        $lastRunStatus = $runStatus
        Append-SessionLine -Line ("RUN {0} {1} {2}" -f $runLabel, $runStatus.ToUpperInvariant(), (Get-Date).ToString('o'))
        Write-CaptureStatusFiles -Status $runStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus

        if ($runStatus -ne 'complete') {
            $currentStatus = $runStatus
            break
        }

        if ($runIndex -lt $TotalRuns -and $PauseBetweenRunsSec -gt 0) {
            $currentStatus = 'pausing'
            $pauseDeadline = (Get-Date).AddSeconds($PauseBetweenRunsSec)
            Write-CaptureStatusFiles -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus
            while ((Get-Date) -lt $pauseDeadline) {
                $line = Try-ReadLine -SerialPort $port
                if ($null -ne $line) {
                    Append-SessionLine -Line $line
                }
                Update-HeartbeatIfDue -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus
            }
        }
    }

    if ($lastRunStatus -eq 'complete' -and $currentRun -eq $TotalRuns) {
        $currentStatus = 'complete'
    }

    Append-SessionLine -Line ("raw_pcm_batch_{0}={1}" -f $currentStatus, (Get-Date).ToString('o'))
    Write-CaptureStatusFiles -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus
    Write-Host "RAW PCM capture status: $currentStatus"
    Write-Host "Batch root: $batchRoot"
    Write-Host "Session log: $sessionPath"
} catch {
    $currentStatus = 'failed'
    $message = $_.Exception.Message
    Append-TextLine -Path $sessionPath -Line ("raw_pcm_batch_failed={0}" -f (Get-Date).ToString('o'))
    Append-TextLine -Path $sessionPath -Line ("error_type={0}" -f $_.Exception.GetType().FullName)
    Append-TextLine -Path $sessionPath -Line ("error_message={0}" -f $message)
    $script:latestLine = "ERROR: $message"
    Write-CaptureStatusFiles -Status $currentStatus -CurrentRunValue $currentRun -LastRunStatusValue $lastRunStatus
    throw
} finally {
    if ($null -ne $port) {
        if ($port.IsOpen) {
            $port.Close()
        }
        $port.Dispose()
    }
}
