"""실제 host/CLI 명령 생성부를 fake target에 연결한다. 모든 실제 subprocess는 금지한다."""
import contextlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(TOOLS))
import bluetooth_swd as host
import bringup


def fixture_elf(destination):
    """731f 보존 ELF의 non-ALLOC symbol/string metadata만 바꾼 임시 ABI fixture를 만든다.

    코드는 바꾸지 않는다. 큰 기존 SRAM object를 세 진단 object로 명명/축소한다.
    strtab은 끝에 복사·확장하고 section header의 offset/size만 갱신한다.
    원본 보존 파일 및 실제 장치에는 쓰지 않는다.
    """
    source = TOOLS.parents[2]/'analysis/2026-09-12-integrated-bringup/primitive-final-image/FuckNudo_Noodoe_CFW_Project.elf'
    raw = source.read_bytes(); elf = host.Elf32(raw); data = bytearray(raw)
    shoff = struct.unpack_from('<I',raw,32)[0]
    shentsize = struct.unpack_from('<H',raw,46)[0]
    replacements = {'g_bluetooth':('g_bsp_bt_hci_fault',host.FAULT_BYTES),
                    'g_graphics_arc_sweep':('g_bluetooth_control',host.MAILBOX_BYTES),
                    'g_bsp_capture':(host.RAW_RESET_SYMBOL,host.RAW_RESET_BYTES)}
    changed = set()
    for section in elf.sections:
        if section['type'] != 2:continue
        strings = elf.sections[section['link']]
        table = bytearray(raw[strings['offset']:strings['offset']+strings['size']])
        for relative in range(0,section['size'],16):
            offset = section['offset']+relative
            name,value,size,info,other,shndx = struct.unpack_from('<IIIBBH',data,offset)
            old = bytes(table[name:table.index(0,name)]).decode()
            if old not in replacements:continue
            new,newsize = replacements[old]
            assert size >= newsize
            struct.pack_into('<I',data,offset,len(table))
            struct.pack_into('<I',data,offset+8,newsize)
            table.extend(new.encode()+b'\0');changed.add(old)
        newoffset = len(data);data.extend(table)
        struct.pack_into('<II',data,shoff+section['link']*shentsize+16,newoffset,len(table))
    assert changed == set(replacements)
    destination.write_bytes(data)
    contract = host.image_contract(destination)
    assert contract['image'] == host.validate(elf)[0]
    return contract


def initial_control():
    """이전 실패가 끝난 FAULT 상태를 만들어 오래된 completion의 오귀속을 검사한다."""
    box = dict.fromkeys(host.FIELDS,0)
    box.update(magic=host.MAILBOX_MAGIC,version=1,request_sequence=5,command=1,ack_sequence=5,
               operation_id=5,phase=4,completion_sequence=5,completion_result=0xFFFFFFF2,
               controller_state=3,result_sequence=8)
    return box


