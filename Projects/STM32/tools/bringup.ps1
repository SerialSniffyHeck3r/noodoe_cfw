[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$Serial = 'STLINK_SERIAL_REQUIRED',
    [ValidateSet(50, 100)]
    [int]$FrequencyKhz = 100,
    [ValidateSet(0,50,100,400,950)]
    [int]$ReadFrequencyKhz = 0,
    [ValidateRange(1, 20)]
    [int]$Cycles = 3,
    [switch]$TestOnly,
    [switch]$SkipBuild,
    [switch]$LCDTest,
    [switch]$GraphicsTest,
    [switch]$GraphicsPinnedCase,
    [switch]$LiveFlashReads,
    [string]$PythonExe = ''
)
$ErrorActionPreference = 'Stop'
if ($LCDTest -and $GraphicsTest) { throw 'Choose -LCDTest or -GraphicsTest, not both.' }
$projectDir = Split-Path -Parent $PSScriptRoot
$researchDir = Split-Path -Parent (Split-Path -Parent $projectDir)

# Python은 표준 라이브러리만 사용한다. pip나 별도 가상 환경은 필요하지 않다.
# 이 PC의 Codex runtime을 우선 사용하고 없으면 PATH의 Python을 찾는다.
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $bundled = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundled -PathType Leaf) { $PythonExe = $bundled }
    else { $PythonExe = (Get-Command python -ErrorAction Stop).Source }
}

# 매 실행의 원본 백업/이미지/읽기값/로그를 별도 디렉터리에 보관한다.
# 기존 결과를 덮지 않으며 이 명령은 기본적으로 APP 기록 및 reset을 수행한다.
$timestamp = Get-Date -Format 'yyyy-MM-dd-HHmmss-fff'
$outputDir = Join-Path $researchDir "analysis\bringup-runs\$timestamp-$Configuration"
$arguments = @((Join-Path $PSScriptRoot 'bringup.py'), '--serial', $Serial,
    '--frequency-khz', [string]$FrequencyKhz,
    '--configuration', $Configuration, '--cycles', [string]$Cycles, '--output', $outputDir)
if ($TestOnly) { $arguments += '--test-only' }
if ($SkipBuild) { $arguments += '--skip-build' }
if ($LCDTest) { $arguments += '--lcd-test' }
if ($GraphicsTest) { $arguments += '--graphics-test' }
if ($GraphicsPinnedCase) { $arguments += '--graphics-pinned-case' }
if ($LiveFlashReads) { $arguments += '--live-flash-reads' }
if ($ReadFrequencyKhz -ne 0) { $arguments += @('--read-frequency-khz',[string]$ReadFrequencyKhz) }
Write-Host "Evidence: $outputDir"
& $PythonExe @arguments
if ($LASTEXITCODE -ne 0) { throw "Bring-up failed; inspect $outputDir\summary.json and logs." }
