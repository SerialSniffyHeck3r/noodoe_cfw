# Bootstrap 환영 화면

첫 화면을 중앙 정렬된 두 줄 `FuckNudo Bootstrap` / `Welcome`으로 변경했다.
EVE 초기화 직후, NOR·Bluetooth·SDRAM 초기화 전에 즉시 제출한다. 약 1.5초 뒤
진행 중인 검사 또는 메뉴를 표시하며, 초기화·워치독·파일 검사는 계속 진행된다.
버튼을 누르면 환영 화면만 닫고 숨겨진 메뉴를 선택하지 않는다. 오류·복구·설치
화면이 우선하며, 설치 취소로 메뉴에 돌아올 때 환영 화면을 다시 띄우지 않는다.

메뉴와 Bootstrap 내부 초기 복구 렌더러의 `NOODOE INSTALLER` 제목도 같은
브랜딩으로 바꿨다. 프로토콜 식별 문자열·패키지 식별자·Android 앱 이름은 유지한다.

## 검증·배포

- Bootstrap 빌드: 451,500B, 여유 7,252B. 이전보다 248B 증가.
- ARM O0/Os 기존 UI·세션·복구·화면 50개 시나리오 통과. 환영 화면의 tick wrap,
  입력 소비, 검사 계속 진행, 오류 우선 및 취소 후 재표시 방지 검사 포함.
- 최종 ZIP을 Android의 실제 RecoveryBundle importer로 검사했다.
- ZIP 안의 Product+Gate, 순정 APP, 자산은 기존 6.4 패키지와 바이트 단위로 동일하다.
- 하드웨어 접근·플래시 기록 없음. 실제 LCD 표시나 무선 설치 시험으로 표현하지 않는다.

파일: `C:/shared/KYMCO/Downloads/NoodoeInstaller-6.4-Welcome.zip`

기존 컴패니언 APK 6.4를 그대로 사용한다. 환영 화면 변경은 새 Bootstrap을 올렸을 때
적용된다. 이미 실행 중인 이전 Bootstrap에 새 ZIP을 강제로 이어 쓰지 않는다.
새 Bootstrap은 순정에서 이 ZIP으로 올린 뒤, 같은 ZIP으로 CFW 설치를 이어간다.
해시와 자세한 결과는 `verification-summary.json`에 기록했다.
