# Noodoe 도너 USB 보존 및 파일시스템·콘텐츠 분석

2026-09-09 KST. 사용자가 연결한 실제 AK550 도너 Noodoe USB에서 자료를 로컬로 보존하고, 이후 분석·복구는 로컬 이미지에서 수행했다.

> **2026-09-11 후속 확인:** 실물 내부 플래시 A/B의 APP가 V5.16 OTA와 완전히 일치했다. 복원한 resident 부트 코드에서 USB 비노출 NOR 끝부분의 BL staging `0x07F80000`, APP staging `0x07F90000`도 확인했다. [부트·BT 업데이트 연구노트](2026-09-11-noodoe-bootloader-and-bluetooth-update.md)를 참조한다. 이 발견이 아래 USB 이미지의 백업 범위를 넓히는 것은 아니며, 숨겨진 512 KiB는 해당 이미지에 없다.

**USB에 노출된 127.5MiB 전체를 두 번 읽었고 두 이미지가 바이트 단위로 같다. 파일이 일부 보이지 않는 현상에서는 실제 FAT 연결 오류와 디렉터리 충돌이 확인됐다. 순정 리소스와 해시가 맞는 내용도 이미지에서 추가 추출했다.**

## 1. 무엇을 백업했는가

| 항목 | 확인값 |
|---|---|
| Windows 장치 | STM Product USB Device / D: NOODOE |
| USB 식별 | VID 0483, PID 5720, serial [redacted USB serial], inquiry revision 0.01 |
| 노출 볼륨 크기 | 133,693,440바이트 = 127.5MiB |
| 섹터/클러스터 | 4096바이트 섹터, 8섹터/클러스터 = 32KiB |
| 파티션 시작 | offset 0 |
| 첫 읽기 | 2026-09-09 07:27:07–07:29:30 UTC |
| 두 번째 읽기 | 2026-09-09 07:30:12–07:32:55 UTC |
| 두 이미지 비교 | 크기·SHA-256 일치, 별도 검사에서 모든 바이트 일치 |
| 정상 파일 복사 | 193개, 1,710,403바이트 |

두 이미지의 SHA-256:

```text
0ab3d0de83f774e61eb548e5109fa53f1f2f86cd138b9e5a7d16fef8a45879b8
```

보존 파일:

- [첫 USB 볼륨 이미지](../evidence/usb/2026-09-09-noodoe-[redacted USB serial]/noodoe-usb-volume.img)
- [두 번째 USB 볼륨 이미지](../evidence/usb/2026-09-09-noodoe-[redacted USB serial]/noodoe-usb-volume-read2.img)
- [정상 복사 파일의 SHA-256 목록](../evidence/usb/2026-09-09-noodoe-[redacted USB serial]/copied-files-sha256.json)
- [장치 정보](../evidence/usb/2026-09-09-noodoe-[redacted USB serial]/device.json), [PnP 정보](../evidence/usb/2026-09-09-noodoe-[redacted USB serial]/pnp.json)
- [읽기 및 이미지 비교 검증](../analysis/2026-09-09-noodoe-usb-acquisition/acquisition-verification.json)

`PhysicalDrive1`의 읽기 open은 OS에서 거부했으나 `\\.\D:` 볼륨 handle의 읽기는 허용됐다. partition offset0/size와 USB 용량이 같아 추가 권한 요청 없이 노출된 볼륨 전부를 읽었다. 수집 스크립트는 FileAccess.Read로 source를 열고 로컬에만 출력했다.

USB는 mounted/live 상태였고 write blocker는 없었다. 직접 실행한 source 쓰기·포맷·파일시스템 복구 명령은 없다. 다만 이것만으로 Windows나 장치 펌웨어가 연결 전후에 배경 쓰기를 전혀 하지 않았다고 단정하지 않는다. 두 번 읽은 구간에서 얻은 이미지가 일치한다는 것이 검증 범위다.

## 2. 이 USB 공간은 STM32 부트로더인가

