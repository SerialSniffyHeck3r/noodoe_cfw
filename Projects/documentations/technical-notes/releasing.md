# GitHub 다운로드 배포

사용자 지시: 수정본을 배포할 때마다 저장소 첫 화면에서 최신 APK와 대응 설치 ZIP을 바로 받을 수 있게 유지한다. 기존 공개 범위(비공개 저장소)는 바꾸지 않는다.

1. APK·설치 ZIP의 빌드/검증을 끝내고 새 태그 릴리스에 업로드한다. 기존 릴리스 자산은 덮어쓰지 않는다.
2. GitHub asset의 크기·SHA256을 로컬과 대조하고 `github-release.json`을 남긴다.
3. 공용 `tools/github_release_latest.py --record <github-release.json>`을 실행한다. 현재 배포 스크립트는 이 단계를 자동 호출한다.
4. 이 도구는 업로드를 다시 검증하고 `prerelease=false`, `make_latest=true`로 최신 릴리스를 지정한다. README의 관리 블록에 해당 릴리스 APK·ZIP 직접 링크를 갱신하고 기존 나머지 본문을 보존한다.
5. `/releases/latest`와 README를 다시 읽어 실제 갱신을 확인한다. 검증·실기기 시험 범위는 릴리스 본문에 정확히 유지한다. Latest 표시는 실제 무선 시험 완료라는 뜻이 아니다.

항상 최신 페이지: https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest

이전 릴리스를 확인하기 위해 과거 게시 스크립트를 다시 실행하지 않는다. 읽기 전용 `--inspect`를 사용한다. 바이너리·토큰·개인 데이터를 소스 저장소에 추가하지 않는다. 이 작업은 릴리스 자산과 README 다운로드 링크를 관리한다.
