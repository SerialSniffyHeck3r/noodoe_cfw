# NDCP1 전화·업데이트 프로토콜

모든 정수는little-endian이다. paired PHONE SPP만 이endpoint로 전달한다. 이 프레임 자체에는 인증·암호화·서명이 없다. Bluetooth 역할/허용주소/페어링 정책은 Runtime이 소유한다. CRC/SHA가 인증을 대신한다고 해석하지 않는다.

| Offset | 형식 | 의미 |
|---:|---|---|
|0|4bytes|ASCII `NDCP`|
|4|u8|version1|
|5|u8|opcode:00..1F전화제어,40..47업데이트|
|6|u16|flags:bit0 response,bit1 error; 요청은0|
|8|u32|sequence.응답은요청과같은값|
|12|u16|payload length0..1024|
|14|u16|reserved0|
|16|bytes|payload|
|16+length|u32|header+payload의CRC32/ISO-HDLC|

총20..1044bytes다. parser는잘못된magic/header/CRC뒤1byte씩재검색하며무수신2초partialtimeout을둔다. 응답공통payload는 `[result,state,transaction,received,verified]` 5u32(20bytes)다. result0이면성공,그외는UpdateResult이며flagsERROR도설정한다. accepted DATA는수신조각저장/readback만뜻하고설치성공이아니다.

|Opcode|요청payload|성공응답추가내용/의미|
|---:|---|---|
|40 BEGIN|txid,version,length,crc32_iso(각u32),sha256[32],STAG u32=`53544147`,reserved u32=0;총56B|수신세션만열며아직erase하지않음|
|41 DATA|txid u32,offset u32,data1..1016B|연속offset만기록.이미ACK한동일byte재전송은재기록없이ACK|
|42 FINISH|txid u32|전체NOR readback SHA/CRC/vector 검증이끝난뒤sha256[32]추가.비동기응답|
|43 COMMIT|txid u32,COMT u32=`434F4D54`,sha256[32]|검증상태및별도authorization을확인하고순정설치요청기록.재시작하지않음|
|44 ABORT|txid u32|진행중세션을닫으며NOR는지우지않음.이미COMMITTED이면PENDING오류로메타보존.예약Reset은취소하지만pending은남음|
|45 STATUS|없음|version,expectedISOCRC,authorization,metadata5words,actualSHA32;총추가64B|
|46 READ_STAGE|offset u32,count u32(1..512)|offset,count,data;읽기만수행|
|47 RESET|txid u32,RST! u32=`52535421`|COMMITTED일때만예약.해당응답전체local전송완료통지후1500ms뒤platform reset|

수신length는이번구현에서정확히`0x70000`(448KiB)이다. PC는검토된APP만읽고원본끝부터FF로패딩한다.512KiB SWD전체덤프는거절한다. CRC가0/FFFFFFFF이면resident의요청sentinel과겹쳐거절한다. versionFFFFFFFF도거절한다. MSP는8-byte정렬mainSRAM(20000000초과..20030000이하),Reset은자기APP안의Thumb주소여야한다.

DATA와 READ_STAGE의 offset/data는 **canonical APP 논리 순서**다. CRC/SHA도 이 전체448KiB에 대한 값이다. 순정 BL의 SPI16 DMA 저장 계약 때문에 RuntimeUpdate adapter만 physical NOR 주소 `logical address XOR1`로 바꾸며 READ_STAGE 때 되돌린다. PC가 DATA를 미리 swap하거나 메타 CRC를 physical raw CRC로 바꾸지 않는다. raw NOR128MiB 백업은 StorageSWD/StorageBackup의 별도 API이며 이 logical READ_STAGE와 구분한다.

`UpdateService_Authorize(STAG)`는full128MiB NOR A/B백업검증뒤상위Runtime만호출한다. BEGIN의STAG는이잠금을열지않는다. 연결종료시authorization을닫고수신/검증중이면실패로정리한다. VERIFIED의RAMmanifest/hash는별도COMMIT연결을위해남지만재연결후상위승인이없으면COMMIT도LOCKED다. 전원상실로RAMmanifest가없어지면기존NOR byte만으로자동resume/commit하지않는다. COMMITTED record는읽기·연결종료·ABORT로지우지않는다.

IOTask는NDCP_Feed와UpdateService_Handle만실행하고StorageTask는UpdateService_Process로SPI5/metadata를단독사용한다. 깊이2 SPSC request/replyqueue는acquire/release로publish한다. 큐가차면Handle은BUSY를반환하며rootdispatcher가오류를응답해야한다. TakeReply의완성frame은BT TXqueue보다클수있으므로pendingtail로분할하되다른NDCP응답byte와섞지않는다. 전체frame의실제local전송이끝났을때만NotifyReplyTransmitted(seq,now)를호출한다. Reset ACK는peer수신·설치완료보장이아니다.
