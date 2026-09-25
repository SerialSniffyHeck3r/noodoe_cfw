## 최신 홈/전원 배경 (2026-09-16)

- IGN OFF 뒤 첫1000ms는 밝기까지 포함한 직전 프레임을 그대로 유지한다. 배경 shade/사진 GPU 쓰기, 화면 재구성, 절전 정책 변경, 세션 종료를 하지 않는다. 대기 종료 시 SESSION_END를 한 번 확정하고 주행 snapshot을 만든다. 그때부터 선택 사진 전환·20% 밝기로240ms fade·현재 위치에서 링400ms exit·요약24px rise/240ms fade를 함께 시작한다. 링 scanout을 추가로 기다리지 않는다. 1초 이내 ON 복귀는 같은 주행 세션/배경을 유지한다. 요약은 확정 후5초이며 전환 완료 후 AWAKE 프레임을 유지한다.
- 이미지 휘도 기반 자동 조절은 제거했다. ON 정보 화면20%, 음악32%, 홈 사진100%에 사용자 중앙 밝기 설정을 적용한다. OFF 전환/대기와 Ride Summary는20%다. 첫1초 동안은 기존 밝기를 전혀 변경하지 않으며20% 요청은 종료 확정 이후에만 시작한다. 음악앨범은IGN ON 음악에만 사용한다. 강제OFF사진 선택은 저장된 wallpaper enabled 설정을 변경하지 않는다.
- 홈 UP/DOWN은 사진만/날짜/날짜+UART속도3단계 순환한다. 날짜는RTC Gregorian 결과의영문 월약어·자연 일·요일, 예 Sep. 16 Wed다. 사용자의Thu는포맷 예시이며2026-09-16을Thu로하드코딩하지않는다. 숫자는D-DIN64 고정폭,km/h·mph는Lato24 고정위치,미수신은---다. DATA_DEBUG 시작카드는홈0이다. 실제UART/날짜는placeholder로대체하지않는다.
- 홈 아이콘 strip은 주카테고리 변경5초 후240msfade로숨긴다. 하위모드·속도값 갱신은타이머를리셋하지않으며 다른카테고리는다시표시한다. shell가시성과idlealpha를분리한다.
- Ride Summary의Dist./Time/OIL은동일X114·168·338 고정prefix/value/unit칸이다. 값D-DIN48,설명Lato20,잉크하단246/302/358. 거리자연소수1자리,시간H:MM,OIL자연정수%,미확인은--.24px상승/240msfade는그룹전체공유한다.
- 사진은순정album을보존한별도root WALL0.JPG..WALL2.JPG가우선이다. PhotoImport ABI2의slot|0x100은해당override를create-only로생성한다. 완전JPEGdecode검증은출력tile을버려현재pixels를절대로수정하지않고,읽기대조후명시적재부팅에서만활성화한다. 기존파일덮어쓰기/삭제/포맷은하지않는다. tools/wallpaper_install.py는전체백업+검증journal의현재기준·ARM정확쓰기계획·실기기모든변경영역preimage/readback을필수로한다. 추가사진교체는새로운버전저장정책이필요하며현재한번생성경로를overwrite로완화하지않는다.

# Shared wallpaper contract

`Wallpaper.h` owns three photo slots, selection/off and source priority. Call
these nonblocking functions from the product UI owner task:

```c
Wallpaper_SetPhoto(0, &photo); // slots0..2; NULL unregisters
Wallpaper_SelectPhoto(0);
Wallpaper_SetEnabled(1);      //0 disables regular wallpaper
Wallpaper_SetCenterBrightness(100); // original center brightness
Wallpaper_SetCenterBrightness(35);  // e.g. a text-heavy trip page
```

The user brightness setting persists across pages until changed. App policy
multiplies it by100% on the photo-only Home subview and20% on information pages (Trip, Phone, OBD,
Remote, Trail and Quick Settings). Music uses a fixed32% multiplier independent of artwork luminance. The renderer eases brightness changes over240ms without modifying
the saved user setting. At100%, Y160..320 has no vertical shading; black alpha
rises smoothly toward238 atY60/Y430. At lower brightness the central band has
uniform dimming. Foreground colors and physical backlight PWM are unaffected.
Clock/ODO remain excluded from background drawing. Light mode is not implemented.

Music with valid album art always takes priority, including when regular
wallpaper is disabled. Leaving Music or losing art restores the selected
photo; an empty slot/off produces black. Slot selection survives this temporary
override. Music shows a bold title, artist, progress/time and physical-key
hints. Development notes, MUSIC header and thumbnail are not drawn.

Music computes a readability multiplier only when artwork changes. RGB565
luma uses integer77R+150G+29B weights; mean luminance is capped near70 and
peak near100, with multiplier25..80%. White artwork selects27%, black80%.
This multiplier is applied to the user's center setting without modifying it;
missing album data starts from40%, also capped at32%. Leaving Music applies
the destination page multiplier (Blank100%, information20%).
Top/bottom smooth gradients retain the clock/ODO contrast. This is dimming of
the image, not HDR/exposure recovery or a claim that every possible title has
measured contrast. Future large-art providers need corresponding statistics.

