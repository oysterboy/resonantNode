[CmdletBinding()]
param(
    [string]$Profile = 'TonalPulseScalar',
    [int]$TotalRuns = 100,
    [int]$BlockSize = 10,
    [int]$Tries = 50,
    [int]$StartRun = 1,
    [ValidateSet('quiet', 'trial', 'inspect', 'source', 'system', 'explain')]
    [string]$SeqMode = 'source',
    [ValidateSet('off', 'miss', 'all')]
    [string]$SeqWhen = 'all',
    [ValidateRange(0, 2)]
    [int]$SeqVerbose = 1,
    [string]$PortName = 'COM6',
    [int]$BaudRate = 115200,
    [string]$Root = '',
    [string]$BatchRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $PSScriptRoot '..\logs\seq-tests'
}

if ($StartRun -lt 1) {
    throw 'StartRun must be at least 1.'
}

if ($StartRun -gt $TotalRuns) {
    throw 'StartRun cannot be greater than TotalRuns.'
}

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

    Add-Content -LiteralPath $Path -Value $Line -Encoding utf8
}

function ConvertTo-ProfileSlug {
    param([Parameter(Mandatory = $true)][string]$Value)
    $slug = [regex]::Replace($Value.Trim(), '(?<!^)(?=[A-Z])', '-')
    $slug = $slug -replace '[^a-zA-Z0-9]+', '-'
    $slug = $slug -replace '-+', '-'
    $slug = $slug.Trim('-')
    return $slug.ToLowerInvariant()
}

function Get-BlockCount {
    param(
        [int]$Runs,
        [int]$RunsPerBlock
    )

    if ($RunsPerBlock -le 0) {
        return 1
    }

    return [int][math]::Ceiling([double]$Runs / [double]$RunsPerBlock)
}

