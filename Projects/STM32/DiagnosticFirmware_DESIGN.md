# 독립 Diagnostic 펌웨어

2026-09-23 구현. 사용자의 최신 확정에 따라 **진단 종료는 순정 복귀**다.
이 문서는 과거의 자동 CFW 복귀 제안을 대체한다. 현재 검증 결과는
`../../analysis/2026-09-23-driving-expansion/WORKLOG.md` 및 최종 보고를 따른다.
빌드 성공과 ARM 모의시험은 실제 무선·전원 차단 검증이 아니다.

## 분리와 경계

- `Diagnostic/`와 `tools/diagnostic_build.py`: 같은 Cube 생성 코드 및 공용
  BT/HCI/SPP, NOR, 버튼, 조도 서비스, 워치독을 링크하는 독립 빌드.
- Product 실행 구간 `0x08020000..0x0807FFFF`, 최대 384KiB. Gate·BL·공장 구간에
  실행 코드를 쓰지 않는다. UI는 EVE ROM과 기본 도형이며 LVGL/외장 폰트 불필요.
- Product는 정상 BT/조도 드라이버와 읽기 전용 상태 표시를 유지한다.
  raw HCI 리셋과 조도 bit-bang ID/주소 시험만 Diagnostic으로 분리한다.
- 진단에서 설정/사진 초기화를 실행하지 않는다. 기존 페어링 필드만 공용 codec으로
  읽고 필요할 때 저장하며, 진단 이벤트는 기존 CFWLOG의 동일 저널을 사용한다.

## 설치와 독립 복구

1. Product 공용 NDCP 수신 경로의 target4로 384KiB를 비활성 A/B에 기록한다.
   섹터 체크포인트·물리 readback·CRC/SHA·중단 재개는 일반 업데이트와 동일하다.
2. Gate의 고정 feature descriptor `0x08010200`에서 typed Diagnostic 지원을 확인한다.
   구 Gate에는 쓰기를 시작하지 않는다. 기존 순정→Bootstrap→새 Gate+CFW로 이관한다.
3. 이미지 role3, 외장 자산 요구0, container kind1 및 `GATE_F_DIAGNOSTIC`을 함께 검증한다.
   Product role2로 위장하거나 정상 trial/confirmed 상태로 바꾸지 않는다.
4. Gate는 검증한 진단 이미지를 설치하고 한 번 실행한다. 복사 중 중단은 기존 완료
   저널에 따라 재개한다. 진단 실행 뒤 리셋·폴트·워치독은 Gate 복구 화면에 머문다.
   자동 순정 설치·무한 진단 재시작은 하지 않는다. 이전 Product와 자산은 보존한다.
5. 본체 `Back to stock`에서 O를 새로 2초 눌러 승인하거나 기존 긴급 키+O 제스처를
   사용한다. StorageTask의 진행 중 쓰기를 정리한 뒤 Gate가 CFWREC를 재검증하고
   순정 BL 경로로 복원한다. BT/폰/PH9와 독립이다.
6. 폰의 종료 버튼은 본체 승인 화면만 연다. 순정의 실제 식별 응답을 확인하기 전에는
   복귀 완료라고 표시하지 않는다. 진단에는 일반 Product 30초 확정 정책이 없다.

## NDCP 인터페이스

- `0x58`: 기존 88바이트 식별 ABI, role3와 실제 384KiB 이미지 SHA.
- Product `0x96`, 빈 payload: schema1/feature flags. bit0은 typed Diagnostic.
- Diagnostic `0x96`, 빈 payload: 76바이트 결과. schema/role/선택항목/요청ID/결과,
  복원상태, BT/조도/저장소/로그 상태와 연결 epoch.
- Diagnostic `0x96`, epoch+action: 고정 진단 1..6 및 본체 순정 승인 화면7.
  같은 epoch/sequence의 중복 실행을 억제한다. 임의 레지스터·NOR 쓰기 없음.
- `0x92`: 기존 공용 RadioSelfTest, 실제 연결·전송 시험. 가짜 RF 성공 없음.
- `0x97`: offset/bytes, 최대960바이트 로그 읽기. 첫 요청 시 완료된 로그를 잠시
  고정하며 순차 조회·15초 만료·연결 종료·복구 시 해제한다. 중단 파일은 partial.
- 요청 접수와 실제 하드웨어 완료를 구분한다. BT/조도 owner의 같은 작업ID가
  완료됐을 때만 최종 결과를 표시한다. 35초 미완료는 오류로 남는다.

## 빌드와 패키지

`tools/build.ps1 -Profile Diagnostic` 또는 `diagnostic_build.py --output ...`.
`diagnostic.bin`은 FF 패딩된384KiB이며 ELF/BIN/role/경계/자산 계약을 패키저에서
검사한다. APK·Product·새 Gate·자산·진단 이미지는 대응 패키지로 배포한다.
Product Debug/Release 메모리 예산과 힙/큐/스택 크기는 낮추지 않는다.

## 검증 상태

실제 ARM codec/copy/update adapter 시험에서 정상 Product, typed Diagnostic,
구 Gate 거부, 잘못된 role/자산 요구 거부, 복사 중단·재개, 이전 슬롯 보존 및
진단에 정상 확정이 적용되지 않는 것을 검사했다. Android의 진단 역할 라우팅,
읽기 조회/본체 승인 유도는 무선 없는 단위시험이다. 실제 무선 설치·진단·순정
왕복과 물리 전원 차단은 별도 시험 결과가 없는 한 완료로 간주하지 않는다.
