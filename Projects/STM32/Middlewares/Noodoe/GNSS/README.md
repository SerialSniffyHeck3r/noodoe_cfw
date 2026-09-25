# 외부 GNSS와 전화 위치 선택

`GnssService_Init`, 연결 이벤트의 `SetExternalConnected`, 외부 SPP bytes의 `FeedExternal`, 전화 위치의 `UpdatePhone`, UI의 `GetSnapshot`을 한 태스크에서 호출한다. 외부 SPP가 연결됐으면 위치 없음·수신 stale에도 EXTERNAL을 선택한다. 연결이 끊겼을 때만 PHONE을 선택하며 전화 sample 역시 신선도 검사를 받는다. 연결 전환은 외부 partial sentence/fix를 지운다. 같은 연결 상태를 반복 보고하면 지우지 않는다.

RMC/GGA의 GP/GN talker만 지원한다. `$`에서 시작해 CR/LF까지 최대159문자, checksum 필수, 초과/형식 오류는 다음 `$`에서 복구한다. 잘못된 문장은 이전 fix를 덮어쓰지 않는다. 정상 no-fix 문장은 위치·관련 motion 유효성을 즉시 내린다. 문장 수신 시각과 각 필드 시각을 따로 두므로 새 GGA가 오래된 RMC 속도를 되살리지 않는다. 기본 stale 한계는3000ms다.

단위: 좌표 degree×10⁷, 속도 mm/s, 방위 millidegree, 고도 mm, HDOP×1000, UTC 자정부터ms, 날짜 YYYYMMDD. NMEA 속도는knots에서 반올림 변환한다. 고도는GGA의 M 단위만 받으며 ellipsoid고도가 아닌 송신기의 해당 field값이다. UTC leap-second `235960`은 시간 invalid, 날짜는1980..2079로 한정한다. 유효 위치라도 수신기의 정확도/무결성을 보증하지 않는다. quality는최근GGA값이며 별도 정확도 정책이 없다. RMC만 받으면 quality0이어도 position valid일 수 있으므로 `snapshot.valid`를 사용한다.

필드와 단위는 제조사 [Trimble RMC](https://receiverhelp.trimble.com/alloy-gnss/en-us/nmea0183-messages-rmc.html), [Trimble GGA](https://receiverhelp.trimble.com/alloy-gnss/en-us/nmea0183-messages-gga.html)와 대조했다. 이는GPS 수신기 실장 시험이 아니다. 구조체는 caller 소유이며 다른 태스크로 복사할 때 caller의 동기화가 필요하다.
