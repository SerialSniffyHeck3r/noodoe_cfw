[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('', 'Integrated', 'Graphics', 'Product', 'UninstallBootstrap', 'Diagnostic')]
    [string]$Profile = '',
    [string]$PythonExe = ''
)

$ErrorActionPreference = 'Stop'
$Configuration = if ($Configuration -ieq 'Debug') { 'Debug' } else { 'Release' }
$projectDir = Split-Path -Parent $PSScriptRoot
$researchDir = Split-Path -Parent (Split-Path -Parent $projectDir)
# Independent owned linker/startup: do not select a Cube Product profile or
# modify generated sources for the standalone uninstall build.
if ($Profile -eq 'UninstallBootstrap') {
    if ([string]::IsNullOrWhiteSpace($PythonExe)) {
        $PythonExe = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    }
    & $PythonExe (Join-Path $PSScriptRoot 'uninstall_build.py') --output (Join-Path $researchDir "analysis/uninstall-build/$Configuration") --stock (Join-Path $researchDir 'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin')
    if ($LASTEXITCODE -ne 0) { throw 'UninstallBootstrap build failed.' }
    return
}
if ($Profile -eq 'Diagnostic') {
    if ([string]::IsNullOrWhiteSpace($PythonExe)) {
        $PythonExe = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    }
    & $PythonExe (Join-Path $PSScriptRoot 'diagnostic_build.py') --output (Join-Path $researchDir "analysis/diagnostic-build/$Configuration")
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic build failed.' }
    return
}
$cprojectPath = Join-Path $projectDir '.cproject'
$ideExe = 'C:\ST\STM32CubeIDE_1.18.1\STM32CubeIDE\stm32cubeidec.exe'
# A failed link/budget check must not leave yesterday's app.bin looking like a
# successful result. Preserve prior exports as evidence, outside the build tree.
$priorExports = @('app.bin', 'app.manifest.json', 'memory-report.json') | ForEach-Object {
    $candidate = [IO.Path]::GetFullPath((Join-Path $projectDir "$Configuration/$_"))
    if (-not $candidate.StartsWith(([IO.Path]::GetFullPath($projectDir) + [IO.Path]::DirectorySeparatorChar), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Export path escaped the project.'
    }
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { $candidate }
}
if ($priorExports) {
    $historyBase = [IO.Path]::GetFullPath((Join-Path $researchDir 'analysis/build-artifact-history'))
    $archive = [IO.Path]::GetFullPath((Join-Path $historyBase ("{0}-{1}" -f $Configuration, [Guid]::NewGuid().ToString('N'))))
    if (-not $archive.StartsWith(($historyBase + [IO.Path]::DirectorySeparatorChar), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Export archive escaped the analysis history directory.'
    }
    New-Item -ItemType Directory -Path $archive -Force | Out-Null
    foreach ($prior in $priorExports) { Move-Item -LiteralPath $prior -Destination $archive }
    Write-Host "Prior exports archived (not this build's result): $archive"
}
if ($Profile -ne '') {
    $profilePath = Join-Path $PSScriptRoot 'build_profile.json'
    $profileData = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
    $profileData.profile = $Profile
    [IO.File]::WriteAllText($profilePath, ($profileData | ConvertTo-Json), (New-Object Text.UTF8Encoding($false)))
}

# 재생성으로 덮이는 프로젝트 소유 링크/경로만 먼저 복구한다. USER CODE와 생성
# 주변장치 계약은 자동 재작성하지 않으며 검사 실패 시 컴파일 전에 중단한다.
& (Join-Path $PSScriptRoot 'sync_project.ps1')
& (Join-Path $PSScriptRoot 'check_project.ps1')
if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $bundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundledPython -PathType Leaf) { $PythonExe = $bundledPython }
    else { $PythonExe = (Get-Command python -ErrorAction Stop).Source }
}

if (-not (Test-Path -LiteralPath $cprojectPath -PathType Leaf)) {
    throw 'Missing .cproject. Generate this project with STM32CubeIDE first.'
}
if (-not (Test-Path -LiteralPath $ideExe -PathType Leaf)) {
    throw "STM32CubeIDE 1.18.1 is not installed at: $ideExe"
}
[xml]$cproject = Get-Content -LiteralPath $cprojectPath -Raw
$configNames = @($cproject.SelectNodes('//cconfiguration/storageModule[@moduleId="org.eclipse.cdt.core.settings"]') |
    ForEach-Object { $_.GetAttribute('name') })
