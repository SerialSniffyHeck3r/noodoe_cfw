# 앱 로직

제품이 **무엇을 할지** 결정하는 코드만 이 디렉터리에 둔다. BSP나 통신 라이브러리를 여기로 옮기지 않는다.

| 디렉터리 | 책임 |
| --- | --- |
| `UI/inc`, `UI/src` | 전체/중앙 상태 머신, 버튼 의미 변환, 데이터 표시 모델, 서비스와 화면 연결 |
| `Runtime` | 태스크 구성, 장치 서비스 실행 조정, 연결/소스 정책, 업데이트 허가 연결 |
| `Control` | 휴대전화 NDCP 명령의 제품 동작·권한·응답 처리 |
| `Settings` | 제품 설정/BT 등록 키/사용 시간 레코드와 저장 정책 |
| `Vehicle` | IGN ON 사용 시간 누적과 오일 사용량 정책 |

화면 전이 계약은 [STATE_MACHINE.md](STATE_MACHINE.md)에 있다. `UI/src/product_input.c`는 가운데 **ENTER=PA15**를 포함한 기존 BSP 버튼 번호를 UI 이벤트로 변환한다. PRESS/RELEASE/SHORT를 중복 소비하지 않으며, 짧게 눌렀다 떼면 중앙 카드만 한 단계 바뀐다. 전원·경고·메뉴·장시간 누름의 우선순위는 상태 머신이 소유한다.

`Graphics/UI`에는 LVGL 표시기, 좌표, 글꼴·아이콘·표시 컴포넌트와 기존 그래픽 시험이 남는다. `Middlewares/Noodoe`는 통신·파싱·저장·업데이트 장치 서비스, `Middlewares/Third_Party`는 고정된 외부 원본, `Drivers/BSP`는 보드 하드웨어를 소유한다.

Cube 생성 `Core` 코드는 옮기지 않는다. 기존 `LCDTest()` 진입을 보존하고 `StartDefaultTask`에는 여전히 `LCDTest();`만 둔다. 모든 사용자 include는 구성 디렉터리 기준 상대 경로다. `tools/sync_project.ps1` → `services_build.py`가 App_Logic 루트와 기능별 include를 Debug/Release에 복원한다. Product에서만 `App_Logic/UI`를 컴파일하고 Integrated/Graphics는 기존 시험을 선택한다. 나머지 앱 실행 조정은 기존 프로필과 같은 조건으로 유지한다.
