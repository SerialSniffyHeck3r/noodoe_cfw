# 공용 SPP 유지보수 계약

## Product6.9.8 음악 타일 압축

Capabilities0x0D의 비트0x4000은 kind9(A4 music RLE)를 뜻한다. 기존
0x1000 marquee와 함께 협상하며 구 CFW는 kind7의11,184바이트 원본을 받는다.
BEGIN/DATA/FINISH/STATUS, 연결 epoch, CRC, page generation은 바뀌지 않는다.
첫16바이트는 기존 track key/view generation/tile index/schema1 LE32 헤더다.
이후 RLE control의 하위7비트+1이 길이이며 최상위비트1이면 뒤1바이트를
반복하고0이면 해당 길이의 literal이 따른다. 복원 길이는 정확히11,168B다.
CRC는 전송된 압축 페이로드 전체에 적용한다. 길이 초과·잘림·CRC·이전 세대는
완성 이미지로 공개하지 않는다. 기존 SDRAM 입력 버퍼의 별도 scratch를 재사용한다.
곡명28px/아티스트24px이며 GPU304×72 view와 두 title tile 계약은 유지한다.
JPEG는 기존 kind2, 최대480×480/48KiB이며 NOR에 저장하지 않는다.

## Product6.2 시각 콘텐츠 확장

음악 A4/CJK, JPEG 아트 및 사진3슬롯 전송의 정확한 필드·상태는
`../Reversing/analysis/2026-09-21-companion-completion/NDCP-VISUAL.md`에 있다.
Capability0x100, 명령0x79–0x7C, 현재 연결 epoch를 사용한다. FINISH 응답은 완료가 아니다.
0x70 schema2는 ride-session을 추가하고,0x76은 선택적 visual-key를 지원한다.
사진 READY만 물리 검증·영구 저장 완료이며 음악/아트는 RAM 데이터다.

## 계층

Android `transport/SppTransport`는 연결된 byte stream만 제공한다.
`protocol/ndcp/NdcpClient`는 NDCP v1 프레이밍·CRC·sequence·분할 수신만 소유한다.
`installer/NdcpSession`은 업데이트/복구 명령과 영구 저널을 소유한다.
`maintenance/MaintenanceService`는 단일 연결 owner이며 일반 앱의 autosync를 시작하지 않는다.
차후 Product 서비스도 동일 NdcpClient/SppTransport를 사용하면 된다.

펌웨어는 기존 `Middlewares/Noodoe/Update/src/NDCP.c`와 `Update_Service.c`를 그대로 공유한다.
Bootstrap 정책과 Product 정책만 별도로 둔다. 이번 작업은 새 Product 음악/알림/GPS 통신 구현이 아니다.

## 프레임

모든 정수는 little-endian. 최대 payload1024B, 단일 동기 request/response.

| 바이트 | 내용 |
|---|---|
| 0..3 | ASCII NDCP |
| 4 | version1 |
| 5 | opcode |
| 6..7 | flags: response1/error2 |
| 8..11 | sequence uint32 |
| 12..13 | payload 길이 |
| 14..15 | 0 |
| 16.. | payload |
| 마지막4 | 앞의 전체 바이트에 대한 CRC32/ISO |

응답 payload 첫 uint32는 result다. error flag와 result!=0은 일치해야 한다.
모르는 opcode는 result4를 반환한다. 나머지 result는 해당 명령 모듈의 enum을 따른다.
예: opcode45, sequence9, 빈 요청의 golden bytes는
`4e444350014500000900000000000000a85267b5`.
CRC 오류/ACK 손실/재연결에서 쓰기 명령을 자동 재전송하지 않는다.

## 명령