if ($configNames -cnotcontains $Configuration) {
    throw "No matching configuration '$Configuration'. Available: $($configNames -join ', ')"
}
[xml]$project = Get-Content -LiteralPath (Join-Path $projectDir '.project') -Raw
$projectName = [string]$project.projectDescription.name
if ([string]::IsNullOrWhiteSpace($projectName)) { throw 'Missing project name in .project.' }

# Keep all Eclipse metadata outside the active GUI workspace. No generator or device commands.
$analysisDir = Join-Path $researchDir 'analysis\2026-09-12-cube-generation-repair'
# CDT marks the opposite configuration dirty during FULL_BUILD, even when
# invoked through -build. Isolate its build-state metadata per configuration
# so Debug/Release alternation does not force an unrelated whole-project clean.
$workspace = Join-Path $analysisDir ("headless-workspace-{0}" -f $Configuration.ToLowerInvariant())
$logFile = Join-Path $analysisDir ("build-{0}.log" -f $Configuration.ToLowerInvariant())
New-Item -ItemType Directory -Path $analysisDir -Force | Out-Null
$buildArgs = @(
    '--launcher.suppressErrors', '-nosplash', '-data', $workspace,
    '-application', 'org.eclipse.cdt.managedbuilder.core.headlessbuild',
    '-no-indexer', '-import', $projectDir, '-build', "$projectName/$Configuration"
)
Write-Host "Building $projectName/$Configuration; log: $logFile"

# Windows PowerShell treats native stderr as ErrorRecord objects; preserve it in the log.
$savedPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & $ideExe @buildArgs 2>&1 | Tee-Object -FilePath $logFile
    $buildExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $savedPreference
}
if ($buildExitCode -ne 0) { throw "Build failed (exit $buildExitCode). See $logFile" }
$buildLog = Get-Content -LiteralPath $logFile -Raw
if ($buildLog -notmatch 'Build Finished\.\s+0 errors\b') {
    throw "No successful build summary was reported. Check configuration/import errors in $logFile"
}

# 링크 성공만으로 순정 BL 보존을 증명할 수 없다. 실제 ELF의 LMA/벡터/reset과
# NOLOAD 배치를 검사한 후에만 APP 전용 BIN과 해시 manifest를 새로 만든다.
$policy = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build_profile.json') -Raw | ConvertFrom-Json
if ($policy.profile -eq 'Product') {
    & $PythonExe (Join-Path $PSScriptRoot 'resource_pack.py') --check
    if ($LASTEXITCODE -ne 0) { throw 'External resource package does not match source assets.' }
}
$elfPath = Join-Path $projectDir "$Configuration\$projectName.elf"
$validationLog = Join-Path $analysisDir ("image-{0}.json" -f $Configuration.ToLowerInvariant())
& $PythonExe (Join-Path $PSScriptRoot 'validate_image.py') $elfPath --check-only > $validationLog
if ($LASTEXITCODE -ne 0) { throw "APP image validation failed. See $validationLog" }
$image = Get-Content -LiteralPath $validationLog -Raw | ConvertFrom-Json
$flashCapacity = if ($policy.profile -eq 'Product') { 393216 } else { 458752 }
$freeFlash = $flashCapacity - [int]$image.binary_size
if ($Configuration -eq 'Release' -and $policy.profile -eq 'Integrated' -and
    $freeFlash -lt [int]$policy.release_min_free_bytes) {
    throw "Integrated APP reserve is too small: $freeFlash bytes remain; required $($policy.release_min_free_bytes)."
}
& $PythonExe (Join-Path $PSScriptRoot 'memory_report.py') $elfPath --profile $policy.profile --configuration $Configuration --output (Join-Path $projectDir "$Configuration/memory-report.json")
if ($LASTEXITCODE -ne 0) { throw 'Linked memory budget failed; see memory-report.json' }
# Publish raw APP files only after both address validation and memory gates pass.
& $PythonExe (Join-Path $PSScriptRoot 'validate_image.py') $elfPath > $validationLog
if ($LASTEXITCODE -ne 0) { throw "APP export failed. See $validationLog" }
Write-Host "Profile $($policy.profile): APP $($image.binary_size) bytes, reserve $freeFlash bytes."
Write-Host "Build succeeded: $projectName/$Configuration"
