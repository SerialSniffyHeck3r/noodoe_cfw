# 순정 Noodoe 런타임 콘텐츠 표본

2026-09-01 무선 ADB와 `su` 읽기 권한으로 Galaxy Tab Active3의
`/data/user/0/com.noodoe.sunray/files/`에서 회수했다. 원본 앱 데이터는 변경하지
않았으며, 임시 외부 저장소 복사본은 호스트 회수 후 삭제했다.

보존 범위:

- `installed/`: 순정 앱이 만든 활성 생성 콘텐츠 ZIP 및 전송용 `.bundle` 디렉터리
- `gallery/`: 사용자가 고른 원본과 순정 앱이 만든 전송용 JPEG
- `creation_dir/`: 시계/속도계 편집 원본, `creation.json`, 앱 데이터 ZIP 생성 재료

`.bundle` 안의 파일명은 각 파일 전체 내용의 MD5와 일치한다. 갤러리 전송용 JPEG는
JPEG EOI 뒤에 슬롯 바이트를 붙인 전체 데이터의 MD5를 파일명으로 사용한다. 이
디렉터리는 순정 포맷 비교와 회귀 테스트용 증거이며, OpenNoodoe가 임의로 덮어쓰지
않는다.
