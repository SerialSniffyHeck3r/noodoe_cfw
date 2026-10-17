# NewDoe 0.11.4 beta

## 0.11.4 Gate update

This release replaces Gate. **From every earlier release, including 0.11.3, use keep-data stock return and reinstall 0.11.4.** Install the 0.11.4 APK over the existing app, select the matching ZIP and follow its Gate migration guidance. A Product-only update cannot replace Gate.

1. Stop riding integration and choose **Keep CFW data → Return to stock → Check current device**.
2. Install **NewDoe-CFW-0.11.4.zip**, selecting **Restore saved data** in Bootstrap. If connection fails, follow the existing manual pairing help, then **Check current device → Continue from here**.
3. Finish the actual **CFW UPDATE** confirmation and the phone's firmware confirmation after successful startup.
4. Reuse **NewDoe-LANGUAGE-B-0.11.2.zip**. An identical verified pack does not need retransmission. The 0.11.3 language stability fixes remain included.

Small text and the version are now at least as large as the main menu; menus are centered. Dashboard values and status use separate rows. Once the runnable firmware passes verification, **Start NewDoe** works while optional backups are still being checked.

Automatic recovery follows **three consecutive unconfirmed boot failures**. Healthy boot confirmation clears the count. Power loss and BOR exclude the last interrupted handoff without discarding earlier real failures. Invalid firmware/storage can still enter recovery immediately to avoid running corrupted code. Only observed DOWN/O input is described as a button request; other entries show interrupted startup, saved recovery state or the actual error.

**Maintain module power throughout stock return and reinstallation; do not crank the engine.** The previously accepted old-code stock-return power-loss limitation remains. New Gate BOR handling cannot repair the older installer code already running on a device.

Hardware check: verify V0.11.4 at startup, readable centered menus, More/Dashboard, Start becoming available during backup checks, and normal reboot without an unexpected recovery request. Do not deliberately cut installation power. ARM execution and display commands were software-tested; this release has no physical power or LCD qualification.

## 0.11.4 Gate 업데이트

이번에는 Gate가 바뀝니다. **0.11.3을 포함한 이전 버전에서는 데이터 유지 순정 복귀 후 0.11.4를 재설치하세요.** 0.11.4 APK를 기존 앱 위에 설치하고 같은 버전 ZIP을 선택한 뒤, 앱의 Gate 교체 안내를 따릅니다. 일반 Product 업데이트만으로 새 Gate를 적용할 수 없습니다.

1. 주행 연동을 중지하고 **CFW 데이터 유지 → 순정 복귀 → 현재 기기 확인**을 진행하세요.
2. **NewDoe-CFW-0.11.4.zip**으로 설치하고 Bootstrap에서 **저장된 데이터 복구**를 선택하세요. 연결이 안 되면 기존 연결 도움말에 따라 수동 재페어링 후 **현재 기기 확인 → 계속하기**를 사용합니다.
3. 정상 실행 후 실제 **CFW UPDATE** 확인과 앱의 펌웨어 확정을 끝내세요.
4. **NewDoe-LANGUAGE-B-0.11.2.zip**은 이번에도 그대로 사용합니다. 이미 검증된 같은 팩은 다시 보내지 않습니다. 0.11.3의 언어 안정성 수정도 유지됩니다.

작은 글씨와 버전 글씨를 메인 메뉴 크기 이상으로 키우고 메뉴를 중앙 정렬했습니다. Dashboard는 값과 상태를 각각 줄로 나눴습니다. 정상 펌웨어의 실행 검증이 끝나면 백업 검사 중에도 **Start NewDoe**를 선택할 수 있습니다.

확인되지 않은 부팅 실패가 **연속 3회** 누적되면 자동 복구로 들어갑니다. 정상 부팅 확인은 횟수를 초기화하고, 전원 차단·BOR로 중단된 마지막 시도는 실패로 세지 않습니다. 펌웨어나 저장소 검증 실패는 손상된 코드를 실행하지 않도록 즉시 복구로 들어갈 수 있습니다. 실제 DOWN/O 입력을 확인한 경우에만 버튼 요청이라고 표시하고, 다른 진입은 부팅 중단·저장된 복구 상태·오류로 구분합니다.

**순정 복귀·재설치 중 상시 전원을 유지하고 스타터를 돌리지 마세요.** 기존에 수락한 구버전 순정 복귀 구간의 전원 중단 한계는 남아 있습니다. 새 Gate의 BOR 처리가 구버전 설치 코드까지 고치는 것은 아닙니다.

실기 확인: 부팅 버전 V0.11.4 → 메뉴 글씨·중앙 정렬 → More/Dashboard → 복구 검사 중 Start 활성화 → 정상 부팅 후 다시 켰을 때 불필요한 복구 진입 여부를 확인하세요. 고의로 설치 중 전원을 끊어 시험하지 마세요. 소프트웨어 ARM 실행과 화면 명령 검증을 완료했으며, 이번 변경의 실기 전원·LCD 시험은 수행하지 않았습니다.