## Image providers and ownership

`BackgroundImage` is tightly packed little-endian RGB565, width/height1..480,
with exact `bytes=width*height*2`. Supply **one** of:

- `pixels`: immutable CPU-readable storage, retained until deselection/replacement;
- `read(context,offset,dst,count)`: a bounded file/NOR provider returning1 for
  a filled chunk,0 if temporarily busy, or-1 for failure. It must not wait or
  retry internally. Context and underlying content stay valid until replaced.

Change `revision` when content at the same source changes. Source registration
does not copy or persist entire photos. PhotoService now loads the stock album0..2
JPEG files from NOR into immutable SDRAM RGB565 images. Its explicit create-only
import can restore missing photos; normal wallpaper calls never write NOR. See
Middlewares/Noodoe/Photos/README.md. Settings remain RAM-only. The current phone model
supplies32x32 RGB565 artwork; it is copied into an owned2KiB buffer and enlarged
with GPU bilinear filtering. It is not a high-resolution album-art transport.

## Renderer and bounds

`product_background_view.c` creates the first480x480 shell child. The EVE port
draws a center-cropped photo and one1x480 L8 alpha strip, then the existing
ring, clock, page, footer and final circular mask. It allocates no full MCU
framebuffer and does not repaint the outer area with UI content.

Product RAM_G: fonts/icons `[0,0xE800)`, small artwork `[0xE800,0xF000)`,
shade `[0xF000,0xF1E0)`, resident photo `[0xF800,0x80000)`, snapshot starts
at`0x80000`. The guard enforces these
boundaries. Integrated/Graphics keep their existing512KiB asset cache. Future
fonts/images must respect the reduced Product cache budget; the unused64px
numeric font cannot simply be added to the current full glyph set budget.

Clock and ODO areas are now excluded from photo AND shade drawing. The EVE
stencil follows the exact clock separator above-edge and footer separator
below-edge coordinates from SpeedHome_Layout. Their interiors retain the black
shell clear; changing brightness cannot reveal an image there. Gradients still
operate in the remaining wallpaper area. Temporary stencil bits are cleared
before restoring the draw context so later arcs/triangles are unaffected.

Image changes use the global `Graphics/UI/Scalar_Transition.h` slow-fast-slow
animation. Current32x32 artwork has a dedicated2KiB GPU bank; Music never evicts
the480x480 photo. Switching between retained photo and artwork crossfades over
240ms without a black interval or photo re-upload. The full photo is drawn as
an opaque base and the artwork alpha eases between0 and255. Reversal samples
the current blend, so repeated button input does not jump to an endpoint.
A missing target bank uploads behind the outgoing image before blending.

Changing between two large photos still requires the same full-resolution bank:
fade-out240ms, actual black DL retirement, bounded upload, fade-in240ms. That path
is not simultaneous full-resolution crossfade. Upload sends at most16KiB per
call from immutable direct pixels, or1KiB from a bounded provider. The snapshot
region is untouched and no additional MCU framebuffer is allocated. A480px photo
requires29 direct-upload calls; the cached Music-to-OBD path requires none.

Rapid requests coalesce and GPU banks must retire before overwrite. A provider
failure keeps the other resident image when possible and sets read_error;
without a resident image it stays black. A new source/revision retries. Brightness
easing updates only the480-byte shade, not photo pixels. Diagnostic ready refers
to a resident image; pending dimensions/upload fields describe the requested
source. Version2 adds alpha, phase, displayed brightness and resident dimensions.
Phase5 blends retained banks; settled phase is0, alpha255. Source policy is still
regular selected photo on every non-Music page and album priority only in Music.

FT81x uses8.8 bitmap transforms, not the later BT81x1.15 extension. Its L8
texture is alpha-only. See the [FT81X programmer guide](https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf).

## Stock photo provider and OFF

PhotoService_RequestLoad(slot) queues nonblocking work for the I/O owner. Decode
uses at most8 JPEG MCUs per service call; one compressed-file read and explicit
filesystem import run in that worker, never in the rendering task. Pixel buffers
are immutable for the remainder of the boot, and registration occurs only after
complete decode. Up to three480x480 RGB565 images use external SDRAM, not the
FreeRTOS/LVGL internal heap. WallpaperRuntime_Process publishes ready descriptors.

The stock filesystem is detected and byte-pair decoded without formatting. Generic
writes and the overlapping NVM journal are blocked. A verified-backup import tool
can create missing album files and verifies the NOR regions actually modified.

During IGN OFF the central wallpaper remains dimmed (brightness20), including
when leaving full Settings. The selected image stays behind the summary and OFF
clock/ODO until display sleep. ON restores the user's setting multiplied by the current page readability policy.
