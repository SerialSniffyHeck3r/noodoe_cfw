[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$buildPolicy = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build_profile.json') -Raw | ConvertFrom-Json
$profileLinker = if ($buildPolicy.profile -eq 'Product') { '../Linker/Noodoe_Product.ld' } else { '../Linker/Noodoe_APP.ld' }
$lvglBuild = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'lvgl_build.json') -Raw | ConvertFrom-Json
$lvglRoot = Join-Path $projectDir 'Middlewares/Third_Party/LVGL'
$lvglUpstream = Get-Content -LiteralPath (Join-Path $lvglRoot 'UPSTREAM.json') -Raw | ConvertFrom-Json
if ($lvglUpstream.version -cne $lvglBuild.lvgl_version -or
    $lvglUpstream.commit -cne $lvglBuild.lvgl_commit) {
    throw 'LVGL provenance and source selection do not refer to the same pinned release.'
}
# 파일 목록은 버전 갱신 시 다시 검토한다. 무관한 플랫폼 backend를 빌드에
# 포함하지 않으면서 EVE 명령 라이브러리/폰트 같은 필요한 원본이 빠지는 것도 막는다.
foreach ($relativeSource in @($lvglBuild.selected_runtime_translation_units)) {
    $sourcePath = Join-Path (Join-Path $projectDir $lvglBuild.source_root) $relativeSource
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Pinned LVGL runtime source is missing: $relativeSource"
    }
}

# Cube 재생성 결과를 조용히 덮어쓰지 않는다. 설정/USER CODE가 손실되면 빌드를
# 장치 작업 전에 중단하여 원인을 표시한다. 검증은 실제 ELF 검사와 함께 사용한다.
[xml]$metadata = Get-Content -LiteralPath (Join-Path $projectDir '.cproject') -Raw
foreach ($configuration in @('Debug', 'Release')) {
    $config = $metadata.SelectSingleNode("//configuration[@name='$configuration']")
    if ($null -eq $config) { throw "Missing $configuration configuration" }
    $linker = $config.SelectSingleNode('.//option[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.script"]')
    if ($null -eq $linker -or $linker.value -cne $profileLinker) {
        throw "$configuration must select $profileLinker; Cube regeneration may have replaced it."
    }
    $captureGuard = $config.SelectSingleNode('.//tool[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker"]/option/listOptionValue[@value="-Wl,--wrap=lv_draw_eve_ramg_get_addr"]')
    if ($null -eq $captureGuard) {
        throw "$configuration is missing the snapshot RAM_G allocation guard; run tools/sync_project.ps1."
    }
    $resolvedLinker = [IO.Path]::GetFullPath((Join-Path (Join-Path $projectDir $configuration) $linker.value))
    if (-not (Test-Path -LiteralPath $resolvedLinker -PathType Leaf)) {
        throw "$configuration linker script is missing: $resolvedLinker"
    }
    $roots = @($config.SelectNodes('.//sourceEntries/entry') | ForEach-Object { $_.name })
    if ($roots -notcontains 'Drivers' -or $roots -notcontains 'App_Logic' -or $roots -contains 'Services' -or
        $roots -notcontains 'Graphics' -or $roots -notcontains $lvglBuild.source_root) {
        throw "$configuration must compile Drivers, App_Logic, Graphics and the selected LVGL src root."
    }
    $middlewareEntry = $config.SelectSingleNode('./sourceEntries/entry[@name="Middlewares"]')
    if ($null -eq $middlewareEntry -or
        @($middlewareEntry.GetAttribute('excluding').Split('|')) -notcontains $lvglBuild.parent_exclusion) {
        throw "$configuration must exclude the LVGL distribution from the parent Middlewares source root."
    }
    $lvglEntry = $config.SelectSingleNode("./sourceEntries/entry[@name='$($lvglBuild.source_root)']")
    $lvglExclusions = @($lvglEntry.GetAttribute('excluding').Split('|'))
    # Product provides an exact project-owned Montserrat descriptor/metrics TU
    # whose bitmap binds to verified SDRAM. Other profiles use vendor original.
    $expectedLvglExclusions = @($lvglBuild.exclusions_relative_to_source_root)
    if ($buildPolicy.profile -eq 'Product') { $expectedLvglExclusions += 'font/lv_font_montserrat_14.c' }
    if ((@($lvglExclusions | Sort-Object -Unique) -join '|') -cne (@($expectedLvglExclusions | Sort-Object -Unique) -join '|')) {
        throw "$configuration LVGL source exclusions differ from tools/lvgl_build.json."
    }
    $cCompiler = $config.SelectSingleNode('.//tool[@superClass="com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler"]')
    $definedSymbols = @($cCompiler.SelectNodes('./option[@valueType="definedSymbols"]/listOptionValue') | ForEach-Object { $_.value })
    foreach ($symbol in @($lvglBuild.defines)) {
        if ($definedSymbols -notcontains $symbol) { throw "$configuration is missing define $symbol" }
    }
    $includes = $config.SelectNodes('.//option[@valueType="includePath"]')
    foreach ($include in $includes) {
        $values = @($include.listOptionValue | ForEach-Object { $_.value })
        if ($values -notcontains '../Drivers/BSP/inc' -or $values -notcontains '../App_Logic/UI/inc' -or $values -contains '../Services') {
            throw "$configuration has incorrect BSP/App_Logic include paths."
        }
        foreach ($path in @($lvglBuild.include_paths)) {
            if ($values -notcontains $path) { throw "$configuration is missing Graphics/LVGL include path $path" }
            if (-not (Test-Path -LiteralPath (Join-Path (Join-Path $projectDir $configuration) $path) -PathType Container)) {
                throw "$configuration include path does not exist: $path"
            }
        }
    }
}

