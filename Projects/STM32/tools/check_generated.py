"""Cube 생성본이 현재 Noodoe IOC 계약을 반영했는지 읽기 전용으로 검사한다.

호출 시점은 빌드/장치 시험 전이며, 실패하면 Generate Code가 필요함을 표시한다.
IOC 해시나 생성 시각은 비교하지 않는다. GUI의 정상적인 정렬·공백 변경을 허용하고
실제 C 함수, 클록 설정, 자동 초기화 호출을 검사한다. 생성/빌드/장치 명령은 없다.
이 검사는 컴파일러나 전처리기를 대신하지 않으며 통과 후에도 ELF/실행 검증이 필요하다.
"""
from __future__ import annotations

import json
from pathlib import Path
import re
import sys

PROJECT = Path(__file__).resolve().parents[1]
CONTRACT = Path(__file__).with_name('generated_contract.json')
# 문자열·주석 안의 예제 코드가 실제 설정으로 오인되지 않도록 먼저 가린다.
NON_CODE = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
USER_BLOCK = re.compile(
    r'/\* USER CODE BEGIN ([^\r\n*]+?) \*/[\s\S]*?/\* USER CODE END \1 \*/')


def code_only(source: str) -> str:
    """주석/문자열을 같은 길이의 공백으로 바꿔 중괄호와 호출을 안전하게 검색한다.

    입력은 C 소스 전체이고 출력은 검사 전용 문자열이다. 원본 파일에는 쓰지 않으며
    줄바꿈은 유지하여 오류 위치를 사람이 대조할 수 있게 한다.
    """
    return NON_CODE.sub(lambda match: re.sub(r'[^\n]', ' ', match.group()), source)


def function_body(source: str, name: str, returns: str = 'void') -> tuple[str, bool] | None:
    """생성기의 일반 함수 정의에서 본문과 static 여부를 추출한다.

    프로토타입은 제외하며 균형 잡힌 중괄호의 끝까지 읽는다. 주석/문자열은 호출자가
    code_only()로 제거해야 한다. 정의가 없거나 본문이 닫히지 않으면 None으로 실패한다.
    """
    pattern = rf'\b(?P<static>static\s+)?{returns}\s+{re.escape(name)}\s*\(\s*void\s*\)\s*\{{'
    match = re.search(pattern, source)
    if not match:
        return None
    depth = 1
    for offset in range(match.end(), len(source)):
        depth += (source[offset] == '{') - (source[offset] == '}')
        if depth == 0:
            return source[match.end():offset], bool(match.group('static'))
    return None


def normalize_value(value: str) -> str:
    """공백과 정수 리터럴 표현만 정규화한다. C 식 자체를 실행하지 않는다.

    25, 25U, 0x19를 동일하게 비교하되 다른 매크로나 산술식은 자동 추측하지 않는다.
    따라서 Cube 설정이 예상 밖 표현으로 바뀌면 검토를 요구하는 실패가 발생한다.
    """
    value = re.sub(r'\s+', '', value)
    if re.fullmatch(r'(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*', value):
        number = re.sub(r'[uUlL]+$', '', value)
        return str(int(number, 16 if number.lower().startswith('0x') else 10))
    return value


