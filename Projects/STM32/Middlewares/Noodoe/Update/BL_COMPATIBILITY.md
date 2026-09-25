# BL 0.14 / 0.15 호환 경로

2026-09-21 구현. 순정 BL을 다운그레이드하거나 다시 쓰지 않는다.

## 지원 근거와 한계

- BL0.14 / sr0601: 확보한 BL 코드 32KiB의 SHA-256과 대조한다.
- BL0.15 / SR0701: 실차 순정 DeviceInfo 및 OQC에서 확인한 SAA1AA(KR), HW0, 패널4와 공장 필드를 사용한다. **BL0.15 코드 자체는 아직 확보하지 못했다.** 프로필 일치는 해당 코드의 검증이나 제조사 인증을 뜻하지 않는다.
- 순정 V5.16의 APP 바이트는 실차 식별값으로 선택된 공식 OTA와 벤치 원본에서 동일했다. 메타데이터 word0은 BL 버전이며 순정 APP도 보존한다. 0.15의 내부 복사·전원 중단 동작까지 입증한 것은 아니다.
- 패널 selector3/4의 순정 초기화 명령은 동일하다. PCBA 문자열에서 GPIO strap을 추정하지 않는다. PA3/PH3/PH2/PB10을 읽고 현재 구현이 사용하는 revision>=3 경로만 허용한다.

## 설치와 결합

1. Android가 실제 순정 DeviceInfo와 패키지의 모델·HW·BL·PCBA·APP 버전을 대조한다.
2. 일치하면 순정 APP updater로 APP 전용 Bootstrap을 전송할 수 있다. `bootstrap-diagnostics`는 설치 시도 허용이며 no-SWD 실증 인증은 아니다.
3. 암호화된 Bootstrap SPP 연결에서 NDCP 0x60으로 하위64KiB를 두 번 읽는다. 동일성과 BL 코드 해시를 확인하고 앱의 UID별 백업 파일을 fsync/readback 후 게시한다. 하위64KiB에는 개인정보성 공장 데이터가 포함될 수 있으므로 일반 이벤트 로그에 넣지 않는다.
4. `CFWREC.DAT` v2에는 원래 BL 버전, UID, 순정 APP SHA 및 **해당 기기의 BL 코드 SHA**를 기록한다. 물리 BL 코드와 다른 복구 파일은 사용하지 않는다.
5. NOR/FAT 감사·백업·자산·복구 파일 검사와 정상 BT/화면/NOR/RAM 진단을 통과한 뒤 기존 사용자 확인 절차로 Gate+CFW 설치를 허용한다.
6. 이후 패키지가 바뀌어도 동일 UID 백업을 재사용한다. Product에서 원본 백업이 없으면 새로 추정하지 않고 Bootstrap 백업을 요구한다.

기존 v1 복구 파일은 알려진0.14 경로에만 허용한다. 최초 백업의 메타데이터는 이후 정상 업데이트에서 변할 수 있으므로 재접속 때 비교하는 불변 해시는 코드32KiB다. 백업 파일에는 당시 하위64KiB 전체 해시도 기록한다.

## NDCP 0x60 (Bootstrap 전용)

요청: little-endian `offset:u32, count:u32`. offset은 하위64KiB 상대값, count는1..480. 덧셈 overflow를 허용하지 않는다. APP/옵션바이트/임의 주소 접근은 제공하지 않는다.

응답: `status, schema=1, offset, count, BLversion, panel, strap, qualified`의8개u32 뒤 원본 데이터. qualified는 offset0에서만 프로필 확인 결과이며 나머지는0이다. 저장/설치/복구 진행 중에는 BUSY다. 오류 응답은 status만 전송한다. 연결 인증 실패는 DENIED다.

## 메타데이터와 복구 v2

순정 metadata는 `[원래 BLversion, APPversion, 0x7f90, 0x70000, CRC]`다. word0을0.14 상수로 덮어쓰지 않는다. 트랜잭션 시작 후 word0이 달라져도 실패한다. 실제 writer는 SRAM 실행·검증·CRC 마지막 기록 계약을 유지한다.

CFWREC v2는 v1의4096B 헤더 및512KiB 파일 크기를 유지한다. 헤더 offset4의형식=2, offset36의원래BLversion, offset72..103의코드SHA를 사용한다.104..4087은FF,4088의CRC와4092의완료표식은 기존 위치다. UID/순정 APP 해시/전체 파일 체인 검사는 그대로 적용한다.

## 검증 구분

모의시험의 합성0.15는 donor코드에 factory/metadata fixture를 바꾼 것이다. 새로운 프로필·범위·해시·트랜잭션 검사의 시험이지 실제0.15 BL 실행 시험이 아니다. 정상 무선 기판에서 최초 설치/중단/버튼 복귀는 별도로 확인해야 한다.

앱이 읽는 하위64KiB 스냅샷은 **Bootstrap 설치 후 시점**이다. BL 코드·공장 데이터는 원본이지만 sector2의 APP 설치 메타데이터는 이미 변경됐을 수 있다. 이를 설치 전 전체 이미지 백업으로 표시하지 않는다.
