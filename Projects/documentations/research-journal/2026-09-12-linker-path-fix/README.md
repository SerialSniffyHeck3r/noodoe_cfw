# CubeIDE의 linker-script-missing 수정

사용자 GUI 빌드의 Debug makefile에는 ELF 의존성으로 `fail-specified-linker-script-missing`이 붙고 실제 링크 명령의 `-T`가 빠져 있었다. `Linker/Noodoe_APP.ld` 파일 자체는 존재했다. 수정 전 [makefile](failing-debug.makefile)과 [설정](before.cproject)을 보존했다.

기존 `${workspace_loc:/${ProjName}/Linker/Noodoe_APP.ld}`가 해당 GUI 빌드 문맥에서 빈 값으로 해석된 것이 원인이다. 설치된 ST MakefileGenerator의 addRuleForTool 분기에서도, 비어 있지 않은 옵션을 resolveValueToMakefileFormat으로 해석한 결과가 빈 문자열이면 실패 타깃을 추가함을 확인했다. 그 분기는 File.exists 검사가 아니다. 별도 headless workspace에서만 경로가 해석됐다는 사실로 GUI 동작을 보장할 수 없었다.

## 수정

- `.cproject`의 Debug/Release 링커를 `../Linker/Noodoe_APP.ld` 리터럴 상대 경로로 통일했다. Debug/Release make의 실행 디렉터리에서 같은 실제 파일을 가리킨다.
- `sync_project.ps1`의 복원값과 `check_project.ps1`의 계약을 같은 상대 경로로 바꾸고 실제 파일 존재도 확인한다.
- Windows PowerShell 5.1의 직접 `-File` 호출에서는 param 기본식 평가 중 PSScriptRoot가 비는 현상이 있어, 기본 프로젝트 디렉터리 계산을 본문으로 옮겼다.
- 생성 makefile을 수동 편집하지 않고 실제 CubeIDE managed builder로 다시 생성했다.

## 검증

[Debug 빌드](debug-build.log), [Release 빌드](release-build.log) 모두 0 errors/0 warnings. 두 makefile의 ELF 의존성에서 실패 타깃이 빠지고 `-T"../Linker/Noodoe_APP.ld"`와 실제 파일 의존성이 생성됐다. 실패 타깃의 일반 정의/.PHONY는 정상 생성물에도 남으므로 단순 문자열 존재로 실패를 판정하지 않는다.

실제 ELF/APP 검증도 통과했으며 APP 시작 주소는0x08010000이다. Debug49,148bytes, Release28,572bytes이고 BIN 해시는 이전 버튼 펌웨어와 동일하다. `sync_project.ps1` 직접 실행과 멱등성, `check_project.ps1`의 Windows PowerShell 실행도 통과했다. [결과](result.json).

GUI 자동화와 장치 접근은 수행하지 않았다. 열린 IDE가 이전 설정을 캐시하면 사용자가 프로젝트 Close Project→Open Project 후 Project→Clean과 빌드를 수행해 새 `.cproject`를 다시 로드한다.
