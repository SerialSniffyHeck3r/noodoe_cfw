"""실제 호스트 코드/Board CLI 생성부를 검사한다. subprocess.run은 전체 시험에서 금지한다."""
import argparse
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
sys.path.insert(0, str(TOOLS))
import ambient_swd as host
import bringup


def fixture_elf(destination):
    """보존 ELF의 코드/LOAD bytes는 유지하고 test-only symbol metadata만 바꾼다.

    기존 g_storage_service와 g_ambient_mailbox는 같은17문자다. 로컬 임시 ELF의
    symbol 이름/size만 바꿔 아직 빌드 전인 새 mailbox를 모델링한다. 실제 장치나
    보존 ELF를 변경하지 않는다. canonical APP는 원래 preserved BIN과 동일하다.
    """
    source = TOOLS.parents[2] / 'analysis/2026-09-12-integrated-bringup/primitive-final-image/FuckNudo_Noodoe_CFW_Project.elf'
    original = source.read_bytes()
    elf = host.Elf32(original)
    data = bytearray(original)
    matches = 0
    for section in elf.sections:
        if section['type'] != 2:
            continue
        strings = elf.sections[section['link']]
        for relative in range(0, section['size'], 16):
            offset = section['offset'] + relative
            name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', data, offset)
            start = strings['offset'] + name
            end = data.index(0, start)
            if bytes(data[start:end]) != b'g_storage_service':
                continue
            data[start:end] = b'g_ambient_mailbox'
            struct.pack_into('<I', data, offset + 8, host.MAILBOX_BYTES)
            matches += 1
    if matches != 1:
        raise AssertionError('Fixture source symbol changed')
    destination.write_bytes(data)
    return host.image_contract(destination, host.MAILBOX_BYTES)


def fixture_bitbang_elf(destination, version=host.BITBANG_VERSION, address=False):
    """선택한184B/208B/212B/288B diagnostic의 test symbol을 기존732B g_usb_device에 만든다.

    non-ALLOC string/symbol metadata만 갱신하고 canonical APP는 그대로임을 검증한다.
    저장 원본은 수정하지 않으며 임시 ELF는 FakeCLI 시험에서만 사용한다.
    """
    original_contract=fixture_elf(destination)
    diagnostic_size=host.ADDRESS_BYTES if address else host.BITBANG_ABIS[version][0]
    diagnostic_symbol=host.ADDRESS_SYMBOL if address else host.BITBANG_SYMBOL
    raw=destination.read_bytes();data=bytearray(raw);elf=host.Elf32(raw)
    shoff=struct.unpack_from('<I',raw,32)[0];shentsize=struct.unpack_from('<H',raw,46)[0]
    count=0
    for section in elf.sections:
        if section['type']!=2:continue
        strings=elf.sections[section['link']]
        table=bytearray(raw[strings['offset']:strings['offset']+strings['size']])
        for relative in range(0,section['size'],16):
            offset=section['offset']+relative
            name,value,size,info,other,shndx=struct.unpack_from('<IIIBBH',data,offset)
            if bytes(table[name:table.index(0,name)])!=b'g_usb_device':continue
            assert size>=diagnostic_size
            struct.pack_into('<I',data,offset,len(table))
            struct.pack_into('<I',data,offset+8,diagnostic_size)
            table.extend(diagnostic_symbol.encode()+b'\0');count+=1
        newoffset=len(data);data.extend(table)
        struct.pack_into('<II',data,shoff+section['link']*shentsize+16,newoffset,len(table))
    assert count==1
    destination.write_bytes(data)
    contract=host.image_contract(destination,host.MAILBOX_BYTES,id_bitbang=not address,address_diagnostic=address)
    assert contract['image']==original_contract['image']
    return contract


def empty_box():
    """서비스가 초기화됐지만 아직 host 요청을 받지 않은 ABI fixture다."""
    box = dict.fromkeys(host.FIELDS, 0)
    box.update(magic=host.MAILBOX_MAGIC, version=1)
    return box


def address_record():
    """주소4개를 실제 방문했고 모두 NACK인 완료 fixture다. GPIO/I2C 복원은 성공한다."""
    item=dict.fromkeys(host.ADDRESS_HEADER_FIELDS+host.ADDRESS_TAIL_FIELDS,0)
    item.update(magic=host.ADDRESS_MAGIC,version=host.ADDRESS_VERSION,bytes=host.ADDRESS_BYTES,
                sequence=2,operation_id=11,result=7,attempted_mask=15,elapsed_us=2300,
                pin_changes=1,pullup_mode=1,max_gap_us=3,max_low_us=28)
    item['entry']=[]
    for address in host.ADDRESS_CANDIDATES:
        entry=dict.fromkeys(host.ADDRESS_ENTRY_FIELDS,0)
        entry.update(address=address,attempted=1,result=7,phase=4,elapsed_us=560)
        item['entry'].append(entry)
    return item


def address_bytes(item):
    """실제 헤더 ABI 그대로72word를 serialize한다. Host decoder를 역으로 사용하지 않는다."""
    words=[item[k] for k in host.ADDRESS_HEADER_FIELDS]
    for entry in item['entry']:words.extend(entry[k] for k in host.ADDRESS_ENTRY_FIELDS)
    words.extend(item[k] for k in host.ADDRESS_TAIL_FIELDS)
    return struct.pack('<72I',*words)


