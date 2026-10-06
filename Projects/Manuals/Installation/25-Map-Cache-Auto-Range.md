# FuckNudo 0.9.58 — map cache, automatic range and artwork return

지도 화면 복귀 시 남아 있는 EVE 텍스처를 즉시 재사용합니다. 앨범아트 등으로
EVE 공간을 덮어쓴 경우에도 완성된 RAM 지도 캐시가 유효하면 재계산을 생략합니다.
캐시가 없거나 위치·축척·지도 내용이 달라지면 정상적으로 새 지도를 준비합니다.

- 지도 선행 로딩은 보이는 타일을 먼저 확보하고 진행 방향과 속도를 반영합니다.
  지도 래스터의 공간 인덱스로 불필요한 도로 반복 검사를 줄였습니다.
  지도 화면 밖에서는 지도 계산·전송을 중단합니다. 도로 조각을 삭제하지 않습니다.
- 지도 화면에서 **위 길게: 자동/수동 전환**. 자동에서는 위/아래로 **−2~+2**
  선호도를 조절합니다. 음수는 더 가까이, 양수는 더 넓게 표시합니다.
  수동에서는 기존 축척을 직접 조절합니다. 자동으로 돌아올 때 선호도를 유지합니다.
  새 축척 지도가 준비된 뒤600ms 확대·축소 효과를 적용합니다.
- 같은 곡의 앨범아트는 RAM에 유지하며, EVE에 복구하는 프레임 사이 복사량을
  늘렸습니다. 해상도, 색상, 기존240ms 페이드와 음악→전화→알림→지도 우선순위를 유지합니다.
- 큰 연료 경고의 배경은 초기 버전의 일반 LVGL 검은 팝업 음영 경로로 복원했습니다.
  RESV 연료 아이콘도 기존 Google Material Icons Round 글리프로 복원했습니다.

**기존 주행 연동을 중지하고 APK0.9.58을 덮어 설치한 뒤, 함께 제공된0.9.58 ZIP으로
Product를 업데이트하세요.** 일반 업데이트는 기존 설정·사진을 보존합니다.
Gate, Bootstrap, 순정, 자원, 제거·진단 이미지는0.9.57과 동일합니다.
구버전이 업데이트를 수행하는 동안의 구버전 재시작 오류가 새 APK만으로 없어지는 것은 아닙니다.

소프트웨어 비교에서 GPU 지도 캐시가 남은 복귀는60회 작업 방문→1회,
앨범아트가 GPU 공간을 사용한 뒤의 복귀는68회→48회였고, 완성된 지도 픽셀은 같았습니다.
앨범아트 복사는 고정5ms 방문/35ms 구성 슬롯 모델에서400ms→310ms였습니다.
이 수치는 실제 차량의 소요 시간이나 FPS 측정값이 아닙니다. 처음 보는 지역,
메모리에서 사라진 타일, 연결 지연에는 준비 시간이 필요합니다.

실차 설치, 실제 RF 처리량, 지속적인 LCD30fps 및 시동 중 연료 음영은 미검증입니다.
상세 시험 결과는 [0.9.58 검증 보고서](../../documentations/display-map-cache-0.9.58.md)를 참고하세요.

English: reuses untouched GPU maps, restores completed RAM rasters after eviction,
adds predictive tile preparation and adjustable speed-based automatic range, and
accelerates retained artwork uploads. It restores the original rounded fuel icon
and LVGL warning backdrop. Install the matching APK and ZIP. Software validation
is separate from physical vehicle, radio and display validation. The optional
relink kit contains compiled replacement-library objects, not application source or keys.
