# FuckNudo 0.9.57 — map loading and music return

지도 축척 변경·타일 경계 이동과 음악 화면 복귀의 대기를 줄입니다. 이전0.9.56 후보의 화면 구성 최적화·연료 경고 배경 수정과 그 뒤의 화면 처리 단순화도 포함합니다.

- 휴대폰에 준비된 지도 타일32개를 캐시하고, 전송 중 다음 타일을 준비합니다. 보이는 영역을 먼저 처리합니다. 최근 축척으로 돌아오면 캐시를 재사용합니다.
- 화면 밖 타일 때문에 같은 지도를 다시 그리던 작업을 제거했습니다. 확대·축소 시 미완성 작업을 새 축척으로 전환하며, 기존 화면과 도로 연결·폭을 유지합니다. 지도 화면 밖에서는 지도 작업을 멈춥니다.
- 같은 곡의 음악 정보와 앨범아트를 RAM에 유지합니다. 지도가 EVE 메모리를 빌려 쓴 뒤 음악으로 돌아올 때, 프레임 사이에도 앨범아트 업로드를 처리합니다. 일반 화면 복귀에 GPU 캐시가 남아 있으면 재업로드하지 않습니다.
- 리소스 전송 순서는 음악 → 전화 → 알림 → 지도입니다. 고속 링크는 기존40ms 작업 시간을 더 활용하며, 제어·GPS와 두 패킷의 전송 한도를 유지합니다.
- 반복 UI 스타일 갱신과 사용하지 않는 음영 텍스처 업로드를 줄였습니다. 큰 연료 경고는 독립된 전체 배경 음영을 사용합니다.

**기존 주행 연동을 중지한 뒤 APK0.9.57을 덮어 설치하고, 함께 제공된0.9.57 ZIP으로 Product를 업데이트하세요.** APK와 ZIP은 한 쌍입니다. 기존 앱 데이터와 일반 업데이트의 설정·사진 보존 정책은 유지됩니다. Gate, Bootstrap, 순정, 리소스, 제거·진단 이미지는0.9.55와 동일하며 Gate 교체가 필요하지 않습니다.

0.9.44 등 구버전에서 처음 올리는 동안에는 구버전의 업데이트 코드가 실행됩니다. 마지막 검증 이후 재시작 정지가 나타났다고 설치 성공으로 단정하지 말고, 진단을 보존한 뒤 기존 설치·복구 안내에 따라 상태를 확인하세요.

검증: Android54개 시험과 실제 ZIP importer, lint 오류0, 동일 APK 서명, Debug/Release 메모리 예산을 통과했습니다. 해당 코드의 ARM/LVGL/EVE1,203프레임에서 표시 중 텍스처 덮어쓰기가 없었습니다.28개 지도 패킷과28개 손상 패킷, 숨겨진 지도 중단, 확대·축소와 캐시를 검사했습니다. 앨범아트 업로드는5ms 작업 방문/35ms 구성 슬롯의 소프트웨어 모델에서1,015ms→400ms였으며 페이드 시간은 별도입니다.

**실제 차량 설치, 무선 전송 속도, 지속적인 LCD30fps 및 시동 중 연료 음영은 이번에 실기로 검증하지 않았습니다.** Latest 표시는 소프트웨어 검증된 배포판이라는 뜻이며, 실기 확인 완료를 의미하지 않습니다.

English: this release caches prepared map tiles, overlaps compilation with transmission, removes redundant offscreen-triggered raster uploads, accelerates retained-artwork restoration and prioritizes audio/calls/notifications/maps. It includes the0.9.56 display and warning-background candidate fixes. Install the matching APK and ZIP together. Existing settings and photos are retained by ordinary updates. All auxiliary firmware images and Gate remain unchanged. Software validation passed; physical RF, LCD frame rate and ignition reproduction are unverified. The optional relink-object kit contains compiled objects for replacing Mapsforge, without application source or signing keys.