def validate_sources(contract: dict, files: dict[str, str]) -> list[str]:
    """파일 내용만 받아 새 IOC의 생성 계약 위반을 모두 반환한다.

    주변장치 함수는 공개 정의와 헤더 선언이 모두 필요하다. main의 USER CODE는
    의도적인 BSP 제어 영역으로 제외하고, 생성기 자동 MX 호출은 RTOS glue만 허용한다.
    함수는 순수 검사이며 테스트에서 메모리상의 생성본을 넣을 수 있다.
    """
    errors: list[str] = []
    required = {'Core/Src/main.c', 'Core/Inc/stm32f4xx_hal_conf.h'}
    required.update(contract.get('forbidden_generated_symbols', {}))
    for stem in contract['peripheral_files']:
        required.update((f'Core/Src/{stem}.c', f'Core/Inc/{stem}.h'))
    for path in sorted(required - files.keys()):
        errors.append(f'Missing generated file: {path}')

    for path, symbols in contract.get('forbidden_generated_symbols', {}).items():
        code = code_only(files.get(path, ''))
        for symbol in symbols:
            if re.search(rf'\b{re.escape(symbol)}\b', code):
                errors.append(f'{path}: retired USB symbol {symbol}; regenerate from the current IOC')

    for stem, functions in contract['peripheral_files'].items():
        c_path, h_path = f'Core/Src/{stem}.c', f'Core/Inc/{stem}.h'
        for name in functions:
            if c_path in files:
                definition = function_body(code_only(files[c_path]), name)
                if definition is None:
                    errors.append(f'{c_path}: missing definition {name}(void)')
                elif definition[1]:
                    errors.append(f'{c_path}: {name} must be public, not static')
            if h_path in files:
                header = code_only(files[h_path])
                if not re.search(rf'\bvoid\s+{re.escape(name)}\s*\(\s*void\s*\)\s*;', header):
                    errors.append(f'{h_path}: missing public declaration {name}(void)')

    main = files.get('Core/Src/main.c', '')
    generated = code_only(USER_BLOCK.sub('', main))
    body = function_body(generated, 'main', 'int')
    if body is None:
        errors.append('Core/Src/main.c: main(void) body missing or malformed')
    else:
        calls = set(re.findall(r'\b(MX_\w+_Init)\s*\(', body[0]))
        forbidden = calls - set(contract['allowed_main_mx_calls'])
        if forbidden:
            errors.append('main() automatically initializes deferred peripherals outside USER CODE: '
                          + ', '.join(sorted(forbidden)))
        if len(re.findall(r'\bSystemClock_Config\s*\(', body[0])) != 1:
            errors.append('main() must call SystemClock_Config() exactly once outside USER CODE')

    clock = function_body(generated, 'SystemClock_Config')
    if clock is None:
        errors.append('Missing generated SystemClock_Config(void)')
    else:
        for field, allowed in contract['clock']['assignments'].items():
            values = re.findall(rf'\b\w+\.{re.escape(field)}\s*=\s*([^;]+);', clock[0])
            if len(values) != 1 or normalize_value(values[0]) not in allowed:
                errors.append(f'SystemClock_Config: {field} expected {" or ".join(allowed)}, '
                              f'found {values or "missing"}')
        oscillator_types = re.findall(r'\b\w+\.OscillatorType\s*=\s*([^;]+);', clock[0])
        if len(oscillator_types) != 1 or not re.search(r'\bRCC_OSCILLATORTYPE_HSE\b', oscillator_types[0]):
            errors.append('SystemClock_Config: OscillatorType must configure HSE')
        if not re.search(r'__HAL_PWR_VOLTAGESCALING_CONFIG\s*\(\s*PWR_REGULATOR_VOLTAGE_SCALE1\s*\)', clock[0]):
            errors.append('SystemClock_Config: expected voltage Scale 1 for 168 MHz')
        if not re.search(r'HAL_RCC_ClockConfig\s*\(\s*&\w+\s*,\s*FLASH_LATENCY_5\s*\)', clock[0]):
            errors.append('SystemClock_Config: expected FLASH_LATENCY_5 for 168 MHz')

    # HSE_VALUE는 실제 C 생성본의 상수도 맞아야 한다. IOC의 계산 표시만 믿지 않는다.
    hal_config = code_only(files.get('Core/Inc/stm32f4xx_hal_conf.h', ''))
    for module in contract.get('forbidden_hal_modules', []):
        if re.search(rf'^\s*#\s*define\s+{re.escape(module)}\b', hal_config, re.M):
            errors.append(f'stm32f4xx_hal_conf.h: {module} must remain disabled')
    hse = re.findall(r'^\s*#\s*define\s+HSE_VALUE\s+([^\r\n]+)', hal_config, re.M)
    expected_hse = str(contract['clock']['hse_hz'])
    if len(hse) != 1 or not re.search(rf'\b{expected_hse}[uUlL]*\b', hse[0]):
        errors.append(f'stm32f4xx_hal_conf.h: HSE_VALUE must be {expected_hse} Hz')
    return errors