function Get-BlockTuneSnapshot {
    param([Parameter(Mandatory = $true)][int]$BlockIndex)

    switch ($BlockIndex) {
        1 {
            return [ordered]@{
                scalar_max_duration_ms = 300
                scalar_onset_threshold = 20000
                scalar_release_threshold = 5000
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 30
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        2 {
            return [ordered]@{
                scalar_max_duration_ms = 280
                scalar_onset_threshold = 20000
                scalar_release_threshold = 5000
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 30
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        3 {
            return [ordered]@{
                scalar_max_duration_ms = 260
                scalar_onset_threshold = 20000
                scalar_release_threshold = 5000
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 25
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        4 {
            return [ordered]@{
                scalar_max_duration_ms = 240
                scalar_onset_threshold = 20000
                scalar_release_threshold = 5000
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 25
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        5 {
            return [ordered]@{
                scalar_max_duration_ms = 220
                scalar_onset_threshold = 19000
                scalar_release_threshold = 4800
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 25
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        6 {
            return [ordered]@{
                scalar_max_duration_ms = 220
                scalar_onset_threshold = 19000
                scalar_release_threshold = 4800
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 20
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        7 {
            return [ordered]@{
                scalar_max_duration_ms = 220
                scalar_onset_threshold = 18000
                scalar_release_threshold = 4500
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 20
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        8 {
            return [ordered]@{
                scalar_max_duration_ms = 200
                scalar_onset_threshold = 18000
                scalar_release_threshold = 4500
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 20
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        9 {
            return [ordered]@{
                scalar_max_duration_ms = 200
                scalar_onset_threshold = 17500
                scalar_release_threshold = 4200
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 15
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
        default {
            return [ordered]@{
                scalar_max_duration_ms = 180
                scalar_onset_threshold = 17500
                scalar_release_threshold = 4200
                scalar_cooldown_ms = 50
                scalar_release_debounce_ms = 15
                scalar_min_duration_ms = 60
                scalar_min_peak_strength = 0
            }
        }
    }
}

function Format-ParamCommand {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Snapshot)

    $parts = New-Object System.Collections.Generic.List[string]
    foreach ($key in $Snapshot.Keys) {
        $parts.Add(('{0}={1}' -f $key, $Snapshot[$key]))
    }
    return 'PARAM ' + ($parts -join ' ')
}

function Get-ScalarTuneLine {
    param([Parameter(Mandatory = $true)][string]$StatusLine)

    if ([string]::IsNullOrWhiteSpace($StatusLine)) {
        return $null
    }

    $freqIndex = $StatusLine.IndexOf(' freqScore=')
    if ($freqIndex -gt 0) {
        return $StatusLine.Substring(0, $freqIndex)
    }

    return $StatusLine
}

function Format-RunLabel {
    param([Parameter(Mandatory = $true)][int]$Value)

    if ($Value -gt 0) {
        return '{0:D2}' -f $Value
    }

    return '00'
}

function Get-LatestBlockName {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$BlockSummaries)

    if ($BlockSummaries.Keys.Count -gt 0) {
        return (($BlockSummaries.Keys | Sort-Object | Select-Object -Last 1))
    }

    return 'unknown'
}

function Try-ReadLine {
    param(
        [Parameter(Mandatory = $true)]$Port
    )

    try {
        return ($Port.ReadLine()).TrimEnd("`r")
    } catch [System.TimeoutException] {
        return $null
    } catch {
        return $null
    }
}

function Drain-Serial {
    param(
        [Parameter(Mandatory = $true)]$Port,
        [Parameter(Mandatory = $true)][int]$Millis
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($Millis)
    while ([DateTime]::UtcNow -lt $deadline) {
        $line = Try-ReadLine -Port $Port
        if ($null -ne $line) {
            return $line
        }
    }

    return $null
}

function Write-CampaignState {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][string]$ProfileName,
        [Parameter(Mandatory = $true)][string]$BatchRootValue,
        [Parameter(Mandatory = $true)][int]$PidValue,
        [Parameter(Mandatory = $true)][int]$StartRunValue,
        [Parameter(Mandatory = $true)][int]$CurrentRunValue,
        [Parameter(Mandatory = $true)][string]$CurrentBlockValue,
        [Parameter(Mandatory = $true)][string]$CurrentTuneValue,
        [Parameter(Mandatory = $true)][string]$LatestSummaryValue
    )

    $state = [ordered]@{
        status = $Status
        pid = $PidValue
        profile = $ProfileName
        batch_root = $BatchRootValue
        start_run = $StartRunValue
        current_run = $CurrentRunValue
        current_block = $CurrentBlockValue
        current_tune = $CurrentTuneValue
        latest_summary = $LatestSummaryValue
        updated_at = (Get-Date).ToString('o')
    }

    $json = $state | ConvertTo-Json -Depth 4
    Set-Content -LiteralPath $Path -Value $json -Encoding utf8
}

function Write-CampaignHeartbeat {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][string]$ProfileName,
        [Parameter(Mandatory = $true)][string]$BatchRootValue,
        [Parameter(Mandatory = $true)][string]$CurrentBlockValue,
        [Parameter(Mandatory = $true)][string]$CurrentRunValue,
        [Parameter(Mandatory = $true)][string]$CurrentTuneValue,
        [Parameter(Mandatory = $true)][string]$LatestSummaryValue
    )

    Set-Content -LiteralPath $Path -Value @(
        '# LOG-001 Heartbeat',
        '',
        "- status: $Status",
        "- profile: $ProfileName",
        "- batch_root: $BatchRootValue",
        "- current_block: $CurrentBlockValue",
        "- current_run: $CurrentRunValue",
        "- current_tune: $CurrentTuneValue",
        "- latest_summary: $LatestSummaryValue",
        "- updated_at: $(Get-Date -Format o)"
    ) -Encoding utf8
}

function Open-CampaignLock {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$StatePath,
        [Parameter(Mandatory = $true)][string]$BatchRootValue,
        [Parameter(Mandatory = $true)][string]$ProfileName
    )

    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None
        )
        $writer = New-Object System.IO.StreamWriter($stream, [System.Text.UTF8Encoding]::new($false))
        $writer.AutoFlush = $true
        $stream.SetLength(0)
        $writer.WriteLine("pid=$PID")
        $writer.WriteLine(("started_at={0}" -f (Get-Date).ToString('o')))
        $writer.WriteLine("batch_root=$BatchRootValue")
        $writer.WriteLine("profile=$ProfileName")
        return [pscustomobject]@{
            Stream = $stream
            Writer = $writer
        }
    } catch [System.IO.IOException] {
        $stateNote = 'no campaign state file'
        $ownerPid = $null
        if (Test-Path -LiteralPath $StatePath) {
            try {
                $stateRaw = Get-Content -LiteralPath $StatePath -Raw
                $stateNote = $stateRaw.Trim()
                $stateObj = $stateRaw | ConvertFrom-Json
                $ownerPid = $stateObj.pid
            } catch {
                $stateNote = 'campaign state unreadable'
            }
        }

        if ($null -ne $ownerPid) {
            $stateNote = "owner_pid=$ownerPid`n$stateNote"
        }

        throw @"
Campaign already running for `$BatchRootValue.
Lock file: `$Path
State:
$stateNote
"@
    }
}

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$profileSlug = ConvertTo-ProfileSlug -Value $Profile
$resumeMode = -not [string]::IsNullOrWhiteSpace($BatchRoot) -and (Test-Path -LiteralPath $BatchRoot)
$batchName = if ([string]::IsNullOrWhiteSpace($BatchRoot)) {
    "{0}-log-001-{1}" -f $timestamp, $profileSlug
} else {
    Split-Path -Path $BatchRoot -Leaf
}
$batchRoot = if ([string]::IsNullOrWhiteSpace($BatchRoot)) {
    Join-Path $Root $batchName
} else {
    $BatchRoot
}
$blockCount = Get-BlockCount -Runs $TotalRuns -RunsPerBlock $BlockSize
$launchCommand = "SEQ start profile=$Profile tries=$Tries mode=$SeqMode when=$SeqWhen verbose=$SeqVerbose"
$paramStatusCommand = 'PARAM STATUS'
$progressPath = Join-Path $batchRoot 'progress.md'
$sessionPath = Join-Path $batchRoot 'session.log'
$heartbeatPath = Join-Path $batchRoot 'heartbeat.md'
$lockPath = Join-Path $batchRoot 'campaign.lock'
$statePath = Join-Path $batchRoot 'campaign_state.json'

New-Item -ItemType Directory -Force -Path $batchRoot | Out-Null

$lockHandle = $null
$lockWriter = $null
$port = $null
$campaignCompleted = $false
$campaignFailed = $false
$blockSummaries = @{}
$runIndex = $null
$blockName = $null
$tuneCommand = $null
$summaryLine = $null
$resumeTuneBlockIndex = $null
$currentBlockName = $null
$currentTuneCommand = $null

try {
$lockInfo = Open-CampaignLock -Path $lockPath -StatePath $statePath -BatchRootValue $batchRoot -ProfileName $Profile
    $lockHandle = $lockInfo.Stream
    $lockWriter = $lockInfo.Writer

    $readme = @(
        '# LOG-001 Batch Run',
        '',
        'Status: running',
        '',
        '## Snapshot',
        "- Profile: ``$Profile``",
        '- Mode: Codex-run',
        "- Launch command: ``$launchCommand``",
        "- Param snapshot command: ``$paramStatusCommand``",
        "- Port: ``$PortName``",
        "- Baud: ``$BaudRate``",
        "- Total launches: ``$TotalRuns``",
        "- Block size: ``$BlockSize``",
        "- Blocks: ``$blockCount``",
        '',
        '## Log Layout',
        '',
        '- `session.log` records the full serial session.',
        '- `campaign.lock` prevents overlapping batch runs.',
        '- `campaign_state.json` records the current resume state.',
        '- `run_01.log` through `run_10.log` capture the block transcripts.',
        '- `block_01_summary.md` through `block_10_summary.md` capture the block decisions.',
        '- `progress.md` is updated while the campaign runs.',
        '- If the batch is resumed, earlier completed runs stay in place and only the incomplete suffix is rewritten.',
        '',
        '## Tuning Ladder',
        '',
        '- Block 1: baseline snapshot.',
        '- Block 2: shorten max duration first.',
        '- Block 3: lower release debounce a little.',
        '- Block 4: continue duration tightening.',
        '- Block 5 and later: lower onset / release thresholds gradually if the earlier blocks stay clean.',
        '',
        '## Notes',
        '',
        '- The campaign uses the current scalar snapshot unless a block decision changes it later.',
        '- Keep the saved batch folder self-contained.'
    )

    $sessionLines = @(
        "root=$batchRoot",
        "mode=Codex-run",
        "profile=$Profile",
        "port=$PortName",
        "baud=$BaudRate",
        "total_runs=$TotalRuns",
        "block_size=$BlockSize",
        "start_run=$StartRun",
        "launch_command=$launchCommand",
        "param_status_command=$paramStatusCommand",
        "resume_mode=$resumeMode",
        "started_at=$(Get-Date -Format o)"
    )

    if (-not $resumeMode) {
        New-TextFile -Path (Join-Path $batchRoot 'README.md') -Lines $readme
        New-TextFile -Path $sessionPath -Lines $sessionLines
        New-TextFile -Path $progressPath -Lines @(
            '# LOG-001 Progress',
            '',
            'Status: starting',
            '',
            "- current_block: 0",
            "- current_run: 0",
            '- latest_summary: none'
        )
    } else {
        Append-TextLine -Path $sessionPath -Line ''
        foreach ($line in $sessionLines) {
            Append-TextLine -Path $sessionPath -Line $line
        }
    }

    $resumeCurrentRun = [Math]::Max(0, $StartRun - 1)
    $resumeCurrentBlock = if ($resumeCurrentRun -gt 0) {
        '{0:D2}' -f ([int][math]::Ceiling([double]$resumeCurrentRun / [double]$BlockSize))
    } else {
        '00'
    }
    $initialRequestedTune = Format-ParamCommand -Snapshot (Get-BlockTuneSnapshot -BlockIndex ([int][math]::Ceiling([double][Math]::Max(1, $StartRun) / [double]$BlockSize)))
    $initialRunLabel = Format-RunLabel -Value $resumeCurrentRun
    $initialAppliedTune = $initialRequestedTune
    Write-CampaignState -Path $statePath -Status 'running' -ProfileName $Profile -BatchRootValue $batchRoot -PidValue $PID -StartRunValue $StartRun -CurrentRunValue $resumeCurrentRun -CurrentBlockValue $resumeCurrentBlock -CurrentTuneValue $initialAppliedTune -LatestSummaryValue 'none'
    Write-CampaignHeartbeat -Path $heartbeatPath -Status 'starting' -ProfileName $Profile -BatchRootValue $batchRoot -CurrentBlockValue $resumeCurrentBlock -CurrentRunValue $initialRunLabel -CurrentTuneValue $initialAppliedTune -LatestSummaryValue 'none'

    $port = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, 'None', 8, 'One'
    $port.DtrEnable = $false
    $port.RtsEnable = $false
    $port.NewLine = "`n"
    $port.ReadTimeout = 250
    $port.WriteTimeout = 1000
    $port.Open()

    Start-Sleep -Milliseconds 1200

    $bootDrainDeadline = [DateTime]::UtcNow.AddSeconds(2)
    while ([DateTime]::UtcNow -lt $bootDrainDeadline) {
        $line = Try-ReadLine -Port $port
        if ($null -ne $line) {
            Append-TextLine -Path $sessionPath -Line $line
            if ($line -match '^PARAM\b' -and $line -match 'freqScore=') {
                $initialAppliedTune = Get-ScalarTuneLine -StatusLine $line
                $currentTuneCommand = $initialAppliedTune
            }
        }
    }
    Write-CampaignHeartbeat -Path $heartbeatPath -Status 'booted' -ProfileName $Profile -BatchRootValue $batchRoot -CurrentBlockValue $resumeCurrentBlock -CurrentRunValue $initialRunLabel -CurrentTuneValue $initialAppliedTune -LatestSummaryValue 'none'

    $blockSummaries = @{}
    $resumeTuneBlockIndex = $null

    if ($resumeMode -and $StartRun -gt 1) {
        $resumeTuneBlockIndex = [int][math]::Ceiling([double]$StartRun / [double]$BlockSize)
        $resumeTuneBlockName = '{0:D2}' -f $resumeTuneBlockIndex
        $resumeTuneSnapshot = Get-BlockTuneSnapshot -BlockIndex $resumeTuneBlockIndex
        $resumeTuneCommand = Format-ParamCommand -Snapshot $resumeTuneSnapshot
        $resumeSummaryPath = Join-Path $batchRoot ("block_{0}_summary.md" -f $resumeTuneBlockName)

        $blockSummaries[$resumeTuneBlockName] = New-Object System.Collections.Generic.List[string]
        if (Test-Path -LiteralPath $resumeSummaryPath) {
            foreach ($line in Get-Content -LiteralPath $resumeSummaryPath) {
                if ($line -match '^SEQ_SUMMARY\b') {
                    $blockSummaries[$resumeTuneBlockName].Add($line)
                }
            }
        }

        $port.WriteLine($resumeTuneCommand)
        Append-TextLine -Path $sessionPath -Line ("resume_tune={0}" -f $resumeTuneCommand)
        $currentTuneCommand = $resumeTuneCommand
        Start-Sleep -Milliseconds 250
        $resumeTuneDeadline = [DateTime]::UtcNow.AddSeconds(2)
        while ([DateTime]::UtcNow -lt $resumeTuneDeadline) {
            $line = Try-ReadLine -Port $port
            if ($null -eq $line) {
                continue
            }
            Append-TextLine -Path $sessionPath -Line $line
            if ($line.Length -gt 0) {
                $blockSummaries[$resumeTuneBlockName].Add($line)
                if ($line -match '^PARAM\b' -and $line -match 'freqScore=') {
                    $currentTuneCommand = Get-ScalarTuneLine -StatusLine $line
                }
            }
        }

        $port.WriteLine($paramStatusCommand)
        Append-TextLine -Path $sessionPath -Line ("resume_cmd={0}" -f $paramStatusCommand)
        Start-Sleep -Milliseconds 250
        $resumeStatusDeadline = [DateTime]::UtcNow.AddSeconds(2)
        while ([DateTime]::UtcNow -lt $resumeStatusDeadline) {
            $line = Try-ReadLine -Port $port
            if ($null -eq $line) {
                continue
            }
            Append-TextLine -Path $sessionPath -Line $line
            if ($line.Length -gt 0) {
                $blockSummaries[$resumeTuneBlockName].Add($line)
                if ($line -match '^PARAM\b' -and $line -match 'freqScore=') {
                    $currentTuneCommand = Get-ScalarTuneLine -StatusLine $line
                }
            }
        }
    }

    for ($runIndex = $StartRun; $runIndex -le $TotalRuns; $runIndex++) {
        $blockIndex = [int][math]::Ceiling([double]$runIndex / [double]$BlockSize)
        $runName = '{0:D2}' -f $runIndex
        $blockName = '{0:D2}' -f $blockIndex
        $runPath = Join-Path $batchRoot ("run_{0}.log" -f $runName)
        $blockSummaryPath = Join-Path $batchRoot ("block_{0}_summary.md" -f $blockName)
        $isNewBlock = (-not $blockSummaries.ContainsKey($blockName))
        if ($isNewBlock) {
            $blockSummaries[$blockName] = New-Object System.Collections.Generic.List[string]
            Append-TextLine -Path $sessionPath -Line ("block={0} start={1}" -f $blockName, (Get-Date -Format o))
            $tuneSnapshot = Get-BlockTuneSnapshot -BlockIndex $blockIndex
            $tuneCommand = Format-ParamCommand -Snapshot $tuneSnapshot
            $currentTuneCommand = $tuneCommand
            if ($blockIndex -gt 1 -and -not ($resumeTuneBlockIndex -eq $blockIndex)) {
                $port.WriteLine($tuneCommand)
                Append-TextLine -Path $sessionPath -Line ("block={0} tune={1}" -f $blockName, $tuneCommand)
                Start-Sleep -Milliseconds 250
                $tuneStatusDeadline = [DateTime]::UtcNow.AddSeconds(2)
                while ([DateTime]::UtcNow -lt $tuneStatusDeadline) {
                    $line = Try-ReadLine -Port $port
                    if ($null -eq $line) {
                        continue
                    }
                    Append-TextLine -Path $sessionPath -Line $line
                    if ($line.Length -gt 0) {
                        $blockSummaries[$blockName].Add($line)
                        if ($line -match '^PARAM\b' -and $line -match 'freqScore=') {
                            $currentTuneCommand = Get-ScalarTuneLine -StatusLine $line
                        }
                    }
                }
            }
            $paramStatusLine = "PARAM STATUS"
            $port.WriteLine($paramStatusLine)
            Append-TextLine -Path $sessionPath -Line ("block={0} cmd={1}" -f $blockName, $paramStatusLine)
            Start-Sleep -Milliseconds 250
            $blockStatusDeadline = [DateTime]::UtcNow.AddSeconds(2)
            while ([DateTime]::UtcNow -lt $blockStatusDeadline) {
                $line = Try-ReadLine -Port $port
                if ($null -eq $line) {
                    continue
                }
                Append-TextLine -Path $sessionPath -Line $line
                if ($line.Length -gt 0) {
                    $blockSummaries[$blockName].Add($line)
                    if ($line -match '^PARAM\b' -and $line -match 'freqScore=') {
                        $currentTuneCommand = Get-ScalarTuneLine -StatusLine $line
                    }
                }
            }
        }

        $port.WriteLine($launchCommand)
        Append-TextLine -Path $sessionPath -Line ("run={0} cmd={1}" -f $runName, $launchCommand)
        New-TextFile -Path $runPath -Lines @(
            "# Run $runName",
            '',
            "Command: $launchCommand",
            "Block: $blockName"
        )

        $runLines = New-Object System.Collections.Generic.List[string]
        $runLines.Add("# Run $runName")
        $runLines.Add('')
        $runLines.Add("Command: $launchCommand")
        $runLines.Add("Block: $blockName")

        $deadline = [DateTime]::UtcNow.AddMinutes(4)
        $sawSummary = $false
        while ([DateTime]::UtcNow -lt $deadline) {
            $line = Try-ReadLine -Port $port
            if ($null -eq $line) {
                continue
            }

            $runLines.Add($line)
            Append-TextLine -Path $sessionPath -Line $line
            if ($line.Length -gt 0) {
                $blockSummaries[$blockName].Add($line)
            }

            if ($line -match '^SEQ_SUMMARY\b') {
                $sawSummary = $true
                break
            }
        }

        if (-not $sawSummary) {
            $runLines.Add('ERROR: summary timeout')
            Append-TextLine -Path $sessionPath -Line ("run={0} summary_timeout=1" -f $runName)
        }

        Set-Content -LiteralPath $runPath -Value $runLines -Encoding utf8

        $summaryLine = $runLines | Where-Object { $_ -match '^SEQ_SUMMARY\b' } | Select-Object -Last 1
        if ($null -eq $summaryLine) {
            $summaryLine = 'SEQ_SUMMARY missing'
        }

        $summaryLines = @(
            "# Block $blockName Summary",
            '',
            "Status: $(if ($sawSummary) { 'running' } else { 'timeout' })",
            '',
            "Last run: $runName",
            "Requested tune: $tuneCommand",
            "Applied tune: $currentTuneCommand",
            "Summary: $summaryLine",
            '',
            '## Run Summaries'
        )
        foreach ($summary in $blockSummaries[$blockName]) {
            if ($summary -match '^SEQ_SUMMARY\b') {
                $summaryLines += "- $summary"
            }
        }
        $summaryLines += ''
        $summaryLines += '## Notes'
        $summaryLines += '- This campaign keeps the current scalar parameters unless a later tuning pass changes them.'
        $summaryLines += '- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.'
        Set-Content -LiteralPath $blockSummaryPath -Value $summaryLines -Encoding utf8

        Set-Content -LiteralPath $progressPath -Value @(
            '# LOG-001 Progress',
            '',
            'Status: running',
            '',
            "- current_block: $blockName",
            "- current_run: $runName",
            "- current_tune: $currentTuneCommand",
            "- latest_summary: $summaryLine"
        ) -Encoding utf8

        Write-CampaignHeartbeat -Path $heartbeatPath -Status 'running' -ProfileName $Profile -BatchRootValue $batchRoot -CurrentBlockValue $blockName -CurrentRunValue $runName -CurrentTuneValue $currentTuneCommand -LatestSummaryValue $summaryLine

        Write-CampaignState -Path $statePath -Status 'running' -ProfileName $Profile -BatchRootValue $batchRoot -PidValue $PID -StartRunValue $StartRun -CurrentRunValue $runIndex -CurrentBlockValue $blockName -CurrentTuneValue $currentTuneCommand -LatestSummaryValue $summaryLine
    }

    Set-Content -LiteralPath $progressPath -Value @(
        '# LOG-001 Progress',
        '',
        'Status: complete',
        '',
        "- current_block: $blockCount",
        "- current_run: $TotalRuns",
        "- current_tune: $currentTuneCommand",
        "- latest_summary: $(($blockSummaries[$('{0:D2}' -f $blockCount)] | Where-Object { $_ -match '^SEQ_SUMMARY\b' } | Select-Object -Last 1))"
    ) -Encoding utf8

    Write-CampaignState -Path $statePath -Status 'complete' -ProfileName $Profile -BatchRootValue $batchRoot -PidValue $PID -StartRunValue $StartRun -CurrentRunValue $TotalRuns -CurrentBlockValue ('{0:D2}' -f $blockCount) -CurrentTuneValue $currentTuneCommand -LatestSummaryValue $(($blockSummaries[$('{0:D2}' -f $blockCount)] | Where-Object { $_ -match '^SEQ_SUMMARY\b' } | Select-Object -Last 1))

    Append-TextLine -Path $sessionPath -Line ("campaign_complete={0}" -f (Get-Date -Format o))
    Write-CampaignHeartbeat -Path $heartbeatPath -Status 'complete' -ProfileName $Profile -BatchRootValue $batchRoot -CurrentBlockValue ('{0:D2}' -f $blockCount) -CurrentRunValue ('{0:D2}' -f $TotalRuns) -CurrentTuneValue $currentTuneCommand -LatestSummaryValue $(($blockSummaries[$('{0:D2}' -f $blockCount)] | Where-Object { $_ -match '^SEQ_SUMMARY\b' } | Select-Object -Last 1))
    $campaignCompleted = $true
} catch {
    $campaignFailed = $true
    $failureTime = Get-Date -Format o
    $failureHeader = "campaign_failed=$failureTime"
    $failureMessage = $_.Exception.Message
    $failureType = $_.Exception.GetType().FullName
    $failureStack = $_.ScriptStackTrace
    $failureLine = if ($null -ne $_.InvocationInfo -and $null -ne $_.InvocationInfo.ScriptLineNumber) {
        "line=$($_.InvocationInfo.ScriptLineNumber) column=$($_.InvocationInfo.OffsetInLine)"
    } else {
        'line=unknown'
    }

    Append-TextLine -Path $sessionPath -Line $failureHeader
    Append-TextLine -Path $sessionPath -Line ("campaign_failed_type={0}" -f $failureType)
    Append-TextLine -Path $sessionPath -Line ("campaign_failed_message={0}" -f $failureMessage)
    Append-TextLine -Path $sessionPath -Line ("campaign_failed_location={0}" -f $failureLine)
    if (-not [string]::IsNullOrWhiteSpace($failureStack)) {
        Append-TextLine -Path $sessionPath -Line 'campaign_failed_stack_begin'
        foreach ($stackLine in ($failureStack -split "(`r`n|`n|`r)")) {
            if (-not [string]::IsNullOrWhiteSpace($stackLine)) {
                Append-TextLine -Path $sessionPath -Line $stackLine
            }
        }
        Append-TextLine -Path $sessionPath -Line 'campaign_failed_stack_end'
    }

    Set-Content -LiteralPath $progressPath -Value @(
        '# LOG-001 Progress',
        '',
        'Status: failed',
        '',
        "- current_block: $(if ([string]::IsNullOrWhiteSpace($blockName)) { 'unknown' } else { $blockName })",
        "- current_run: $(if ($null -ne $runIndex) { '{0:D2}' -f $runIndex } else { 'unknown' })",
        "- current_tune: $(if ($null -ne $tuneCommand) { $tuneCommand } else { 'unknown' })",
        "- latest_summary: $(if ($null -ne $summaryLine) { $summaryLine } else { 'unknown' })",
        "- failure_type: $failureType",
        "- failure_message: $failureMessage"
    ) -Encoding utf8

    $failureBlockName = if ([string]::IsNullOrWhiteSpace($blockName)) { 'unknown' } else { $blockName }
    $failureRunLabel = if ($null -ne $runIndex) { Format-RunLabel -Value $runIndex } else { 'unknown' }
    $failureTune = if ($null -ne $currentTuneCommand) { $currentTuneCommand } elseif ($null -ne $tuneCommand) { $tuneCommand } else { 'unknown' }
    $failureSummary = if ($null -ne $summaryLine) { $summaryLine } else { $failureMessage }
    Write-CampaignState -Path $statePath -Status 'failed' -ProfileName $Profile -BatchRootValue $batchRoot -PidValue $PID -StartRunValue $StartRun -CurrentRunValue $(if ($null -ne $runIndex) { $runIndex } else { 0 }) -CurrentBlockValue $failureBlockName -CurrentTuneValue $failureTune -LatestSummaryValue $failureSummary
    Write-CampaignHeartbeat -Path $heartbeatPath -Status 'failed' -ProfileName $Profile -BatchRootValue $batchRoot -CurrentBlockValue $failureBlockName -CurrentRunValue $failureRunLabel -CurrentTuneValue $failureTune -LatestSummaryValue $failureSummary
    throw
} finally {
    if (-not $campaignCompleted -and -not $campaignFailed) {
        Append-TextLine -Path $sessionPath -Line ("campaign_stopped_without_completion={0}" -f (Get-Date -Format o))
        $stoppedBlockName = Get-LatestBlockName -BlockSummaries $blockSummaries
        $stoppedRunLabel = if ($StartRun) { Format-RunLabel -Value ([Math]::Max(0, $StartRun - 1)) } else { 'unknown' }
        Set-Content -LiteralPath $progressPath -Value @(
            '# LOG-001 Progress',
            '',
            'Status: stopped',
            '',
            "- current_block: $stoppedBlockName",
            "- current_run: $stoppedRunLabel",
            "- latest_summary: unknown"
        ) -Encoding utf8
        Write-CampaignState -Path $statePath -Status 'stopped' -ProfileName $Profile -BatchRootValue $batchRoot -PidValue $PID -StartRunValue $StartRun -CurrentRunValue $(if ($StartRun) { [Math]::Max(0, $StartRun - 1) } else { 0 }) -CurrentBlockValue $stoppedBlockName -CurrentTuneValue 'unknown' -LatestSummaryValue 'unknown'
        Write-CampaignHeartbeat -Path $heartbeatPath -Status 'stopped' -ProfileName $Profile -BatchRootValue $batchRoot -CurrentBlockValue $stoppedBlockName -CurrentRunValue $stoppedRunLabel -CurrentTuneValue 'unknown' -LatestSummaryValue 'unknown'
    }
    if ($lockWriter -ne $null) {
        $lockWriter.Dispose()
    }
    if ($lockHandle -ne $null) {
        $lockHandle.Dispose()
    }
    if ($null -ne $port -and $port.IsOpen) {
        $port.Close()
    }
    if ($null -ne $port) {
        $port.Dispose()
    }
}