class FakeCLI:
    """APP/UID/mailbox/DHCSR만 반환하고 허용 request writes에서만 완료 payload를 만든다."""
    def __init__(self, contract):
        self.contract = contract
        self.image = contract['image']
        self.uid = host.EXPECTED_UID
        self.box = empty_box()
        self.commands = []
        self.writes = []
        self.result_by_rate = {80000: 0, 100000: 0, 400000: 4}
        self.rejected = False
        self.bad_rate = False
        self.halted = False
        self.bitbang_fields=host.BITBANG_ABIS[contract['bitbang_version']][1] if 'bitbang_version' in contract else []
        self.bitbang=dict.fromkeys(self.bitbang_fields,0)
        if self.bitbang_fields:
            self.bitbang.update(magic=host.BITBANG_MAGIC,version=contract['bitbang_version'],bytes=contract['bitbang_bytes'])
        self.bitbang_result=7
        self.bitbang_restore=0
        self.bitbang_bad_opid=False
        self.bitbang_mode_override=None
        self.address=address_record()
        self.address_bad_request=False
        self.address_bad_operation=False

    def raw_box(self):
        """고정31word ABI의 원시 little-endian byte 순서를 만든다."""
        return struct.pack('<31I', *(self.box[key] for key in host.FIELDS))

    def complete(self):
        """worker 응답을 모델링한다. 실패400k/성공100k 및 operation ID를 분리한다."""
        if self.box['command']==3:
            self.complete_bitbang();return
        if self.box['command']==4:
            self.complete_address();return
        rate = self.box['argument']
        result = self.result_by_rate[rate]
        self.box.update(operation_id=0 if self.rejected else self.box['operation_id'] + 1,
                        request_status=1 if self.rejected else 0, result=result,
                        completed_ms=321, state=5 if result else 3, driver_magic=0x414C5331,
                        ready=0 if result else 1, error=result, manufacturer=0 if result else 0x5449,
                        device=0 if result else 0x3001, phase=5 if result else 7,
                        hal_status=3 if result else 0, hal_error=0x20 if result else 0,
                        sr1=0x200 if result else 0, elapsed_ms=20 if result else 2,
                        bus_hz=123 if self.bad_rate else rate, enabled=0 if result else 1)
        self.box['response_seq'] = self.box['request_seq']

    def complete_bitbang(self):
        """normal HAL snapshot은 실패 상태 그대로 두고 이번 bitbang evidence만 따로 완료한다."""
        result=self.bitbang_result
        self.box.update(operation_id=0 if self.rejected else 11,request_status=1 if self.rejected else 0,
                        result=result,completed_ms=1234,state=6,driver_magic=0x414C5331,
                        ready=0,error=4,manufacturer=0,device=0,bus_hz=80000,phase=5,sr1=0x200)
        if not self.rejected:
            self.bitbang.update(sequence=2,operation_id=12 if self.bitbang_bad_opid else 11,
                request_seq=self.box['request_seq'],result=result,restore_result=self.bitbang_restore,
                manufacturer=0 if result else 0x5449,device=0 if result else 0x3001,
                ids_valid=0 if result else 1,elapsed_us=4000,phase=12,failed_phase=4 if result else 0,
                reg=0x7E if result else 0x7F,ack_count=0 if result else 6,ack_mask=0 if result else 0x3F,
                initial_lines=3,last_lines=3,clock_hz=168000000,max_gap_us=25,max_low_us=26,completed_ms=1234,
                pin_changes=1)
            if self.contract['bitbang_version'] >= 3:
                self.bitbang['pullup_mode']=(self.box['argument'] if self.bitbang_mode_override is None
                                             else self.bitbang_mode_override)
        self.box['response_seq']=self.box['request_seq']

    def complete_address(self):
        """cmd4만 새 주소 evidence를 완성하고 이전 cmd3 및 normal HAL evidence는 쓰지 않는다."""
        self.box.update(operation_id=0 if self.rejected else 11,request_status=1 if self.rejected else 0,
                        result=self.address['result'],completed_ms=1234,state=6)
        if not self.rejected:
            self.address.update(request_seq=self.box['request_seq']^(1 if self.address_bad_request else 0),
                                operation_id=12 if self.address_bad_operation else 11,completed_ms=1234)
        self.box['response_seq']=self.box['request_seq']

    def run(self, command, log, timeout=240):
        """실제 실행 대신 로그와 raw output 파일만 쓴다. 임의 주소/명령은 assertion 실패다."""
        self.commands.append(command)
        assert 'mode=HOTPLUG' in command
        assert not set(command) & {'-halt','-run','-rst','-w','-e','-rdu'}
        ops = command[command.index('mode=HOTPLUG') + 1:]
        if ops == ['-ob','displ']:
            text = 'Device ID : 0x419\nFlash size : 512 KBytes\nRDP : 0xAA\n'
        elif ops[0] == '-u':
            address, length, destination = int(ops[1],0), int(ops[2],0), Path(ops[3])
            base = self.contract['mailbox']
            if host.APP <= address < address + length <= host.APP + len(self.image):
                data = self.image[address-host.APP:address-host.APP+length]
            elif address == host.UID_ADDRESS:
                data = struct.pack('<3I', *self.uid)
            elif address == host.DHCSR:
                data = struct.pack('<I', 1 << 17 if self.halted else 0)
            elif base <= address < address + length <= base + host.MAILBOX_BYTES:
                data = self.raw_box()[address-base:address-base+length]
            elif 'bitbang' in self.contract and self.contract['bitbang'] <= address < address+length <= self.contract['bitbang']+self.contract['bitbang_bytes']:
                offset=address-self.contract['bitbang']
                data=struct.pack('<'+str(len(self.bitbang_fields))+'I',
                                 *(self.bitbang[key] for key in self.bitbang_fields))[offset:offset+length]
            elif 'address_diagnostic' in self.contract and self.contract['address_diagnostic'] <= address < address+length <= self.contract['address_diagnostic']+host.ADDRESS_BYTES:
                offset=address-self.contract['address_diagnostic'];data=address_bytes(self.address)[offset:offset+length]
            else:
                raise AssertionError('Unknown modeled read')
            destination.write_bytes(data)
            text = 'Data read successfully\n'
        elif ops[0] == '-w32':
            assert 'freq=50' in command
            base = self.contract['mailbox']
            for at in range(0,len(ops),3):
                address,value = int(ops[at+1],0), int(ops[at+2],0)
                key = {base+8:'request_seq',base+12:'command',base+16:'argument'}[address]
                self.writes.append((address,value))
                self.box[key] = value
                if key == 'request_seq':
                    self.complete()
            text = 'Memory written successfully\n'
        else:
            raise AssertionError('Unknown modeled command')
        log.write_text(text, encoding='utf-8')
        return text


class HostTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.elf = self.root / 'fixture.elf'
        self.contract = fixture_bitbang_elf(self.elf)
        self.fake = FakeCLI(self.contract)
        self.addCleanup(patch.stopall)
        patch.object(subprocess, 'run', side_effect=AssertionError('Real subprocess forbidden')).start()
        patch.object(bringup, 'run', side_effect=self.fake.run).start()

    def board(self):
        """한 run마다 새 output directory를 쓰는 실제 AmbientBoard를 만든다."""
        folder = self.root / 'board'
        folder.mkdir()
        return host.AmbientBoard('STLINK_SERIAL_REQUIRED', folder, self.contract['mailbox'],
                                 host.MAILBOX_BYTES,(8,12,16),950)

    def bitbang_board(self, sda_pullup=False):
        """bitbang의 더 좁은 1회 request allowlist를 실제 생성한다."""
        folder=self.root/'bitbang-board';folder.mkdir()
        return host.AmbientBitbangBoard('STLINK_SERIAL_REQUIRED',folder,self.contract,950,sda_pullup)

    def test_default_plan_performs_no_device_io(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            rc = host.main(['--elf',str(self.elf),'--output',str(self.root/'unused')])
        self.assertEqual(rc,0)
        self.assertFalse(json.loads(output.getvalue())['hardware_access'])
        self.assertEqual(self.fake.commands,[])
        self.assertFalse((self.root/'unused').exists())

    def test_id_bitbang_and_rates_are_mutually_exclusive(self):
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit) as raised:
            host.main(['--elf',str(self.elf),'--output',str(self.root/'unused'),
                       '--rates','80000','--id-bitbang','--execute'])
        self.assertEqual(raised.exception.code,2)
        self.assertEqual(self.fake.commands,[])

    def test_id_bitbang_plan_no_reads_or_writes(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            rc=host.main(['--elf',str(self.elf),'--output',str(self.root/'unused'),'--id-bitbang'])
        plan=json.loads(output.getvalue())
        self.assertEqual(rc,0);self.assertEqual(plan['rates_hz'],[])
        self.assertEqual(plan['command'],3);self.assertEqual(plan['argument'],0)
        self.assertEqual(plan['bitbang_bytes'],212);self.assertEqual(plan['bitbang_version'],3)
        self.assertFalse(self.fake.commands)

    def test_id_bitbang_success_ignores_stale_hal_driver_failure(self):
        self.fake.bitbang_result=0;folder=self.root/'bitbang-run'
        with contextlib.redirect_stdout(io.StringIO()):
            rc=host.main(['--elf',str(self.elf),'--output',str(folder),'--id-bitbang','--execute'])
        result=json.loads((folder/'manifest.json').read_text());request=result['requests'][0]
        self.assertEqual(rc,0);self.assertTrue(request['interpretation']['successful'])
        self.assertEqual(request['completion']['ready'],0)
        self.assertEqual(request['completion']['manufacturer'],0)
        self.assertEqual(request['bitbang']['manufacturer'],0x5449)
        self.assertEqual(request['interpretation']['source_clock_hz'],168000000)
        self.assertEqual(request['interpretation']['nominal_bus_hz'],20000)
        self.assertEqual([a-self.contract['mailbox'] for a,v in self.fake.writes],[12,16,8])
        self.assertEqual([v for a,v in self.fake.writes[:2]],[3,0])

    def test_id_bitbang_nack_is_failure_with_independent_restoration(self):
        board=self.bitbang_board();board.verify_identity(self.contract)
        result=host.request_id_bitbang(board,30,dict(requests=[]),self.root/'record.json')
        self.assertEqual(result['interpretation']['result'],'NACK')
        self.assertEqual(result['interpretation']['restore_result'],0)
        self.assertFalse(result['interpretation']['successful'])
        self.assertEqual(len(self.fake.writes),3)

    def test_id_bitbang_wrong_operation_and_restore_mismatch_rejected(self):
        board=self.bitbang_board();board.verify_identity(self.contract);self.fake.bitbang_bad_opid=True
        with self.assertRaises(RuntimeError):
            host.request_id_bitbang(board,30,dict(requests=[]),self.root/'record.json')
        # 성공 assertion은 restore mismatch도 거부한다. 두 번째 게시 없이 해석만 검사한다.
        item=dict(self.fake.bitbang,result=0,restore_result=1,ids_valid=1,manufacturer=0x5449,device=0x3001)
        self.assertFalse(host.bitbang_interpretation(item)['successful'])

    def test_id_bitbang_rejection_has_no_new_diagnostic_claim(self):
        self.fake.rejected=True;board=self.bitbang_board();board.verify_identity(self.contract)
        result=host.request_id_bitbang(board,30,dict(requests=[]),self.root/'record.json')
        self.assertEqual(result['state'],'rejected');self.assertNotIn('bitbang',result)

    def test_id_bitbang_write_guards_and_timeout_one_publication(self):
        board=self.bitbang_board();board.verify_identity(self.contract)
        for address in (board.bitbang,0x40005C00,board.mailbox+20):
            with self.assertRaises(RuntimeError):board.command('x','-w32',hex(address),'0')
        with patch.object(host.time,'monotonic',side_effect=iter(range(10000))),patch.object(host.time,'sleep'),\
             patch.object(self.fake,'complete',return_value=None):
            with self.assertRaises(RuntimeError):
                host.request_id_bitbang(board,30,dict(requests=[]),self.root/'record.json')
        self.assertEqual(len(self.fake.writes),3);self.assertIsNone(board.deadline)
        with self.assertRaises(RuntimeError):
            board.command('again','-w32',hex(board.mailbox+12),'3','-w32',hex(board.mailbox+16),'0',
                          '-w32',hex(board.mailbox+8),'77')

    def test_bitbang_header_constants_and_all53_fields_match(self):
        source=(TOOLS.parent/'Drivers/BSP/inc/BSP_AmbientBitbang.h').read_text()
        for name,value in [('MAGIC',host.BITBANG_MAGIC),('BYTES',host.BITBANG_BYTES),('VERSION',3),('HZ',20000)]:
            match=re.search(r'#define\s+BSP_AMBIENT_BITBANG_'+name+r'\s+(0x[0-9A-Fa-f]+|\d+)U',source)
            self.assertEqual(int(match[1],0),value)
        clean=re.sub(r'/\*.*?\*/','',source,flags=re.S)
        body=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*BSP_AmbientBitbang_Diagnostics\s*;',clean,re.S)[1]
        fields=[]
        for declaration in re.findall(r'uint32_t\s+([^;]+);',body):
            fields += [part.strip() for part in declaration.split(',')]
        self.assertEqual(fields,host.BITBANG_FIELDS)
        self.assertEqual(fields[:46],host.BITBANG_PREFIX_FIELDS)
        self.assertEqual(fields[:52],host.BITBANG_V2_FIELDS)
        self.assertEqual(fields.index('pullup_mode')*4,208)
        self.assertEqual([fields.index(k)*4 for k in ('sequence','operation_id','request_seq','result','restore_result')],[12,16,20,24,28])
        self.assertEqual([fields.index(k)*4 for k in host.BITBANG_SAMPLE_FIELDS],[184,188,192,196,200,204])

    def test_bitbang_odd_zero_torn_sequence_not_accepted(self):
        values=dict(self.fake.bitbang,sequence=3)
        good=dict(values,sequence=4)
        class Snapshot:
            bitbang=0x20001000
            bitbang_bytes=host.BITBANG_BYTES
            bitbang_version=host.BITBANG_VERSION
            def __init__(self):
                self.data=iter([struct.pack('<I',3),struct.pack('<53I',*(values[k] for k in host.BITBANG_FIELDS)),
                    struct.pack('<I',3),struct.pack('<I',4),struct.pack('<53I',*(good[k] for k in host.BITBANG_FIELDS)),struct.pack('<I',4)])
            def dump(self,*args):return next(self.data)
        self.assertEqual(host.stable_bitbang(Snapshot(),'test')['sequence'],4)
        with self.assertRaises(RuntimeError):host.decode_bitbang(bytes(host.BITBANG_BYTES))

    def test_v1_elf_selects184_bytes_and_remains_usable(self):
        contract=fixture_bitbang_elf(self.elf,version=1);fake=FakeCLI(contract);fake.bitbang_result=0
        folder=self.root/'v1-run'
        with patch.object(bringup,'run',side_effect=fake.run),contextlib.redirect_stdout(io.StringIO()):
            rc=host.main(['--elf',str(self.elf),'--output',str(folder),'--id-bitbang','--execute'])
        result=json.loads((folder/'manifest.json').read_text())
        self.assertEqual(rc,0);self.assertEqual(result['bitbang_version'],1)
        self.assertEqual(result['bitbang_bytes'],184)
        evidence=result['requests'][0]['bitbang']
        self.assertNotIn('first_pre_lines',evidence)
        self.assertFalse(result['requests'][0]['interpretation']['first_address_bit']['available'])

    def test_v2_late_high_does_not_reclassify_early_conflict(self):
        board=self.bitbang_board();board.verify_identity(self.contract)
        self.fake.bitbang_result=6
        self.fake.bitbang.update(first_pre_lines=2,first_pre_us=51,first_early_lines=1,first_early_us=54,
                                 first_late_lines=3,first_late_us=81)
        result=host.request_id_bitbang(board,30,dict(requests=[]),self.root/'record.json')
        observed=result['interpretation']['first_address_bit']
        self.assertTrue(observed['early_conflict']);self.assertTrue(observed['late_high_after_conflict'])
        self.assertEqual(observed['pre']['elapsed_us'],51)
        self.assertEqual(result['bitbang']['result'],6)
        self.assertEqual(result['interpretation']['result'],'SDA_CONFLICT')
        self.assertFalse(result['interpretation']['successful'])
        # 잘못된 firmware가 result0을 주어도 early conflict 관측은 성공으로 승격하지 않는다.
        fabricated=dict(self.fake.bitbang,result=0,restore_result=0,ids_valid=1,manufacturer=0x5449,
                        device=0x3001,ack_count=6,ack_mask=0x3F)
        self.assertFalse(host.bitbang_interpretation(fabricated)['successful'])

    def test_v2_elf_selects208_and_executes_nopull_only(self):
        """과거 v2의208B read/arg0은 유지하고 모드가 없는 구 ABI에서 arg1은 차단한다."""
        contract=fixture_bitbang_elf(self.elf,version=2);fake=FakeCLI(contract);fake.bitbang_result=0
        folder=self.root/'v2-run'
        with patch.object(bringup,'run',side_effect=fake.run),contextlib.redirect_stdout(io.StringIO()):
            rc=host.main(['--elf',str(self.elf),'--output',str(folder),'--id-bitbang','--execute'])
        result=json.loads((folder/'manifest.json').read_text())
        self.assertEqual(rc,0);self.assertEqual(result['bitbang_bytes'],208)
        self.assertEqual(result['bitbang_version'],2)
        self.assertFalse(result['requests'][0]['interpretation']['temporary_sda_weak_pullup'])
        self.assertNotIn('pullup_mode',result['requests'][0]['bitbang'])

    def test_pullup_requires_id_bitbang_and_abi3_before_io(self):
        """단독/정상probe/구ABI의 약 bias 요청은 장치 I/O 및 output 생성 전에 실패한다."""
        for options in (['--sda-pullup'],['--rates','100000','--sda-pullup']):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                host.main(['--elf',str(self.elf),'--output',str(self.root/'unused'),*options])
        for version in (1,2):
            fixture_bitbang_elf(self.elf,version=version)
            with self.assertRaises(RuntimeError):
                host.main(['--elf',str(self.elf),'--output',str(self.root/'unused'),
                           '--id-bitbang','--sda-pullup','--execute'])
        self.assertFalse(self.fake.commands);self.assertFalse((self.root/'unused').exists())

    def test_pullup_plan_and_success_are_explicit_id_only(self):
        """명시 arg1은 한 번만 게시한다. 성공은 weak-bias ID이고 하드웨어/조도 복구가 아니다."""
        options=['--elf',str(self.elf),'--output',str(self.root/'pullup-run'),'--id-bitbang','--sda-pullup']
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(host.main(options),0)
        plan=json.loads(output.getvalue())
        self.assertEqual(plan['argument'],1);self.assertTrue(plan['temporary_sda_weak_pullup'])
        self.assertEqual(self.fake.commands,[])
        self.fake.bitbang_result=0
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(host.main(options+['--execute']),0)
        result=json.loads((self.root/'pullup-run/manifest.json').read_text())
        item=result['requests'][0];meaning=item['interpretation']
        self.assertEqual(item['argument'],1);self.assertEqual(item['bitbang']['pullup_mode'],1)
        self.assertEqual([v for a,v in self.fake.writes[:2]],[3,1]);self.assertEqual(len(self.fake.writes),3)
        self.assertTrue(meaning['successful']);self.assertTrue(meaning['temporary_sda_weak_pullup'])
        self.assertFalse(meaning['original_hardware_proven']);self.assertFalse(meaning['lux_measurement_verified'])
        self.assertEqual(meaning['measurement_scope'],'temporary-PC9-weak-pullup-ID-only')

    def test_pullup_mode_mismatch_preserves_raw_and_fails(self):
        """mailbox arg1인데 별도 완료가 mode0이면 stale/다른 시험으로 간주해 성공을 거부한다."""
        board=self.bitbang_board(sda_pullup=True);board.verify_identity(self.contract)
        self.fake.bitbang_result=0;self.fake.bitbang_mode_override=0
        record=dict(requests=[])
        with self.assertRaisesRegex(RuntimeError,'pull-up mode disagrees'):
            host.request_id_bitbang(board,30,record,self.root/'record.json')
        self.assertEqual(record['requests'][0]['bitbang']['pullup_mode'],0)
        self.assertEqual(record['requests'][0]['argument'],1);self.assertEqual(len(self.fake.writes),3)
        # arg0 요청에서 mode1이 나오는 반대 불일치 역시 request 검증으로 차단된다.
        other_folder=self.root/'nopull-board';other_folder.mkdir()
        other=host.AmbientBitbangBoard('STLINK_SERIAL_REQUIRED',other_folder,self.contract,950)
        other.verify_identity(self.contract);self.fake.bitbang_mode_override=1
        with self.assertRaisesRegex(RuntimeError,'pull-up mode disagrees'):
            host.request_id_bitbang(other,30,dict(requests=[]),self.root/'other.json')

    def test_pullup_unselected_argument_and_unknown_wire_mode_rejected(self):
        """기본 Board에 arg1을 몰래 넣거나 wire mode2를 제공할 수 없다."""
        board=self.bitbang_board();board.verify_identity(self.contract)
        with self.assertRaises(RuntimeError):
            board.command('bad','-w32',hex(board.mailbox+12),'3','-w32',hex(board.mailbox+16),'1',
                          '-w32',hex(board.mailbox+8),'1')
        self.assertEqual(self.fake.writes,[])
        item=dict(self.fake.bitbang,pullup_mode=2)
        with self.assertRaisesRegex(RuntimeError,'Unknown ID-bitbang pull-up mode'):
            host.decode_bitbang(struct.pack('<53I',*(item[k] for k in host.BITBANG_FIELDS)),expected_version=3)

    def test_pullup_precondition_rejection_is_requested_not_applied(self):
        """variant1은 인수 전 거절에도 기록된다. pin_changes가 없으면 적용했다고 부르지 않는다."""
        item=dict(self.fake.bitbang,pullup_mode=1,result=2,restore_result=2,pin_changes=0)
        meaning=host.bitbang_interpretation(item)
        self.assertTrue(meaning['sda_pullup_requested']);self.assertFalse(meaning['pin_takeover_observed'])
        self.assertFalse(meaning['temporary_sda_weak_pullup']);self.assertFalse(meaning['successful'])

    def test_v2_zero_timestamp_is_unobserved_even_if_lines_zero(self):
        report=host.bitbang_interpretation(self.fake.bitbang)['first_address_bit']
        self.assertTrue(report['available']);self.assertFalse(report['pre']['observed'])
        self.assertIsNone(report['pre']['raw_lines']);self.assertIsNone(report['pre']['sda_high'])
        item=dict(self.fake.bitbang,first_pre_us=55,first_pre_lines=0)
        report=host.bitbang_interpretation(item)['first_address_bit']
        self.assertTrue(report['pre']['observed']);self.assertFalse(report['pre']['sda_high'])

    def test_bitbang_wire_version_size_mismatch_and_unknown_size_rejected(self):
        raw=struct.pack('<53I',*(self.fake.bitbang[k] for k in host.BITBANG_FIELDS))
        with self.assertRaises(RuntimeError):host.decode_bitbang(raw,expected_version=1)
        with self.assertRaises(RuntimeError):host.decode_bitbang(raw[:184],expected_version=1)
        with self.assertRaises(RuntimeError):host.decode_bitbang(raw+bytes(4))
        changed=bytearray(self.elf.read_bytes());elf=host.Elf32(bytes(changed))
        for section in elf.sections:
            if section['type']!=2:continue
            strings=elf.sections[section['link']]
            for relative in range(0,section['size'],16):
                offset=section['offset']+relative;name=struct.unpack_from('<I',changed,offset)[0]
                start=strings['offset']+name;end=changed.index(0,start)
                if bytes(changed[start:end]).decode()==host.BITBANG_SYMBOL:
                    struct.pack_into('<I',changed,offset+8,188)
        unknown=self.root/'unknown-size.elf';unknown.write_bytes(changed)
        with self.assertRaises(RuntimeError):host.image_contract(unknown,host.MAILBOX_BYTES,id_bitbang=True)

    def test_failure400_then_success100_frozen_results(self):
        output = self.root/'run'
        with contextlib.redirect_stdout(io.StringIO()):
            rc = host.main(['--elf',str(self.elf),'--output',str(output),'--execute'])
        self.assertEqual(rc,2)
        manifest=json.loads((output/'manifest.json').read_text())
        self.assertEqual([r['completion']['result'] for r in manifest['requests']],[4,0])
        self.assertEqual([r['completion']['bus_hz'] for r in manifest['requests']],[400000,100000])
        self.assertEqual(manifest['requests'][0]['interpretation']['sr1_error_flags'],['ARLO'])
        self.assertTrue(manifest['cpu_running_after'])
        self.assertEqual([a-self.contract['mailbox'] for a,v in self.fake.writes],[12,16,8,12,16,8])
        self.assertTrue(list(output.glob('*-full.bin')))

    def test_explicit80k_is_transmitted_without_default_change(self):
        output=self.root/'80k-run'
        with contextlib.redirect_stdout(io.StringIO()):
            rc=host.main(['--elf',str(self.elf),'--output',str(output),'--rates','80000','--execute'])
        result=json.loads((output/'manifest.json').read_text())
        self.assertEqual(rc,0)
        self.assertEqual(result['rates_hz'],[80000])
        self.assertEqual(result['requests'][0]['completion']['bus_hz'],80000)
        self.assertEqual(len(self.fake.writes),3)

    def test_wrong_uid_or_app_blocks_request_writes(self):
        board=self.board()
        self.fake.uid=(0,0,0)
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.assertEqual(self.fake.writes,[])
        self.fake.uid=host.EXPECTED_UID
        self.fake.image=bytes([self.fake.image[0]^1])+self.fake.image[1:]
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.assertEqual(self.fake.writes,[])

    def test_unverified_or_out_of_range_writes_and_flash_rejected(self):
        board=self.board()
        with self.assertRaises(RuntimeError):board.command('x','-w32',hex(board.mailbox+8),'1')
        board.verify_identity(self.contract)
        for address in (0x08010000,0x40005C00,board.mailbox,board.mailbox+20):
            with self.assertRaises(RuntimeError):board.command('x','-w32',hex(address),'1')
        for operation in (('-halt',),('-run',),('-rst',),('-ob','RDP=0xAA')):
            with self.assertRaises(RuntimeError):board.command('x',*operation)
        with self.assertRaises(RuntimeError):board.program(self.elf)
        self.assertEqual(self.fake.writes,[])

    def test_halted_target_not_resumed(self):
        board=self.board();self.fake.halted=True
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.assertEqual(self.fake.writes,[])
        self.assertFalse(any('-run' in command for command in self.fake.commands))

    def test_accepted_rate_mismatch_rejected(self):
        board=self.board();board.verify_identity(self.contract);self.fake.bad_rate=True
        with self.assertRaises(RuntimeError):
            host.request_probe(board,100000,1,dict(requests=[]),self.root/'record.json')

    def test_early_init_failure_retains_observed_old_rate(self):
        board=self.board();board.verify_identity(self.contract)
        normal_complete=self.fake.complete
        def early_failure():
            """새 속도 적용 전에 HAL_Init이 실패한 합법적인 응답을 만든다."""
            normal_complete()
            self.fake.box.update(result=2,error=2,ready=0,phase=2,bus_hz=400000)
        with patch.object(self.fake,'complete',side_effect=early_failure):
            result=host.request_probe(board,100000,1,dict(requests=[]),self.root/'record.json')
        self.assertEqual(result['state'],'completed')
        self.assertEqual(result['interpretation']['requested_hz'],100000)
        self.assertEqual(result['interpretation']['observed_bus_hz'],400000)
        self.assertFalse(result['interpretation']['requested_rate_applied'])

    def test_header_abi_constants_offsets_and_fields_match(self):
        project=TOOLS.parent
        service=(project/'Middlewares/Noodoe/Ambient/inc/AmbientService.h').read_text()
        driver=(project/'Drivers/BSP/inc/BSP_Ambient.h').read_text()
        magic=re.search(r'#define\s+AMBIENT_MAILBOX_MAGIC\s+(0x[0-9A-Fa-f]+)U',service)
        size=re.search(r'#define\s+AMBIENT_MAILBOX_BYTES\s+(\d+)U',service)
        version=re.search(r'#define\s+AMBIENT_SERVICE_VERSION\s+(\d+)U',service)
        self.assertEqual(int(magic[1],16),host.MAILBOX_MAGIC)
        self.assertEqual(int(size[1]),host.MAILBOX_BYTES)
        self.assertEqual(int(version[1]),1)
        def fields(text,name):
            """C 헤더의 실제 u32 선언 순서를 가져와 wire ABI 필드와 비교한다."""
            clean=re.sub(r'/\*.*?\*/','',text,flags=re.S)
            body=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*'+name+r'\s*;',clean,re.S)[1]
            result=[]
            for declaration in re.findall(r'uint32_t\s+([^;]+);',body):
                result += [part.strip() for part in declaration.split(',')]
            return result
        mailbox=fields(service,'Ambient_Mailbox')
        bsp=fields(driver,'BSP_Ambient_Diagnostics');bsp[0]='driver_magic'
        self.assertEqual(mailbox+bsp,host.FIELDS)
        self.assertEqual([mailbox.index(key)*4 for key in ('request_seq','command','argument','response_seq')],[8,12,16,20])

    def test_rejected_request_does_not_claim_operation_result(self):
        board=self.board();board.verify_identity(self.contract);self.fake.rejected=True
        result=host.request_probe(board,100000,1,dict(requests=[]),self.root/'record.json')
        self.assertEqual(result['state'],'rejected')
        self.assertEqual(result['completion']['operation_id'],0)
        self.assertEqual(result['interpretation']['result'],'NOT_APPLICABLE_REJECTED')

    def test_host_timeout_has_no_retry_or_reset(self):
        board=self.board();board.verify_identity(self.contract)
        with patch.object(host.time,'monotonic',side_effect=[0,2]):
            with self.assertRaises(RuntimeError):
                host.request_probe(board,100000,1,dict(requests=[]),self.root/'record.json')
        self.assertEqual(len(self.fake.writes),3)
        self.assertFalse(any('-run' in command or '-rst' in command for command in self.fake.commands))

    def test_invalid_magic_and_length_are_rejected(self):
        with self.assertRaises(RuntimeError):host.decode(bytes(host.MAILBOX_BYTES))
        with self.assertRaises(RuntimeError):host.decode(bytes(host.MAILBOX_BYTES-4))

    def test_torn_sequence_is_retried_before_use(self):
        one=empty_box();one['response_seq']=1
        two=dict(one,response_seq=2)
        class Snapshot:
            mailbox=0x20001000
            def __init__(self):
                self.data=iter([struct.pack('<I',1),struct.pack('<31I',*(one[k] for k in host.FIELDS)),
                               struct.pack('<I',2),struct.pack('<I',2),
                               struct.pack('<31I',*(two[k] for k in host.FIELDS)),struct.pack('<I',2)])
            def dump(self,*args):return next(self.data)
        self.assertEqual(host.stable_snapshot(Snapshot(),'test')['response_seq'],2)

    def use_address(self):
        """한 시험에서만288B ELF/FakeCLI로 바꾸며 real subprocess 금지는 그대로 둔다."""
        self.contract=fixture_bitbang_elf(self.elf,address=True);self.fake=FakeCLI(self.contract)
        patch.object(bringup,'run',side_effect=self.fake.run).start()
        folder=self.root/'address-board';folder.mkdir()
        return host.AmbientAddressBoard('STLINK_SERIAL_REQUIRED',folder,self.contract,950)

    def match_address(self,index=2):
        """고정 후보 한 곳만 OPT3001 ID를 반환하도록 만든다. ACK 자체를 ID로 대체하지 않는다."""
        self.fake.address.update(result=0,address_ack_mask=1<<index,id_match_mask=1<<index)
        self.fake.address['entry'][index].update(address_ack=1,result=0,phase=12,manufacturer=0x5449,
                                               device=0x3001,ids_valid=1)

    def test_address_plan_and_mode_exclusion(self):
        self.use_address()
        options=['--elf',str(self.elf),'--output',str(self.root/'unused'),'--address-diagnostic']
        with contextlib.redirect_stdout(io.StringIO()) as output:self.assertEqual(host.main(options),0)
        plan=json.loads(output.getvalue())
        self.assertEqual(plan['command'],4);self.assertEqual(plan['argument'],0)
        self.assertEqual(plan['fixed_addresses'],['0x44','0x45','0x46','0x47'])
        self.assertEqual(plan['diagnostic_bytes'],288);self.assertFalse(self.fake.commands)
        for invalid in (['--rates','100000'],['--id-bitbang'],['--sda-pullup']):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):host.main(options+invalid)
        self.assertFalse((self.root/'unused').exists())

    def test_address_actual_decoder_all_nack_and_once(self):
        board=self.use_address();board.verify_identity(self.contract)
        result=host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')
        self.assertEqual(result['state'],'completed')
        self.assertEqual(result['interpretation']['result'],'NACK')
        self.assertEqual(result['interpretation']['matched_addresses'],[])
        self.assertEqual([row['result'] for row in result['interpretation']['entry']],['NACK']*4)
        self.assertFalse(result['interpretation']['successful'])
        self.assertEqual([v for a,v in self.fake.writes[:2]],[4,0]);self.assertEqual(len(self.fake.writes),3)
        with self.assertRaises(RuntimeError):
            board.command('again','-w32',hex(board.mailbox+12),'4','-w32',hex(board.mailbox+16),'0',
                          '-w32',hex(board.mailbox+8),'123')

    def test_address_success_only_proves_fixed_id_under_bias(self):
        self.use_address();self.match_address();output=self.root/'address-success'
        with contextlib.redirect_stdout(io.StringIO()):
            rc=host.main(['--elf',str(self.elf),'--output',str(output),'--address-diagnostic','--execute'])
        result=json.loads((output/'manifest.json').read_text());meaning=result['requests'][0]['interpretation']
        self.assertEqual(rc,0);self.assertTrue(meaning['successful']);self.assertEqual(meaning['matched_addresses'],['0x46'])
        self.assertFalse(meaning['original_hardware_proven']);self.assertFalse(meaning['lux_measurement_verified'])
        self.assertTrue(meaning['temporary_sda_weak_pullup'])
        self.assertNotIn('bitbang',result['requests'][0]);self.assertEqual(len(self.fake.writes),3)

    def test_address_fault_after_match_keeps_failure_and_unattempted(self):
        board=self.use_address();board.verify_identity(self.contract);self.match_address(0)
        self.fake.address.update(result=6,attempted_mask=3)
        self.fake.address['entry'][1].update(result=6,phase=4)
        for entry in self.fake.address['entry'][2:]:entry.update(attempted=0,result=2,phase=0,stop_result=2)
        result=host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')
        meaning=result['interpretation'];self.assertFalse(meaning['successful'])
        self.assertEqual(meaning['matched_addresses'],['0x44']);self.assertEqual(meaning['result'],'SDA_CONFLICT')
        self.assertEqual(meaning['entry'][2]['result'],'NOT_ATTEMPTED')

    def test_address_restore_mismatch_and_false_success_rejected(self):
        board=self.use_address();board.verify_identity(self.contract);self.match_address()
        self.fake.address.update(restore_result=1,final_pc9=32)
        with self.assertRaisesRegex(RuntimeError,'restoration'):
            host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')
        # 일치 ID가 하나 있어도 STOP/timing 실패가 숨겨진 success0은 합격시키지 않는다.
        self.fake.address.update(restore_result=0,final_pc9=0)
        self.fake.address['entry'][3]['stop_result']=5
        self.assertFalse(host.address_interpretation(self.fake.address)['successful'])
        self.fake.address['entry'][3]['stop_result']=0;self.fake.address['timing_uncertain']=1
        self.assertFalse(host.address_interpretation(self.fake.address)['successful'])

    def test_address_stale_request_or_operation_rejected(self):
        board=self.use_address();board.verify_identity(self.contract);self.fake.address_bad_request=True
        with self.assertRaisesRegex(RuntimeError,'another request/operation'):
            host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')
        folder=self.root/'other-address';folder.mkdir()
        other=host.AmbientAddressBoard('STLINK_SERIAL_REQUIRED',folder,self.contract,950)
        other.verify_identity(self.contract);self.fake.address_bad_request=False;self.fake.address_bad_operation=True
        with self.assertRaisesRegex(RuntimeError,'another request/operation'):
            host.request_address_diagnostic(other,30,dict(requests=[]),self.root/'other.json')

    def test_address_rejection_and_precheck_do_not_claim_measurements(self):
        board=self.use_address();board.verify_identity(self.contract);self.fake.rejected=True
        result=host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')
        self.assertEqual(result['state'],'rejected');self.assertNotIn('address_diagnostic',result)
        item=address_record();item.update(result=2,attempted_mask=0,restore_result=2,pin_changes=0)
        for entry in item['entry']:entry.update(attempted=0,result=2,phase=0,elapsed_us=0,stop_result=2)
        decoded=host.decode_address(address_bytes(item));meaning=host.address_interpretation(decoded)
        self.assertFalse(meaning['pin_takeover_observed']);self.assertFalse(meaning['temporary_sda_weak_pullup'])
        self.assertEqual([row['result'] for row in meaning['entry']],['NOT_ATTEMPTED']*4)

    def test_address_wire_mask_address_abi_and_length_rejected(self):
        item=address_record()
        for key,value in [('magic',0),('version',2),('bytes',284),('pullup_mode',0),('attempted_mask',16),('address_ack_mask',1)]:
            with self.assertRaises(RuntimeError):host.decode_address(address_bytes(dict(item,**{key:value})))
        item['entry'][0]['address']=0x48
        with self.assertRaises(RuntimeError):host.decode_address(address_bytes(item))
        for raw in (bytes(284),bytes(292)):
            with self.assertRaises(RuntimeError):host.decode_address(raw)

    def test_address_torn_sequence_and_frozen_change_rejected(self):
        item=address_record()
        class Snapshot:
            diagnostic=0x20001000
            def __init__(self):
                self.raw=iter([struct.pack('<I',3),address_bytes(dict(item,sequence=3)),struct.pack('<I',3),
                               struct.pack('<I',2),address_bytes(item),struct.pack('<I',2)])
            def dump(self,*args):return next(self.raw)
        self.assertEqual(host.stable_address(Snapshot(),'test')['sequence'],2)
        board=self.use_address();board.verify_identity(self.contract)
        original=host.stable_address
        def replaced(target,name):
            """두 번째 고정 확인 사이에 새 완료가 덮인 것을 모델링한다."""
            value=original(target,name)
            if 'frozen' in name:value['elapsed_us']+=1
            return value
        with patch.object(host,'stable_address',side_effect=replaced),self.assertRaisesRegex(RuntimeError,'not frozen'):
            host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')

    def test_address_identity_and_write_whitelist_timeout(self):
        board=self.use_address()
        with self.assertRaises(RuntimeError):
            board.command('early','-w32',hex(board.mailbox+12),'4','-w32',hex(board.mailbox+16),'0','-w32',hex(board.mailbox+8),'1')
        self.fake.uid=(0,0,0)
        with self.assertRaises(RuntimeError):board.verify_identity(self.contract)
        self.assertEqual(self.fake.writes,[]);self.fake.uid=host.EXPECTED_UID;board.verify_identity(self.contract)
        for command,arg in [(3,0),(4,1),(4,0x44)]:
            with self.assertRaises(RuntimeError):
                board.command('invalid','-w32',hex(board.mailbox+12),str(command),'-w32',hex(board.mailbox+16),str(arg),
                              '-w32',hex(board.mailbox+8),'1')
        with patch.object(host.time,'monotonic',side_effect=iter(range(10000))),patch.object(host.time,'sleep'),\
             patch.object(self.fake,'complete',return_value=None),self.assertRaises(RuntimeError):
            host.request_address_diagnostic(board,30,dict(requests=[]),self.root/'record.json')
        self.assertEqual(len(self.fake.writes),3);self.assertIsNone(board.deadline)

    def test_address_header_exact_layout(self):
        source=(TOOLS.parent/'Drivers/BSP/inc/BSP_AmbientAddress.h').read_text()
        for name,value in [('MAGIC',host.ADDRESS_MAGIC),('VERSION',1),('BYTES',288),('COUNT',4)]:
            match=re.search(r'#define\s+BSP_AMBIENT_ADDRESS_'+name+r'\s+(0x[0-9A-Fa-f]+|\d+)U',source)
            self.assertEqual(int(match[1],0),value)
        clean=re.sub(r'/\*.*?\*/','',source,flags=re.S)
        def fields(name):
            body=re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*'+name+r'\s*;',clean,re.S)[1]
            return [part.strip() for decl in re.findall(r'uint32_t\s+([^;]+);',body) for part in decl.split(',')]
        self.assertEqual(fields('BSP_AmbientAddress_Entry'),host.ADDRESS_ENTRY_FIELDS)
        self.assertEqual(fields('BSP_AmbientAddress_Diagnostics'),host.ADDRESS_HEADER_FIELDS+host.ADDRESS_TAIL_FIELDS)
        self.assertEqual(len(host.ADDRESS_HEADER_FIELDS)*4,56)
        self.assertEqual((len(host.ADDRESS_HEADER_FIELDS)+4*len(host.ADDRESS_ENTRY_FIELDS))*4,216)
        self.assertEqual((14+4*10+18)*4,host.ADDRESS_BYTES)


if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(HostTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    destination=Path(__file__).with_name('ambient_swd_output');destination.mkdir(exist_ok=True)
    host.save(destination/'results.json',dict(hardware_access=False,subprocess_forbidden=True,
        tests_run=result.testsRun,failures=len(result.failures),errors=len(result.errors),successful=result.wasSuccessful(),
        source_sha256=host.sha(Path(host.__file__).read_bytes()),test_sha256=host.sha(Path(__file__).read_bytes()),
        elf_fixture='Preserved731f ELF; temporary non-ALLOC symbols/strings only changed for mailbox and bitbang ABI; code unchanged'))
    raise SystemExit(0 if result.wasSuccessful() else 1)
