# 사선 경계·가운데 버튼·앱 계층 정리

2026-09-13 최종 사용자 정정을 반영한 실제 설치/화면 검증이다.

- 시계 baseline77→71: 실제 캡처 글자 위/아래 모두6px 상승.
- 시계 구분선 `(127,51)-(172,87)-(308,87)-(353,51)`: 기존24:19 기울기를 정수 픽셀 오차 이내로 위로 연장해 속도 링 안쪽에서 끝난다. 선 두께3px 포함 반경221.704, 링 안쪽 반경222.
- 거리 위 구분선 `(87,424)-(116,401)-(364,401)-(392,424)`: 기존 사선을 아래로 연장해 속도 호가 없는 하단 원 경계에 닿는다. 기존 stencil이 끝부분을 자른다. 수평 덧붙임은 없다.
- 거리 아래 직선, 숫자·단위·폰트·중앙336×271·오일 사용 시간/잔량선은 유지한다.
- 가운데 ENTER=PA15 매핑은 유지한다. `ProductInput_Dispatch`가 BSP 이벤트를 명시적으로 변환한다. 실제 Graphics 입력 소비기→앱 bridge→상태 머신의 ARM 통합 시험은 짧은 누름 한 번 한 카드, RELEASE/SHORT 중복 방지, 고장난 UP의 독립성, OBD 미연결 건너뜀, 장시간 입력을 확인했다. 이번에 사용자의 물리 버튼 누름을 다시 요청해 검증한 것은 아니다.

## 실제 화면

![최종 EVE 출력](final/display.png)

`before`는 원래 화면, `after`는 사용자가 거부한 수평 연장 중간 결과다. **최종 결과는 `final`만 사용한다.** LCD 사진이 아니라 실행 중 EVE CMD_SNAPSHOT2 출력이다. `pixels.json`에서 시계6px 상승, 구분선3px, 속도 링 보호 영역 변경0픽셀, 링 침범0픽셀을 검사했다.

## 코드 계층

- `App_Logic/UI`: 상태 머신·입력·프레젠터·제품 표시 모델·서비스/표시기 연결.
- `App_Logic/Runtime`, `Control`, `Settings`, `Vehicle`: 실행 조정·폰 커맨드 정책·제품 설정·IGN/오일 사용 시간.
- `Middlewares/Noodoe`: 통신·파싱·저장·업데이트 서비스. `Middlewares/Third_Party` 원본은 이동/수정하지 않았다.
- `Graphics`: 렌더러·좌표·폰트·아이콘·기존 그래픽 시험. `Drivers/BSP`: 하드웨어.

145개 파일을 이동했고 이동 대상 C/헤더80개의 내용 해시가 동일하다. `relocation.json`, `layer-audit.json` 참조. 기존 Services는 제거했다. Cube 재생성으로 App_Logic 소스/include·APP linker가 유실된 격리 fixture에서 실제 동기화 스크립트의 복원·반복 멱등성과3개 프로필을 검사했다. Core/IOC byte 보존을 확인했으며 GUI Generate Code를 직접 실행한 시험은 아니다.

## 빌드/실기 검증

- 최종 Release APP: 381616bytes, SHA256 `f3dc89ce55090ea0f5720ef7e9f82949f08d5d152ed13fd027b7024a6254b1de`.
- 최종 Debug APP: 439248bytes. 두 구성 모두0errors. 기존 RWX LOAD segment 링크 경고1건은 남아 있다.
- `install-final`의 첫 검증은 실패했다. 두 번의 전체 읽기로 실제 APP 불일치를 확인했고 하위64KiB는 정상이었다. 원인은 확정하지 않았다. 실패 로그와 실제 읽기 파일은 `verify-mismatch`에 보존한다.
- `install-retry`: 동일 APP을0x08010000에 재설치, 전체 APP readback 일치, 하위64KiB 순정 BL/config 보존, 두 번 정상 부팅과 HAL/RTOS 진행을 검증한다. 마지막에 최초 debug freeze 값으로 복원한다.
- UI 상태 시험 O0/Os 각각110603개 상태 assertion+416개 표시 모델 assertion.
- Graphics 입력 통합 O0/O2 각각71assertions,11cases.
- 설정/오일 시간 O0/Os 각각630checks, 폰 제어 O0/Os 각각41checks.
- 폰2대 동시 세션 구현과 OBD 중앙 카드 코드/시험은 이전 작업에서 유지했다. 손상된 donor BT의 실제 무선 연결 확인으로 해석하지 않는다.
