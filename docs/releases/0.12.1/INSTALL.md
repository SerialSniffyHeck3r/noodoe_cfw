# NewDoe 0.12.1 beta

장기 주차 후 Bluetooth 복구, 주유 감지와 주행 요약의 오일 표시를 개선하고 음악 볼륨 팝업을 추가했습니다. APK와 CFW ZIP을 함께 업데이트하세요.

- **음악 볼륨:** 음악 페이지에서 O 길게 → UP 올리기 / DOWN 내리기 / O 짧게 닫기. 폰의 미디어 음량을 한 단계씩 조절합니다.
- **Bluetooth:** 정상 동작했던 무선 모듈의 일시적인 오류를 감지하면 종료를 확인한 뒤 최대 3회 재시도합니다. 페어링 정보와 수동 연결 중지는 유지합니다.
- **SINCE REFUEL:** 연료 수신이 느리거나 잠깐 무효가 되어도 정상 수신값을 모아 주유를 판정합니다. 두 칸 이상의 상승이 안정적으로 유지돼야 합니다.
- **주행 요약:** 키 OFF 뒤에도 이번 부팅에서 마지막으로 받은 ODO로 오일 비율을 계산합니다. ODO를 플래시에 저장하지 않으며 트립·정비 기록은 유지합니다.
- **앱 HOME:** 속도의 km/h와 ODO 사이 간격을 넓혔습니다.

## 업데이트

**0.12.0 → 0.12.1은 일반 CFW 업데이트입니다.** Gate·Bootstrap·B 0.11.2 언어팩은 그대로입니다.

1. `NewDoe-0.12.1.apk`를 기존 앱 위에 설치합니다. 앱 데이터와 페어링을 지우지 마세요.
2. 주행 연동을 멈추고 `NewDoe-CFW-0.12.1.zip`을 선택합니다.
3. **현재 기기 확인 → CFW 업데이트**를 선택하고 안전 안내를 확인합니다.
4. 전송·검증·재시작이 끝나면 누도의 **CFW UPDATE** 화면을 앱에서 확인합니다.
5. 정상 B 0.11.2 언어팩은 다시 보내지 않습니다. 설정·트립·정비·사진을 유지합니다.

0.11.x 등 구형 Gate에서는 앱 안내대로 **순정 복귀 → CFW 데이터 유지 → 재설치 → 보관 데이터 복원**으로 진행하세요. 순정 복귀·설치 중 전원을 분리하지 마세요. 이미 설치된 구버전 순정 복귀 코드의 전원 차단 취약 구간은 그대로이며, 이때 중단되면 외부 복구 장비가 필요할 수 있습니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

Update both APK and CFW ZIP to **0.12.1**.

- Hold O on the music page to open volume controls: UP raises media volume, DOWN lowers it, short O closes. Calls and warnings take priority.
- Transient faults after a working Bluetooth session get up to three controller recovery attempts after transport shutdown. Bonds and manual stop are preserved.
- Refuelling tolerates sparse readings and temporarily unavailable fuel data. A stable rise of at least two bars is still required.
- Key-OFF Ride Summary uses this boot's last received ODO for the oil percentage. There is no persistent ODO backup or correction; trips and maintenance records remain.
- The Android speed unit has more space before ODO.

**From 0.12.0, use normal CFW update.** Install the APK over the existing app, pause the riding connection, select the new ZIP, check the device, and choose CFW update. Complete the safety screens and confirm CFW UPDATE after restart. Keep healthy B 0.11.2 language resources.

Older Gate versions, including 0.11.x, follow the app's keep-data stock return/reinstall route. Keep power connected. The previously accepted interruption limitation in already-installed legacy stock-return code remains; interruption there may require external programming equipment.

Software validation details are in `verification.json`. The reporting vehicle's radio fault and fuel trace have not been captured on hardware; please report any remaining failure with a diagnostic export and Dashboard frames.
