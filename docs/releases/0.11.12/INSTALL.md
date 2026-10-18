# NewDoe 0.11.12 beta — Parking power

## 한국어

**APK와 펌웨어 ZIP을 함께 업데이트하세요. APK만 설치하면 누도의 절전 코드는 바뀌지 않습니다.** 이미 배터리가 크게 방전됐다면 충전·점검을 먼저 하고, 전원이 안정된 상태에서 설치하세요.

- `NewDoe-0.11.12.apk`와 `NewDoe-CFW-0.11.12.zip`을 사용합니다. 0.11.4 이후의 같은 Gate 기기는 기존 **CFW 업데이트**로 설치합니다. 0.10.x 또는 이전 Gate 기기는 앱의 기존 안내대로 데이터를 유지해 순정으로 돌아간 후 재설치합니다.
- Gate 0.11.4, Bootstrap 0.11.8, 언어 자원 0.11.2는 그대로입니다. 이번 변경 때문에 재페어링하거나 언어팩을 다시 보낼 필요는 없습니다. 기존 업데이트 확인·복구 절차도 유지됩니다.
- **설정 → 시스템 → 전원 순서 → Long parking power off**를 확인하세요. 기본값은 **After 2 days**이고 3일·7일·Off도 지원합니다. 기존 설정은 보존되므로, 전에 Off로 바꿨다면 업데이트 후에도 Off입니다.
- 정상적인 화면·Bluetooth 유지 시간, 웰컴 라이트, 주행 화면은 유지합니다. 장기 주차 종료 이후에는 휴대폰 접근으로 다시 연결하거나 웰컴 라이트를 켜지 않으며, IGN을 켜면 Gate부터 다시 시작합니다.

### 수정 내용

- RTC에 문제가 있을 때 CPU가 빈 루프를 돌지 않고 일반 Sleep으로 대기합니다. 정상 RTC에서는 기존 tickless Sleep/STOP을 사용합니다.
- Bluetooth·계기판 UART 오류 때 실제 DMA가 멈췄는지 확인해 절전 진입을 허용합니다. 오류가 난 버퍼를 정상으로 취급하거나 재사용하지 않습니다.
- 장기 주차 시간 계산과 종료 준비를 StorageTask 한 곳에서 처리해 화면/설정 초기화 실패에도 동작하도록 했습니다.
- 새 설정 스냅샷을 준비하지 못하면 30초 뒤 마지막으로 저장된 데이터를 보존한 채 종료 준비를 진행합니다. 아직 저장하지 못한 변경은 남지 않을 수 있습니다.
- NOR 쓰기가 끝나고 저장 담당 태스크가 정지했음을 확인한 뒤에는, 화면이나 Bluetooth 오류가 전원 차단 요청을 끝없이 막지 못하게 했습니다. 주변 장치 종료 유예는 10초입니다. 진행 중인 펌웨어·언어팩·NOR 기록을 타이머로 끊지 않습니다.

### 확인할 점

이번 릴리스는 **소프트웨어 검증을 마친 베타**입니다. 실제 차량에서의 전류, 전원 차단 회로 동작, 일주일 주차 후 배터리 상태는 측정하지 않았습니다. 화면이 꺼졌거나 전원 차단 GPIO를 설정했다는 사실만으로 무전류 상태를 보장하지 않습니다. 전원 차단 요청 후에도 전원이 남으면 절전을 계속 시도하며, DMA가 실제로 동작 중인 상태를 안전한 STOP으로 간주하지 않습니다.

구버전 펌웨어의 짧은 주기 작업이 STOP 진입을 막던 문제는 이전 0.11 계열에 포함됐고 이번에도 유지됩니다. 이것이 보고된 8V 방전의 유일한 원인이었다고 확정할 수는 없습니다. 충전 후 배터리 상태와 IGN OFF 대기 전류를 확인하고, 필요하면 누도 연결/분리 상태를 비교해야 합니다.

이전에 허용한 **구버전 코드에서 순정 복귀하는 도중의 전원 중단 취약 구간**은 그대로입니다. 새 코드가 실행되기 전의 그 구간을 이번 업데이트가 고치지는 않습니다.

## English

**Install both the APK and matching firmware ZIP. Updating the APK alone does not update the Noodoe power-management code.** Charge and check a deeply discharged battery first; install with stable power.

- Use `NewDoe-0.11.12.apk` and `NewDoe-CFW-0.11.12.zip`. Devices with the same Gate from 0.11.4 onward use normal **CFW update**. For 0.10.x or an older Gate, follow the existing keep-data stock-return/reinstall instructions in the app.
- Gate 0.11.4, Bootstrap 0.11.8 and language resources 0.11.2 are unchanged. This update does not require pairing again or reinstalling the language pack. Existing update confirmation and recovery procedures remain.
- Check **Settings → System → Power sequence → Long parking power off**. The default is **After 2 days**; 3 days, 7 days and Off remain available. Existing preferences are preserved, including Off.
- Normal screen/Bluetooth retention, welcome lights and riding visuals are unchanged. After long-parking shutdown, phone approach no longer reconnects or triggers welcome lights. Turning IGN on restarts through Gate.

### Changes

- An unavailable RTC falls back to ordinary CPU Sleep instead of spinning. Healthy RTC operation retains the existing tickless Sleep/STOP path.
- Faulted Bluetooth and dashboard UART transports can acknowledge sleep only after their actual DMA has stopped. Quarantined buffers remain unavailable for reuse.
- StorageTask now owns both the parking timer and shutdown transition, including when UI/settings restoration fails.
- If a fresh settings snapshot cannot be prepared, a 30-second grace period permits shutdown preparation while preserving the last durable records. Unsaved changes may be lost.
- Once NOR writes have drained and the storage owner has acknowledged stopping, failed display/radio owners cannot indefinitely veto the external power-off request. Peripheral grace is 10 seconds. Active firmware, language and NOR transactions are not interrupted by this deadline.

### Validation limits

This is a **software-verified beta**, not a hardware current or battery-endurance qualification. Actual parking current, external cutoff behavior and a week of vehicle storage have not been measured. A dark screen or a GPIO power-off request does not prove zero current. If power remains present, the firmware continues attempting low-power operation; active DMA is never falsely declared safe for STOP.

The earlier 0.11 fix for short polling intervals preventing STOP remains included. Software findings do not prove the sole cause of the reported 8V discharge. Check battery health after charging and measure IGN-OFF current, comparing the Noodoe connected/disconnected where necessary.

The previously accepted **power-interruption limitation while old firmware returns to stock** remains. This update cannot repair that interval before the new code runs.

[Detailed parking guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/36-NewDoe-Recovery.en.md)
