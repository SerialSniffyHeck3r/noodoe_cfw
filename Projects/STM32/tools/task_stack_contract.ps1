# IOC에서 읽은 CMSIS-RTOS task stack 계약을 생성 C의 정확한 속성 한 곳과
# 대조한다. 이 파일 자체는 읽기/쓰기를 하지 않는다. sync와 check, fixture가
# 같은 parser를 공유하여 한쪽만 4KiB를 허용하는 불일치를 만들지 않는다.
function Get-NoodoeDefaultTaskStackContract {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string]$IocText,
          [Parameter(Mandatory=$true)][string]$FreeRTOSText)

    $taskLines = [regex]::Matches($IocText, '(?m)^FREERTOS\.Tasks01=([^\r\n]*)\r?$')
    if ($taskLines.Count -ne 1) { throw 'Expected exactly one IOC FREERTOS.Tasks01 property.' }
    $task = [regex]::Match($taskLines[0].Groups[1].Value,
        '^defaultTask,24,(?<words>[0-9]+),StartDefaultTask,As weak,NULL,Dynamic,NULL,NULL$')
    if (-not $task.Success) { throw 'IOC defaultTask name/priority/entry/weak/dynamic contract changed.' }
    # IOC 스택 단위는 StackType_t word(4 bytes), osThreadAttr_t.stack_size는 byte다.
    # 실물에서 1024 words가 넘쳤다. menu의 객체 8단 재귀만 약4992 bytes이고
    # text/draw/log 호출 여유도 필요하므로 3072 words(12KiB)를 요구한다.
    $stackWords = [int]::Parse($task.Groups['words'].Value)
    if ($stackWords -ne 3072) { throw 'IOC defaultTask must use 3072 words (12288 bytes) for the graphics task.' }

    # 주석의 예제 코드가 실제 속성 선언으로 잡히지 않도록 주석만 같은 길이의
    # 공백으로 가린다. 문자열은 그대로 두며 원본 byte 위치를 끝까지 유지한다.
    $tokens = '/\*[\s\S]*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*'''
    $code = [regex]::Replace($FreeRTOSText, $tokens, [System.Text.RegularExpressions.MatchEvaluator]{
        param($match)
        if ($match.Value.StartsWith('/*') -or $match.Value.StartsWith('//')) {
            return [regex]::Replace($match.Value, '[^\r\n]', ' ')
        }
        return $match.Value
    })
    $attributes = [regex]::Matches($code,
        '(?m)^const[ \t]+osThreadAttr_t[ \t]+defaultTask_attributes[ \t]*=[ \t]*\{(?<body>[^{}]*)\}[ \t]*;')
    if ($attributes.Count -ne 1) { throw 'Expected one generated defaultTask_attributes initializer; refusing broad replacement.' }
    $body = $attributes[0].Groups['body']
    if ($body.Value -notmatch '(?m)^[ \t]*\.name[ \t]*=[ \t]*"defaultTask"[ \t]*,') {
        throw 'defaultTask_attributes must describe defaultTask.'
    }
    $stackFields = [regex]::Matches($body.Value,
        '(?m)^[ \t]*\.stack_size[ \t]*=[ \t]*(?<words>[0-9]+)[ \t]*\*[ \t]*4[ \t]*,[ \t]*\r?$')
    if ($stackFields.Count -ne 1 -or [regex]::Matches($body.Value, '\.stack_size\b').Count -ne 1) {
        throw 'Expected one generated .stack_size = <words> * 4 field; unsupported form will not be rewritten.'
    }
    $wordGroup = $stackFields[0].Groups['words']
    $offset = $body.Index + $wordGroup.Index
    $newText = $FreeRTOSText.Substring(0, $offset) + [string]$stackWords +
               $FreeRTOSText.Substring($offset + $wordGroup.Length)
    return [pscustomobject]@{
        StackWords = $stackWords
        StackBytes = $stackWords * 4
        GeneratedWords = [int]::Parse($wordGroup.Value)
        Changed = $newText -cne $FreeRTOSText
        GeneratedText = $newText
    }
}
