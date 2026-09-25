> 2026-09-21: BL0.15/SR0701 호환 구현은 [BL_COMPATIBILITY.md](BL_COMPATIBILITY.md)를 따른다. 아래의 0.14 분석 기록은 당시 원본의 관측값이며, 현재 writer는 0.14/0.15의 word0을 그대로 보존한다.

# 순정 부트로더를 사용하는 APP 업데이트

이서비스는장치에접근하지않은상태에서구현·모형검증했다. 첫실기단계는읽기전용NOR백업이며실제stage/내부메타기록은상위backup승인과PC의별도명령이있어야한다. 단순빌드에플래시쓰기를포함하지않는다.

순정BL은메타`08008000`의5words를읽는다. resident=`000E0000`,APPslot=`7F90`,length가1..70000,요청CRC가비영이면NOR `07F90000`부터APP `08010000`으로복사한다. 실제복사는마지막4KiB블록도전부쓰므로이번서비스는항상448KiB전체를검증·저장한다. 영역은`07F90000..07FFFFFF`뿐이고BLstage64KiB(`07F80000..07F8FFFF`)와FS/NVM을건드리지않는다.

**APP 논리 바이트와 NOR 물리 바이트는 다르다.** NDCP DATA·서비스의 SHA/CRC/vector·READ_STAGE는 canonical APP 순서다. 순정 BL은 SPI16 halfword DMA로 physical wire byte-pair를 교환해 받으므로 `RuntimeUpdate` adapter가 APP staging에만 `physical address = logical address XOR1` codec을 적용한다. raw `BSP_NOR`·128MiB A/B 백업·FS/NVM의 표현은 그대로다. physical raw backup을 canonical APP 파일로 취급하거나 raw CRC를 메타 CRC로 쓰면 안 된다. 홀수 offset/length·재전송·page/stage 경계 처리 및 정적/실물 근거는 [RuntimeUpdate 계약](../Runtime/UPDATER.md)에 있다.

메타최종값은`[000E0000,version,00007F90,00070000,crc32_iso]`다. **이CRC는순정APP의전송CRC이며이미지trailer의STM32 wordCRC와다르다.** 순정V5.16전송CRC는AF819880,wordCRC trailer는70067874다. BL복사경로는CRC를재계산하지않고비영요청gate로쓴다. 여기서는전체NOR재읽기에서ISOCRC와SHA256을둘다확인하며MSP/Reset/길이도검사한다. SHA256은자체작은FIPS180-4구현이며외부렌더/암호framework를추가하지않았다. [NIST FIPS180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf).

정적근거: [BL복사분석](../../../../analysis/2026-09-11-bootloader-re/full-flash-update/README.md)의`20004782/200047D6`, [실제APP계약](../../../../analysis/2026-09-11-update-failure-contract/mcu-app/README.md)의`080241A4`, [메타loader ASM](../../../../analysis/2026-09-11-bootloader-re/startup/200044FC-20004A00.asm.txt), [메타clear ASM](../../../../analysis/2026-09-11-bootloader-re/full-flash-update/metadata_result.asm.txt). 원본fullSHA256 `38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`, 복원RAMSHA256 `a5d037b3e3e8da1b11a9492e52807701278259fbf535c1f4fc0e3c352b9c3636`.

BSP_NOR의APP전용capability와UpdatePlatform의콜백을StorageTask에서연결한다. UpdateMetadata_Commit에는BSP_RAM_Allocate(4000h)로얻은독점scratch를넘긴다. 메타writer는S2를백업한뒤같은sector만erase/복원하고CRCword를마지막에기록·readback한다. S0/S1/S3는주소경계로배제한다. [메타writer의정확한한계](METADATA.md)를따른다. BL의성공후clear는S2전체를지우고첫20bytes만복원하므로S2에새영구설정을두지않는다.

Runtime adapter는 서비스를 포함한 SDRAM 할당 안에서 전용16KiB scratch를 분리한다. COMMIT 전 BT owner quiesce가 성공해야 writer를 호출하고 반환 후 항상 resume를 요청한다. 연결 epoch가 달라진 이전 요청의 다음 물리 program은 거절한다. 실제 adapter·서비스·NDCP·SHA를 연결한 O0/Os 각각45,529개 모델 검사는 실물 설치 검증과 구분한다.

부트의설치는제자리교체이며A/Brollback이없다. 성공후BL은CRC0으로지우지만slot/length는남을수있다. metadatawrite오류는미기록이라고단정할수없으므로자동재시도erase/reset하지않는다. 서비스는COMMIT_AMBIGUOUS로잠그며실기SWD/메타읽기확인이필요하다. 이미지검증통과도새APP의정상기동을증명하지않는다.

[wire/API계약](WIRE.md), [PC명령](../../tools/noodoe_ota.py), [실제ARM시험](../../tools/tests/protocol_services/README.md)을함께본다. PC`prepare`는오프라인이며`stage`, `commit`, `reset`을명시적으로분리한다. 전송journal에는주소·입력/패딩SHA·CRC·A/B백업SHA·ACKoffset·commitintent를보존한다. `commit_intent_result_unknown`이면자동재시도하지않는다.
