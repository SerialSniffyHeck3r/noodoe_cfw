# 2017 AK550 도너: 2026-09-09 USB 연결 및 실물 관찰

사용자 실물 관찰과 PC에서 직접 확인한 사실을 구분한 기록이다. 차량 계기판과 Noodoe는 별도 장치이며, 확보한 차량↔계기판 핀아웃을 두 장치 사이 핀아웃으로 취급하지 않는다.

## 사용자가 보고한 관찰

- Noodoe→계기판으로 추정한 선에서 펄스 같은 파형을 발견했다. UART일 가능성을 언급했으나, TX/RX 핀 위치·전압·baud·실제 프레임은 아직 확정하지 않았다.
- USB를 연결하니 약128MB 저장장치가 보이고 일부 파일은 보이지만 일부는 보이지 않는다.
- 차량 계기판의 Speed 핀을 -/+ 쪽으로 천천히 전환하자 1km/h가 표시됐다. 펄스형 속도 입력이라는 가설을 지지하나, 사용한 전압·주기·펄스당 거리·입력회로 형식은 기록되지 않았다. 3.3V MCU 직접 연결 호환성을 입증한 관찰은 아니다.
- 밝기 조절이 동작하지 않는 것으로 보였다. 테스트 조도·전원 조건·계기판/Noodoe 어느 화면인지·A1 실제 송신 여부는 미확인이다. 기존 펌웨어에서 A1 조도단계 송신 코드를 찾았다는 사실과 이 실물 관찰은 별개이다.
- SWD 접점은 여전히 찾지 못했다.

## 이 PC에서 직접 확인

- Windows drive D:, volume label NOODOE.
- USB VID:PID 0483:5720, serial[redacted USB serial].
- USB storage model STM Product / STM Product USB Device, revision0.01.
- Windows Get-Disk / partition size133,693,440 bytes =127.5MiB; partition offset0.
- Windows Get-Partition은 Type FAT16으로 보고했다. 실제 BPB 기반 파일시스템 분류는 별도의 이미지 분석을 따른다(첫 BPB는4096B sectors/8sectors per cluster/FAT12 식별 문자열).
- 디렉터리 열거에서 font/notification/speed/weather/compass에 ERROR1392, 손상되어 읽을 수 없음이 발생했다. 정상적으로 열거된 파일 중에도 복사 오류가 있었다. 단순 hidden 속성만으로 현재 현상을 설명할 수 없다.
- Robocopy 첫 유효 실행:231파일 중193복사성공/38실패,33디렉터리 중5열거실패,exit9. 명령은 source에 대한 읽기와 로컬destination 쓰기만 요청했으며, /MIR·삭제·수정·파일시스템복구는 사용하지 않았다.
- PhysicalDrive1 읽기 open은 OS Access denied였지만 volume handle \\.\D: 읽기는 허용됐다. 따라서 추가 권한 요청 없이 USB에 노출된 볼륨 전체를 읽었다.

## 보존 위치

[evidence/usb/2026-09-09-noodoe-[redacted USB serial]](../evidence/usb/2026-09-09-noodoe-[redacted USB serial]/) 아래에 식별정보, 파일목록, 오류로그, 복사성공 파일, 볼륨 이미지와 수집 스크립트를 저장했다.

첫 볼륨 이미지는133,693,440바이트를 모두 읽었고 SHA256은 `0ab3d0de83f774e61eb548e5109fa53f1f2f86cd138b9e5a7d16fef8a45879b8`이다. 이미지 대상은 **USB로 노출된 외부 저장공간**이다. STM32 내부플래시/부트로더와 외부NOR 전체128MiB를 백업했다는 의미는 아니다.

장치는 mounted/live 상태였고 하드웨어 write blocker는 없다. 분석자가 source 쓰기·복구 명령을 실행하지 않았다는 것과, Windows 또는 장치 펌웨어가 배경에서 저장장치에 전혀 쓰지 않았다는 것은 구분한다. 첫 이미지 확보 후 다시 전체를 읽어 일관성을 확인한다.

[USB 수집 및 분석 보고서](2026-09-09-noodoe-usb-acquisition.md)에서 최종 이미지 검증, 파일시스템 분석 및 콘텐츠 결과를 이어 기록한다.