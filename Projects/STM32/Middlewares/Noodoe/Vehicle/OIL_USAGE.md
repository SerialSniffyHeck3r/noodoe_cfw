# 시동 ON 시간 계측

OIL의 보조 수명 값은 **시동 ON 누적 시간**이다. 엔진 회전 시간이나 예상
주행 가능 거리를 계산하지 않는다. 위쪽 OIL 거리 값과는 별도 값이다.

`UsageCounter_Sample()`은 하드웨어 없는 순수 계측기다. 첫 관측을 시작점으로
삼고, 이전에 유효하게 관측된 IGN ON 구간만 monotonic millisecond로 더한다.
OFF·미확인 구간은 더하지 않는다. 32bit tick 순환은 unsigned 차로 처리하며
누계는64bit 포화 연산이다. 1초를 넘는 관측 공백은 추정하지 않고 gaps를
올린다. GPIO 디바운스·폴링 간격만큼의 오차는 있으며 실제 엔진 hour meter가 아니다.

`OilUsageService_Process()`는 Runtime I/O task에서 BSP_Power 뒤에 호출한다.
storage worker 초기화 완료 후 저장된 누계가 있으면 복원하고 이번 부팅의
관측 시간을 더한다. 기록이 없는 기기는 **이 펌웨어로 추적을 시작한 시점이0**이다.
기존 차량의 과거 오일 교환 시점이나 이전 운행 시간을 복원한 것으로 취급하지 않는다.
정비 리셋 메뉴와 OIL 거리·시간의 동시 기준점 갱신은 별도 도메인 연결 단계다.

상위 `OilUsageService_Get()`은 대기 없는 snapshot API다. Product UI는
`total_ms / 360000`을 HOURS의 소수 첫 자리로 표시한다(0.1h=6분).
이 API는 날짜를 수정하거나 NOR를 직접 기다리지 않는다.

저장은 기존 SettingsService 큐와 검증된 NVM journal만 사용한다.
60초 누적마다, 또는 키 OFF에서 비동기 checkpoint를 요청하고 오류/BUSY는
최소1초 후 재시도한다. 완료 ID와 검증 성공을 받은 뒤에만 committed_ms를 올린다.
갑작스러운 전원 손실은 마지막 성공 checkpoint 이후의 시간을 잃을 수 있다.
저장소 오류/큐 지연이 있으면 그 간격이60초보다 길어질 수 있다.

프로비저닝되지 않은 순정 저장소에서는 자동 포맷·초기화·기록을 하지 않는다.
계측은 RAM에서 계속하고 `persistent=0`과 Settings 오류 상태를 보고한다.
이 경우 완전 재부팅 후 누계가 유지된다고 주장하지 않는다. 표시와 영구 저장의
성공은 서로 다른 상태다.

`g_oil_usage`는72bytes, 12개u32 다음에 total_ms/committed_ms/boot_on_ms u64다.
seq, ready, restored, persistent, save_result, gaps로 실기 검증한다.
settings_host 시험은 실제 서비스/codec/counter를 O0/Os로 실행해 ON/OFF,
미확인 구간, tick 순환, 포화, checkpoint ACK와 저장값 복원을 검사한다.
