# FuckNudo 0.9.59 — warning compositor, manual range and F4 compatibility

큰 연료 경고의 음영을 새 방식으로 구현하고, 수동 지도 축척 지연과 업데이트
재시작 경로를 수정했습니다. 다른 차종에 연결한 F4 누도도 모델명·PCBA 때문에
설치가 거부되지 않도록 호환성 검사를 변경했습니다.

- **연료 경고:** 일반 화면을 다 그린 뒤 EVE가 전체 검은 음영과 경고를 마지막에
  그립니다. 스텐실·알파·블렌드 상태를 명시적으로 설정하며, 음영은 아이콘의
  깜빡임과 독립적입니다. 기존 Google Rounded 연료 아이콘과 글꼴을 유지합니다.
- **지도:** AUTO/MANUAL·선호도 글씨를16→20px로 키우고 도로 종류별 색상을
  더 구분했습니다. MANUAL은 축척을 즉시 변경하며 전환 애니메이션을 사용하지
  않습니다. 새 타일이 도착하기 전에도 남아 있는 지도를 해당 축척으로 표시합니다.
  AUTO의 준비 완료 후600ms 전환과−2~+2 선호도는 유지합니다.
- **설치 허용:** F4(HW0), 부트로더 **0.10~0.19**, 순정 **5.16**을 허용합니다.
  SAA1AA 등 모델명과 PCBA 값으로 차단하지 않습니다. 실제 기기의 부트로더를
  캡처·검증해 묶는 절차와 원본 백업, 무결성·시험부팅 확인은 유지합니다.
- **업데이트:** 본체 버튼 승인인데도 전화기의 RESET 응답을 요구하던 경로를
  수정했습니다. 동일한 Bluetooth 키 통보가 반복될 때 불필요한 저장을 줄이고,
  키 저장 대기 시간과 실패 처리를 바로잡았습니다. 실제 저장 실패는 계속 멈춥니다.

**기존 주행 연동을 중지하고 APK0.9.59를 덮어 설치한 뒤, 함께 제공된0.9.59 ZIP으로
Product를 업데이트하세요.** 일반 업데이트는 기존 설정·사진을 보존합니다.
정상적인 기존 CFW 업데이트에 Gate 재설치는 필요하지 않습니다. 처음 설치할
기기는 새 APK와 새 ZIP을 함께 사용하세요. 새 ZIP에는 호환성 정책을 반영한
Gate·Bootstrap·제거·진단 이미지가 포함되며, 순정·자원 이미지는0.9.58과 같습니다.

0.9.58 이하에서 넘어올 때 첫 재시작은 기존 펌웨어가 수행하므로, 그 단계의
구버전 오류까지 새 APK만으로 없어지지는 않습니다. 새 재시작 로직은0.9.59가
실행된 뒤 적용됩니다. 보고된 모든 Code6/Phase4 현상의 원인을 확인한 것은 아닙니다.

지도 선행 로딩, 화면 밖 지도 작업 중단, 지도·앨범아트 캐시와
음악→전화→알림→지도 전송 우선순위는 유지합니다. Android551개 시험,
ARM 저장·재시작·호환성 시험, EVE1,203프레임 스트레스 및18개 상태 교란 시험을
통과했습니다. Debug 여유1,896B, Release33,880B로 승인된 최소 여유를 충족합니다.

**실차 시동 중 음영, 실제 설치·RF 처리량·LCD30fps는 아직 실측하지 못했습니다.**
소프트웨어에서 수정·검증한 결과이며 모든 F4 리비전의 실기 동작을 보증하는 의미는
아닙니다. 상세 시험 결과는 [0.9.59 검증 보고서](../../documentations/display-install-0.9.59.md)를 참고하세요.

English: a final EVE warning pass isolates the full-screen veil from normal LVGL
and stencil state while preserving the original rounded fuel icon. Manual range
changes are immediate; automatic easing remains. Labels are larger and road
colors more distinct. F4 HW0 / bootloader0.10–0.19 / stock5.16 admission ignores
model and PCBA while retaining exact device capture and integrity checks. Local
update approval no longer incorrectly requires a remote RESET acknowledgment;
unchanged key notifications no longer trigger needless persistence. Use the
matching APK and ZIP. Old firmware still controls the first handoff when updating
from an older release. Physical vehicle validation remains pending.
