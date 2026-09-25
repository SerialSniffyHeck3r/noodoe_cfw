# 제품 글꼴 자원

2026-09-13 사용자 수정: 고정 폭은 **UI 표시 영역**을 뜻한다. 글자와 공백을
monospace로 만들지 않는다. 계측 숫자는 일반 본문과 독립적으로 관리한다.

| 역할 | 원본 | 정적 크기 | API |
| --- | --- | --- | --- |
| 본문·단위 | Lato Regular | 16,20,24px ASCII | `Product_TextFont(px)` |
| 음악 제목 | Lato Bold | 32px ASCII | `Product_TextFont(32)` |
| 시각·거리·계측값 | D-DIN Regular | 20,32,36,40,48,64px 숫자/구두점 | `Product_NumberFont(px)` |

생성 파일도 `product_text_fonts.c`와 `product_number_fonts.c`로 분리했다.
`tools/product_fonts.py`는 원본 SHA256을 확인하고 EVE가 직접 읽는 비압축
4bpp/byte-aligned `lv_font_fmt_txt`를 만든다. MCU가 TTF/OTF를 파싱하지 않는다.
숫자0..9만 동일 cell에 원래 그림을 중앙 배치한다. 공백/구두점은 원래 advance를
유지한다. 본문은 자연 advance를 사용하며 현재 pair kerning은 생성하지 않는다.
새 크기나 언어팩은 명시적으로 생성해야 하며 알 수 없는 API 크기는 NULL이다.

이름의 nominal px와 글자의 실제 잉크 높이는 같지 않다. view는 font의 실제
baseline으로 정렬한다. 고정 박스에는 auto-fit/글자 강제 축소/가짜 Bold를 쓰지 않는다.
현재 영문 UI subset에는 한글 glyph가 없다.

## 변환 자원 이름과 유료 배포

변환한 자원의 고유 이름은 **Noodoe UI Text**와 **Noodoe UI Numbers**다.
Lato·D-DIN은 그 원본 출처를 설명하는 이름이다. 두 원본에 Reserved Font Name
선언이 있으므로, 축소·비트맵 변환·숫자 advance 수정본을 원본 이름의 정식
폰트로 배포하지 않는다. API와 C 자원 이름은 이미 `product_text_*`,
`product_number_*`로 구분되어 있다.

OFL1.1은 이 폰트를 포함한 유료 비공개 펌웨어 판매를 허용한다. 자체 커펌의
소스 공개 의무를 만드는 것은 아니다. 폰트 및 변환 폰트 부분은 OFL로 유지하고,
배포물에 `FONT_NOTICES.txt`의 저작권·라이선스 전문을 함께 포함한다.
폰트 자체만 따로 판매하거나 폰트의 OFL 권리를 막는 별도 제한을 적용하지 않는다.
해석 근거: https://openfontlicense.org/ofl-faq/ 항목1.3,1.4,1.21,3.6.

## 출처

- Lato 배포: https://github.com/google/fonts/tree/main/ofl/lato
- Lato 제작사: https://github.com/latofonts/lato-source
- D-DIN 배포: https://github.com/amcchord/datto-d-din
- DIN 계열의 자동차 적용 근거는 Monotype의 Denso 사례다.
  https://www.monotype.com/de/ressourcen/fallstudien/denso-faehrt-mit-monotype
  해당 사례의 DIN Next 파생 글꼴과 여기의 D-DIN은 다른 글꼴이다. D-DIN이
  그 차량에 사용되었다거나 인기 순위1위라고 주장하지 않는다.

원본 URL·크기·SHA256은 `source/upstream.json`, 글꼴별 라이선스/저작권 고지는
`source/lato/OFL.txt`, `source/ddin/OFL-1.1.txt`, `COPYING.txt`, `FONTLOG.txt`에 있다.
둘 다 SIL OFL1.1이다. 생성 subset 역시 해당 폰트 라이선스의 적용을 받는다.
이전 `source/NotoSansMonoCJKkr-Bold.otf`와 `source/LICENSE.txt`는 비교 이력이며
현재 제품 font generator나 APP에서 사용하지 않는다.