**아니다. 기존 SR1.5 V516의 원시 코드에서는 외부 SPI NOR에 연결된다.** 정적 예측과 실제 관측이 정확히 맞는다.

```text
USB MSC READ(10)
 → storage read callback 08073F32
 → 0803F7E0 →0803624E
 → 외부 NOR read 08066472
 → SPI5
```

- NOR geometry: 128MiB, 4096바이트 섹터 32768개.
- USB capacity는 여기서 128개 섹터를 빼서 32640개를 광고한다.
- `32640 × 4096 = 133693440`으로 실제 USB 크기와 같다.
- LBA0은 NOR offset0에 매핑된다. USB 범위는 `0x00000000..0x07F7FFFF`.
- 마지막 `0x07F80000..0x07FFFFFF`, 512KiB는 표준 USB 노출에서 제외된다.
- descriptor의 0483:5720 및 STM/Product/0.01도 실제 USB와 일치한다.

따라서 이번 작업은 **외부 저장공간의 USB 노출 부분 보존**이다. STM32 내부 플래시·부트로더, 외부 NOR 마지막512KiB는 이번 이미지에 포함되지 않는다. 관측값이 V516의 저장소 구현과 맞는다는 사실도 도너의 실행 펌웨어 전체가 V516이라는 증명은 아니다.

이 MSC 경로에는 파일명이나 숨김 속성을 해석해 특정 파일을 차단하는 필터가 확인되지 않았다. READ/WRITE는 블록 단위로 처리된다. USB 시작 뒤 후속 일반 초기화가 계속되지만, 화면·조도·FatFS 작업의 정지나 상호 배제까지 확인하지는 못했다. 이를 FAT 손상의 원인이나 밝기 동작 실패 원인으로 연결하지 않는다.

[펌웨어 매핑의 주소·바이트·재현 근거](../analysis/2026-09-09-noodoe-usb-acquisition/firmware/README.md)

## 3. 일부 파일이 안 보이는 직접적인 이유

Windows는 `font`, `notification`, `speed`, `weather`, `compass` 디렉터리에 ERROR1392, 손상되어 읽을 수 없음을 반환했다. 열거된231파일 중38개도 복사 실패했다. 디렉터리 열거 자체가 실패한 영역은 이231개에 포함되지 않으므로 이것을 전체 파일 수로 취급하지 않는다.