| opcode | 기능 |
|---|---|
| 01 | 읽기 전용 HELLO |
| 1F | 같은 encrypted session의 검증된 백업 또는 scoped-v1 설치·보존 검사 완료를 근거로 staging 허가 |
| 40..47 | BEGIN/DATA/FINISH/COMMIT/ABORT/STATUS/READ_STAGE/RESET |
| 48 | 순정 복구 조회0/준비1/확정2/취소3; 확인 token53544F43 |
| 50..57,59 | 전체 NOR A/B 백업과 고정 FAT 파일 설치; Bootstrap_Storage.h 계약 |
| 58 | schema/role/UID/실행 APP SHA/순정 BL SHA |
| 5A | 읽기 전용 유지보수 UI snapshot |
| 80 | Bootstrap scoped-v1: NOR offset/count 읽기, 최대960B |
| 81 | Bootstrap scoped-v1: FAT 해시 +9개 예상 할당주소, 독립 감사·보존 해시 시작 |
| 82 | Bootstrap scoped-v1: 9파일 완료 후 기존 데이터 보존 해시 재검증 |
| 83 | Bootstrap scoped-v1: 현재 업로드의 FF 구간 선언(offset/count, 최대64KiB) |
| 84 | Bootstrap 읽기 전용 상세 진행/정체/오류 snapshot, schema2 |

80..83은 기존 Product62..64(업데이트 준비·조회·Gate 식별)와 별개다.
80은 읽기지만 로컬 Install CFW 진입과 같은 인증 세션이 필요하다. 임의 NOR 쓰기를 제공하지 않는다.
81 요청은68B(논리 FAT0..0x9000 SHA32B +물리주소9×4B), 완료된 설치 준비의 재접속은100B
(앞의68B +이전 보존SHA32B)다. 기기 자체가 first-fit 빈 영역/기존9파일 체인을 다시 검증한다.
보존SHA는 메타데이터0x1000..0x9000과 새9파일 영역을 제외한 전체 NOR의
각4KiB 블록에 대해 `little-endian address32 +physical4096B`를 순서대로 해시한 값이다.
메타데이터는 폰의 독립 FAT 계획과 별도로 대조한다. 82가 끝나기 전에는1F가 승인되지 않는다.
83은 RAM 수신 버퍼의 FF만 채운다. 전체 이미지 SHA·내용 검사·범위 제한·소거 후 물리 readback을 생략하지 않는다.
기존53 업로드는960B까지 수용한다. 응답·프레임은 공용 NDCP1024B payload 한도를 유지한다.

새 최초 설치는 순정 사진을 CFWPIC에 복사하지 않는다. 3개 빈 A/B 슬롯이 정상 초기 상태다.
일상 업데이트의 설정·사진 초기화는 시험 부팅 확정 후 RESET_PENDING 저널 아래에서 수행한다.
실패 롤백 전에는 이전 설정·사진을 보존한다. 순정 앨범 파일은 삭제하거나 덮어쓰지 않는다.
복구 자료는 변경 영역 preimage와 계획이며 **전체 NOR 백업이 아니다**. sparse expected.bin을 장치에 쓰지 않는다.

58의 role1은 Bootstrap, role2는 Product다. 앱은 설치 작업 전에 role1 및 가져온 Bootstrap
SHA와 BL SHA를 확인한다. 58의 APP 해시는 448KiB이고 GIM1의 Product 해시는 384KiB이다.

5A 응답은 uint32 9개, 총36B다:
`result, schema=1, state, phase, position, total, error, flags, pairing_remaining_ms`.
state는 CHECK0/READY1/CONNECT2/WORK3/INSTALL_READY4/INSTALL5/PAUSED6/RECOVERY7/ERROR8/BT_TEST9.
flags는 connected1/busy2/local-install-approved4/cancellable8.
phase100은 전체 OTA 검증 완료, 101은 commit/reset 단계, 103은 순정 복귀 이행이다.
그 외에는 Bootstrap storage phase다. 바이트 수는 해당 단계만의 진행률이며 전체 예상 시간으로 해석하지 않는다.

기기 화면은 검증 후 **Back**을 기본 선택한다. Install 승인은 transaction과 encrypted link epoch에
묶이고 새 BEGIN/연결 변경/IGN OFF에서 해제된다. Back은 아직 commit되지 않은 작업만 취소한다.
COMMIT 응답 손실·메타데이터 결과 불명확 상태를 취소로 덮어쓰지 않는다.

## 최초 설치 형식

- target0: Gate64KiB + Product384KiB =448KiB, 순정 설치기를 통한 최초 설치.
- target1: 정확한 승인 순정 APP448KiB만 허용.
- target2: 독립 Gate 이후 Product384KiB 일상 업데이트.