# 이 장치에는 480x480 CPU framebuffer와 소프트웨어 fallback을 준비하지 않았다.
# LVGL OS=none은 애플리케이션 전체가 bare-metal이라는 뜻이 아니라 LVGL을 한
# FreeRTOS 태스크가 소유한다는 계약이다. allocator는 RTOS와 별도의 정적64KiB다.
$lvglConfig = Get-Content -LiteralPath (Join-Path $projectDir 'Graphics/Port/inc/lv_conf.h') -Raw
$lvglRequired = [ordered]@{
    LV_USE_DRAW_EVE = '1'; LV_USE_DRAW_SW = '0'; LV_DRAW_EVE_EVE_GENERATION = '2'
    LV_DRAW_EVE_WRITE_BUFFER_SIZE = '512'; LV_USE_OS = 'LV_OS_NONE'
    LV_USE_STDLIB_MALLOC = 'LV_STDLIB_BUILTIN'; LV_MEM_SIZE = '(64 * 1024U)'
    LV_FONT_MONTSERRAT_14 = '1'; LV_FONT_MONTSERRAT_20 = '1'
    LV_FONT_MONTSERRAT_28 = '1'; LV_FONT_MONTSERRAT_40 = '1'
    LV_USE_CANVAS = '0'; LV_USE_LOTTIE = '0'; LV_USE_3DTEXTURE = '0'; LV_USE_ARCLABEL = '0'
    LV_USE_THORVG_INTERNAL = '0'; LV_USE_THORVG_EXTERNAL = '0'; LV_USE_SVG = '0'
    LV_USE_LIBPNG = '0'; LV_USE_LIBJPEG_TURBO = '0'; LV_USE_RLOTTIE = '0'
    LV_BUILD_EXAMPLES = '0'; LV_BUILD_DEMOS = '0'
}
foreach ($item in $lvglRequired.GetEnumerator()) {
    $escapedName = [regex]::Escape($item.Key)
    $escapedValue = [regex]::Escape($item.Value)
    if ($lvglConfig -notmatch "(?m)^\s*#define\s+$escapedName\s+$escapedValue(?:\s|/|$)") {
        throw "LVGL configuration contract changed: $($item.Key) must be $($item.Value)."
    }
}
if ($lvglConfig -notmatch '(?m)^\s*#define\s+LV_ASSERT_HANDLER\s+Graphics_AssertFail\(__FILE__, __LINE__\);\s*$') {
    throw 'LVGL asserts must reach Graphics_AssertFail for board diagnostics.'
}
if ($lvglConfig -notmatch 'extern\s+void\s+Graphics_AssertFail\(const char \*file, uint32_t line\)\s+__attribute__\(\(noreturn\)\)\s*;') {
    throw 'LVGL must know Graphics_AssertFail never returns from invalid/unsupported rendering paths.'
}