이미지의 BPB로 계산하면 데이터 클러스터는4078개다. FAT 종류를 클러스터 수로 판정하는 Microsoft 규칙상 FAT12이며 BPB 문자열도 FAT12다. Windows Get-Partition의 Type FAT16 표시는 별도로 기록했다. [Microsoft FAT Specification §3.5](https://www.scs.stanford.edu/~zyedidia/docs/_other/fat.pdf)

| 발견 | 의미 |
|---|---|
| `Default_Dashboard`와 `System Volume Information`이 동일한 cluster2를 가리킴 | 디렉터리 교차 연결. 기본 대시보드 경로에서 Windows 메타데이터가 보이는 이유 |
| 여러 폴더 시작 클러스터의 FAT 값이 free 또는 잘못된 다음 클러스터 | 정상 디렉터리 탐색·파일 읽기를 방해하는 연결 오류 |
| `font`가 가리키는 시작 데이터가 JPEG 헤더 | 현재 그 위치를 정상 폰트 디렉터리로 읽을 수 없음. FAT 표만 고치면 해결된다는 가정은 성립하지 않음 |
| `resource_config.json`은119991바이트인데 FAT 체인은 시작 cluster2603에서 종료 | 정상 체인으로는32768바이트밖에 읽을 수 없음 |
| FAT 복사본 두 개가 동일하지만 예약 엔트리도 비정상 | 두 FAT가 같다는 것이 정상이라는 뜻은 아님 |

핵심 클러스터를 FAT16으로 다시 해석해도 free/비정상 연결 문제가 남는다. 따라서 FAT 종류 표기 차이만으로 오류를 설명하거나 고칠 수 없다.

손상이 생긴 시점과 원인은 미확정이다. 이 자료만으로 Windows, 특정 사용자의 조작, 특정 펌웨어 결함 중 하나를 원인으로 지목하지 않는다.

## 4. 로컬 이미지에서 추가로 확보한 자료

`resource_config.json`은 디렉터리 엔트리의 시작 위치에서119991바이트가 연속 배치됐다는 가정으로 추출했다. 전체 JSON 파싱이 성공했으며, 아래 순정 OTA 대조도 맞았다. **원래 FAT 체인을 정상 복원한 파일이 아니라 검증을 추가한 연속 배치 후보**로 보관한다.

- [설정 복구 후보](../analysis/2026-09-09-noodoe-usb-acquisition/filesystem/contiguous-candidates/resource_config.json)
- SHA-256: `4c9230e61e214dc127e12049e8c46a5d9959f2f97d01847c31c59be508824050`

추가 추출의 최종 집계:

| 검증 기준 | 결과 |
|---|---:|
| 참조 allFiles 목록의 고유 내용 MD5 | 1043개 |
| 이미지에서 찾아 MD5가 일치한 고유 내용 | **965개** |
| 그 내용으로 대응 가능한 참조 경로 | 1089개 중1011개 |
| 전체 chunk CRC/IEND/IDAT zlib 검사에 성공한 PNG | 568개 위치, 중복 제거484개 |
| JPEG marker와 실제 decode 검증에 성공한 JPEG | 29개 위치, 중복 제거26개 |

이 숫자들은 서로 겹치므로 합산하면 안 된다. 특히 **1011은 현재 설치된 정상 파일 수나 현재 파일트리 완전 복구 수가 아니다.** 참조 리소스 목록에 맞는 내용이 이미지 안에 존재했다는 뜻이며, 비할당 영역·이전 설치·중복 배치가 포함될 수 있다.

확인한 실제 바이트만 `filesystem/manifest-matched/`에 순정 manifest 경로로 정리했다. OTA에서 파일 내용을 가져와 빈 부분을 채운 것은 아니다. 각 파일의 image offset, 크기, MD5, SHA-256, 가능한 디렉터리 엔트리 위치를 [출처 인덱스](../analysis/2026-09-09-noodoe-usb-acquisition/filesystem/manifest-matched-index.json)에 남겼다. 별도 검증에서1011경로 모두 저장한 바이트가 원본 이미지 안의 기록 위치와 같고, 참조 경로·MD5 조합도 일치함을 확인했다.

참조 목록에서 미확보된78경로는 Default_Dashboard77개와 resource_config 자기 항목1개다. 위119991바이트 JSON 후보는 별도로 존재하지만 manifest의 해당 자기 항목 MD5와는 다르므로 해시 일치 복구에 포함하지 않았다. 폰트 디렉터리의 원래 전체 목록/내용도 확보했다고 주장하지 않는다.

- [파일시스템·복구 상세](../analysis/2026-09-09-noodoe-usb-acquisition/filesystem/README.md)
- [최종 복구 집계](../analysis/2026-09-09-noodoe-usb-acquisition/filesystem/verified-recovery-summary.json)
- [원본 이미지와 복구 파일의 독립 대조](../analysis/2026-09-09-noodoe-usb-acquisition/manifest-recovery-verification.json)

## 5. 데이터 안에서 확인한 Noodoe 구조

정상 복사193개는 PNG176개, JPEG9개, JSON cfg4개, Windows 메타데이터4개다. PNG176개 모두 실제 RGBA8 PNG이며 chunk CRC·압축·scanline 검사를 통과했다. JPEG9개는480×480이다. 확인한 PNG 집합을 EVE 전용 raw blob이나 암호화 파일로 해석할 근거는 없다.

대시보드 cfg는 평문 JSON이며 `files`와 `configuration.widgets` 구조다.

| 슬롯 | 확인한 위젯 |
|---|---|
| 01 | BackgroundWidget, ClockDigitWidget, DateWidget |
| 02 | BackgroundWidget, ConditionTextWidget, TemperatureWidget, WeatherConditionWidget |
| 03 | BackgroundWidget, OdometerWidget, SpeedBarWidget, SpeedDigitWidget |
| 05 | BackgroundWidget, MembersWidget |

01–03의 각21개 자산 참조는 정상 파일 복사로 모두 확보했다. 03의 OdometerWidget는 숫자 이미지10개와 자리 위치6개를 갖는다. 이전 펌웨어에서 추적한 위젯명과 여섯 자리 ODO 렌더러를 실제 저장 데이터로 대조한 결과다. 05의 배경은 정상 복사 단계에서는 실패했지만, 후속 이미지 추출에서 순정 manifest 해시와 맞는 내용을 확보했다.

02의 TemperatureWidget는 날씨 위젯들과 함께 사용된다. 이름만으로 UART의 raw-40 온도형 필드와 연결하지 않는다. 사진 픽셀과 불필요한 그룹·계정 식별자는 보고서에 노출하지 않았다.

## 6. 순정 APK/OTA 자료와의 연결

복구 후보의 metadata는 majorVersion5/minorVersion14, ResourceID1/LangpackID2다. `files`379개는 `allFiles`1089개의 부분집합이며, 실제 코드에서 두 목록이 수행하는 정확한 역할까지 이번 데이터 검사만으로 확정하지 않는다.

**allFiles 배열은 기존에 확보한 공식 common Pack B v5.14의 배열과 순서·값까지 완전히 같다.** 정상 복사된117개도 같은 OTA 내용과 SHA-256이 같다(네비게이션116 + 05 cfg1).

비교한 기존 순정 파일:

```text
artifacts/ota-archive/2026-08-31-full/blobs/resource/
1526957902822-s4-resource_common_Pack_B_v5.14.zip
SHA256 e36be99a28614781951b2624390e671890b007628d30451bb2263cfe441b692e
```

이는 리소스 계열을 연결하는 증거다. 복구 후보에는 `files` 필드가 추가돼 있으므로 설정 전체 파일이 OTA 설정과 바이트 동일하다는 주장은 아니다. 공통 자산은 다른 OTA 세대에서도 재사용된다. **리소스5.14라는 단서로 도너 MCU 펌웨어 버전5.14 또는5.16을 판정하지 않는다.**

[콘텐츠 형식·슬롯 구조·OTA 해시 대조 상세](../analysis/2026-09-09-noodoe-usb-acquisition/content/README.md)

## 7. 실물 관찰의 현재 상태

사용자가 보고한 Speed 핀 전환→1km/h 표시는 속도 입력이 펄스에 반응한다는 가설을 지지한다. 입력 전압·회로 형식·주파수 환산은 미확정이다. Noodoe↔계기판에서 본 펄스도 아직 실제 UART 캡처로 확인하지 않았다. SWD 미식별과 밝기 조절 미동작 관찰은 계속 별도 과제로 남는다.

[실물 관찰 기록](2026-09-09-donor-hardware-usb-observations.md)

## 재현 산출물

- [분석 디렉터리 안내](../analysis/2026-09-09-noodoe-usb-acquisition/README.md)
- [두 볼륨 이미지 검증 스크립트](../analysis/2026-09-09-noodoe-usb-acquisition/verify_acquisition.py): 로컬 이미지 비교 및 복사 파일 해시 작성; 장치 접근 없음.
- [참조 경로별 복구 바이트 검증](../analysis/2026-09-09-noodoe-usb-acquisition/verify_manifest_recovery.py): 로컬 이미지 위치와 복구 파일·manifest 대조.
- 실제 장치 취득 스크립트와 로그는 보존 폴더에 있다. 재실행은 별도의 새로운 장치 읽기이므로 단순 오프라인 검증과 구분한다.