def validate_ioc(contract: dict, source: str) -> list[str]:
    """IOC의 생성 정책을 의미 단위로 검사한다. 순서나 파일 해시는 비교하지 않는다.

    functionlistsort 형식은 rank-function-IP-noGenerateCall-driver-isStatic이다.
    noGenerateCall=true로 자동호출을 막고 isStatic=false로 BSP의 공개 호출을 허용한다.
    함수 목록을 파싱하지 못하면 누락으로 보고하여 GUI 설정 변경을 조용히 허용하지 않는다.
    """
    values = dict(line.split('=', 1) for line in source.splitlines()
                  if '=' in line and not line.lstrip().startswith('#'))
    entries = {}
    for item in values.get('ProjectManager.functionlistsort', '').split(','):
        parts = item.split('-')
        if len(parts) == 6:
            entries[parts[1]] = parts
    errors = []
    for ip in contract.get('forbidden_ips', []):
        if any(k.startswith(ip + '.') or (k.startswith('Mcu.IP') and v == ip)
               for k, v in values.items()):
            errors.append(f'IOC: retired peripheral {ip} must remain disabled')
    for pin in contract.get('analog_pins', []):
        if values.get(pin + '.Signal') != 'GPIO_Analog':
            errors.append(f'IOC: unused {pin} must be GPIO_Analog')
    for functions in contract['peripheral_files'].values():
        for name in functions:
            entry = entries.get(name)
            if entry is None or entry[3] != 'true' or entry[5] != 'false':
                errors.append(f'IOC: {name} requires noGenerateCall=true and isStatic=false '
                              '(public definition, no automatic main call)')
    if values.get('RCC.HSE_VALUE') != str(contract['clock']['hse_hz']):
        errors.append('IOC: expected RCC.HSE_VALUE=25000000')
    if values.get('RCC.SYSCLKFreq_VALUE') != str(contract['clock']['runtime_hz']):
        errors.append('IOC: expected RCC.SYSCLKFreq_VALUE=168000000')
    if values.get('RCC.PLLSourceVirtual') != 'RCC_PLLSOURCE_HSE':
        errors.append('IOC: explicitly select PLLSourceVirtual=RCC_PLLSOURCE_HSE; do not rely on Cube defaults')
    return errors


def main() -> int:
    """필수 텍스트를 읽고 검사 결과만 출력한다. 실패 코드는 상위 PS 빌드를 중단한다.

    이 도구를 실행해도 생성 파일을 고치거나 Cube를 실행하지 않는다. 16MHz 구
    생성본이나 누락된 주변장치 정의는 실패하며 사용자의 GUI 재생성과 재검증을 요구한다.
    """
    try:
        contract = json.loads(CONTRACT.read_text(encoding='utf-8-sig'))
        paths = {'Core/Src/main.c', 'Core/Inc/stm32f4xx_hal_conf.h'}
        paths.update(contract.get('forbidden_generated_symbols', {}))
        for stem in contract['peripheral_files']:
            paths.update((f'Core/Src/{stem}.c', f'Core/Inc/{stem}.h'))
        files = {path: (PROJECT / path).read_text(encoding='utf-8-sig')
                 for path in paths if (PROJECT / path).is_file()}
        errors = validate_sources(contract, files)
        ioc_path = PROJECT / f'{PROJECT.name}.ioc'
        if ioc_path.is_file():
            errors.extend(validate_ioc(contract, ioc_path.read_text(encoding='utf-8-sig')))
        else:
            errors.append(f'Missing IOC: {ioc_path.name}')
        if errors:
            print('FAIL: Cube-generated code is pending regeneration or violates the 168 MHz/deferred-init contract.', file=sys.stderr)
            for error in errors:
                print(f'  - {error}', file=sys.stderr)
            print('Generate Code in STM32CubeIDE, then rerun tools/check_project.ps1. No files were changed.', file=sys.stderr)
            return 1
        print('Generated contract verified: HSE25/PLL168, public peripheral functions, deferred main initialization.')
        return 0
    except (OSError, ValueError, KeyError) as error:
        print(f'FAIL: Cannot inspect generated contract: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
