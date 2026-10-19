# NewDoe 0.11.15 beta

## 0.11.15: 설치 때 검사하고, 부팅 때는 불러오기

**NewDoe-0.11.15.apk와 NewDoe-CFW-0.11.15.zip을 함께 사용하세요.** 같은 Gate의 0.11.4 이상에서는 기존 CFW 업데이트로 진행합니다. Gate·Bootstrap·B 언어 자원 0.11.2는 그대로입니다. 이미 정상 설치된 언어팩을 다시 보내거나 순정으로 돌아갈 필요가 없습니다. 기존 ODO/UART 수정과 트립·정비·설정 보존도 유지합니다.

언어팩 전체 해시·인덱스 검사는 새 팩을 설치하거나 갱신해 저장을 확정할 때 수행합니다. 이후 부팅·언어 변경·상태 다시 확인에서는 전체 무결성 검사를 반복하지 않습니다. 설치된 팩의 작은 헤더와 저장된 식별 정보를 열고, 선택 언어의 문구와 필요한 글자만 불러옵니다. 파일을 읽을 수 없거나 사용하려는 레코드의 길이·주소·문자 형식이 잘못된 경우를 막는 검사는 유지합니다. 나중에 생긴 모든 미사용 영역 손상을 매번 찾아내는 방식은 아닙니다.

언어 변경 또는 팩 갱신 뒤 재시작하면 기존 푸른 상태 화면에 **Loading menu text / Preparing menu fonts**와 현재 단계·개수를 보여주고 준비가 끝난 뒤 HOME을 엽니다. 언어가 그대로인 다음 부팅에서는 특별한 언어 준비 안내를 반복하지 않습니다. 완전히 재부팅하면 RAM에 필요한 문구·글자를 다시 올리는 읽기 시간은 남지만, 팩 전체를 재검사하지 않습니다. STOP에서 그대로 깨어날 때는 유지된 RAM을 사용합니다. 설정을 저장한 뒤 재시작하는 기존 승인 절차는 바뀌지 않았습니다.

재시작 후 폰 버튼이나 IGN 전환이 언어 활성화 조건이 아닙니다. 한국어를 선택했으면 필요한 한국어 글자가 준비된 상태로 HOME을 엽니다. 자원/메모리 오류에서는 영어로 돌아가며 저장된 언어 선호도는 지우지 않습니다. 전체 팩을 RAM이나 EVE에 복사하지 않습니다. 모든 메뉴 번역은 팩에 유지하고, 영어는 기존 Lato를 사용합니다.

설치 완료 후 Bluetooth 상태 조회가 끊기면 앱은 같은 기기·Product·확정 상태를 다시 확인합니다. 펌웨어나 팩 쓰기를 반복하지 않습니다. 재연결 후에도 미확인이라면 **언어 자원 설치 / 이어가기**로 상태를 다시 확인하세요. 펌웨어 완료와 언어 준비 완료는 별개입니다.

휴대폰이 잠깐 바빠졌을 때 이미 수신한 응답을 버리지 않도록 했으며, 이번에는 큐에 나뉘어 도착한 응답까지 한 번에 확인합니다. 실제 EOF는 기존 자동 재연결로 넘어가고, 응답 시간 초과와 소켓 종료를 진단에서 구분합니다. 카메라 실행 중 끊김은 차량이 센터에 있어 실기 재현을 못 했으므로 완치됐다고 보장하지 않습니다.

구형 펌웨어의 순정 복귀 과정에는 새 코드가 실행되기 전 내부 메타데이터를 다시 쓰는 구간이 있습니다. 그 순간 모듈 전원이 끊기면 구형 부팅 코드가 복구하지 못할 수 있습니다. 이 이미 설치된 옛 코드의 제한은 그대로입니다. IGN 신호를 끄는 것과 모듈의 상시 전원을 끊는 것은 다릅니다. 업데이트·복구 중 모듈 전원을 유지하세요. 이번 검증은 소프트웨어 검증입니다.

## 0.11.15: verify on installation, load on boot

**Use NewDoe-0.11.15.apk with NewDoe-CFW-0.11.15.zip.** Versions 0.11.4 or newer with the same Gate use the existing CFW update route. Gate, Bootstrap and B resources 0.11.2 are unchanged. An already installed healthy pack needs no retransmission or stock roundtrip. Existing ODO/UART fixes and trip, maintenance and settings preservation remain.

Complete pack hashes and indices are verified when installing or updating resources, before publication. Subsequent boots, language changes and status reloads do not repeat that full integrity scan. They open the small header and saved resource identity, then load the selected language's text and required glyphs. Read failures and invalid used-record lengths, addresses or encoding still fail safely. This does not repeatedly detect every later corruption in unused parts of the pack.

After changing language or updating the pack, the existing blue status screen shows **Loading menu text / Preparing menu fonts**, with stage and item counts, before HOME opens. An unchanged subsequent boot does not repeat the special language preparation notice. A cold restart still needs to reload its selected text and glyphs into RAM; it does not rehash the entire pack. STOP wake reuses retained RAM. The existing save-and-confirm restart flow is unchanged.

No phone button or IGN transition is required to activate the language. HOME opens with the selected alphabet prepared. Resource or allocation failures fall back to English without deleting the saved preference. Neither RAM nor EVE receives the whole pack. All menu translations remain in the pack; English keeps its existing Lato appearance.

If final language readback disconnects, the app reconnects and verifies the same device, Product and confirmed boot. It does not replay firmware or resource writes. If readiness remains unconfirmed, reconnect and use **Install / resume language resources** to check again. Firmware completion and language readiness are separate outcomes.

Replies already received during a phone scheduling stall are processed before timeout, including a response split across the receive queue. Real EOF still invokes existing automatic reconnection; reply timeout and socket failure have distinct diagnostic categories. Camera-triggered disconnection has not been reproduced with the vehicle, which is at a service centre; no verified hardware fix is claimed.

Previously installed old firmware has a stock-return interval that rewrites internal metadata before new code can run. Cutting module power in that interval can leave the old boot path unable to recover automatically. This accepted legacy limitation remains. Turning IGN off is different from disconnecting the module's permanent supply. Keep module power connected throughout updates and recovery. Validation in this release is software only.
