# NewDoe 0.11.14 beta

## 0.11.14: 언어 준비와 설치 상태 확인

**NewDoe-0.11.14.apk와 NewDoe-CFW-0.11.14.zip을 함께 사용하세요.** 같은 Gate의 0.11.4 이상에서는 기존 CFW 업데이트로 진행합니다. Gate와 B 언어팩 0.11.2는 바뀌지 않았습니다. 정상 언어팩은 다시 전송할 필요가 없습니다. ODO·UART 수정과 트립·정비 기록 보존도 유지합니다.

언어를 고르고 기존 재시작 안내를 확인하면, 부팅할 때 저장된 선택 언어의 문구·글꼴을 준비한 다음 홈 화면을 엽니다. 준비 중에는 SYSTEM STARTUP 안내가 표시됩니다. 앱 버튼을 누르거나 IGN을 오래 껐다 켤 필요가 없습니다. 차량 통신·트립 계산·정상 부팅 확정은 준비 중에도 계속됩니다. 자원이 없거나 손상됐거나 RAM 확보가 실패하면 기존 영어 표시로 돌아갑니다. 영어 전용 선택과 언어 선호도는 보존합니다.

펌웨어 확정 이후 언어 상태 조회 중 Bluetooth가 끊기면 동일 기기·Product·확정 상태를 다시 확인해 저장된 팩과 메뉴 준비 상태를 조회합니다. 이 과정에서 펌웨어나 팩을 자동 재전송하지 않습니다. 전송량이 끝나도 인덱스·폰트 검사와 메뉴 준비가 남을 수 있습니다. 마지막 준비 완료 안내까지 기다리세요. 계속 미확인이면 연결을 복구한 뒤 **언어 자원 설치 / 이어가기**를 누르세요. 펌웨어 재설치나 순정 복귀는 필요하지 않습니다.

새 Bootstrap은 설치 중 재사용 파일을 검사할 때도 설치 화면을 유지합니다. 전체 바는 파일별 전송·검증·NOR 기록·재검사 작업을 한 번씩 합산하며, 아래 바는 현재 작업을 나타냅니다. 검사 단계에서 아래 바가 다시 시작해도 전체 바는 뒤로 가지 않습니다. 순정 → Bootstrap의 수동 현재 상태 확인, 기본 No인 Bluetooth 초기화 질문, 마지막 설치 승인과 기록 안전 조건은 유지됩니다. Bootstrap 화면 변경만을 위해 순정 복귀하지 마세요.

휴대폰이 잠깐 바쁠 때 이미 받은 정상 응답을 시간 초과로 버리던 경로를 고쳤고, 주행 응답 대기를 3초에서 10초로 늘렸습니다. 실제 소켓 종료는 즉시 감지해 기존 재연결을 수행합니다. 카메라 실행 중 끊김은 차량이 센터에 있어 실기 재현을 완료하지 못했습니다. 해결이 확인됐다고 보장하지 않습니다.

소프트웨어 검증 빌드입니다. 모듈의 상시 전원을 유지하세요. 기존 구형 코드에서 순정 복귀 도중 전원이 끊길 경우 자동 복구를 보장하지 못하는 제한은 그대로입니다.

## 0.11.14: language startup and installation status

**Use NewDoe-0.11.14.apk with NewDoe-CFW-0.11.14.zip.** Versions 0.11.4 or newer with the same Gate use the existing CFW update route. Gate and B language resources 0.11.2 are unchanged. A healthy installed pack does not need retransmission. The live ODO/UART fixes and trip/maintenance record preservation remain.

After choosing a language and accepting the existing restart prompt, startup prepares the saved language's text and fonts before opening HOME. SYSTEM STARTUP appears during preparation. No phone button or long ignition cycle is required. Vehicle communication, trip accounting and boot confirmation continue during preparation. Missing/corrupt resources or failed RAM allocation fall back to English. English-only mode and language preferences remain saved.

If Bluetooth disconnects while checking language readiness after confirmed firmware installation, the app reconnects and verifies the same device, Product and confirmed boot before reading the saved pack and menu readiness again. It does not replay firmware or resource writes. Completed byte transfer can still be followed by index/font checks and menu preparation. Wait for the final readiness result. If the result remains unconfirmed, reconnect and select **Install / resume language resources**. Firmware reinstallation or stock recovery is not required.

The new Bootstrap keeps the installation screen while inspecting reused files. The overall bar accounts for each file's transfer, verification, NOR write and readback once; the lower bar shows the current operation. Repeated reads do not rewind overall progress. Manual Check current status after stock transfer, the default-No Bluetooth reset question, final approval and write-safety conditions remain unchanged. Do not return to stock just to update the Bootstrap display.

The phone now parses a valid reply already received before declaring a timeout, and riding response waits allow 10 seconds instead of 3. Actual socket closure still triggers the existing reconnect path immediately. Camera-triggered disconnection could not be reproduced with the vehicle because it is at a service centre; this release does not claim a verified hardware fix for that symptom.

Software validation only. Keep the module powered. The previously accepted old-code stock-return power-interruption limitation remains unchanged.
