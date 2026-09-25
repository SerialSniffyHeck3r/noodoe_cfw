"""실행 APP/UID를 확인한 뒤 조도 서비스 SRAM mailbox만 사용하는 진단 도구.

기본 계획 출력에는 장치 접근이 없다. --execute 경로도 GPIO/전원/FLASH/reset을
직접 변경하지 않으며, 실행 중인 I/O owner에게 명시적인 버스 시험을 요청한다.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import secrets
import struct
import time
from capture_display import LiveBoard
from bringup import APP, require, sha
from verified_flash import read_expected
from validate_image import Elf32, validate

UID_ADDRESS = 0x1FFF7A10
EXPECTED_UID = (3735583, 875974927, 892810041)
DHCSR = 0xE000EDF0
# AmbientService_Init의 g_ambient_mailbox.magic와 동일한 고정 ABI1 식별자다.
MAILBOX_MAGIC = 0x414D4231
MAILBOX_BYTES = 124
FIELDS = ('magic version request_seq command argument response_seq operation_id request_status '
          'result completed_ms state reserved driver_magic ready error manufacturer device configuration '
          'raw millilux valid sample_ms samples failures phase hal_status hal_error sr1 elapsed_ms bus_hz enabled').split()
PHASES = ['none', 'deinit', 'init', 'analog_filter', 'digital_filter', 'manufacturer', 'device', 'enable', 'config', 'result']
RESULTS = {0:'OK', 2:'HAL_ERROR', 3:'HAL_BUSY', 4:'HAL_TIMEOUT', 5:'ID_MISMATCH',
           6:'INVALID_SAMPLE', 7:'ARGUMENT', 8:'NOT_READY', 9:'CONTEXT'}
BITBANG_MAGIC = 0x41424231
BITBANG_VERSION = 3
BITBANG_BYTES = 212
BITBANG_SYMBOL = 'g_bsp_ambient_bitbang'
BITBANG_PREFIX_FIELDS = ('magic version bytes sequence operation_id request_seq result restore_result '
    'manufacturer device ids_valid elapsed_us phase failed_phase reg byte_index bit_index ack_count ack_mask '
    'initial_lines last_lines scl_high_checks scl_low_checks sda_high_checks sda_low_checks '
    'max_gap_us timing_uncertain clock_hz saved_cr1 final_cr1 saved_cr2 final_cr2 saved_ccr final_ccr '
    'saved_trise final_trise saved_fltr final_fltr saved_ph7 final_ph7 saved_pc9 final_pc9 '
    'pin_changes stop_result completed_ms max_low_us').split()
BITBANG_SAMPLE_FIELDS = ('first_pre_lines first_pre_us first_early_lines first_early_us '
                         'first_late_lines first_late_us').split()
BITBANG_V2_FIELDS = BITBANG_PREFIX_FIELDS + BITBANG_SAMPLE_FIELDS
BITBANG_FIELDS = BITBANG_V2_FIELDS + ['pullup_mode']
# ELF object의 실제 크기로만 선택한다. host 인수로 임의 크기/버전을 지정할 수 없다.
BITBANG_ABIS = {1:(184,BITBANG_PREFIX_FIELDS), 2:(208,BITBANG_V2_FIELDS), 3:(212,BITBANG_FIELDS)}
BITBANG_RESULTS = ('OK','CONTEXT','PRECONDITION','CLOCK','BUS_BUSY','SCL_TIMEOUT','SDA_CONFLICT',
                   'NACK','TIMING_UNCERTAIN','ID_MISMATCH','RESTORE_FAILED')
BITBANG_PHASES = ('NONE','PRECHECK','IDLE','START','ADDRESS_WRITE','REGISTER','RESTART',
                  'ADDRESS_READ','READ_HIGH','READ_LOW','STOP','RESTORE','COMPLETE')
ADDRESS_MAGIC = 0x41424131
ADDRESS_VERSION = 1
ADDRESS_BYTES = 288
ADDRESS_SYMBOL = 'g_bsp_ambient_address'
ADDRESS_CANDIDATES = (0x44,0x45,0x46,0x47)
ADDRESS_HEADER_FIELDS = ('magic version bytes sequence operation_id request_seq result restore_result '
                        'attempted_mask address_ack_mask id_match_mask elapsed_us timing_uncertain completed_ms').split()
ADDRESS_ENTRY_FIELDS = ('address attempted address_ack result phase manufacturer device ids_valid elapsed_us stop_result').split()
ADDRESS_TAIL_FIELDS = ('saved_cr1 final_cr1 saved_cr2 final_cr2 saved_ccr final_ccr saved_trise final_trise '
                       'saved_fltr final_fltr saved_ph7 final_ph7 saved_pc9 final_pc9 '
                       'pin_changes pullup_mode max_gap_us max_low_us').split()


def save(path: Path, value: dict) -> None:
    """한 요청의 계획·오류·완료 상태를 로컬 JSON에 저장한다. raw dump를 덮지 않는다."""
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def image_contract(path: Path, mailbox_bytes: int, id_bitbang: bool = False, address_diagnostic: bool = False) -> dict:
    """기존 strict validator로 APP를 재구성하고 실제 ELF의 mailbox object 범위를 확인한다."""
    data = path.read_bytes()
    elf = Elf32(data)
    image, manifest = validate(elf)
    symbol = elf.symbols.get('g_ambient_mailbox')
    require(symbol and symbol['type'] == 1 and symbol['size'] == mailbox_bytes,
            'ELF does not contain the supported g_ambient_mailbox object')
    address = symbol['value']
    require(address % 4 == 0 and 0x20000000 <= address <= 0x20030000 - mailbox_bytes,
            'Ambient mailbox is outside aligned main SRAM')
    section = elf.sections[symbol['section']]
    require(section['flags'] & 3 == 3 and section['addr'] <= address and
            address + mailbox_bytes <= section['addr'] + section['size'],
            'Mailbox is not within a writable allocated ELF section')
    contract = dict(elf=str(path.resolve()), elf_sha256=sha(data), app_sha256=sha(image),
                    app_size=len(image), mailbox=address, image=image, manifest=manifest)
    if id_bitbang:
        # 별도 진단도 실제 ELF의 object여야 한다. 임의 SRAM 주소 입력/추측을 허용하지 않는다.
        extra = elf.symbols.get(BITBANG_SYMBOL)
        require(extra and extra['type'] == 1 and extra['size'] in (184,208,212),
                'ELF does not contain the supported '+BITBANG_SYMBOL)
        extra_size = extra['size']
        extra_version = {184:1,208:2,212:3}[extra_size]
        start = extra['value']
        require(start % 4 == 0 and 0x20000000 <= start <= 0x20030000-extra_size,
                'Bitbang diagnostic is outside aligned main SRAM')
        require(0 < extra['section'] < len(elf.sections), 'Invalid bitbang diagnostic section')
        owner = elf.sections[extra['section']]
        require(owner['flags'] & 3 == 3 and owner['addr'] <= start and
                start+extra_size <= owner['addr']+owner['size'], 'Bitbang diagnostic section bounds invalid')
        require(start+extra_size <= address or address+mailbox_bytes <= start, 'Diagnostic objects overlap')
        contract['bitbang'] = start
        contract['bitbang_bytes'] = extra_size
        contract['bitbang_version'] = extra_version
    if address_diagnostic:
        # cmd4는 별도288B object만 읽는다. cmd3 evidence의 존재/주소를 대신 사용하지 않는다.
        extra=elf.symbols.get(ADDRESS_SYMBOL)
        require(extra and extra['type']==1 and extra['size']==ADDRESS_BYTES,
                'ELF does not contain the supported '+ADDRESS_SYMBOL)
        start=extra['value']
        require(start%4==0 and 0x20000000<=start<=0x20030000-ADDRESS_BYTES,
                'Address diagnostic is outside aligned main SRAM')
        require(0<extra['section']<len(elf.sections),'Invalid address diagnostic section')
        owner=elf.sections[extra['section']]
        require(owner['flags']&3==3 and owner['addr']<=start and
                start+ADDRESS_BYTES<=owner['addr']+owner['size'],'Address diagnostic section bounds invalid')
        require(start+ADDRESS_BYTES<=address or address+mailbox_bytes<=start,'Diagnostic objects overlap')
        contract.update(address_diagnostic=start,address_bytes=ADDRESS_BYTES,address_version=ADDRESS_VERSION)
    return contract


class AmbientBoard(LiveBoard):
    """기존 HOTPLUG transport에 더 좁은 read/request allowlist를 적용한다."""
    def __init__(self, serial: str, folder: Path, mailbox: int, mailbox_bytes: int,
                 request_offsets: tuple[int, ...], read_khz: int):
        """제어 쓰기 clock은50kHz로 고정한다. 허용 request offset은 ABI의 필드만 받는다."""
        require(re.fullmatch(r'[0-9A-Fa-f]{8,64}', serial), 'Invalid ST-LINK serial')
        require(read_khz in (100, 950, 4000), 'Unsupported qualified read clock')
        require(0x20000000 <= mailbox <= 0x20030000 - mailbox_bytes and mailbox % 4 == 0,
                'Invalid mailbox address')
        require(all(0 <= offset <= mailbox_bytes - 4 and offset % 4 == 0 for offset in request_offsets),
                'Invalid request offsets')
        super().__init__(serial, folder, 50, mailbox)
        self.mailbox_bytes = mailbox_bytes
        self.request_addresses = {mailbox + offset for offset in request_offsets}
        self.read_frequency_khz = read_khz
        self.identity_verified = False
        self.counter = 0

    def program(self, image: Path) -> None:
        """상속한 APP programmer도 이 도구에서는 명시적으로 차단한다."""
        del image
        raise RuntimeError('Flash programming is unavailable in ambient diagnostics')

    def command(self, name: str, *operations: str, timeout: int = 30) -> str:
        """옵션 조회, 한 번의 upload, 지정 SRAM request words 이외의 명령은 거부한다."""
        valid = operations == ('-ob', 'displ')
        if len(operations) == 4 and operations[0] == '-u':
            address, size = int(operations[1], 0), int(operations[2], 0)
            intervals = ((APP, 0x08080000), (UID_ADDRESS, UID_ADDRESS + 12),
                         (DHCSR, DHCSR + 4), (self.mailbox, self.mailbox + self.mailbox_bytes))
            destination = Path(operations[3]).resolve()
            valid = size > 0 and any(lo <= address < address + size <= hi for lo, hi in intervals)
            valid = valid and destination.parent == self.folder.resolve() and not destination.exists()
        elif operations and len(operations) % 3 == 0 and operations[0] == '-w32':
            require(self.identity_verified, 'APP/UID must be verified before mailbox writes')
            valid = all(operations[index] == '-w32' and
                        int(operations[index + 1], 0) in self.request_addresses and
                        0 <= int(operations[index + 2], 0) <= 0xFFFFFFFF
                        for index in range(0, len(operations), 3))
        require(valid, 'Command is outside ambient live-read/request whitelist')
        self.counter += 1
        return super().command(f'{self.counter:05d}-{name}', *operations, timeout=timeout)

    def dump(self, name: str, address: int, size: int, resume: bool = False) -> bytes:
        """새 raw 파일로 읽기만 한다. 상위 함수가 resume를 요청해도 실행 명령을 허용하지 않는다."""
        require(not resume, 'Ambient diagnostic must never resume/halt target')
        path = self.folder / f'{self.counter + 1:05d}-{name}.bin'
        self.command(name, '-u', hex(address), hex(size), str(path))
        require(path.is_file() and path.stat().st_size == size, 'Incomplete ambient memory read')
        return path.read_bytes()

    def verify_identity(self, contract: dict) -> dict:
        """실행중 여부, canonical APP 전부와 고정 UID를 확인한 뒤 SRAM 쓰기 권한을 연다."""
        self.identity_verified = False
        self.prepare()  # LiveBoard.prepare는 옵션 읽기이며 freeze/halt/write를 하지 않는다.
        self.assert_running('dhcsr-before-identity')
        expected = contract['image']
        actual = read_expected(self, 'app-identity', APP, expected, resume=False)
        require(actual == expected, 'Running APP differs from canonical ELF')
        uid = struct.unpack('<3I', self.dump('uid', UID_ADDRESS, 12))
        require(uid == EXPECTED_UID, 'Wrong MCU UID for the current Noodoe module')
        self.assert_running('dhcsr-after-identity')
        self.identity_verified = True
        return dict(app_sha256=sha(actual), uid_words=list(uid), cpu_running=True)


class AmbientBitbangBoard(AmbientBoard):
    """command3 한 번과 ELF가 확인한184B/208B/212B 진단 읽기만 허용한다."""
    def __init__(self, serial: str, folder: Path, contract: dict, read_khz: int, sda_pullup: bool = False):
        """기존 identity/HOTPLUG 보호를 유지하고 별도 ELF 진단 주소와 제한시간만 저장한다."""
        super().__init__(serial,folder,contract['mailbox'],MAILBOX_BYTES,(8,12,16),read_khz)
        self.bitbang = contract['bitbang']
        self.bitbang_bytes = contract['bitbang_bytes']
        self.bitbang_version = contract['bitbang_version']
        require(self.bitbang_version in BITBANG_ABIS and
                BITBANG_ABIS[self.bitbang_version][0] == self.bitbang_bytes,'Unsupported bitbang object ABI')
        # 추가 약 pull-up은 명시 opt-in과 이를 기록하는 ABI3가 모두 있어야 허용한다.
        require(not sda_pullup or self.bitbang_version == 3,'SDA pull-up requires diagnostic ABI3/212B')
        self.pullup_mode = int(bool(sda_pullup))
        self.request_attempted = False
        self.deadline = None

    def command(self, name: str, *operations: str, timeout: float = 30) -> str:
        """cmd3/선택한arg/seq-last 외 쓰기를 거부하며 게시 timeout도 재시도하지 않는다."""
        if self.deadline is not None:
            remaining = self.deadline-time.monotonic()
            require(remaining > 0, 'Bitbang operation host deadline expired')
            timeout = min(timeout,remaining)
        if operations and operations[0] == '-w32':
            require(self.identity_verified, 'APP/UID must be verified before bitbang request')
            require(not self.request_attempted, 'Only one ID-bitbang publication is allowed per run')
            require(len(operations) == 9 and operations[3] == operations[6] == '-w32' and
                    int(operations[1],0) == self.mailbox+12 and int(operations[2],0) == 3 and
                    int(operations[4],0) == self.mailbox+16 and int(operations[5],0) == self.pullup_mode and
                    int(operations[7],0) == self.mailbox+8 and 0 < int(operations[8],0) <= 0xFFFFFFFF,
                    'ID-bitbang permits only command3/selected argument and nonzero request sequence last')
            self.request_attempted = True
        if len(operations) == 4 and operations[0] == '-u':
            address,size = int(operations[1],0),int(operations[2],0)
            if self.bitbang <= address < address+size <= self.bitbang+self.bitbang_bytes:
                destination = Path(operations[3]).resolve()
                require(destination.parent == self.folder.resolve() and not destination.exists(),
                        'Bitbang diagnostic must use a fresh local raw file')
                self.counter += 1
                return LiveBoard.command(self,f'{self.counter:05d}-{name}',*operations,timeout=timeout)
        return super().command(name,*operations,timeout=timeout)


class AmbientAddressBoard(AmbientBoard):
    """확인된288B object 읽기와 cmd4/arg0 한 번만 허용하는 독립 host capability다."""
    def __init__(self,serial: str,folder: Path,contract: dict,read_khz: int):
        """APP/UID 보호와 SRAM mailbox를 상속하며 주소 목록을 host 입력으로 받지 않는다."""
        super().__init__(serial,folder,contract['mailbox'],MAILBOX_BYTES,(8,12,16),read_khz)
        require(contract['address_bytes']==ADDRESS_BYTES and contract['address_version']==ADDRESS_VERSION,
                'Unsupported address diagnostic ABI')
        self.diagnostic=contract['address_diagnostic']
        self.request_attempted=False
        self.deadline=None

    def command(self,name: str,*operations: str,timeout: float=30) -> str:
        """선택한 진단 외 명령/주소를 거부한다. 게시 timeout도 두 번째 게시로 복구하지 않는다."""
        if self.deadline is not None:
            remaining=self.deadline-time.monotonic()
            require(remaining>0,'Address diagnostic host deadline expired')
            timeout=min(timeout,remaining)
        if operations and operations[0]=='-w32':
            require(self.identity_verified,'APP/UID must be verified before address diagnostic')
            require(not self.request_attempted,'Only one address diagnostic publication is allowed per run')
            require(len(operations)==9 and operations[3]==operations[6]=='-w32' and
                    int(operations[1],0)==self.mailbox+12 and int(operations[2],0)==4 and
                    int(operations[4],0)==self.mailbox+16 and int(operations[5],0)==0 and
                    int(operations[7],0)==self.mailbox+8 and 0<int(operations[8],0)<=0xFFFFFFFF,
                    'Address diagnostic permits only command4/argument0 and sequence last')
            self.request_attempted=True
        if len(operations)==4 and operations[0]=='-u':
            address,size=int(operations[1],0),int(operations[2],0)
            if self.diagnostic<=address<address+size<=self.diagnostic+ADDRESS_BYTES:
                destination=Path(operations[3]).resolve()
                require(destination.parent==self.folder.resolve() and not destination.exists(),
                        'Address diagnostic must use a fresh local raw file')
                self.counter+=1
                return LiveBoard.command(self,f'{self.counter:05d}-{name}',*operations,timeout=timeout)
        return super().command(name,*operations,timeout=timeout)


def decode_address(raw: bytes) -> dict:
    """288B 고정 header/4entry/tail을 나누고 ABI·enum·고정 주소·mask 일관성을 검사한다."""
    require(len(raw)==ADDRESS_BYTES,'Address diagnostic size mismatch')
    words=struct.unpack('<72I',raw)
    item=dict(zip(ADDRESS_HEADER_FIELDS,words[:14]))
    require((item['magic'],item['version'],item['bytes'])==(ADDRESS_MAGIC,ADDRESS_VERSION,ADDRESS_BYTES),
            'Address diagnostic ABI mismatch')
    item['entry']=[dict(zip(ADDRESS_ENTRY_FIELDS,words[14+10*i:24+10*i])) for i in range(4)]
    item.update(zip(ADDRESS_TAIL_FIELDS,words[54:]))
    require(item['result']<len(BITBANG_RESULTS) and item['restore_result']<=2 and
            item['pullup_mode']==1 and item['pin_changes'] in (0,1) and item['timing_uncertain'] in (0,1),
            'Unknown address diagnostic enum/mode')
    require(all(item[k]<=15 for k in ('attempted_mask','address_ack_mask','id_match_mask')),
            'Address diagnostic masks contain an unsupported candidate')
    attempted=acked=0
    for i,entry in enumerate(item['entry']):
        require(entry['address']==ADDRESS_CANDIDATES[i],'Address diagnostic contains a non-fixed candidate')
        require(entry['attempted'] in (0,1) and entry['address_ack'] in (0,1) and entry['ids_valid'] in (0,1) and
                entry['result']<len(BITBANG_RESULTS) and entry['phase']<len(BITBANG_PHASES) and
                entry['stop_result']<len(BITBANG_RESULTS),'Unknown address diagnostic entry enum')
        if entry['attempted']:attempted|=1<<i
        if entry['address_ack']:
            require(entry['attempted']==1,'Unattempted address claims an ACK')
            acked|=1<<i
        require(not entry['ids_valid'] or entry['address_ack']==1,'IDs lack an initial address ACK')
        if item['id_match_mask']&(1<<i):
            require(entry['ids_valid']==1 and entry['manufacturer']==0x5449 and entry['device']==0x3001,
                    'Address ID-match mask lacks matching ID words')
    require(attempted==item['attempted_mask'] and acked==item['address_ack_mask'] and
            not item['id_match_mask']&~acked,'Address diagnostic entry flags and masks disagree')
    return item


def stable_address(board: AmbientAddressBoard,name: str,attempts: int=4) -> dict:
    """cmd4 별도 sequence@12의 nonzero equal-even 완료만 선택하며 재읽기는 제한한다."""
    for attempt in range(attempts):
        before=struct.unpack('<I',board.dump(f'{name}-{attempt}-seq-before',board.diagnostic+12,4))[0]
        raw=board.dump(f'{name}-{attempt}-full',board.diagnostic,ADDRESS_BYTES)
        after=struct.unpack('<I',board.dump(f'{name}-{attempt}-seq-after',board.diagnostic+12,4))[0]
        sequence=struct.unpack_from('<I',raw,12)[0]
        if before==after==sequence and sequence!=0 and sequence%2==0:return decode_address(raw)
    raise RuntimeError('Address diagnostic evidence did not reach a stable nonzero even sequence')


def address_interpretation(item: dict) -> dict:
    """ACK·ID 일치·미방문·전체복원을 분리한다. 약 bias ID 응답을 조도 측정 성공으로 부르지 않는다."""
    pairs=('cr1','cr2','ccr','trise','fltr','ph7','pc9')
    differences={key:dict(saved=hex(item['saved_'+key]),final=hex(item['final_'+key]))
                 for key in pairs if item['saved_'+key]!=item['final_'+key]}
    taken=item['pin_changes']==1 and item['restore_result']!=2
    rows=[]
    for entry in item['entry']:
        attempted=bool(entry['attempted'])
        ids_match=bool(entry['ids_valid'] and entry['manufacturer']==0x5449 and entry['device']==0x3001)
        rows.append(dict(address=hex(entry['address']),attempted=attempted,address_ack=bool(entry['address_ack']),
                         result=BITBANG_RESULTS[entry['result']] if attempted else 'NOT_ATTEMPTED',
                         phase=BITBANG_PHASES[entry['phase']] if attempted else 'NOT_ATTEMPTED',
                         ids_valid=bool(entry['ids_valid']),ids_match=ids_match,
                         manufacturer=hex(entry['manufacturer']) if entry['ids_valid'] else None,
                         device=hex(entry['device']) if entry['ids_valid'] else None,
                         elapsed_us=entry['elapsed_us'],stop_result=entry['stop_result']))
    # 성공한 ID 뒤 다른 주소의 line/STOP/timing 오류가 발생하면 전체 결과는 실패다.
    rows_complete=all(row['attempted'] and row['result'] in ('OK','NACK','ID_MISMATCH') and
                      row['stop_result']==0 for row in rows)
    return dict(result=BITBANG_RESULTS[item['result']],restore_result=item['restore_result'],entry=rows,
                attempted_mask=hex(item['attempted_mask']),address_ack_mask=hex(item['address_ack_mask']),
                id_match_mask=hex(item['id_match_mask']),all_candidates_attempted=item['attempted_mask']==15,
                matched_addresses=[row['address'] for i,row in enumerate(rows) if item['id_match_mask']&(1<<i)],
                saved_configuration_matches=not differences,restore_differences=differences,
                sda_pullup_requested=True,pin_takeover_observed=taken,temporary_sda_weak_pullup=taken,
                measurement_scope='temporary-PC9-weak-pullup-fixed-address-ID-only',
                original_hardware_proven=False,lux_measurement_verified=False,
                nominal_bus_hz=20000,elapsed_us=item['elapsed_us'],max_gap_us=item['max_gap_us'],
                max_low_us=item['max_low_us'],timing_uncertain=bool(item['timing_uncertain']),
                successful=bool(item['result']==0 and item['restore_result']==0 and not differences and taken and
                                item['id_match_mask'] and rows_complete and not item['timing_uncertain'] and
                                item['elapsed_us']<=100000 and item['max_gap_us']<=10000 and item['max_low_us']<28000))


def decode_bitbang(raw: bytes, expected_version: int | None = None) -> dict:
    """확정된 v1/184B,v2/208B,v3/212B만 읽고 ELF 선택과 wire ABI를 대조한다."""
    require(len(raw) in (184,208,212),'Bitbang diagnostic size mismatch')
    magic,version,size = struct.unpack_from('<3I',raw)
    require(version in BITBANG_ABIS and size == len(raw) == BITBANG_ABIS[version][0] and
            (expected_version is None or version == expected_version), 'Bitbang object and wire ABI disagree')
    fields = BITBANG_ABIS[version][1]
    item = dict(zip(fields,struct.unpack('<'+str(len(fields))+'I',raw)))
    require(magic == BITBANG_MAGIC,
            'Bitbang diagnostic ABI mismatch')
    require(item['result'] < len(BITBANG_RESULTS) and item['restore_result'] <= 2 and
            item['phase'] < len(BITBANG_PHASES) and item['failed_phase'] < len(BITBANG_PHASES),
            'Unknown bitbang diagnostic enum')
    require(item.get('pullup_mode',0) in (0,1),'Unknown ID-bitbang pull-up mode')
    return item


def stable_bitbang(board: AmbientBitbangBoard, name: str, attempts: int = 4) -> dict:
    """sequence@12의 nonzero equal-even 완료만 읽는다. 실행중 odd 값은 제한 재읽기한다."""
    for attempt in range(attempts):
        before = struct.unpack('<I',board.dump(f'{name}-{attempt}-seq-before',board.bitbang+12,4))[0]
        raw = board.dump(f'{name}-{attempt}-full',board.bitbang,board.bitbang_bytes)
        after = struct.unpack('<I',board.dump(f'{name}-{attempt}-seq-after',board.bitbang+12,4))[0]
        sequence = struct.unpack_from('<I',raw,12)[0]
        if before == after == sequence and sequence != 0 and sequence % 2 == 0:
            return decode_bitbang(raw,expected_version=board.bitbang_version)
    raise RuntimeError('ID-bitbang evidence did not reach a stable nonzero even sequence')


def bitbang_interpretation(item: dict) -> dict:
    """wire 결과·GPIO 복원·실제 timing을 각각 표시한다. HAL ready/캐시 ID를 사용하지 않는다."""
    pairs = ('cr1','cr2','ccr','trise','fltr','ph7','pc9')
    differences = {key:dict(saved=hex(item['saved_'+key]),final=hex(item['final_'+key]))
                   for key in pairs if item['saved_'+key] != item['final_'+key]}
    ids_match = item['manufacturer'] == 0x5449 and item['device'] == 0x3001
    first_bit = dict(available=item['version'] >= 2)
    for stage in ('pre','early','late'):
        # v1에 존재하지 않거나 timestamp0이면 미관측이다. lines0은 관측된 양선LOW일 수도 있다.
        stamp = item.get('first_'+stage+'_us',0)
        lines = item.get('first_'+stage+'_lines',0)
        first_bit[stage] = dict(observed=stamp != 0,elapsed_us=stamp if stamp else None,
                               raw_lines=lines if stamp else None,
                               scl_high=bool(lines&1) if stamp else None,
                               sda_high=bool(lines&2) if stamp else None)
    early_conflict = bool(first_bit['early']['observed'] and first_bit['early']['scl_high'] and
                          not first_bit['early']['sda_high'])
    first_bit.update(early_conflict=early_conflict,
                     late_high_after_conflict=bool(early_conflict and first_bit['late']['observed'] and
                                                   first_bit['late']['sda_high']),
                     sample_only=True)
    # 약 bias에서 ID를 읽었더라도 원래 배선/조도 측정의 복구를 뜻하지 않는다.
    pullup_mode = item.get('pullup_mode',0)
    taken = item['pin_changes'] == 1 and item['restore_result'] != 2
    return dict(result=BITBANG_RESULTS[item['result']],restore_result=item['restore_result'],
                pullup_mode=pullup_mode,sda_pullup_requested=bool(pullup_mode),pin_takeover_observed=taken,
                temporary_sda_weak_pullup=bool(pullup_mode and taken),
                measurement_scope='temporary-PC9-weak-pullup-ID-only' if pullup_mode else 'NOPULL-ID-only',
                original_hardware_proven=False,lux_measurement_verified=False,
                first_address_bit=first_bit,
                saved_configuration_matches=not differences,restore_differences=differences,
                manufacturer=hex(item['manufacturer']),device=hex(item['device']),
                ids_valid=bool(item['ids_valid']),ids_match=ids_match,
                phase=BITBANG_PHASES[item['phase']],failed_phase=BITBANG_PHASES[item['failed_phase']],
                register=hex(item['reg']),byte_index=item['byte_index'],bit_index=item['bit_index'],
                ack_count=item['ack_count'],ack_mask=hex(item['ack_mask']),
                initial_scl_high=bool(item['initial_lines'] & 1),initial_sda_high=bool(item['initial_lines'] & 2),
                last_scl_high=bool(item['last_lines'] & 1),last_sda_high=bool(item['last_lines'] & 2),
                nominal_bus_hz=20000,source_clock_hz=item['clock_hz'],elapsed_us=item['elapsed_us'],
                max_gap_us=item['max_gap_us'],max_low_us=item['max_low_us'],
                timing_uncertain=bool(item['timing_uncertain']),sensor_scl_low_timeout_possible=item['max_low_us'] >= 28000,
                successful=(item['result'] == 0 and item['restore_result'] == 0 and not differences and
                    taken and item['ids_valid'] == 1 and ids_match and item['ack_count'] == 6 and item['ack_mask'] == 0x3F and
                    1000000 <= item['clock_hz'] <= 180000000 and
                    not item['timing_uncertain'] and item['max_low_us'] < 28000 and not early_conflict))


def decode(data: bytes) -> dict:
    """정확한124-byte ABI와 service magic/version을 검사한다. 초기화0 값을 성공으로 보지 않는다."""
    require(MAILBOX_MAGIC != 0, 'Mailbox magic must be confirmed from AmbientService header')
    require(len(data) == MAILBOX_BYTES and len(FIELDS) == 31, 'Ambient mailbox size mismatch')
    result = dict(zip(FIELDS, struct.unpack('<31I', data)))
    require(result['magic'] == MAILBOX_MAGIC and result['version'] == 1, 'Ambient mailbox ABI not initialized/mismatched')
    require(result['state'] <= 6 and result['request_status'] <= 4, 'Unknown ambient state/status')
    require(result['reserved'] == 0, 'Unknown ambient mailbox reserved data')
    return result


def interpretation(box: dict) -> dict:
    """HAL 오류와 SR1 flag를 별도로 표시한다. ARLO/일반 HAL_ERROR를 임의 NAK로 바꾸지 않는다."""
    status_bits = {8:'BERR', 9:'ARLO', 10:'AF', 11:'OVR', 12:'PECERR', 14:'TIMEOUT', 15:'SMBALERT'}
    hal_bits = {0:'BERR', 1:'ARLO', 2:'AF', 3:'OVR', 4:'DMA', 5:'TIMEOUT', 6:'SIZE', 9:'WRONG_START'}
    return dict(request_accepted=box['request_status'] == 0, operation_id=box['operation_id'],
                result=(RESULTS.get(box['result'], 'UNKNOWN_' + str(box['result']))
                        if box['request_status'] == 0 else 'NOT_APPLICABLE_REJECTED'),
                phase=PHASES[box['phase']] if box['phase'] < len(PHASES) else 'UNKNOWN',
                requested_hz=box['argument'], observed_bus_hz=box['bus_hz'],
                requested_rate_applied=(box['request_status'] == 0 and (box['phase'] >= 3 or box['result'] == 0)
                                        and box['bus_hz'] == box['argument']),
                manufacturer=hex(box['manufacturer']), device=hex(box['device']),
                ids_match=box['manufacturer'] == 0x5449 and box['device'] == 0x3001,
                hal_status=box['hal_status'], hal_error=hex(box['hal_error']),
                hal_error_flags=[name for bit,name in hal_bits.items() if box['hal_error'] & (1 << bit)],
                sr1=hex(box['sr1']), sr1_error_flags=[name for bit,name in status_bits.items() if box['sr1'] & (1 << bit)],
                elapsed_ms=box['elapsed_ms'], ready=bool(box['ready']), valid=bool(box['valid']))


def stable_snapshot(board: AmbientBoard, name: str, attempts: int = 4) -> dict:
    """응답 sequence를 전체 복사 앞뒤에서 읽는다. commit 중인 torn snapshot은 사용하지 않는다."""
    for attempt in range(attempts):
        before = struct.unpack('<I', board.dump(f'{name}-{attempt}-seq-before', board.mailbox + 20, 4))[0]
        box = decode(board.dump(f'{name}-{attempt}-full', board.mailbox, MAILBOX_BYTES))
        after = struct.unpack('<I', board.dump(f'{name}-{attempt}-seq-after', board.mailbox + 20, 4))[0]
        if before == after == box['response_seq']:
            return box
    raise RuntimeError('Ambient completion changed during every bounded read')


def request_probe(board: AmbientBoard, rate: int, timeout_seconds: float, record: dict, manifest_path: Path) -> dict:
    """한 번의 probe 요청을 commit-last로 게시하고 해당 seq/opid의 완료만 기다린다."""
    require(rate in (80000, 100000, 400000), 'Only80/100/400kHz probes are supported')
    initial = stable_snapshot(board, 'before-request')
    require(initial['request_seq'] == initial['response_seq'], 'An existing mailbox request is unfinished')
    sequence = secrets.randbits(32) or 1
    if sequence == initial['request_seq']:
        sequence = (sequence + 1) & 0xFFFFFFFF or 1
    transaction = dict(request_seq=sequence, command=1, argument=rate,
                       state='before-request', previous=initial)
    record['requests'].append(transaction)
    save(manifest_path, record)
    board.assert_running('dhcsr-before-request')
    # 인수 먼저, nonzero seq 마지막: 그 사이 worker는 기존 완료 seq만 관측한다.
    board.command('probe-request', '-w32', hex(board.mailbox + 12), '1',
                  '-w32', hex(board.mailbox + 16), hex(rate),
                  '-w32', hex(board.mailbox + 8), hex(sequence))
    transaction['state'] = 'published'
    save(manifest_path, record)
    deadline = time.monotonic() + timeout_seconds
    polls = 0
    while time.monotonic() < deadline:
        polls += 1
        box = stable_snapshot(board, f'response-{polls}')
        transaction.update(last_observed=box, polls=polls)
        save(manifest_path, record)
        require(box['request_seq'] == sequence and box['command'] == 1 and box['argument'] == rate,
                'Another client changed the ambient request')
        if box['response_seq'] == sequence:
            board.assert_running('dhcsr-after-completion')
            require(box['request_status'] != 0 or box['operation_id'] != 0,
                    'Accepted completion has no operation ID')
            require(box['request_status'] == 0 or box['operation_id'] == 0,
                    'Rejected request unexpectedly has an operation ID')
            if box['request_status'] == 0:
                # DeInit/Init 실패 시 아직 새 rate가 적용되지 않아 old/zero 값이 정당하다.
                # post-init 단계부터만 실제 적용 rate를 요청값과 대조한다.
                if box['phase'] >= 3 or box['result'] == 0:
                    require(box['bus_hz'] == rate, 'Post-init probe rate differs from requested rate')
                if box['result'] == 0:
                    require(box['manufacturer'] == 0x5449 and box['device'] == 0x3001 and box['ready'] == 1,
                            'Successful probe lacks valid sensor IDs/ready evidence')
            # 완료 payload를 한 번 더 읽어 auto retry/다른 owner가 evidence를 바꾸지 않았는지 확인한다.
            again = stable_snapshot(board, 'frozen-completion')
            require(again == box, 'Ambient completed evidence was not frozen')
            transaction.update(state='completed' if box['request_status'] == 0 else 'rejected',
                               completion=box, interpretation=interpretation(box))
            save(manifest_path, record)
            return transaction
        time.sleep(0.1)
    transaction['state'] = 'timeout-no-reset-or-retry'
    save(manifest_path, record)
    raise RuntimeError('Ambient request did not complete before host timeout')


def request_id_bitbang(board: AmbientBitbangBoard, timeout_seconds: float, record: dict, manifest_path: Path) -> dict:
    """cmd3/선택한arg를 한 번 게시하고 mailbox와 별도 진단의 seq/opid/mode를 대조한다."""
    initial = stable_snapshot(board,'before-bitbang-request')
    require(initial['request_seq'] == initial['response_seq'],'An existing ambient request is unfinished')
    sequence = secrets.randbits(32) or 1
    if sequence == initial['request_seq']:
        sequence = (sequence+1) & 0xFFFFFFFF or 1
    transaction = dict(command=3,argument=board.pullup_mode,request_seq=sequence,state='before-request',previous=initial,observations=[])
    record['requests'].append(transaction)
    save(manifest_path,record)
    board.assert_running('dhcsr-before-id-bitbang')
    board.deadline = time.monotonic()+timeout_seconds
    completion = None
    try:
        transaction['state'] = 'publication-attempted-no-retry'
        save(manifest_path,record)
        board.command('id-bitbang-request','-w32',hex(board.mailbox+12),'3',
                      '-w32',hex(board.mailbox+16),str(board.pullup_mode),'-w32',hex(board.mailbox+8),hex(sequence))
        transaction['state'] = 'published'
        save(manifest_path,record)
        while time.monotonic() < board.deadline:
            box = stable_snapshot(board,'id-bitbang-response-'+str(len(transaction['observations'])+1))
            transaction['observations'].append(box)
            save(manifest_path,record)
            require(box['request_seq'] == sequence and box['command'] == 3 and box['argument'] == board.pullup_mode,
                    'Another client changed the ID-bitbang request')
            if box['response_seq'] == sequence:
                require((box['request_status'] == 0) == (box['operation_id'] != 0),
                        'ID-bitbang acceptance and operation ID disagree')
                completion = box
                break
            time.sleep(min(0.1,max(0,board.deadline-time.monotonic())))
        require(completion is not None,'ID-bitbang request did not complete before host deadline')
    finally:
        # 게시/응답 deadline 뒤의 frozen evidence read는 별도 제한 CLI들이다. 새 요청은 없다.
        board.deadline = None
    transaction['completion'] = completion
    save(manifest_path,record)
    require(stable_snapshot(board,'id-bitbang-frozen-mailbox') == completion,'ID-bitbang mailbox evidence changed')
    if completion['request_status'] == 0:
        item = stable_bitbang(board,'id-bitbang-diagnostic')
        require(item['request_seq'] == sequence and item['operation_id'] == completion['operation_id'],
                'ID-bitbang diagnostic belongs to another request/operation')
        require(item['result'] == completion['result'],'ID-bitbang mailbox and diagnostic results disagree')
        transaction['bitbang'] = item
        # 불일치 evidence도 보존한 뒤 실패한다. arg1 요청이 arg0 결과로 둔갑하면 안 된다.
        save(manifest_path,record)
        require(item.get('pullup_mode',0) == board.pullup_mode,
                'ID-bitbang diagnostic pull-up mode disagrees with the explicit request')
        transaction['interpretation'] = bitbang_interpretation(item)
        save(manifest_path,record)
        require(stable_bitbang(board,'id-bitbang-frozen-diagnostic') == item,'ID-bitbang diagnostic was not frozen')
        # 정상 HAL ready/error/IDs는 이번 시험에서 갱신하지 않으므로 성공 검증에 사용하지 않는다.
        if completion['result'] == 0:
            require(transaction['interpretation']['successful'],
                    'Successful ID-bitbang completion lacks matching IDs/ACKs/restoration/timing evidence')
        transaction['state'] = 'completed'
    else:
        transaction.update(state='rejected',interpretation=dict(request_accepted=False,
                           result='NOT_APPLICABLE_REJECTED'))
    board.assert_running('dhcsr-after-id-bitbang')
    require(stable_snapshot(board,'id-bitbang-final-mailbox') == completion,'ID-bitbang operation was replaced')
    save(manifest_path,record)
    return transaction


def request_address_diagnostic(board: AmbientAddressBoard,timeout_seconds: float,record: dict,manifest_path: Path) -> dict:
    """고정 후보 batch cmd4/arg0 한 번을 게시하고 immutable288B 완료를 mailbox와 대조한다."""
    initial=stable_snapshot(board,'before-address-request')
    require(initial['request_seq']==initial['response_seq'],'An existing ambient request is unfinished')
    sequence=secrets.randbits(32) or 1
    if sequence==initial['request_seq']:sequence=(sequence+1)&0xFFFFFFFF or 1
    transaction=dict(command=4,argument=0,request_seq=sequence,state='before-request',previous=initial,observations=[])
    record['requests'].append(transaction);save(manifest_path,record)
    board.assert_running('dhcsr-before-address-diagnostic')
    board.deadline=time.monotonic()+timeout_seconds
    completion=None
    try:
        transaction['state']='publication-attempted-no-retry';save(manifest_path,record)
        board.command('address-request','-w32',hex(board.mailbox+12),'4',
                      '-w32',hex(board.mailbox+16),'0','-w32',hex(board.mailbox+8),hex(sequence))
        transaction['state']='published';save(manifest_path,record)
        while time.monotonic()<board.deadline:
            box=stable_snapshot(board,'address-response-'+str(len(transaction['observations'])+1))
            transaction['observations'].append(box);save(manifest_path,record)
            require(box['request_seq']==sequence and box['command']==4 and box['argument']==0,
                    'Another client changed the address diagnostic request')
            if box['response_seq']==sequence:
                require((box['request_status']==0)==(box['operation_id']!=0),
                        'Address diagnostic acceptance and operation ID disagree')
                completion=box;break
            time.sleep(min(0.1,max(0,board.deadline-time.monotonic())))
        require(completion is not None,'Address diagnostic did not complete before host deadline')
    finally:
        # 요청 deadline 뒤의 frozen raw 수집은 개별 CLI 제한을 쓴다. 추가 게시/복구는 없다.
        board.deadline=None
    transaction['completion']=completion;save(manifest_path,record)
    require(stable_snapshot(board,'address-frozen-mailbox')==completion,'Address mailbox evidence changed')
    if completion['request_status']==0:
        item=stable_address(board,'address-diagnostic')
        transaction['address_diagnostic']=item;save(manifest_path,record)
        require(item['request_seq']==sequence and item['operation_id']==completion['operation_id'],
                'Address diagnostic belongs to another request/operation')
        require(item['result']==completion['result'],'Address mailbox and diagnostic results disagree')
        transaction['interpretation']=address_interpretation(item);save(manifest_path,record)
        require(stable_address(board,'address-frozen-diagnostic')==item,'Address diagnostic was not frozen')
        if completion['result']==0:
            require(transaction['interpretation']['successful'],
                    'Successful address completion lacks complete IDs/STOP/restoration/timing evidence')
        transaction['state']='completed'
    else:
        transaction.update(state='rejected',interpretation=dict(request_accepted=False,result='NOT_APPLICABLE_REJECTED'))
    board.assert_running('dhcsr-after-address-diagnostic')
    require(stable_snapshot(board,'address-final-mailbox')==completion,'Address diagnostic operation was replaced')
    save(manifest_path,record)
    return transaction


def main(argv=None) -> int:
    """기본은 오프라인 계획 출력이다. 명시 실행은 새 폴더에 모든 raw/log/result를 남긴다."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--elf', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--serial', default='STLINK_SERIAL_REQUIRED')
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('--rates', nargs='+', type=int, choices=(80000, 100000, 400000))
    mode.add_argument('--id-bitbang', action='store_true',
                      help='Request command3 once; default argument0 keeps both pins NOPULL')
    mode.add_argument('--address-diagnostic',action='store_true',
                      help='Request command4/argument0 once: fixed 0x44..0x47 IDs with temporary PC9 weak pull-up')
    ap.add_argument('--sda-pullup', action='store_true',
                    help='With --id-bitbang and ABI3 only: temporary PC9 weak pull-up, argument1')
    ap.add_argument('--read-khz', type=int, choices=(100, 950, 4000), default=950)
    ap.add_argument('--timeout', type=float, default=30)
    ap.add_argument('--execute', action='store_true')
    args = ap.parse_args(argv)
    if args.sda_pullup and not args.id_bitbang:
        ap.error('--sda-pullup requires --id-bitbang; other modes cannot be combined')
    diagnostic=args.id_bitbang or args.address_diagnostic
    args.rates = ([] if diagnostic else args.rates if args.rates is not None else [400000, 100000])
    require(0 < args.timeout <= 120, 'Host wait timeout must be1..120seconds')
    if diagnostic:
        require(args.timeout <= 30,'Diagnostic operation host timeout must be at most30seconds')
    contract = image_contract(args.elf, MAILBOX_BYTES, id_bitbang=args.id_bitbang,address_diagnostic=args.address_diagnostic)
    require(not args.sda_pullup or contract['bitbang_version'] == 3,'SDA pull-up requires diagnostic ABI3/212B')
    plan = dict(schema=1, hardware_access=args.execute, mailbox=hex(contract['mailbox']), mailbox_bytes=MAILBOX_BYTES,
                elf=contract['elf'], elf_sha256=contract['elf_sha256'], app_sha256=contract['app_sha256'],
                app_size=contract['app_size'], expected_uid_words=list(EXPECTED_UID), rates_hz=args.rates,
                read_khz=args.read_khz, request_write_khz=50, requests=[],
                allowed_request_offsets=[8,12,16], direct_flash_power_gpio_reset_writes=False,
                id_bitbang=args.id_bitbang,address_diagnostic=args.address_diagnostic,
                state='plan-only' if not args.execute else 'before-identity')
    if args.id_bitbang:
        plan.update(command=3,argument=int(args.sda_pullup),temporary_sda_weak_pullup=args.sda_pullup,
                    max_publications=1,bitbang_address=hex(contract['bitbang']),
                    bitbang_bytes=contract['bitbang_bytes'],bitbang_version=contract['bitbang_version'],
                    nominal_bitbang_hz=20000)
    if args.address_diagnostic:
        plan.update(command=4,argument=0,max_publications=1,diagnostic_address=hex(contract['address_diagnostic']),
                    diagnostic_bytes=ADDRESS_BYTES,diagnostic_version=ADDRESS_VERSION,
                    fixed_addresses=[hex(value) for value in ADDRESS_CANDIDATES],sda_pullup_requested=True,
                    nominal_bitbang_hz=20000)
    if not args.execute:
        print(json.dumps(plan, indent=2))
        return 0
    require(MAILBOX_MAGIC != 0, 'Mailbox magic is not confirmed; do not access target')
    folder = args.output.resolve()
    require(not folder.exists(), 'Use a fresh diagnostic output folder')
    folder.mkdir(parents=True)
    manifest_path = folder / 'manifest.json'
    save(manifest_path, plan)
    board = (AmbientBitbangBoard(args.serial,folder,contract,args.read_khz,args.sda_pullup) if args.id_bitbang else
             AmbientAddressBoard(args.serial,folder,contract,args.read_khz) if args.address_diagnostic else
             AmbientBoard(args.serial, folder, contract['mailbox'], MAILBOX_BYTES, (8,12,16), args.read_khz))
    try:
        plan['identity'] = board.verify_identity(contract)
        plan['state'] = 'identity-verified'
        save(manifest_path, plan)
        if args.id_bitbang:
            request_id_bitbang(board,args.timeout,plan,manifest_path)
        elif args.address_diagnostic:
            request_address_diagnostic(board,args.timeout,plan,manifest_path)
        else:
            for rate in args.rates:
                request_probe(board, rate, args.timeout, plan, manifest_path)
        plan['state'] = ('completed-all-probes-ok' if all(req['completion']['request_status'] == 0 and
                         req['completion']['result'] == 0 and (not diagnostic or
                         req['interpretation'].get('successful',False)) for req in plan['requests'])
                         else 'completed-device-failure-or-rejection')
        board.assert_running('dhcsr-final')
        plan['cpu_running_after'] = True
        save(manifest_path, plan)
        print(json.dumps(plan, indent=2))
        return 0 if plan['state'] == 'completed-all-probes-ok' else 2
    except Exception as error:
        plan.update(state='host-error-no-automatic-recovery', error=str(error))
        save(manifest_path, plan)
        print(json.dumps(plan, indent=2))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
