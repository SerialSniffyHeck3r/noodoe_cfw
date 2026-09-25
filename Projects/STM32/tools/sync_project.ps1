[CmdletBinding()]
param(
    [string]$ProjectDirectory = ''
)
$ErrorActionPreference = 'Stop'
# Windows PowerShell 5.1의 -File 실행에서는 param 기본식 평가 시 PSScriptRoot가
# 아직 비어 있을 수 있다. 본문에서 계산하여 직접 실행과 build.ps1 호출을 맞춘다.
if ([string]::IsNullOrWhiteSpace($ProjectDirectory)) {
    $ProjectDirectory = Split-Path -Parent $PSScriptRoot
}
$projectDir = (Resolve-Path -LiteralPath $ProjectDirectory).Path
$metadataPath = Join-Path $projectDir '.cproject'
$buildPolicy = Get-Content -LiteralPath (Join-Path $projectDir 'tools/build_profile.json') -Raw | ConvertFrom-Json
$profileLinker = if ($buildPolicy.profile -eq 'Product') { '../Linker/Noodoe_Product.ld' } else { '../Linker/Noodoe_APP.ld' }

# Cube 재생성이 실제로 잃어버린 사용자 소유 설정을 복구한다. 생성 C의 유일한
# 예외는 IOC에서 파싱한 defaultTask_attributes.stack_size 숫자다. IOC 자체와
# 다른 생성 C/USER CODE, 디버거 설정, 생성 makefile은 수정하지 않는다. 별도 프로젝트 경로는
# 임시 fixture 검증에도 쓰며, BSP와 전용 링커가 있는 프로젝트만 대상으로 받는다.
foreach ($required in @('.cproject', 'Linker/Noodoe_APP.ld', 'Drivers/BSP/inc',
                        'Graphics/Port/inc/lv_conf.h', 'tools/lvgl_build.json',
                        'Middlewares/Third_Party/LVGL/UPSTREAM.json')) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectDir $required))) {
        throw "Cannot synchronize project: missing $required in $projectDir"
    }
}
. (Join-Path $PSScriptRoot 'task_stack_contract.ps1')
$iocPath = Join-Path $projectDir 'FuckNudo_Noodoe_CFW_Project.ioc'
$freertosPath = Join-Path $projectDir 'Core/Src/freertos.c'
# strict UTF-8 왕복은 BOM/CRLF/주석을 그대로 보존한다. 단지 목표 숫자의 byte만
# 바꿀 준비를 하며 아래 .cproject 검증까지 모두 끝난 뒤 파일에 반영한다.
$stackEncoding = New-Object System.Text.UTF8Encoding($false, $true)
$stackIocText = $stackEncoding.GetString([IO.File]::ReadAllBytes($iocPath))
$freertosBytes = [IO.File]::ReadAllBytes($freertosPath)
$freertosText = $stackEncoding.GetString($freertosBytes)
$stackContract = Get-NoodoeDefaultTaskStackContract -IocText $stackIocText -FreeRTOSText $freertosText
# LVGL runtime 목록은 고정한 버전과 함께 관리한다. vendor 전체를 소스 루트로
# 등록하면 비활성 Linux/Windows/ThorVG C++ 코드까지 Cube가 발견할 수 있으므로,
# 명시한 src 하위 집합만 컴파일하며 원본 runtime 파일은 그대로 보관한다.
$lvglBuild = Get-Content -LiteralPath (Join-Path $projectDir 'tools/lvgl_build.json') -Raw | ConvertFrom-Json
$lvglSourceRoot = [string]$lvglBuild.source_root
$lvglExclusions = @($lvglBuild.exclusions_relative_to_source_root)
$customIncludes = @('../Drivers/BSP/inc') + @($lvglBuild.include_paths)
$metadata = New-Object System.Xml.XmlDocument
$metadata.PreserveWhitespace = $true
$metadata.Load($metadataPath)
$changed = $false
foreach ($configuration in @('Debug', 'Release')) {
    $config = $metadata.SelectSingleNode("//configuration[@name='$configuration']")
    if ($null -eq $config) { throw "Missing $configuration configuration" }
    $linker = $config.SelectSingleNode('.//option[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.script"]')
    if ($null -eq $linker) { throw "$configuration is missing its GCC linker option" }
    # Debug/Release make는 프로젝트의 바로 아래 디렉터리에서 실행된다.
    # GUI workspace의 중첩 매크로 해석에 의존하지 않는 실제 상대 경로를 쓴다.
    $wantedLinker = $profileLinker
    if ($linker.GetAttribute('value') -cne $wantedLinker) {
        $linker.SetAttribute('value', $wantedLinker)
        $changed = $true
    }

    # Drivers 소스 루트가 BSP를 재귀적으로 포함한다. 중첩 BSP 루트는 중복 빌드를
    # 만들 수 있으므로 제거한다. App_Logic 루트는 제품 로직 전용으로 유지한다.
    $sourceEntries = $config.SelectSingleNode('./sourceEntries')
    if ($null -eq $sourceEntries) { throw "$configuration is missing sourceEntries" }
    foreach ($entry in @($sourceEntries.SelectNodes('./entry'))) {
        $name = $entry.GetAttribute('name').Replace('\', '/').TrimEnd('/')
        if ($name.StartsWith('App_Logic/') -or
            $name -eq 'Drivers/BSP' -or $name.StartsWith('Drivers/BSP/') -or
            $name.StartsWith('Graphics/') -or
            ($name.StartsWith('Middlewares/Third_Party/LVGL') -and $name -ne $lvglSourceRoot)) {
            [void]$sourceEntries.RemoveChild($entry)
            $changed = $true
            continue
        }
        # 명시한 App_Logic/Graphics 루트와 최상위 루트의 중복 컴파일을 막는다.
        if ($name -eq '') {
            $excludes = @($entry.GetAttribute('excluding').Split('|') | Where-Object { $_ -ne '' })
            $wantedExcludes = @($excludes)
            foreach ($exclude in @('App_Logic', 'Graphics', 'Middlewares/Third_Party/LVGL', 'UninstallBootstrap')) {
                if ($wantedExcludes -notcontains $exclude) { $wantedExcludes += $exclude }
            }
            if (($wantedExcludes -join '|') -cne ($excludes -join '|')) {
                $entry.SetAttribute('excluding', ($wantedExcludes -join '|'))
                $changed = $true
            }
        }
        if ($name -eq 'Drivers') {
            $before = $entry.GetAttribute('excluding')
            $excludes = @($before.Split('|') | Where-Object {
                $_ -ne '' -and $_ -notmatch '^BSP(?:[/\\].*)?$'
            })
            if (($excludes -join '|') -cne $before) {
                if ($excludes.Count -eq 0) { $entry.RemoveAttribute('excluding') }
                else { $entry.SetAttribute('excluding', ($excludes -join '|')) }
                $changed = $true
            }
        }
    }
    if ($null -eq $sourceEntries.SelectSingleNode('./entry[@name="Drivers"]')) {
        $entry = $metadata.CreateElement('entry')
        $entry.SetAttribute('flags', 'VALUE_WORKSPACE_PATH|RESOLVED')
        $entry.SetAttribute('kind', 'sourcePath')
        $entry.SetAttribute('name', 'Drivers')
        [void]$sourceEntries.AppendChild($entry)
        $changed = $true
    }

    # BSP와 GUI 정책은 다른 루트다. Middlewares 부모 루트에서 LVGL 전체를
    # 제외한 뒤 전용 src 루트로 한 번만 포함하여 demos/원치 않는 backend를
    # 컴파일하지 않는다. 아래 두 사용자 소유 루트는 재생성 후에도 복원한다.
    foreach ($sourceName in @('Graphics', $lvglSourceRoot)) {
        if ($null -eq $sourceEntries.SelectSingleNode("./entry[@name='$sourceName']")) {
            $entry = $metadata.CreateElement('entry')
            $entry.SetAttribute('flags', 'VALUE_WORKSPACE_PATH|RESOLVED')
            $entry.SetAttribute('kind', 'sourcePath')
            $entry.SetAttribute('name', $sourceName)
            [void]$sourceEntries.AppendChild($entry)
            $changed = $true
        }
    }
    $middlewareEntry = $sourceEntries.SelectSingleNode('./entry[@name="Middlewares"]')
    if ($null -eq $middlewareEntry) { throw "$configuration is missing its Middlewares source root" }
    $parentExcludes = @($middlewareEntry.GetAttribute('excluding').Split('|') | Where-Object { $_ -ne '' })
    if ($parentExcludes -notcontains $lvglBuild.parent_exclusion) {
        $middlewareEntry.SetAttribute('excluding', (($parentExcludes + $lvglBuild.parent_exclusion) -join '|'))
        $changed = $true
    }
    $lvglEntry = $sourceEntries.SelectSingleNode("./entry[@name='$lvglSourceRoot']")
    $wantedLVGLExclusions = $lvglExclusions -join '|'
    if ($lvglEntry.GetAttribute('excluding') -cne $wantedLVGLExclusions) {
        $lvglEntry.SetAttribute('excluding', $wantedLVGLExclusions)
        $changed = $true
    }

    # Debug/Release의 C 및 assembler include를 각각 보장한다. 상대 경로는 두 빌드
    # 디렉터리에서 공통으로 ../Drivers/BSP/inc이며 임의의 PC 절대 경로를 넣지 않는다.
    foreach ($toolName in @('assembler', 'c.compiler')) {
        $tool = $config.SelectSingleNode(".//tool[@superClass='com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.$toolName']")
        if ($null -eq $tool) { throw "$configuration is missing $toolName" }
        $include = $tool.SelectSingleNode('./option[@valueType="includePath"]')
        if ($null -eq $include) {
            $include = $metadata.CreateElement('option')
            $include.SetAttribute('id', "noodoe.$configuration.$toolName.includepaths")
            $include.SetAttribute('superClass', "com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.$toolName.option.includepaths")
            $include.SetAttribute('valueType', 'includePath')
            [void]$tool.AppendChild($include)
            $changed = $true
        }
    }
    foreach ($include in @($config.SelectNodes('.//option[@valueType="includePath"]'))) {
        foreach ($item in @($include.SelectNodes('./listOptionValue'))) {
            if ($item.GetAttribute('value').Replace('\', '/').TrimEnd('/') -eq '../App_Logic/inc') {
                [void]$include.RemoveChild($item)
                $changed = $true
            }
        }
        foreach ($includePath in $customIncludes) {
            if ($null -eq $include.SelectSingleNode("./listOptionValue[@value='$includePath']")) {
                $item = $metadata.CreateElement('listOptionValue')
                $item.SetAttribute('builtIn', 'false')
                $item.SetAttribute('value', $includePath)
                [void]$include.AppendChild($item)
                $changed = $true
            }
        }
    }

    # lv_conf.h는 vendor 옆의 임의 파일이 아니라 Graphics/Port/inc에 있다.
    # Simple include 선택을 명시하여 compiler의 우연한 __has_include 탐지에
    # 의존하지 않는다. 원래 MCU/HAL/DEBUG define은 그대로 보존한다.
    $cCompiler = $config.SelectSingleNode('.//tool[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler"]')
    $defines = $cCompiler.SelectSingleNode('./option[@valueType="definedSymbols"]')
    if ($null -eq $defines) {
        $defines = $metadata.CreateElement('option')
        $defines.SetAttribute('id', "noodoe.$configuration.c.compiler.definedsymbols")
        $defines.SetAttribute('superClass', 'com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler.option.definedsymbols')
        $defines.SetAttribute('valueType', 'definedSymbols')
        [void]$cCompiler.AppendChild($defines)
        $changed = $true
    }
    foreach ($define in @($lvglBuild.defines)) {
        if ($null -eq $defines.SelectSingleNode("./listOptionValue[@value='$define']")) {
            $item = $metadata.CreateElement('listOptionValue')
            $item.SetAttribute('builtIn', 'false')
            $item.SetAttribute('value', $define)
            [void]$defines.AppendChild($item)
            $changed = $true
        }
    }

    # Compiler policy is restored once, by services_build.py below.

}
if ($stackContract.Changed) {
    [IO.File]::WriteAllBytes($freertosPath, $stackEncoding.GetBytes($stackContract.GeneratedText))
    Write-Host "Restored generated defaultTask stack from IOC: $($stackContract.StackWords) words / $($stackContract.StackBytes) bytes."
}
if ($changed) {
    # 전체 검사가 끝난 뒤에만 저장한다. 두 번째 실행은 파일을 쓰지 않는 멱등 작업이다.
    $metadata.Save($metadataPath)
    Write-Host 'Restored APP linker, BSP/Graphics paths and pinned LVGL source selection in .cproject.'
} else {
    Write-Host 'Project-owned linker, BSP/Graphics and LVGL settings already synchronized.'
}
# 새 vendor/service 목록은 각 모듈의 manifest가 소유한다. Cube가 생성한 C나
# makefile을 고치지 않고 상대 include/source 선택만 마지막에 복원한다.
$serviceSync = Join-Path $projectDir 'tools/services_build.py'
if (Test-Path -LiteralPath $serviceSync) {
    $servicePython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (-not (Test-Path -LiteralPath $servicePython)) { $servicePython = (Get-Command python -ErrorAction Stop).Source }
    & $servicePython $serviceSync
    if ($LASTEXITCODE -ne 0) { throw 'Service source selection failed.' }
}
