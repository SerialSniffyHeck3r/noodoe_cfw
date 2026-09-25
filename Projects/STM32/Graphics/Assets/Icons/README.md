# Product icons

모든 새 제품 아이콘은 **Google Material Icons Round**를 사용한다. Material
Symbols Rounded, Filled 아이콘, LV_SYMBOL로 조용히 대체하지 않는다.

| 용도 | 공식 이름 | Unicode | 표시 조건 |
|---|---|---|---|
| 정비 | build | U+E869 | SERV에서 모드 글자 대신 한 개 |
| 저연료 주행거리 | local_gas_station | U+E546 | RESV/TRIP F에서 모드 글자 대신 한 개 |

OIL/BELT는 모드 글자를 유지한다. 이 아이콘 정책은 정비 바의 표시 조건과
별개다. 정비 바는 OIL/BELT/SERV 세 모드 모두 사용한다.

원본은 google/material-design-icons commit
`40a7a292a79d9394157e1ea24f83d52d5e17c556`에 고정했다.
`source/upstream.json`에 OTF/SVG/codepoints/LICENSE의 URL, 길이, SHA-256을 보관한다.
`tools/product_icons.py`는 검증된 OTF에서 필요한 두 glyph만 24px의 byte-aligned
4bpp 정적 LVGL font로 만든다. 그림은 늘이지 않고 빈 여백만 제거한다.
실제 잉크 크기는 build22×22, local_gas_station18×18이며 잉크 위쪽을 Y410에
맞춘다. 전체 font bitmap404bytes, 생성 해시는 manifest.json에 기록된다.

상위 API는 `Product_IconFont(24)`와 `Product_IconText(ProductIcon)`이며
본문 Lato·숫자 D-DIN과 별도 자원이다. 런타임 파일 로딩이나 메모리 할당은 없다.
다른 크기/정의하지 않은 이름은 NULL/빈 문자열을 반환한다.

## 중앙 모드 표시줄

| 페이지 | Material Icons Round | 선택 이유 |
|---|---|---|
| 기본 화면 | home | 기본 위치 |
| 트립 | analytics | 주행 통계 |
| 휴대전화 | smartphone | 배터리·알림을 제공하는 폰 |
| 음악 | music_note | 미디어 재생 |
| OBD | car_repair | 차량 진단; footer SERV의 단독 스패너와 구분 |
| 폰 제어 | settings_remote | 리모컨 조작 |
| 폰 GPS | route | 위치 핀 한 개보다 이동 궤적을 표현 |
| 환경설정 | settings | 설정 |

tools/product_mode_icons.py는 같은 고정 OTF에서 별도 A4 마스크8개를 생성한다.
Product_ModeIcon은 footer ProductIcon enum과 독립된 자원이며 bitmap 합계2304bytes다.
24px 원본을16..32px로 bilinear 확대/축소하고 작은 회색에서 큰 흰색으로240ms 보간한다.
48px 간격의 표시줄이 움직여 선택 아이콘이 화면X240에 온다. 정지 시5개가 보이며
8모드를 원형으로 연결해 마지막에서 처음으로 한 칸 이동한다. 위치·크기·색은
ProductModeStrip_Update 한 함수에서 함께 계산한다. 색은 선택 흰색과 비선택 회색
두 상태뿐이며 실제 잠금 정책은 App에서 처리한다.
mode-manifest.json에 codepoint/실제 잉크 경계/해시, MODE_ICON_NOTICES.txt와
MODE_ICON_LICENSE.txt에 원본 라이선스 및 수정 내용을 보관한다.

Apache-2.0 원본 라이선스와 프로젝트의 subset/raster 변환 고지는
`ICON_NOTICES.txt`에 보관한다. 배포 자원에도 해당 고지를 함께 유지한다.


## 트립 기호

최고속도는 같은 고정 Material Icons Round의 arrow_upward(U+E5D8)다. 평균 기호는
카탈로그에 해당 수학 기호가 없으므로 사용자 허용 fallback으로 프로젝트가 그린
원/사선을 사용한다. Google 아이콘이라고 표시하지 않는다. tools/product_trip_icons.py,
trip-manifest.json, TRIP_ICON_NOTICES.txt에 출처/생성 해시를 보관한다.

거리 표시는 같은 Round OTF의 two_wheeler(U+E9F9)와 signpost(U+EB91)를 사용하며,
그 사이 세 점은 단순 그래픽 primitive다. 표지판 . . . 오토바이 순서로24px 아이콘
두 개와32px 간격을 사용해80px 조합을 만든다. 각 점은2px이며6px씩 빈칸을 둔다.
조합은 고정 거리 숫자·단위 칸의 왼쪽16px 앞에 위치하며 값이 바뀌어도 움직이지 않는다.
네24px A4 마스크의 원본 합계1152bytes를 EVE로 캐시하며 footer/모드 자원은 유지한다.
