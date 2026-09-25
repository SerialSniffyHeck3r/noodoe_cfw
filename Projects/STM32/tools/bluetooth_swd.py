"""실행 APP/UID를 검증한 뒤 BT owner에 명시한 동작 한 번만 요청하는 SWD 진단.

기본은 오프라인 계획이다. --execute도 CPU halt/reset/FLASH/전원 GPIO를 직접
조작하지 않는다. owner의 요청 접수와 HCI 시작 완료를 분리해 기록한다.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct
import time
from ambient_swd import AmbientBoard, EXPECTED_UID, DHCSR, UID_ADDRESS, save
from capture_display import LiveBoard
from bringup import APP, require, sha
from validate_image import Elf32, validate

MAILBOX_MAGIC = 0x42435431
MAILBOX_BYTES = 48
FAULT_MAGIC = 0x42464631
FAULT_BYTES = 184
RAW_RESET_MAGIC = 0x42524431
RAW_RESET_BYTES = 192
RAW_RESET_SYMBOL = 'g_bsp_bt_reset_diagnostic'
RAW_RESET_FIELDS = ('magic version sequence operation_id request_sequence phase result primary_result '
    'started_ms elapsed_ms uart_brr cr3_before cr3_bypass cr3_restored tx_submit_count tx_requested_bytes '
    'tx_completed_bytes rx_captured_bytes hal_error uart_sr dma_rx_cr dma_tx_cr dma_rx_remaining '
    'dma_tx_remaining gpioa_idr gpioi_idr close_result flow_restore data_valid reset_status reserved0 reserved1').split()
RAW_RESET_RESULTS = ('OK','CONTEXT','NOT_READY','OPEN','RX_ARM','TX_SUBMIT','TX_TIMEOUT','RX_TIMEOUT',
                     'INVALID_RESPONSE','UART_DMA_ERROR','RESET_STATUS','CLEANUP','CTSE_VERIFY')
RAW_RESET_PHASES = ('IDLE','OPEN','RX_ARM','TX','RX_WAIT','CLEANUP','DONE')
FIELDS = ('magic version request_sequence command ack_sequence accept_result operation_id '
          'phase completion_sequence completion_result controller_state result_sequence').split()
TRANSPORT = ('magic version opened baud tx_blocks rx_blocks tx_bytes rx_bytes errors '
             'last_hal_error tx_busy rx_busy reset_count cts_bypasses rx_complete tx_complete').split()
FAULT_FIELDS = (['magic','version','sequence','reason','tick'] + ['transport_'+key for key in TRANSPORT]
    + 'uart_sr uart_brr uart_cr1 uart_cr2 uart_cr3 gpioa_moder gpioa_idr gpioa_odr gpioa_afr0 gpioa_afr1 '
      'gpioi_moder gpioi_idr gpioi_odr'.split()
    + ['dma_'+direction+'_'+field for direction in ('rx','tx') for field in ('cr','ndtr','par','m0ar','m1ar','fcr')])
STATES = ('OFF','STARTING','READY','FAULT','STOPPING')
PHASES = ('IDLE','QUEUED','RUNNING','SUCCEEDED','FAILED')
ACCEPT_RESULTS = {0:'ACCEPTED',-1:'INVALID',-2:'BUSY',-3:'NOT_READY',-4:'QUEUE_FULL'}


def signed(value: int) -> int:
    """wire u32의 비트는 보존하고 int32 결과 필드만 2의 보수 signed 값으로 해석한다."""
    return value - 0x100000000 if value & 0x80000000 else value


def object_address(elf: Elf32, name: str, size: int) -> int:
    """정확한 크기의 쓰기 가능한 ALLOC object만 허용한다. 임의 SRAM 주소를 CLI로 받지 않는다."""
    symbol = elf.symbols.get(name)
    require(symbol and symbol['type'] == 1 and symbol['size'] == size,
            'Unsupported or missing ELF object: '+name)
    address = symbol['value']
    require(address % 4 == 0 and 0x20000000 <= address <= 0x20030000-size,
            'ELF object is outside aligned main SRAM: '+name)
    require(0 < symbol['section'] < len(elf.sections), 'Invalid object section: '+name)
    section = elf.sections[symbol['section']]
    require(section['flags'] & 3 == 3 and section['addr'] <= address and
            address+size <= section['addr']+section['size'], 'Object section bounds invalid: '+name)
    return address


def image_contract(path: Path, raw_reset: bool = False) -> dict:
    """canonical APP와 선택한 모드의 정확한 ELF object 범위·크기를 검증한다."""
    raw = path.read_bytes()
    elf = Elf32(raw)
    image, manifest = validate(elf)
    mailbox = object_address(elf, 'g_bluetooth_control', MAILBOX_BYTES)
    fault = object_address(elf, 'g_bsp_bt_hci_fault', FAULT_BYTES)
    require(mailbox+MAILBOX_BYTES <= fault or fault+FAULT_BYTES <= mailbox, 'Diagnostic objects overlap')
    contract = dict(elf=str(path.resolve()),elf_sha256=sha(raw),app_sha256=sha(image),app_size=len(image),
                    mailbox=mailbox,fault=fault,image=image,manifest=manifest)
    if raw_reset:
        address = object_address(elf,RAW_RESET_SYMBOL,RAW_RESET_BYTES)
        for other,size in ((mailbox,MAILBOX_BYTES),(fault,FAULT_BYTES)):
            require(address+RAW_RESET_BYTES <= other or other+size <= address,'Raw reset object overlaps diagnostics')
        contract['raw_reset'] = address
    return contract


class BluetoothBoard(AmbientBoard):
    """검증된 HOTPLUG transport를 재사용하며 BT request 두 word와 fault read만 추가한다."""
    def __init__(self, serial: str, folder: Path, contract: dict, read_khz: int):
        """read 영역은 ELF object 두 개다. request clock50kHz와 1회 게시 제한을 유지한다."""
        super().__init__(serial,folder,contract['mailbox'],MAILBOX_BYTES,(8,12),read_khz)
        self.fault = contract['fault']
        self.raw_reset = contract.get('raw_reset')
        self.command_id = 2 if self.raw_reset is not None else 1
        self.request_attempted = False
        self.deadline = None

    def command(self, name: str, *operations: str, timeout: float = 30) -> str:
        """선택한 command 하나만 1회 게시한다. timeout 뒤 자동 재전송하지 않는다."""
        if self.deadline is not None:
            remaining = self.deadline-time.monotonic()
            require(remaining > 0, 'Bluetooth operation host deadline expired')
            timeout = min(timeout,remaining)
        if operations and operations[0] == '-w32':
            require(self.identity_verified, 'APP/UID must be verified before publication')
            require(not self.request_attempted, 'Only one publication is allowed per run')
            require(len(operations) == 6 and operations[3] == '-w32' and
                    int(operations[1],0) == self.mailbox+12 and int(operations[2],0) == self.command_id and
                    int(operations[4],0) == self.mailbox+8 and 0 < int(operations[5],0) <= 0xFFFFFFFF,
                    'Only the selected command then nonzero request sequence is allowed')
            self.request_attempted = True
        if len(operations) == 4 and operations[0] == '-u':
            address,size = int(operations[1],0),int(operations[2],0)
            regions = [(self.fault,FAULT_BYTES)]
            if self.raw_reset is not None:regions.append((self.raw_reset,RAW_RESET_BYTES))
            if any(start <= address < address+size <= start+length for start,length in regions):
                destination = Path(operations[3]).resolve()
                require(destination.parent == self.folder.resolve() and not destination.exists(),
                        'Fault snapshot must use a fresh local raw file')
                self.counter += 1
                return LiveBoard.command(self,f'{self.counter:05d}-{name}',*operations,timeout=timeout)
        return super().command(name,*operations,timeout=timeout)


def decode_control(raw: bytes) -> dict:
    """48-byte mailbox와 enum 범위를 확인하고 접수/완료 결과를 signed로 변환한다."""
    require(len(raw) == MAILBOX_BYTES,'Bluetooth control size mismatch')
    box = dict(zip(FIELDS,struct.unpack('<12I',raw)))
    require(box['magic'] == MAILBOX_MAGIC and box['version'] == 1,'Bluetooth control ABI mismatch')
    require(box['phase'] < len(PHASES) and box['controller_state'] < len(STATES),'Unknown Bluetooth state')
    for key in ('accept_result','completion_result'):
        box[key] = signed(box[key])
    return box


def decode_fault(raw: bytes) -> dict:
    """이전 또는 현재 Open의 최초 fault184 bytes를 해석한다. sequence0은 미수집이다."""
    require(len(raw) == FAULT_BYTES and len(FAULT_FIELDS) == 46,'Bluetooth fault size mismatch')
    fault = dict(zip(FAULT_FIELDS,struct.unpack('<46I',raw)))
    require(fault['magic'] == FAULT_MAGIC and fault['version'] == 1,'Bluetooth fault ABI mismatch')
    return fault


def decode_raw_reset(raw: bytes) -> dict:
    """독립192B raw 진단을 해석한다. RX는 유효 길이와 전체64B 원본을 함께 보존한다."""
    require(len(raw) == RAW_RESET_BYTES and len(RAW_RESET_FIELDS) == 32,'Raw reset record size mismatch')
    result = dict(zip(RAW_RESET_FIELDS,struct.unpack_from('<32I',raw)))
    require(result['magic'] == RAW_RESET_MAGIC and result['version'] == 1,'Raw reset ABI mismatch')
    require(result['phase'] < len(RAW_RESET_PHASES) and result['result'] < len(RAW_RESET_RESULTS)
            and result['primary_result'] < len(RAW_RESET_RESULTS),'Unknown raw reset result/phase')
    require(result['rx_captured_bytes'] <= 64 and result['flow_restore'] <= 2 and result['data_valid'] <= 1,
            'Invalid raw reset count/flags')
    require(result['reset_status'] <= 255 or result['reset_status'] == 0xFFFFFFFF,'Invalid reset status')
    result['close_result'] = signed(result['close_result'])
    result['raw_data_hex'] = raw[128:].hex()
    return result


def stable_snapshot(board: BluetoothBoard, name: str, fault: bool = False, attempts: int = 4,
                    raw_reset: bool = False) -> dict:
    """sequence/전체/sequence를 읽어 equal-even인 복사만 사용한다. odd/torn 값은 제한 재읽기한다."""
    address,size,offset,key,decoder = ((board.fault,FAULT_BYTES,8,'sequence',decode_fault) if fault else
                                      (board.mailbox,MAILBOX_BYTES,44,'result_sequence',decode_control))
    if raw_reset:
        require(not fault and board.raw_reset is not None,'Raw reset mode is not selected')
        address,size,offset,key,decoder = board.raw_reset,RAW_RESET_BYTES,8,'sequence',decode_raw_reset
    for attempt in range(attempts):
        before = struct.unpack('<I',board.dump(f'{name}-{attempt}-seq-before',address+offset,4))[0]
        raw = board.dump(f'{name}-{attempt}-full',address,size)
        after = struct.unpack('<I',board.dump(f'{name}-{attempt}-seq-after',address+offset,4))[0]
        # decode는 coherent bytes에서만 한다. 갱신 중 transient enum을 ABI 손상으로 오인하지 않는다.
        captured = struct.unpack_from('<I',raw,offset)[0]
        if before == after == captured and before % 2 == 0:
            return decoder(raw)
    raise RuntimeError('Bluetooth diagnostic seqlock did not stabilize in bounded reads')


def fault_summary(fault: dict, previous: dict) -> dict:
    """새 fault와 오래된 snapshot을 구별한다. SRAM 기록에 없는 실제 전압/파형을 추정하지 않는다."""
    return dict(new_snapshot=fault['sequence'] != previous['sequence'],
                captured=bool(fault['sequence']),reason=hex(fault['reason']),tick_ms=fault['tick'],
                uart_sr=hex(fault['uart_sr']),uart_brr=hex(fault['uart_brr']),
                uart_cr1=hex(fault['uart_cr1']),uart_cr3=hex(fault['uart_cr3']),
                cts_pin_high=bool(fault['gpioa_idr'] & (1<<11)),
                dma_tx_remaining=fault['dma_tx_ndtr'],dma_rx_remaining=fault['dma_rx_ndtr'],
                transport_errors=fault['transport_errors'],hal_error=hex(fault['transport_last_hal_error']))


def raw_reset_summary(raw: dict, previous: dict, sequence: int, completion: dict) -> dict:
    """이번 원샷의 DMA 정지 후 byte 증거·복원을 대조한다. SPP/BT READY를 추론하지 않는다."""
    require(raw['sequence'] != 0 and raw['sequence'] != previous['sequence'],
            'Raw reset evidence is unchanged from the previous operation')
    require(raw['operation_id'] == raw['request_sequence'] == sequence and raw['phase'] == 6,
            'Raw reset evidence belongs to another or unfinished operation')
    require(raw['result'] == completion['completion_result'],'Control and raw reset results disagree')
    require(raw['tx_submit_count'] <= 1 and raw['tx_requested_bytes'] <= 4 and raw['tx_completed_bytes'] <= 4,
            'Raw reset exceeded its one-command/four-byte limit')
    if raw['data_valid']:
        require(raw['close_result'] == 0 and raw['dma_rx_remaining'] <= 64 and
                raw['rx_captured_bytes'] == 64-raw['dma_rx_remaining'],
                'Raw RX length disagrees with stopped-DMA remaining count/cleanup')
    # RX_TIMEOUT 같은 정상 실패에서도 복원 flag만 신뢰하지 않는다. 실제 보존된
    # CTSE/RTSE 세 snapshot이 맞아야 cleanup_verified를 true로 표시한다.
    flow_registers_verified = (raw['cr3_before'] & 0x300 == 0x300 and
                              raw['cr3_bypass'] & 0x300 == 0x100 and
                              raw['cr3_restored'] & 0x300 == 0x300)
    payload = bytes.fromhex(raw['raw_data_hex'])[:raw['rx_captured_bytes']]
    # H4 Command Complete: type04/event0E/length04/credits/opcode030C/status.
    # credits가 반드시1이라고 가정하지 않으며 충분한7 bytes가 있을 때만 해석한다.
    events = [(offset,payload[offset+6]) for offset in range(max(0,len(payload)-6))
              if raw['data_valid'] == 1
              if payload[offset:offset+3] == b'\x04\x0e\x04' and payload[offset+4:offset+6] == b'\x03\x0c']
    if raw['result'] == 0:
        require(raw['primary_result'] == 0 and raw['tx_submit_count'] == 1 and
                raw['tx_requested_bytes'] == raw['tx_completed_bytes'] == 4,
                'Successful raw reset lacks exactly one complete four-byte transmission')
        require(raw['dma_tx_remaining'] == 0 and raw['uart_sr'] & (1<<6),
                'Successful raw reset lacks DMA completion/USART TC evidence')
        require(raw['data_valid'] == 1 and raw['close_result'] == 0 and raw['flow_restore'] == 1,
                'Successful raw reset lacks stopped-DMA bytes and verified cleanup')
        require(flow_registers_verified,
                'Successful raw reset has inconsistent CTS override/restoration')
        require(raw['reset_status'] == 0 and events and events[0][1] == 0,
                'Successful raw reset lacks raw H4 opcode0x0C03 status0 Command Complete')
    return dict(result=RAW_RESET_RESULTS[raw['result']],primary_result=RAW_RESET_RESULTS[raw['primary_result']],
        raw_rx_hex=payload.hex(),raw_bytes_valid=bool(raw['data_valid']),
        reset_command_complete_events=[dict(offset=offset,status=status) for offset,status in events],
        cleanup_verified=raw['close_result'] == 0 and raw['flow_restore'] == 1 and flow_registers_verified,
        flow_registers_verified=flow_registers_verified,
        flow_override_not_applied=raw['flow_restore'] == 0,
        cts_override_applied=raw['flow_restore'] != 0,tx_submit_count=raw['tx_submit_count'],
        tx_completed_bytes=raw['tx_completed_bytes'],cts_pin_high=bool(raw['gpioa_idr'] & (1<<11)),
        bluetooth_stack_started=False,scope='one raw HCI Reset only; not vendor patch, SPP, pairing or READY')


def request_start(board: BluetoothBoard, timeout: float, record: dict, manifest: Path,
                  raw_reset: bool = False) -> dict:
    """OFF/FAULT에 선택한 동작을 한 번 게시한다. raw 진단은 H4 stack을 시작하지 않는다."""
    command = 2 if raw_reset else 1
    require(board.command_id == command,'Board publication mode disagrees with requested operation')
    initial = stable_snapshot(board,'before-control')
    previous_fault = stable_snapshot(board,'before-fault',fault=True)
    record.update(before=initial,before_fault=previous_fault)
    if raw_reset:
        record['before_raw_reset'] = stable_snapshot(board,'before-raw-reset',raw_reset=True)
    save(manifest,record)
    require(initial['request_sequence'] == initial['ack_sequence'] and initial['phase'] not in (1,2),
            'Existing Bluetooth request/start has not completed')
    require(initial['controller_state'] in (0,3),'Explicit START requires OFF or FAULT')
    # Owner는 signed(seq-ack)>0만 받는다. random32는 이전 seq보다 과거로 해석될 수 있다.
    sequence = (initial['request_sequence']+1) & 0xFFFFFFFF or 1
    record.update(request=dict(command=command,request_sequence=sequence),state='before-publication',observations=[])
    save(manifest,record)
    board.assert_running('dhcsr-before-start')
    board.deadline = time.monotonic()+timeout
    terminal = None
    try:
        # 처리 중 프로세스 timeout이라도 target에 도달했을 수 있으므로 자동 재전송하지 않는다.
        record['state'] = 'publication-attempted-no-retry'
        save(manifest,record)
        board.command('raw-reset-request' if raw_reset else 'start-request','-w32',hex(board.mailbox+12),str(command),
                      '-w32',hex(board.mailbox+8),hex(sequence))
        record['state'] = 'published'
        save(manifest,record)
        while time.monotonic() < board.deadline:
            box = stable_snapshot(board,'response-'+str(len(record['observations'])+1))
            record['observations'].append(box)
            save(manifest,record)
            require(box['request_sequence'] == sequence and box['command'] == command,
                    'Another client changed the Bluetooth request')
            if box['ack_sequence'] == sequence:
                record['acceptance'] = dict(result=box['accept_result'],
                    name=ACCEPT_RESULTS.get(box['accept_result'],'UNKNOWN'))
                if box['accept_result'] != 0:
                    terminal = box
                    record['state'] = 'request-rejected'
                    break
                require(box['operation_id'] == sequence,'Accepted operation ID does not match request')
                if box['completion_sequence'] == sequence:
                    require(box['phase'] in (3,4),'Completed operation has nonterminal phase')
                    require((box['phase'] == 3) == (box['completion_result'] == 0),
                            'Completion result and phase disagree')
                    if raw_reset:
                        require(box['controller_state'] in (0,3),'Raw reset unexpectedly started the Bluetooth stack')
                    elif box['phase'] == 3:
                        require(box['controller_state'] == 2,'Successful start lacks READY controller state')
                    terminal = box
                    record['state'] = ('raw-reset-' if raw_reset else 'start-') + ('succeeded' if box['phase'] == 3 else 'failed')
                    break
            time.sleep(min(0.1,max(0,board.deadline-time.monotonic())))
        require(terminal is not None,'Bluetooth request did not complete within host deadline')
    finally:
        # Identity와 완료 후 evidence 수집은 operation30초 제한과 별도다. reset/retry는 없다.
        board.deadline = None
    record['completion'] = terminal
    save(manifest,record)
    after = stable_snapshot(board,'after-control')
    # READY 이후 별도 fault로 controller_state는 변할 수 있지만 완료 payload는 보존돼야 한다.
    frozen_keys = ('request_sequence','command','ack_sequence','accept_result','operation_id',
                   'phase','completion_sequence','completion_result')
    require(all(after[key] == terminal[key] for key in frozen_keys),'Completed Bluetooth operation was replaced')
    fault = stable_snapshot(board,'after-fault',fault=True)
    again = stable_snapshot(board,'frozen-fault',fault=True)
    require(fault == again,'Bluetooth first-fault evidence was not frozen')
    if raw_reset:
        raw = stable_snapshot(board,'after-raw-reset',raw_reset=True)
        record['after_raw_reset'] = raw
        save(manifest,record)
        again = stable_snapshot(board,'frozen-raw-reset',raw_reset=True)
        require(raw == again,'Raw reset evidence was not frozen')
        if terminal['accept_result'] == 0:
            record['raw_reset'] = raw_reset_summary(raw,record['before_raw_reset'],sequence,terminal)
        else:
            # 거절된 요청에는 이전 raw 결과를 귀속하지 않는다. 이번 성공으로 표시하지 않는다.
            record['raw_reset'] = dict(request_rejected=True,previous_evidence_only=True,
                                       bluetooth_stack_started=False)
        require(after['controller_state'] in (0,3),'Controller left OFF/FAULT during raw reset diagnostic')
    board.assert_running('dhcsr-after-start')
    record.update(after=after,after_fault=fault,fault=fault_summary(fault,previous_fault),cpu_running_after=True)
    save(manifest,record)
    return record


def main(argv=None) -> int:
    """기본 계획은 장치/폴더를 건드리지 않는다. 실행은 fresh folder와 명시1회 START만 허용한다."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--serial',default='STLINK_SERIAL_REQUIRED')
    parser.add_argument('--read-khz',type=int,choices=(100,950,4000),default=950)
    parser.add_argument('--timeout',type=float,default=30)
    parser.add_argument('--execute',action='store_true')
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--start',action='store_true',help='Explicit existing START mode (also the default)')
    modes.add_argument('--raw-reset-diagnostic',action='store_true',help='One isolated HCI Reset with temporary CTS override')
    args = parser.parse_args(argv)
    require(0 < args.timeout <= 30,'BT operation timeout must be positive and at most30seconds')
    contract = image_contract(args.elf,raw_reset=args.raw_reset_diagnostic)
    record = dict(schema=1,hardware_access=args.execute,elf=contract['elf'],elf_sha256=contract['elf_sha256'],
        app_sha256=contract['app_sha256'],app_size=contract['app_size'],expected_uid_words=list(EXPECTED_UID),
        mailbox=hex(contract['mailbox']),mailbox_bytes=MAILBOX_BYTES,
        fault_address=hex(contract['fault']),fault_bytes=FAULT_BYTES,
        allowed_request_offsets=[12,8],command=2 if args.raw_reset_diagnostic else 1,max_publications=1,timeout_seconds=args.timeout,
        read_khz=args.read_khz,request_write_khz=50,direct_flash_power_gpio_reset_writes=False,
        state='plan-only' if not args.execute else 'before-identity')
    if args.raw_reset_diagnostic:
        record.update(raw_reset_address=hex(contract['raw_reset']),raw_reset_bytes=RAW_RESET_BYTES,
                      raw_reset_mode=True,max_hci_reset_submissions=1,raw_response_capacity=64,
                      bluetooth_stack_started=False)
    if not args.execute:
        print(json.dumps(record,indent=2))
        return 0
    folder = args.output.resolve()
    require(not folder.exists(),'Use a fresh Bluetooth diagnostic output folder')
    folder.mkdir(parents=True)
    manifest = folder/'manifest.json'
    save(manifest,record)
    board = BluetoothBoard(args.serial,folder,contract,args.read_khz)
    try:
        record['identity'] = board.verify_identity(contract)
        request_start(board,args.timeout,record,manifest,raw_reset=args.raw_reset_diagnostic)
        print(json.dumps(record,indent=2))
        if args.raw_reset_diagnostic:
            return 0 if record['state'] == 'raw-reset-succeeded' else 2
        return 0 if record['state'] == 'start-succeeded' and record['after']['controller_state'] == 2 else 2
    except Exception as error:
        record.update(state='host-error-no-automatic-recovery',error=str(error))
        save(manifest,record)
        print(json.dumps(record,indent=2))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
