[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
. (Join-Path $projectDir 'tools/task_stack_contract.ps1')
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)
$ioc = $utf8.GetString([IO.File]::ReadAllBytes((Join-Path $projectDir 'FuckNudo_Noodoe_CFW_Project.ioc')))
$core = $utf8.GetString([IO.File]::ReadAllBytes((Join-Path $projectDir 'Core/Src/freertos.c')))
$old = $core.Replace('.stack_size = 3072 * 4,', '.stack_size = 1024 * 4,')
if ($old -notmatch '\.stack_size = 1024 \* 4,') { throw 'Fixture needs the current generated stack field.' }
$script:checks = 0
function Check([bool]$Condition, [string]$Message) {
    $script:checks += 1
    if (-not $Condition) { throw $Message }
}
function Reject([scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Check $rejected $Message
}

# USER CODE와 무관한 두 번째 task 속성까지 포함시켜 broad replace를 검출한다.
$other = "`r`nconst osThreadAttr_t unrelated_attributes = {`r`n  .stack_size = 1024 * 4,`r`n};`r`n"
$inputText = $old + $other
$expected = $old.Replace('.stack_size = 1024 * 4,', '.stack_size = 3072 * 4,') + $other
$contract = Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText $inputText
Check ($contract.StackWords -eq 3072 -and $contract.StackBytes -eq 12288) 'IOC word/byte conversion differs.'
Check ($contract.Changed -and $contract.GeneratedText -ceq $expected) 'Only the default task stack digits may change.'
$again = Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText $contract.GeneratedText
Check (-not $again.Changed -and $again.GeneratedText -ceq $expected) 'Second parser pass is not idempotent.'

# BOM과 LF를 포함한 UTF-8 왕복도 byte 단위로 그대로 남아야 한다.
$bomInput = [char]0xFEFF + $inputText.Replace("`r`n", "`n")
$bomExpected = [char]0xFEFF + $expected.Replace("`r`n", "`n")
$bom = Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText $bomInput
Check ([Convert]::ToBase64String($utf8.GetBytes($bom.GeneratedText)) -ceq
       [Convert]::ToBase64String($utf8.GetBytes($bomExpected))) 'BOM/LF byte preservation failed.'

$taskLine = [regex]::Match($ioc, '(?m)^FREERTOS\.Tasks01=[^\r\n]*').Value
Reject { Get-NoodoeDefaultTaskStackContract -IocText ($ioc + "`n" + $taskLine) -FreeRTOSText $old } 'Duplicate IOC task accepted.'
Reject { Get-NoodoeDefaultTaskStackContract -IocText ($ioc.Replace('defaultTask,24,3072,', 'defaultTask,24,1024,')) -FreeRTOSText $old } 'Old IOC stack accepted.'
Reject { Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText ($old + $old) } 'Duplicate target initializer accepted.'
Reject { Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText ($old.Replace('.stack_size = 1024 * 4,', '.stack_size = TASK_STACK_BYTES,')) } 'Unknown generated expression rewritten.'
Reject { Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText ($old.Replace('.stack_size = 1024 * 4,', ".stack_size = 1024 * 4,`r`n  .stack_size = 512 * 4,")) } 'Duplicate stack field accepted.'
$fakeComment = "/*`r`nconst osThreadAttr_t defaultTask_attributes = {`r`n  .name = `"defaultTask`",`r`n  .stack_size = 1024 * 4,`r`n};`r`n*/`r`n"
$commentResult = Get-NoodoeDefaultTaskStackContract -IocText $ioc -FreeRTOSText ($fakeComment + $old)
Check ($commentResult.GeneratedText -ceq ($fakeComment + $old.Replace('.stack_size = 1024 * 4,', '.stack_size = 3072 * 4,'))) 'Comment example was modified.'

# 전체 sync 실행도 작은 별도 프로젝트 fixture에서만 한다. 실제 프로젝트나
# 생성 GUI를 조작하지 않고 재생성된 1024-word C가 복원되는지 검증한다.
$fixture = Join-Path $PSScriptRoot ('fixture-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$copies = @('.cproject', 'Linker/Noodoe_APP.ld', 'Graphics/Port/inc/lv_conf.h',
            'tools/lvgl_build.json', 'Middlewares/Third_Party/LVGL/UPSTREAM.json')
foreach ($relative in $copies) {
    $destination = Join-Path $fixture $relative
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
    [IO.File]::Copy((Join-Path $projectDir $relative), $destination)
}
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'Drivers/BSP/inc'))
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'Core/Src'))
$fixtureIoc = Join-Path $fixture 'FuckNudo_Noodoe_CFW_Project.ioc'
$fixtureCore = Join-Path $fixture 'Core/Src/freertos.c'
[IO.File]::WriteAllBytes($fixtureIoc, $utf8.GetBytes($ioc))
[IO.File]::WriteAllBytes($fixtureCore, $utf8.GetBytes($inputText))
& (Join-Path $projectDir 'tools/sync_project.ps1') -ProjectDirectory $fixture
$after = [IO.File]::ReadAllBytes($fixtureCore)
Check ([Convert]::ToBase64String($after) -ceq [Convert]::ToBase64String($utf8.GetBytes($expected))) 'Sync changed unexpected Core bytes.'
$writeTime = [IO.File]::GetLastWriteTimeUtc($fixtureCore)
& (Join-Path $projectDir 'tools/sync_project.ps1') -ProjectDirectory $fixture
Check ([IO.File]::GetLastWriteTimeUtc($fixtureCore) -eq $writeTime) 'Idempotent sync still wrote freertos.c.'
Check ($utf8.GetString([IO.File]::ReadAllBytes($fixtureIoc)) -ceq $ioc) 'Sync modified IOC.'
$result = [ordered]@{ passed=$true; checks=$script:checks; fixture=$fixture; hardware_access=$false; gui_generation=$false }
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'result.json') -Encoding UTF8
$result | ConvertTo-Json