최초 설치는 RSC/CFG/RIDE/PIC/REC/CFWA/CFWB/CFWBOOT/CFWLOG 9개를 검증해야 한다.
staging 내부 Product SHA가 A/B 둘 및 부팅 저널의 SHA와 같아야 한다.
두 벡터의 범위와 자산 요구 ID도 대조한다. 어느 하나라도 다르면 metadata commit을 거부한다.
최초 FAT 할당과 기존 순정 BL 설치 단계의 전원 중단 위험까지 원자적으로 바뀌는 것은 아니다.

## 실패와 복귀

휴대폰 저널은 요청 전 결과 불명확 상태를 먼저 기록한다. 수신 완료/전체 검증/commit/리셋 요청/
실제 부팅 확인을 구별한다. Bluetooth가 안 되는 최소 복구/Gate에 SPP 명령을 보내는 구조가 아니다.
Bootstrap의 Back to stock 및 초기 복구에서는 O를 놓았다가 새로 2초 유지한다. 추가 IGN 조작은 없다.
Product 독립 Gate의 비상 진입은 키 OFF → O 유지 1초 → 키 ON → O 유지 2초다.
내장 순정본 또는 검증된 CFWREC, HSI/polling NOR로 복구한다.
일반 Product UI 통신과 별개로 transport/parser를 재사용하되 각 역할의 쓰기 권한은 합치지 않는다.

## 상세 진행과 멈춤 구분 (2026-09-21)

기존 5A/36B ABI는 유지한다. 84는 payload가 없는 읽기이며, 같은 SPP 연결의 명령 사이에서만 조회한다.
다른 소켓이나 별도 상태 폴링 스레드는 만들지 않는다. 응답은 little-endian uint32 16개/64B:

`result, schema=2, state, phase, subphase, file_kind, position, total, error,
heartbeat, uptime_ms, progress_idle_ms, requests, replies, flags, connection_epoch`

- flags: progress_stalled=1, connected=2, busy=4, IGN_ON=8. phase/position/total은 현재 단계다.
- file_kind: 자산0/설정1/누계2/사진3/순정복구4/CFW A5/CFW B6/부팅저널7/로그8.
- error 상위16비트: BT1/NOR2/RAM3/DISPLAY4/STORAGE5/UPDATE6. 하위16비트는 해당 모듈의 상세 코드다.
- 예: `00050004`=저장 작업 IO 오류, `0005000A`=저장 진행 정체 제한시간 초과.
- DISPLAY 상세1=FIFO/swap 대기 지속,2=버스/준비 실패 지속. 초기화 실패는 상세0일 수 있다.
- 오류 화면은 첫 원인을 사용자가 O로 확인할 때까지 유지한다. 다음 성공 조회나 후속 오류가 덮어쓰지 않는다.
- heartbeat가 늘어도 byte/phase/subphase/file이 안 바뀌면 진행으로 보지 않는다. 30초 정체 경고,120초 정체 시 비확정 저장 작업을 물리 NOR 호출 사이에서 FAILED로 만들고 쓰기 증명을 폐기한다.
- UPDATE COMMITTED/RESET과 복구 이행은 이 저장 정체 취소 대상이 아니다. 실제 CPU 정지에는 기존 IWDG/초기 복구 경로를 유지한다.
- 화면 제출이10초 연속 실패하면 폰에서 DISPLAY 오류도 읽을 수 있다. 물리 LCD가 고장 났을 때 오류 문자가 보인다고 보장하지 않는다.

폰은1초 주기로 경과/현재 단계 시간/마지막 검증 응답/현재 단계 전송률·잔여시간을 표시한다.
전체 설치 ETA로 표시하지 않으며, 응답 없음과 응답이 있지만 진행 없음은 구분한다.
소켓 connect/write는15초 제한 후 닫는다. 재접속 후 부팅 확인은 하나의175초 창을 공유한다
(진행 중인 개별 IO 제한 시간만큼 끝나는 시각은 늦어질 수 있다). 확정 명령은 재전송하지 않는다.
읽기 응답 타임아웃은 취소/성공 증거가 아니다. 최종 완료는 영구 부팅 저널 CONFIRMED로 판정한다.

폰 로그에는 단계/바이트량/기기 error/phase/subphase/epoch를 최대5초 간격 또는 변화 시 기록한다.
UI 문자열·사진·알림 내용은 넣지 않는다. 디스플레이 캐시는 참고용이며, 재실행 권한이나 트랜잭션 증거로 사용하지 않는다.