# 코드 생성으로 중요한 호출이 사라지면 감지한다. 설정을 추측해 재작성하지 않는다.
# 각 블록 이름은 현재 CubeF4 generator의 USER CODE 마커와 일치해야 한다.
$hooks = @(
    @('Core/Src/main.c', '1', 'BSP_BootRuntimeReady\(\)'),
    @('Core/Src/main.c', 'Init', 'BSP_BringupMark\(2U\)'),
    @('Core/Src/main.c', 'SysInit', 'BSP_BringupMark\(3U\)'),
    @('Core/Src/main.c', '2', 'BSP_BringupMark\(4U\)'),
    @('Core/Src/main.c', 'Callback 0', 'BSP_BringupHalTick\(\)'),
    @('Core/Src/freertos.c', 'RTOS_THREADS', 'BSP_BringupMark\(5U\)'),
    @('Core/Src/main.c', '4', 'LCDTest\(\)'),
    @('Core/Src/main.c', '4', 'BSP_Buttons_IRQHandler\(GPIO_Pin\)'),
    @('Core/Src/freertos.c', 'FunctionPrototypes', '__weak\s+void\s+StartDefaultTask\s*\(\s*void\s*\*\s*argument\s*\)\s*;'),
    @('Core/Src/stm32f4xx_it.c', 'HardFault_IRQn 0', 'BSP_FaultRecord\(3U\)')
)
foreach ($hook in $hooks) {
    $source = Get-Content -LiteralPath (Join-Path $projectDir $hook[0]) -Raw
    $marker = [regex]::Escape($hook[1])
    $block = [regex]::Match($source, "(?s)/\* USER CODE BEGIN $marker \*/(.*?)/\* USER CODE END $marker \*/")
    if (-not $block.Success -or $block.Groups[1].Value -notmatch $hook[2]) {
        throw "Required BSP hook missing from $($hook[0]) USER CODE $($hook[1])."
    }
}
$ioc = Get-Content -LiteralPath (Join-Path $projectDir 'FuckNudo_Noodoe_CFW_Project.ioc') -Raw
if ($ioc -notmatch '(?m)^ProjectManager.KeepUserCode=true\r?$') {
    throw 'CubeMX KeepUserCode must remain true.'
}
# The three stock buttons are PD12/PA15/PI6, not the nearby PI3/PI4/PI5
# interrupt inputs. Both edges are already generated; keep that contract so
# regeneration cannot silently disconnect release notifications from the BSP.
foreach ($pin in @('PD12', 'PA15', 'PI6')) {
    if ($ioc -notmatch "(?m)^$pin\.GPIO_ModeDefaultEXTI=GPIO_MODE_IT_RISING_FALLING\r?$" -or
        $ioc -notmatch "(?m)^$pin\.GPIO_PuPd=GPIO_NOPULL\r?$") {
        throw "Stock button $pin must keep both-edge EXTI with no internal pull."
    }
}
$irqSource = Get-Content -LiteralPath (Join-Path $projectDir 'Core/Src/stm32f4xx_it.c') -Raw
foreach ($pin in @('PD12', 'PA15', 'PI6')) {
    if ($irqSource -notmatch "HAL_GPIO_EXTI_IRQHandler\(STOCK_${pin}_EXTI_Pin\)") {
        throw "Generated EXTI dispatch for $pin is missing."
    }
}
# 기본 태스크는 main USER 4의 strong 함수로 연결하고 생성 freertos 함수는 weak
# fallback으로 남긴다. 생성 전/후 모두 같은 연결을 갖도록 IOC와 USER 선언을 검사한다.
# LCDTest는 내부에서 RTOS 루프를 유지하는 시험 진입이며 task 본문에는 호출 하나만 둔다.
. (Join-Path $PSScriptRoot 'task_stack_contract.ps1')
$generatedFreeRTOS = Get-Content -LiteralPath (Join-Path $projectDir 'Core/Src/freertos.c') -Raw
$mallocUser = [regex]::Match($generatedFreeRTOS, '(?s)/\* USER CODE BEGIN 5 \*/(.*?)/\* USER CODE END 5 \*/').Groups[1].Value
if ($mallocUser -notmatch 'BSP_FaultNotifyMemory\s*\(\s*\)') {
    throw 'The USER CODE recoverable malloc-failure hook is missing.'
}
$stackContract = Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText $generatedFreeRTOS
if ($stackContract.Changed) {
    throw 'Generated defaultTask stack differs from IOC 3072 words / 12288 bytes; run tools/sync_project.ps1 before building.'
}
# task 스택만 키우고 FreeRTOS heap 및 별도 LVGL heap은 각각 64KiB를 유지한다.
if ($ioc -notmatch '(?m)^FREERTOS\.configTOTAL_HEAP_SIZE=65536\r?$') {
    throw 'IOC FreeRTOS heap must remain 65536 bytes.'
}
$rtosConfig = Get-Content -LiteralPath (Join-Path $projectDir 'Core/Inc/FreeRTOSConfig.h') -Raw
# CPU 계측 hook은 Cube USER CODE에 보존한다. 유실되면 idle 시간을0으로 읽어
# 화면에100%를 표시하게 되므로 기능이 빠진 build를 조용히 통과시키지 않는다.
$cpuIncludes = [regex]::Match($rtosConfig, '(?s)/\* USER CODE BEGIN Includes \*/(.*?)/\* USER CODE END Includes \*/').Groups[1].Value
$cpuDefines = [regex]::Match($rtosConfig, '(?s)/\* USER CODE BEGIN Defines \*/(.*?)/\* USER CODE END Defines \*/').Groups[1].Value
if ($cpuIncludes -notmatch '#include\s+"bsp_cpu_load\.h"' -or
    $cpuDefines -notmatch '#define\s+INCLUDE_xTaskGetIdleTaskHandle\s+1\b' -or
    $cpuDefines -notmatch '#define\s+traceTASK_SWITCHED_IN\(\)\s+BSP_CPU_TraceSwitch\(\(void\s*\*\)pxCurrentTCB\)') {
    throw 'CPU load USER CODE hooks are missing from FreeRTOSConfig.h; do not interpret an uninstrumented build as measured CPU usage.'
}
if ($rtosConfig -notmatch '(?m)^\s*#define\s+configTOTAL_HEAP_SIZE\s+\(\(size_t\)65536\)\s*$') {
    throw 'Generated FreeRTOS heap must remain 65536 bytes.'
}
if ($cpuDefines -notmatch '#include\s+"bsp_rtos_profile\.h"') {
    throw 'The USER CODE RTOS profile override is missing; preserve the Integrated48KiB/Graphics64KiB heap contract.'
}
$rtosProfile = Get-Content -LiteralPath (Join-Path $projectDir 'Drivers/BSP/inc/bsp_rtos_profile.h') -Raw
if ($rtosProfile -notmatch 'NOODOE_INTEGRATED' -or $rtosProfile -notmatch 'configTOTAL_HEAP_SIZE\s+\(\(size_t\)49152\)') {
    throw 'Unexpected integrated RTOS heap override.'
}
$mainSource = Get-Content -LiteralPath (Join-Path $projectDir 'Core/Src/main.c') -Raw
$mainUser = [regex]::Match($mainSource, '(?s)/\* USER CODE BEGIN 4 \*/(.*?)/\* USER CODE END 4 \*/').Groups[1].Value
$mainUserCode = [regex]::Replace($mainUser, '(?s)/\*.*?\*/|//[^\r\n]*', '')
$taskBody = [regex]::Match($mainUserCode, '(?s)\bvoid\s+StartDefaultTask\s*\([^{}]*\)\s*\{([^{}]*)\}')
if (-not $taskBody.Success -or $taskBody.Groups[1].Value.Trim() -cne 'LCDTest();' -or
    $mainUserCode -match '__weak\s+void\s+StartDefaultTask') {
    throw 'main.c USER CODE 4 must define strong StartDefaultTask with only LCDTest(); in its body.'
}
# IOC 파일의 저장 성공은 C 코드 재생성 성공을 뜻하지 않는다. 새 주변장치 함수,
# 168MHz PLL 설정과 main 자동호출 정책을 실제 생성본에서 검사한 뒤에만 통과한다.
# build.ps1과 같은 사용자별 bundled Python을 우선 사용하며 전역 PATH는 바꾸지 않는다.
$generationPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $generationPython -PathType Leaf)) {
    $generationPython = (Get-Command python -ErrorAction Stop).Source
}
& $generationPython -B (Join-Path $PSScriptRoot 'check_generated.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Generated code contract failed. Regenerate in STM32CubeIDE and revalidate before any build or board test.'
}
Write-Host 'Project contract verified: APP linker, BSP/Graphics paths, pinned LVGL, USER CODE hooks and generated peripheral/clock contract.'

# Inspect every Debug compiler/linker scope, including inherited overrides.
$policyPython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
if (-not (Test-Path -LiteralPath $policyPython)) { $policyPython = (Get-Command python -ErrorAction Stop).Source }
& $policyPython (Join-Path $PSScriptRoot 'build_optimization.py')
if ($LASTEXITCODE -ne 0) { throw 'Debug optimization policy failed.' }
& $policyPython (Join-Path $PSScriptRoot 'eve_arc_port.py')
if ($LASTEXITCODE -ne 0) { throw 'Pinned EVE arc port verification failed.' }
