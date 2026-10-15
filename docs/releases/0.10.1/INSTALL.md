# FuckNudo0.10.1 beta — B multilingual menus and device text

Use the APK, firmware ZIP and optional language ZIP from this release together.
This beta is selected as the project's Latest download; that is not a claim of
physical power-loss/RF qualification or certification of every vehicle revision.

## Install / update

1. Install **NoodoeCompanion-0.10.1.apk** over the existing app; keep app data.
2. Select **NoodoeInstaller-CFW-0.10.1.zip** while stopped. First installation
   accepts F4 stock5.14/5.16; model/PCBA/HW/bootloader version alone does not
   block admission. Existing identity, storage and stationary checks remain.
3. If an older Gate is detected, use **keep-data stock return → Check current
   device → Bootstrap from this ZIP → Restore stored data → install CFW**.
   This one-time transition replaces Gate. Do not choose full erase/uninstall.
   Try automatic Bootstrap reconnect; if it fails, manually forget/re-pair,
   then choose **Check current device → Continue from here**. Do not clear app
   data or resend Bootstrap just to force a connection.
4. Confirm the new Product is running before optional resources. Transfer100%
   is not confirmation. If asked, follow the displayed UP+O3s reset instruction.
5. Import **NOODOE-LANGUAGE-B-0.10.1.zip**, then choose language-resource
   install/resume while stopped. Firmware and language completion are separate.
   An interrupted resource transfer can resume with the same device/archive;
   do not reinstall firmware or return to stock again for that interruption.

**Known legacy power-interruption limit:** keep permanent power connected
throughout stock return/reinstallation; do not operate the starter. If older
CFW loses power while recording its stock-return request, boot can stop before
the new recovery code runs. Automatic recovery is not guaranteed in that old
code interval. IGN OFF is not removal of permanent power. The new Gate repair
does not retroactively repair already-installed old code. This limit was
explicitly accepted for this beta; no all-power-cuts guarantee is claimed.

## What changes

- B device menus: English, German, Spanish, French, Italian, Greek, Korean,
  Vietnamese and Japanese. Android adds Simplified/Traditional Chinese for11
  app languages. A retains English; Chinese is not a B menu-language option.
- English retains Lato and layout. Other supported stock languages reuse B
  N/R fonts; Japanese menus use the supplied font pack. Only the boot-selected
  language is loaded; a language change asks for restart after durable saving.
- Follow app language or choose a device language manually. Unsupported app
  languages use English on device; reconnect does not overwrite manual choice.
- Choose phone-rendered text (default, broad character/emoji support) or
  device fonts (less text-image transfer, limited to available glyphs) for
  music/HOME/notification detail/popups. Calls and quick replies retain their
  existing path. Unknown characters/sequences show one square; missing
  translations use English. Existing system icons are unchanged.
- LANGUAGE.PAK includes CJ and monochrome Noto Emoji3.002 Regular400 at24/32px.
  The11,965,392-byte pack stays below the24MiB cap, without reducing the agreed
  JP2,136 / CN6,500 / Taiwan4,808 repertoires. Region forms share identical masks.
- Only confirmed structural damage to required B FONT files offers explicit
  Restore / English / Later. Healthy fonts are not rewritten; hash differences
  or transient IO do not authorize repair. Space/RAM/resource failures can use
  English-only and phone rendering without deleting other files. Language and
  renderer preferences persist; explicit English-only remains sticky.
- New Gate/Bootstrap repair torn idle-completion metadata with ordered,
  zero-only writes. Active installer requests still block unsafe writes.

## 한국어 설치 요약

같은0.10.1 APK·설치 ZIP을 사용하고 앱 데이터를 유지하세요. 구형 Gate라면
**데이터 유지 순정 복귀 → 현재 기기 확인 → 같은 ZIP의 Bootstrap → 보관 데이터
복원 → CFW 설치·정상 실행 확인** 순서입니다. 그 뒤 별도 언어 ZIP을 가져와
정차 상태에서 언어 자원을 설치하세요. 언어 전송만 끊겼다면 같은 언어 ZIP으로
이어가며, 펌웨어 재설치나 순정 복귀를 반복하지 않습니다.

**구버전 전환 제한:** 순정 복귀 요청 기록 중 상시 전원이 끊기면 새 복구 코드
실행 전 부팅이 멈출 수 있어 자동 복구를 보장하지 않습니다. 복구·재설치가
끝날 때까지 상시 전원을 유지하고 시동 모터를 돌리지 마세요. IGN OFF는 전원
분리가 아닙니다. 이 제한을 허용한 베타이며 실물 전원 차단 검증을 주장하지 않습니다.

B는9개 기기 언어, 앱은11개 언어를 지원합니다. A는 영어만 유지합니다. 영어는
기존 Lato/배치, 일본어는 새 팩 글꼴, 나머지는 순정 B N/R을 사용합니다. 언어는
재부팅 때 바꾸며 음악·알림은 휴대폰/기기 글꼴 중 선택합니다. FONT 오류가 확정된
경우에만 복구 여부를 묻고, 원치 않거나 자원이 부족하면 영어 전용을 선택하세요.

## Manuals, validation and licenses

- [한국어 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/35-Languages.md)
- [English manual](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/35-Languages.en.md)
- VALIDATION.md describes software evidence and its limits. SHA256SUMS.txt
  identifies all release assets. Source is backed up only in the private repo.
- LANGUAGE-LICENSES.txt and the language archive retain font licenses, including
  OFL1.1. Fonts are not treated as public-domain assets. The relink-objects ZIP
  supplies matching compiled objects, APK and library-replacement instructions;
  it is not a firmware ZIP or language ZIP.
