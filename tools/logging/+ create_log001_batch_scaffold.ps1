<#
.SYNOPSIS
Creates a LOG-001 scalar tuning batch scaffold.

.DESCRIPTION
This helper prepares the folder layout and emits the canonical commands for a
repeatable scalar tuning session.

It supports two workflow modes:
- Codex: the batch scaffold is used for analyzer-led runs.
- User: the helper prints the exact commands and log targets for manual execution.

.PARAMETER Mode
Workflow mode. Codex or User.

.PARAMETER Profile
Analyzer profile to tune.

.PARAMETER TotalRuns
Total launch count for the campaign.

.PARAMETER BlockSize
Launches per block.

.PARAMETER Tries
Per-launch trial count.
#>
[CmdletBinding()]
param(
    [ValidateSet('Codex', 'User')]
    [string]$Mode = 'Codex',

    [string]$Profile = 'TonalPulseScalar',

    [int]$TotalRuns = 100,

    [int]$BlockSize = 10,

    [int]$Tries = 50,

    [ValidateSet('quiet', 'trial', 'inspect', 'source', 'system', 'explain')]
    [string]$SeqMode = 'source',

    [ValidateSet('off', 'miss', 'all')]
    [string]$SeqWhen = 'all',

    [ValidateRange(0, 2)]
    [int]$SeqVerbose = 1,

    [string]$Port = 'COM6',

    [int]$Baud = 115200,

    [string]$Root = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $PSScriptRoot '..\logs\seq-tests'
}

function New-LogFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string[]]$Lines
    )

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    Set-Content -LiteralPath $Path -Value $Lines -Encoding utf8
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

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$profileSlug = ConvertTo-ProfileSlug -Value $Profile
$batchName = "{0}-log-001-{1}" -f $timestamp, $profileSlug
$batchRoot = Join-Path $Root $batchName
$blockCount = Get-BlockCount -Runs $TotalRuns -RunsPerBlock $BlockSize
$launchCommand = "SEQ start profile=$Profile tries=$Tries mode=$SeqMode when=$SeqWhen verbose=$SeqVerbose"
$paramStatusCommand = 'PARAM STATUS'

New-Item -ItemType Directory -Force -Path $batchRoot | Out-Null

$readmeLines = @(
    '# LOG-001 Batch Scaffold',
    '',
    'Status: planned',
    '',
    '## Snapshot',
    "- Profile: ``$Profile``",
    "- Mode: ``$Mode``",
    "- Launch command: ``$launchCommand``",
    "- Param snapshot command: ``$paramStatusCommand``",
    "- Port: ``$Port``",
    "- Baud: ``$Baud``",
    "- Total launches: ``$TotalRuns``",
    "- Block size: ``$BlockSize``",
    "- Blocks: ``$blockCount``",
    '',
    '## Log Layout',
    '',
    '- `session.log` records the full batch session.',
    '- `campaign.lock` prevents overlapping launches.',
    '- `campaign_state.json` stores the resume snapshot.',
    '- `run_01.log` through `run_10.log` hold the block transcripts.',
    '- `block_01_summary.md` through `block_10_summary.md` hold the block decisions.',
    '',
    '## Decision Rule',
    '',
    '- First compare miss count, duplicate count, and late count.',
    '- Then compare `avg_dt_ms` and `avg_strength`.',
    '- Compare requested tune against the confirmed `PARAM STATUS` snapshot.',
    '- Keep the current parameter values if the block regresses.',
    '',
    '## Tuning Ladder',
    '',
    '- Blocks 1-5: hold `scalar_max_duration_ms=220` and `scalar_onset_threshold=19000`, then sweep `scalar_release_debounce_ms` from `30` down to `10`.',
    '- Blocks 6-10: keep the best debounce found in phase 1, then sweep `scalar_release_threshold` from `5000` down to `1000`.',
    '',
    '## Workflow Modes',
    '',
    "- Codex-run: Codex reads the saved logs and decides the next ``PARAM`` shift.",
    "- User-run: the helper prints the exact commands and folder targets.",
    '',
    '## Commands',
    '',
    '```text',
    'PARAM STATUS',
    $launchCommand,
    '```',
    '',
    '## Notes',
    '',
    '- Keep the saved batch folder self-contained.',
    '- Do not depend on pasted serial text alone.',
    '- Update the parameter snapshot before and after each shift.',
    '- Record the applied tune from `PARAM STATUS` in the block summary.'
)

$sessionLines = @(
    "root=$batchRoot",
    "mode=$Mode",
    "profile=$Profile",
    "port=$Port",
    "baud=$Baud",
    "total_runs=$TotalRuns",
    "block_size=$BlockSize",
    "launch_command=$launchCommand",
    "param_status_command=$paramStatusCommand",
    "workflow=log-first"
)

New-LogFile -Path (Join-Path $batchRoot 'README.md') -Lines $readmeLines
New-LogFile -Path (Join-Path $batchRoot 'session.log') -Lines $sessionLines

for ($blockIndex = 1; $blockIndex -le $blockCount; $blockIndex++) {
    $blockName = '{0:D2}' -f $blockIndex
    $runPath = Join-Path $batchRoot ("run_{0}.log" -f $blockName)
    $summaryPath = Join-Path $batchRoot ("block_{0}_summary.md" -f $blockName)
    $blockLines = @(
        "# Block $blockName Transcript",
        '',
        'Status: planned',
        '',
        '## Commands',
        '',
        '```text',
        $paramStatusCommand,
        $launchCommand,
        '```',
        '',
        '## Capture Checklist',
        '',
        '- miss count',
        '- duplicate count',
        '- late count',
        '- avg_dt_ms',
        '- avg_strength',
        '',
        '## Decision',
        '',
        '- keep the current values if the block is stable',
        '- shift one knob at a time if the block regresses',
        '- record the decision in the block summary'
    )
    $summaryLines = @(
        "# Block $blockName Summary",
        '',
        'Status: pending',
        '',
        '## Result',
        '',
        '- miss count:',
        '- duplicate count:',
        '- late count:',
        '- avg_dt_ms:',
        '- avg_strength:',
        '',
        '## Decision',
        '',
        '- keep / shift / revert:',
        '',
        '## Notes',
        '',
        '- Store the summary next to the block transcript.',
        '- Keep the summary concise and reproducible.'
    )

    New-LogFile -Path $runPath -Lines $blockLines
    New-LogFile -Path $summaryPath -Lines $summaryLines
}

Write-Host "LOG-001 batch scaffold created:"
Write-Host $batchRoot
Write-Host ""
Write-Host "Mode: $Mode"
Write-Host "Launch: $launchCommand"
Write-Host "Param status: $paramStatusCommand"
Write-Host "Next step: copy the exact commands into the analyzer session or drive them from Codex."
