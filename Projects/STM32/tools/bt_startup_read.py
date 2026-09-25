"""Read the bounded BT startup trace without requesting a new start or halting.

An exact ELF/APP and fixed MCU UID are required before interpreting SRAM.
GPIO IDR samples are digital observations, not measured voltages or edge times.
"""
import argparse
import json
import struct
from pathlib import Path
from ambient_swd import AmbientBoard, save
from bluetooth_swd import object_address
from validate_image import Elf32, validate
from bringup import require, sha

SYMBOL = 'g_bsp_bt_hci_startup'
MAGIC = 0x42545431
BYTES = 472
HEADER = 'magic version sequence attempt sample_count flags'.split()
SAMPLE = 'tick_ms stage gpioa_idr gpioi_idr uart_sr rx_remaining tx_remaining'.split()
STAGES = ('NONE','ENTER','RESET_ASSERTED','ENABLE_HIGH','UART_READY','RX_ARMED',
          'RESET_RELEASED','SETTLE_SAMPLE','FAULT','CLOSED')


def contract_from_elf(path):
    """Resolve one exact 472-byte writable SRAM object from the validated ELF."""
    raw = path.read_bytes()
    elf = Elf32(raw)
    image, manifest = validate(elf)
    return dict(elf=str(path.resolve()), elf_sha256=sha(raw), app_sha256=sha(image),
                image=image, manifest=manifest, address=object_address(elf,SYMBOL,BYTES))


def decode(raw):
    """Validate the fixed ABI and derive only observed GPIO logic levels."""
    require(len(raw)==BYTES,'Startup trace length mismatch')
    words=struct.unpack('<118I',raw)
    result=dict(zip(HEADER,words[:6]))
    require(result['magic']==MAGIC and result['version']==1,'Startup trace ABI mismatch')
    require(result['sequence'] and not result['sequence']&1,'Startup trace is not committed')
    require(result['attempt'] and 0<result['sample_count']<=16,'Invalid startup trace count/attempt')
    require(result['flags']&~7==0,'Unknown startup flags')
    samples=[]
    release_tick=None
    for i in range(result['sample_count']):
        row=dict(zip(SAMPLE,words[6+i*7:13+i*7]))
        require(0<row['stage']<len(STAGES),'Unknown startup sample stage')
        row['stage_name']=STAGES[row['stage']]
        row['elapsed_ms']=(row['tick_ms']-words[6])&0xFFFFFFFF
        if row['stage']==6: release_tick=row['tick_ms']
        row['since_reset_release_ms']=None if release_tick is None else (row['tick_ms']-release_tick)&0xFFFFFFFF
        require(row['elapsed_ms']<0x80000000,'Non-forward startup sample time')
        if samples: require(row['elapsed_ms']>=samples[-1]['elapsed_ms'],'Out-of-order startup samples')
        for key,bit in (('reset_pa8_high',8),('rx_pa10_high',10),('cts_pa11_high',11),('rts_pa12_high',12)):
            row[key]=bool(row['gpioa_idr']&(1<<bit))
        row['enable_pi1_high']=bool(row['gpioi_idr']&2)
        samples.append(row)
    result['samples']=samples
    result['interpretation']='Digital samples only; requested delays do not establish exact signal edge timing.'
    return result


def stable_read(board):
    """Accept equal-even sequence/full/sequence only, with four bounded tries."""
    for attempt in range(4):
        before=struct.unpack('<I',board.dump(f'trace-{attempt}-before',board.mailbox+8,4))[0]
        raw=board.dump(f'trace-{attempt}-full',board.mailbox,BYTES)
        after=struct.unpack('<I',board.dump(f'trace-{attempt}-after',board.mailbox+8,4))[0]
        if before==after==struct.unpack_from('<I',raw,8)[0] and before and not before&1:
            return decode(raw)
    raise RuntimeError('Startup trace did not stabilize; no retry/start/reset was requested')


def main(argv=None):
    """Plan by default; execute only read commands through an empty write allowlist."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--serial',default='STLINK_SERIAL_REQUIRED')
    parser.add_argument('--read-khz',type=int,choices=(100,950,4000),default=950)
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args(argv)
    contract=contract_from_elf(args.elf)
    result={key:contract[key] for key in ('elf','elf_sha256','app_sha256','address')}
    result.update(schema=1,hardware_access=args.execute,request_writes=False,state='plan-only')
    if not args.execute:
        print(json.dumps(result,indent=2));return 0
    folder=args.output.resolve()
    require(not folder.exists(),'Use a fresh startup trace output folder')
    folder.mkdir(parents=True)
    manifest=folder/'manifest.json'
    result['state']='before-identity';save(manifest,result)
    # Treat the trace as the sole permitted SRAM read interval. No SRAM word
    # is authorized for writing, even though the shared transport supports it.
    board=AmbientBoard(args.serial,folder,contract['address'],BYTES,(),args.read_khz)
    try:
        result['identity']=board.verify_identity(contract)
        result['trace']=stable_read(board)
        board.assert_running('dhcsr-after-trace')
        result.update(state='read-complete',cpu_running_after=True)
        save(manifest,result);print(json.dumps(result,indent=2));return 0
    except Exception as error:
        result.update(state='read-failed',error=str(error))
        save(manifest,result);print(json.dumps(result,indent=2));return 2


if __name__=='__main__':
    raise SystemExit(main())
