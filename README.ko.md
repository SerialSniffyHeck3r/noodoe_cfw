# FuckNudo CFW

**AK 550의 동그란 Noodoe 화면을 다시 쓰기 위한 커스텀 펌웨어.**

Noodoe는 단순한 블루투스 표시기가 아니다. STM32, 그래픽 컨트롤러, 디스플레이, 저장소, 차량 통신을 가진 독립적인 컴퓨터다. 이 프로젝트는 차량과의 신호를 관찰하고 순정 펌웨어의 동작을 분석한 다음 하드웨어를 하나씩 브링업해 만든 대체 펌웨어와 안드로이드 컴패니언 앱이다. KYMCO와 무관한 독립 연구 프로젝트다.

![속도 링과 트립 컴퓨터](Projects/documentations/manual/images/trip-a.png)

원형 표시 영역의 바깥 링에는 속도를, 안쪽에는 트립 컴퓨터·휴대전화 알림·음악과 앨범아트·전화·휴대전화 GPS 궤적을 표시한다. 순정 3버튼, 시동 전환, 저연료 경고, 사진 배경, 라이트/다크 테마와 영구 설정을 지원한다. 폰 연동은 Classic Bluetooth SPP를 사용하고, 속도·연료·ODO는 폰 GPS가 아니라 차량 계기판과 Noodoe 사이 UART에서 받는다.

설치 구조도 이 프로젝트의 중요한 부분이다. 순정 업데이터를 통해 Bootstrap을 실행하고, 외장 NOR의 순정 FAT 빈 공간에 CFW 전용 파일을 준비한다. 독립 RecoveryGate는 메인 펌웨어나 블루투스가 고장 나도 물리 버튼으로 보관된 순정 앱을 복원할 수 있게 설계했다. 이후의 일반 업데이트는 가능한 한 Product 앱만 교체하며, 새 버전이 확인되기 전에는 이전 정상본을 보존한다. 순정 부트로더와 공장 식별 영역을 교체하는 방식이 아니다.

개발용 보드와 AK 550에서 동작한 이력이 있지만 아직 진행 중인 하드웨어 프로젝트다. 벤치 모듈의 블루투스·조도 경로는 손상되어 모든 기판의 무선 및 자동 밝기를 검증할 수 없다. 실차 설치는 정확한 보드·부트로더·순정 복구본에 의존하므로 [설치·복구 문서](Projects/documentations/installer.md)를 먼저 확인해야 한다.

## 문서

| 주제 | 문서 |
|---|---|
| 실물 하드웨어 | [hardware.md](Projects/documentations/hardware.md) |
| 순정 동작 분석 | [reverse-engineering.md](Projects/documentations/reverse-engineering.md) |
| CFW 구조 | [architecture.md](Projects/documentations/architecture.md) |
| 메모리와 자산 | [memory-map.md](Projects/documentations/memory-map.md) |
| 설치·롤백·복구 | [installer.md](Projects/documentations/installer.md) |
| 버튼·차량 데이터 | [vehicle-interface.md](Projects/documentations/vehicle-interface.md) |
| 사용 설명서 | [페이지별 설명서](Projects/documentations/manual/README.md) |

[Android 소스](Projects/Android)와 [STM32CubeIDE 소스](Projects/STM32)를 각각 보관한다. 연구 기록은 [문서 목록](Projects/documentations/README.md)에 모았다. 빌드 산출물은 소스 트리에 넣지 않고 [Releases](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)에서 배포한다. 소스 트리에는 순정 원본 덤프·개별 기기 백업·공장 비밀값을 넣지 않는다. 설치용 릴리스에는 검증된 순정 복구 payload가 포함될 수 있다.

[English](README.md) · [최신 릴리스](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)