class FakeCLI:
    """HAL/SWD 없이 지정 APP, UID, control/fault SRAM만 모델링한다."""
    def __init__(self,contract):
        self.contract=contract;self.image=contract['image'];self.uid=host.EXPECTED_UID
        self.box=initial_control();self.fault=dict.fromkeys(host.FAULT_FIELDS,0)
        self.fault.update(magic=host.FAULT_MAGIC,version=1,sequence=2,reason=0xEEEE)
        self.commands=[];self.writes=[];self.success=False;self.rejected=False
        self.halted=False;self.new_fault=True;self.complete_enabled=True
        self.raw=dict.fromkeys(host.RAW_RESET_FIELDS,0)
        self.raw.update(magic=host.RAW_RESET_MAGIC,version=1,sequence=2,operation_id=5,request_sequence=5,
                        phase=6,result=7,primary_result=7,reset_status=0xFFFFFFFF)
        self.raw_data=bytes(64)

    def packed(self,kind):
        """signed 응답도 bit pattern으로 저장해 실제 little-endian SRAM byte 순서를 재현한다."""
        if kind=='raw_reset':
            return struct.pack('<32I',*(self.raw[k]&0xFFFFFFFF for k in host.RAW_RESET_FIELDS))+self.raw_data
        values,fields=(self.box,host.FIELDS) if kind=='control' else (self.fault,host.FAULT_FIELDS)
        return struct.pack('<'+'I'*len(fields),*(values[k]&0xFFFFFFFF for k in fields))

    def complete(self):
        """ACK와 completion을 별도 필드로 갱신한다. 거절은 예전 opid/결과를 보존한다."""
        if not self.complete_enabled:return
        sequence=self.box['request_sequence']
        self.box.update(ack_sequence=sequence,accept_result=-2 if self.rejected else 0,
                        result_sequence=self.box['result_sequence']+2)
        if self.rejected:return
        if self.box['command']==2:
            self.box.update(operation_id=sequence,phase=3 if self.success else 4,
                completion_sequence=sequence,completion_result=0 if self.success else 7,controller_state=3)
            self.raw.update(sequence=self.raw['sequence']+2,operation_id=sequence,request_sequence=sequence,
                phase=6,result=0 if self.success else 7,primary_result=0 if self.success else 7,
                started_ms=20000,elapsed_ms=501,uart_brr=0x2D9,cr3_before=0x3C1,cr3_bypass=0x1C1,
                cr3_restored=0x3C1,tx_submit_count=1,tx_requested_bytes=4,tx_completed_bytes=4,
                rx_captured_bytes=7 if self.success else 0,uart_sr=0x40,dma_tx_remaining=0,
                dma_rx_remaining=57 if self.success else 64,close_result=0,flow_restore=1,data_valid=1,
                reset_status=0 if self.success else 0xFFFFFFFF)
            self.raw_data=(b'\x04\x0e\x04\x01\x03\x0c\x00' if self.success else b'').ljust(64,b'\0')
            return
        self.box.update(operation_id=sequence,phase=3 if self.success else 4,
                        completion_sequence=sequence,completion_result=0 if self.success else -14,
                        controller_state=2 if self.success else 3)
        if not self.success and self.new_fault:
            self.fault.update(sequence=self.fault['sequence']+2,reason=0x104,tick=10000,
                uart_brr=0x2D9,uart_cr1=0x200C,uart_cr3=0x3C1,gpioa_idr=1<<11,
                dma_tx_ndtr=3,dma_rx_ndtr=256,transport_errors=1)

    def run(self,command,log,timeout=240):
        """실제 Board 생성 CLI를 검사하고 fresh output raw/log만 쓴다. device subprocess는 없다."""
        self.commands.append(command)
        assert 'mode=HOTPLUG' in command
        assert not set(command)&{'-halt','-run','-rst','-w','-e','-rdu'}
        ops=command[command.index('mode=HOTPLUG')+1:]
        if ops==['-ob','displ']:
            text='Device ID : 0x419\nFlash size : 512 KBytes\nRDP : 0xAA\n'
        elif ops[0]=='-u':
            address,size,dest=int(ops[1],0),int(ops[2],0),Path(ops[3])
            if host.APP<=address<address+size<=host.APP+len(self.image):
                result=self.image[address-host.APP:address-host.APP+size]
            elif address==host.UID_ADDRESS:result=struct.pack('<3I',*self.uid)
            elif address==host.DHCSR:result=struct.pack('<I',1<<17 if self.halted else 0)
            else:
                for kind,key,length in [('control','mailbox',host.MAILBOX_BYTES),('fault','fault',host.FAULT_BYTES),
                                        ('raw_reset','raw_reset',host.RAW_RESET_BYTES)]:
                    if key not in self.contract:continue
                    base=self.contract[key]
                    if base<=address<address+size<=base+length:
                        result=self.packed(kind)[address-base:address-base+size];break
                else:raise AssertionError('Unknown read address')
            dest.write_bytes(result);text='Data read successfully\n'
        elif ops[0]=='-w32':
            assert 'freq=50' in command
            for index in range(0,len(ops),3):
                address,value=int(ops[index+1],0),int(ops[index+2],0)
                self.writes.append((address,value))
                key={self.contract['mailbox']+12:'command',self.contract['mailbox']+8:'request_sequence'}[address]
                self.box[key]=value
                if key=='request_sequence':self.complete()
            text='Memory written successfully\n'
        else:raise AssertionError('Unknown command')
        log.write_text(text,encoding='utf-8');return text


class HostTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.elf=self.root/'fixture.elf'
        self.contract=fixture_elf(self.elf);self.fake=FakeCLI(self.contract)
        self.addCleanup(patch.stopall)
        patch.object(subprocess,'run',side_effect=AssertionError('Real subprocess forbidden')).start()
        patch.object(bringup,'run',side_effect=self.fake.run).start()

    def board(self):
        """실제 transport wrapper만 구성한다. verify_identity도 FakeCLI APP bytes를 실제 비교한다."""
        folder=self.root/'board';folder.mkdir()
        return host.BluetoothBoard('STLINK_SERIAL_REQUIRED',folder,self.contract,950)

    def request(self,board):
        """한 요청 결과를 실제 manifest에 기록하는 공통 호출이다."""
        return host.request_start(board,30,{},self.root/'record.json')

    def raw_board(self):
        """별도 raw object가 실제 ELF에 있고 선택된 모드에서만 접근 가능한 fixture다."""
        contract=host.image_contract(self.elf,raw_reset=True)
        self.fake.contract=contract
        folder=self.root/'raw-board';folder.mkdir()
        return host.BluetoothBoard('STLINK_SERIAL_REQUIRED',folder,contract,950)

    def request_raw(self,board):
        """실제 공통 publication 경로의 command2와 독립 raw evidence 검사를 실행한다."""
        return host.request_start(board,30,{},self.root/'raw-record.json',raw_reset=True)

    def test_default_plan_has_no_device_io(self):
        with contextlib.redirect_stdout(io.StringIO()) as out:
            rc=host.main(['--elf',str(self.elf),'--output',str(self.root/'unused')])
        self.assertEqual(rc,0);self.assertFalse(self.fake.commands)
        self.assertEqual(json.loads(out.getvalue())['allowed_request_offsets'],[12,8])
        self.assertFalse((self.root/'unused').exists())

    def test_failure_preserves_before_after_signed_and_new_fault(self):
        board=self.board();board.verify_identity(self.contract);result=self.request(board)
        self.assertEqual(result['state'],'start-failed')
        self.assertEqual(result['before']['completion_result'],-14)
        self.assertEqual(result['completion']['completion_result'],-14)
        self.assertEqual(result['completion']['operation_id'],6)
        self.assertTrue(result['fault']['new_snapshot']);self.assertTrue(result['fault']['cts_pin_high'])
        self.assertEqual(result['fault']['dma_tx_remaining'],3)
        self.assertEqual([a-board.mailbox for a,v in self.fake.writes],[12,8])
        self.assertEqual(self.fake.writes[-1][1],6)
        self.assertTrue(result['cpu_running_after'])

    def test_success_cli_requires_ready_completion(self):
        self.fake.success=True;folder=self.root/'run'
        with contextlib.redirect_stdout(io.StringIO()):
            rc=host.main(['--elf',str(self.elf),'--output',str(folder),'--execute'])
        self.assertEqual(rc,0)
        result=json.loads((folder/'manifest.json').read_text())
        self.assertEqual(result['completion']['phase'],3)
        self.assertFalse(result['fault']['new_snapshot'])

    def test_rejected_ack_preserves_old_operation_and_no_success(self):
        self.fake.rejected=True;board=self.board();board.verify_identity(self.contract)
        result=self.request(board)
        self.assertEqual(result['state'],'request-rejected')
        self.assertEqual(result['acceptance']['result'],-2)
        self.assertEqual(result['completion']['operation_id'],5)
        self.assertEqual(result['completion']['completion_sequence'],5)

    def test_old_fault_is_not_attributed_to_current_failure(self):
        self.fake.new_fault=False;board=self.board();board.verify_identity(self.contract)
        result=self.request(board)
        self.assertEqual(result['state'],'start-failed')
        self.assertFalse(result['fault']['new_snapshot'])

    def test_wrong_app_uid_halted_target_block_publication(self):
        board=self.board();self.fake.uid=(0,0,0)
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.fake.uid=host.EXPECTED_UID;self.fake.halted=True
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.fake.halted=False;self.fake.image=bytes([self.fake.image[0]^1])+self.fake.image[1:]
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.assertFalse(self.fake.writes)

    def test_non_idle_state_refuses_before_writes(self):
        board=self.board();board.verify_identity(self.contract)
        self.fake.box['phase']=2
        with self.assertRaises(RuntimeError):self.request(board)
        self.fake.box['phase']=3;self.fake.box['controller_state']=2
        with self.assertRaises(RuntimeError):self.request(board)
        self.assertFalse(self.fake.writes)

    def test_write_allowlist_one_shot_and_program_guard(self):
        board=self.board()
        with self.assertRaises(RuntimeError):board.command('x','-w32',hex(board.mailbox+12),'1','-w32',hex(board.mailbox+8),'6')
        board.verify_identity(self.contract)
        for operations in [('-halt',),('-run',),('-rst',),('-ob','RDP=0xAA'),
                           ('-w32',hex(board.fault),'0'),('-w32','0x40020018','1'),
                           ('-w32',hex(board.mailbox+12),'2','-w32',hex(board.mailbox+8),'6')]:
            with self.assertRaises(RuntimeError):board.command('x',*operations)
        with self.assertRaises(RuntimeError):board.program(self.elf)
        self.request(board)
        with self.assertRaises(RuntimeError):board.command('again','-w32',hex(board.mailbox+12),'1','-w32',hex(board.mailbox+8),'7')
        self.assertEqual(len(self.fake.writes),2)

    def test_sequence_wrap_uses_positive_signed_delta(self):
        self.fake.box.update(request_sequence=0xFFFFFFFF,ack_sequence=0xFFFFFFFF)
        board=self.board();board.verify_identity(self.contract)
        result=self.request(board)
        self.assertEqual(result['request']['request_sequence'],1)

    def test_timeout_never_retries_and_applies_remaining_deadline(self):
        board=self.board();board.verify_identity(self.contract);self.fake.complete_enabled=False
        # Each mocked timestamp advances one second, ensuring a bounded small offline timeout.
        with patch.object(host.time,'monotonic',side_effect=iter(range(10000))),patch.object(host.time,'sleep'):
            with self.assertRaises(RuntimeError):self.request(board)
        self.assertEqual(len(self.fake.writes),2)
        self.assertIsNone(board.deadline)

    def test_ack_without_matching_completion_does_not_finish(self):
        normal=self.fake.complete
        def queued():
            """ACK0/새 opid가 있어도 예전 completion만 남은 상태는 아직 queued다."""
            normal();self.fake.box.update(phase=1,completion_sequence=5)
        board=self.board();board.verify_identity(self.contract)
        with patch.object(self.fake,'complete',side_effect=queued),patch.object(host.time,'monotonic',side_effect=iter(range(10000))),patch.object(host.time,'sleep'):
            with self.assertRaises(RuntimeError):self.request(board)
        self.assertEqual(len(self.fake.writes),2)

    def test_torn_and_odd_seqlocks_are_not_used(self):
        one=initial_control();one['result_sequence']=9
        two=dict(one,result_sequence=10)
        class Snapshot:
            mailbox=0x20001000
            def __init__(self):
                self.data=iter([struct.pack('<I',9),struct.pack('<12I',*(one[k] for k in host.FIELDS)),struct.pack('<I',9),
                    struct.pack('<I',10),struct.pack('<12I',*(two[k] for k in host.FIELDS)),struct.pack('<I',10)])
            def dump(self,*args):return next(self.data)
        self.assertEqual(host.stable_snapshot(Snapshot(),'test')['result_sequence'],10)

    def test_mutating_failure_snapshot_rejected(self):
        board=self.board();board.verify_identity(self.contract)
        original=host.stable_snapshot
        def mutate(*args,**kwargs):
            """동일 operation의 frozen 재읽기에서 payload가 변하는 모형이다."""
            value=original(*args,**kwargs)
            if args[1]=='frozen-fault':value['reason']^=1
            return value
        with patch.object(host,'stable_snapshot',side_effect=mutate):
            with self.assertRaises(RuntimeError):self.request(board)

    def test_header_and_source_abi_exact_match(self):
        project=TOOLS.parent
        header=(project/'Middlewares/Noodoe/Bluetooth/inc/NoodoeBluetooth.h').read_text()
        clean=re.sub(r'/\*.*?\*/','',header,flags=re.S)
        body=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*Bluetooth_ControlMailbox\s*;',clean,re.S)[1]
        fields=[]
        for declaration in re.findall(r'(?:u?int32_t)\s+([^;]+);',body):fields += [x.strip() for x in declaration.split(',')]
        self.assertEqual(fields,host.FIELDS)
        self.assertEqual([fields.index(x)*4 for x in ('request_sequence','command','ack_sequence','result_sequence')],[8,12,16,44])
        source=(project/'Middlewares/Noodoe/Bluetooth/src/bluetooth_service.c').read_text()
        self.assertRegex(source,r'g_bluetooth_control\s*=\s*\{\s*\.magic=0x42435431U,\.version=1U\s*\}')
        bsp=(project/'Drivers/BSP/src/BSP_BT_HCI.c').read_text()
        self.assertRegex(bsp,r'g_bsp_bt_hci_fault\s*=\s*\{\s*\.magic=0x42464631U,\.version=1U\s*\}')
        driver=(project/'Drivers/BSP/inc/BSP_BT_HCI.h').read_text()
        clean_driver=re.sub(r'/\*.*?\*/','',driver,flags=re.S)
        transport=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*BSP_BT_HCI_Diagnostics\s*;',clean_driver,re.S)[1]
        transport_fields=[]
        for declaration in re.findall(r'uint32_t\s+([^;]+);',transport):
            transport_fields += [x.strip() for x in declaration.split(',')]
        self.assertEqual(transport_fields,host.TRANSPORT)
        body=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*BSP_BT_HCI_FaultSnapshot\s*;',clean_driver,re.S)[1]
        expanded=[]
        for kind,declaration in re.findall(r'(uint32_t|BSP_BT_HCI_Diagnostics)\s+([^;]+);',body):
            if kind=='BSP_BT_HCI_Diagnostics':
                self.assertEqual(declaration.strip(),'transport')
                expanded += ['transport_'+key for key in transport_fields]
                continue
            for field in declaration.split(','):
                field=field.strip()
                if field in ('dma_rx[6]','dma_tx[6]'):
                    expanded += [field[:6]+'_'+key for key in ('cr','ndtr','par','m0ar','m1ar','fcr')]
                else:expanded.append(field)
        self.assertEqual(expanded,host.FAULT_FIELDS)
        self.assertEqual(len(host.FAULT_FIELDS)*4,184)

    def test_invalid_abi_and_short_payload_fail(self):
        with self.assertRaises(RuntimeError):host.decode_control(bytes(host.MAILBOX_BYTES))
        with self.assertRaises(RuntimeError):host.decode_fault(bytes(host.FAULT_BYTES))
        with self.assertRaises(RuntimeError):host.decode_fault(bytes(host.FAULT_BYTES-4))

    def test_raw_plan_is_offline_and_mode_exclusive(self):
        """raw를 명시해도 실행 옵션 없이는 장치/폴더를 건드리지 않는다."""
        arguments=['--elf',str(self.elf),'--output',str(self.root/'unused'),'--raw-reset-diagnostic']
        with contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(host.main(arguments),0)
        plan=json.loads(out.getvalue())
        self.assertEqual((plan['command'],plan['raw_reset_bytes'],plan['max_hci_reset_submissions']),(2,192,1))
        self.assertFalse(self.fake.commands);self.assertFalse((self.root/'unused').exists())
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit) as rejected:
            host.main(arguments+['--start'])
        self.assertEqual(rejected.exception.code,2)

    def test_raw_reset_complete_is_not_stack_ready(self):
        """status0 raw Complete를 관측해도 controller는 FAULT를 유지하며 READY라고 보고하지 않는다."""
        self.fake.success=True;board=self.raw_board();board.verify_identity(self.fake.contract)
        result=self.request_raw(board)
        self.assertEqual(result['state'],'raw-reset-succeeded')
        self.assertEqual(result['after']['controller_state'],3)
        self.assertEqual([value for address,value in self.fake.writes],[2,6])
        self.assertEqual([address-board.mailbox for address,value in self.fake.writes],[12,8])
        self.assertFalse(result['raw_reset']['bluetooth_stack_started'])
        self.assertEqual(result['raw_reset']['reset_command_complete_events'],[dict(offset=0,status=0)])
        self.assertTrue(result['raw_reset']['cleanup_verified']);self.assertTrue(result['cpu_running_after'])

    def test_raw_timeout_preserves_restoration_and_no_response(self):
        """target RX timeout은 coherent한 실패 결과이며 host 오류나 자동 재시작이 아니다."""
        board=self.raw_board();board.verify_identity(self.fake.contract)
        result=self.request_raw(board)
        self.assertEqual(result['state'],'raw-reset-failed')
        self.assertEqual(result['raw_reset']['result'],'RX_TIMEOUT')
        self.assertEqual(result['raw_reset']['raw_rx_hex'],'')
        self.assertEqual(result['raw_reset']['tx_submit_count'],1)
        self.assertTrue(result['raw_reset']['cleanup_verified'])
        self.assertEqual(len(self.fake.writes),2)

    def test_raw_mode_write_read_allowlists_and_identity(self):
        """선택 모드만 게시하며 독립 record 밖 read나 command1 전환도 거부한다."""
        board=self.raw_board()
        request=('-w32',hex(board.mailbox+12),'2','-w32',hex(board.mailbox+8),'6')
        with self.assertRaises(RuntimeError):board.command('before-identity',*request)
        self.fake.uid=(0,0,0)
        with self.assertRaises(RuntimeError):board.verify_identity(self.fake.contract)
        self.assertFalse(self.fake.writes)
        self.fake.uid=host.EXPECTED_UID;board.verify_identity(self.fake.contract)
        for operations in [('-w32',hex(board.mailbox+12),'1','-w32',hex(board.mailbox+8),'6'),
                           ('-w32',hex(board.raw_reset),'0'),
                           ('-w32',hex(board.mailbox+8),'6','-w32',hex(board.mailbox+12),'2')]:
            with self.assertRaises(RuntimeError):board.command('invalid',*operations)
        with self.assertRaises(RuntimeError):board.dump('outside-raw',board.raw_reset-4,8)
        self.request_raw(board)
        with self.assertRaises(RuntimeError):board.command('again',*request)
        self.assertEqual(len(self.fake.writes),2)
        ordinary=self.board();ordinary.verify_identity(self.contract)
        with self.assertRaises(RuntimeError):ordinary.dump('raw-in-start-mode',board.raw_reset,192)

    def test_raw_success_requires_correlated_bytes_and_strict_restore(self):
        """정상 결과 flag만 세운 거짓 성공/이전 record/두 번 송신/불완전 복원을 거부한다."""
        self.fake.success=True;board=self.raw_board();board.verify_identity(self.fake.contract)
        result=self.request_raw(board);raw=result['after_raw_reset']
        mutations=[dict(sequence=result['before_raw_reset']['sequence']),dict(operation_id=5),
            dict(request_sequence=5),dict(phase=5),dict(result=7),dict(primary_result=11),
            dict(tx_submit_count=2),dict(tx_submit_count=0),dict(tx_requested_bytes=3),
            dict(tx_completed_bytes=3),dict(dma_tx_remaining=1),dict(uart_sr=0),
            dict(data_valid=0),dict(flow_restore=2),dict(close_result=-1),dict(cr3_bypass=0x3C1),
            dict(cr3_before=0x1C1),dict(cr3_bypass=0xC1),
            dict(cr3_restored=0x1C1),dict(reset_status=1),dict(rx_captured_bytes=6),
            dict(raw_data_hex=(b'\x04\x0e\x04\x01\x04\x0c\x00').ljust(64,b'\0').hex()),
            dict(raw_data_hex=(b'\x04\x0e\x04\x01\x03\x0c\x01').ljust(64,b'\0').hex())]
        for mutation in mutations:
            with self.subTest(mutation=mutation),self.assertRaises(RuntimeError):
                host.raw_reset_summary(dict(raw,**mutation),result['before_raw_reset'],6,result['completion'])
        # HCI command credits may differ and unrelated initial bytes are preserved.
        shifted=b'\xaa\x04\x0e\x04\x02\x03\x0c\x00'
        summary=host.raw_reset_summary(dict(raw,rx_captured_bytes=8,dma_rx_remaining=56,raw_data_hex=shifted.ljust(64,b'\0').hex()),
                                       result['before_raw_reset'],6,result['completion'])
        self.assertEqual(summary['reset_command_complete_events'],[dict(offset=1,status=0)])

    def test_raw_failure_restoration_and_dma_lengths_are_independent_checks(self):
        """RX 실패에서도 잘못된 복원 flag를 검증 완료라고 표시하지 않는다."""
        board=self.raw_board();board.verify_identity(self.fake.contract);result=self.request_raw(board)
        raw=result['after_raw_reset']
        for changes in (dict(cr3_restored=0),dict(cr3_bypass=0x3C1),dict(cr3_before=0),dict(flow_restore=2)):
            with self.subTest(changes=changes):
                summary=host.raw_reset_summary(dict(raw,**changes),result['before_raw_reset'],6,result['completion'])
                self.assertFalse(summary['cleanup_verified'])
        for changes in (dict(dma_rx_remaining=63),dict(dma_rx_remaining=65),dict(rx_captured_bytes=1),dict(close_result=-1)):
            with self.subTest(changes=changes),self.assertRaises(RuntimeError):
                host.raw_reset_summary(dict(raw,**changes),result['before_raw_reset'],6,result['completion'])
        # Cleanup failure may deliberately leave bytes invalid; never decode stale bytes as an event.
        failure=dict(raw,result=11,data_valid=0,close_result=-1,rx_captured_bytes=7,
                     raw_data_hex=b'\x04\x0e\x04\x01\x03\x0c\x00'.ljust(64,b'\0').hex())
        summary=host.raw_reset_summary(failure,result['before_raw_reset'],6,dict(result['completion'],completion_result=11))
        self.assertFalse(summary['cleanup_verified']);self.assertFalse(summary['raw_bytes_valid'])
        self.assertEqual(summary['reset_command_complete_events'],[])

    def test_raw_rejected_request_does_not_reuse_old_raw_success(self):
        self.fake.rejected=True;self.fake.raw.update(result=0,primary_result=0)
        board=self.raw_board();board.verify_identity(self.fake.contract);result=self.request_raw(board)
        self.assertEqual(result['state'],'request-rejected')
        self.assertTrue(result['raw_reset']['previous_evidence_only'])
        self.assertNotIn('result',result['raw_reset'])
        self.assertEqual(result['after_raw_reset']['sequence'],2)

    def test_raw_hardware_timeout_is_bounded_and_publication_not_retried(self):
        board=self.raw_board();board.verify_identity(self.fake.contract);self.fake.complete_enabled=False
        with patch.object(host.time,'monotonic',side_effect=iter(range(10000))),patch.object(host.time,'sleep'):
            with self.assertRaises(RuntimeError):self.request_raw(board)
        self.assertEqual(len(self.fake.writes),2);self.assertIsNone(board.deadline)

    def test_raw_odd_or_torn_snapshots_retry_and_mutating_final_fails(self):
        board=self.raw_board();board.verify_identity(self.fake.contract)
        original=board.dump;one_torn=True
        def torn(name,address,size,*args,**kwargs):
            nonlocal one_torn
            data=original(name,address,size,*args,**kwargs)
            if one_torn and 'before-raw-reset-0-full' in name:
                one_torn=False
                # odd copy contains an invalid transient enum; decode must wait for coherence.
                altered=bytearray(data);struct.pack_into('<I',altered,8,3);struct.pack_into('<I',altered,20,999)
                return bytes(altered)
            return data
        with patch.object(board,'dump',side_effect=torn):
            result=self.request_raw(board)
        self.assertEqual(result['after_raw_reset']['sequence'],4)
        snap=host.stable_snapshot
        self.fake.box['phase']=4
        board.request_attempted=False  # new process simulation is only for this corruption fixture.
        def mutate(*args,**kwargs):
            value=snap(*args,**kwargs)
            if args[1]=='frozen-raw-reset':value['gpioa_idr']^=1
            return value
        with patch.object(host,'stable_snapshot',side_effect=mutate):
            with self.assertRaisesRegex(RuntimeError,'not frozen'):self.request_raw(board)

    def test_raw_decode_rejects_bounds_abi_and_signed_close(self):
        raw=self.fake.packed('raw_reset')
        for mutation in (raw[:-1],bytes(192)):
            with self.assertRaises(RuntimeError):host.decode_raw_reset(mutation)
        for key,value in [('phase',7),('result',13),('primary_result',13),('rx_captured_bytes',65),
                          ('flow_restore',3),('data_valid',2),('reset_status',256)]:
            changed=bytearray(raw);struct.pack_into('<I',changed,host.RAW_RESET_FIELDS.index(key)*4,value)
            with self.subTest(key=key),self.assertRaises(RuntimeError):host.decode_raw_reset(changed)
        changed=bytearray(raw);struct.pack_into('<I',changed,host.RAW_RESET_FIELDS.index('close_result')*4,0xFFFFFFFF)
        self.assertEqual(host.decode_raw_reset(changed)['close_result'],-1)

    def test_raw_main_exit_status_distinguishes_diagnostic_and_start(self):
        self.fake.contract.update(host.image_contract(self.elf,raw_reset=True));self.fake.success=True
        args=['--elf',str(self.elf),'--output',str(self.root/'raw-main'),'--raw-reset-diagnostic','--execute']
        with contextlib.redirect_stdout(io.StringIO()):self.assertEqual(host.main(args),0)
        result=json.loads((self.root/'raw-main/manifest.json').read_text())
        self.assertEqual(result['after']['controller_state'],3)
        self.assertEqual(result['state'],'raw-reset-succeeded')
        self.fake.success=False;args[3]=str(self.root/'raw-main-failure')
        with contextlib.redirect_stdout(io.StringIO()):self.assertEqual(host.main(args),2)

    def test_raw_header_and_service_abi_exact_match(self):
        """실제 C 헤더의192B 레이아웃을 확장해 host 필드·오프셋과 대조한다."""
        project=TOOLS.parent
        header=(project/'Drivers/BSP/inc/BSP_BT_ResetDiagnostic.h').read_text()
        clean=re.sub(r'/\*.*?\*/','',header,flags=re.S)
        body=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*BSP_BT_ResetDiagnostic\s*;',clean,re.S)[1]
        fields=[]
        for declaration in re.findall(r'(?:u?int32_t)\s+([^;]+);',body):
            fields += [field.strip() for field in declaration.split(',')]
        self.assertEqual(fields,host.RAW_RESET_FIELDS)
        self.assertEqual([fields.index(key)*4 for key in ('sequence','operation_id','request_sequence','close_result')],[8,12,16,104])
        self.assertRegex(body,r'uint8_t\s+raw_data\[64\]')
        self.assertEqual(len(fields)*4+64,host.RAW_RESET_BYTES)
        source=(project/'Drivers/BSP/src/BSP_BT_ResetDiagnostic.c').read_text()
        self.assertRegex(source,r'g_bsp_bt_reset_diagnostic\s*=\s*\{\s*\.magic=0x42524431U,\.version=1U\s*\}')


if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(HostTests))
    out=Path(__file__).with_name('bluetooth_swd_output');out.mkdir(exist_ok=True)
    host.save(out/'results.json',dict(hardware_access=False,subprocess_forbidden=True,tests_run=result.testsRun,
        failures=len(result.failures),errors=len(result.errors),successful=result.wasSuccessful(),
        source_sha256=host.sha(Path(host.__file__).read_bytes()),test_sha256=host.sha(Path(__file__).read_bytes()),
        elf_fixture='Preserved731f ELF; only temporary non-ALLOC symbol/string metadata altered; canonical code unchanged'))
    raise SystemExit(0 if result.wasSuccessful() else 1)
