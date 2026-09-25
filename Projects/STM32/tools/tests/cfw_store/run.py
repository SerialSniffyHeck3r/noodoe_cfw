"""Execute production ARM persistence against fault-injected NOR, no hardware."""
from pathlib import Path
import json, struct, subprocess, sys, io, argparse
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_XPSR,UC_ARM_REG_PC,UC_ARM_REG_R0
from PIL import Image
out=HERE/'output';out.mkdir(exist_ok=True)
ld=out/'test.ld';ld.write_text('MEMORY { FLASH(rx): ORIGIN=0x10000000,LENGTH=2M\n RAM(rwx): ORIGIN=0x20000000,LENGTH=1M }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n .data : { *(.data*) } > RAM AT>FLASH\n .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }'.replace('ORIGIN=','ORIGIN = ').replace(',LENGTH=',', LENGTH = '))
image=Image.new('RGB',(32,32),(20,60,120));bio=io.BytesIO();image.save(bio,format='JPEG',quality=85);fixture=bio.getvalue()
results=[]
parser=argparse.ArgumentParser();parser.add_argument('--test',action='append');parser.add_argument('--opt',action='append',choices=('O0','Os'));parser.add_argument('--image',type=Path);parser.add_argument('--jpeg',type=Path)
parser.add_argument('--photo-reset-output',type=Path,help='Export actual Product reset result for the Bootstrap reader regression')
args=parser.parse_args()
if args.jpeg:
    fixture=args.jpeg.read_bytes()
    if not 0<len(fixture)<=131072:raise ValueError('Fixture exceeds production JPEG limit')
for opt in (args.opt or ('O0','Os')):
    elf=out/f'test-{opt}.elf'
    includes=[P/'App_Logic/Settings/inc',P/'App_Logic/Settings/src',P/'App_Logic/UI/inc',P/'App_Logic/UI/src',P/'App_Logic/Vehicle/inc',P/'App_Logic/Vehicle/src',HERE/'stubs',P/'Drivers/BSP/inc',P/'Middlewares/Noodoe/Storage/inc',P/'Middlewares/Noodoe/Storage/src',P/'Middlewares/Noodoe/Photos/inc',P/'Middlewares/Noodoe/Photos/src',P/'Middlewares/Third_Party/FatFs/src',P/'Middlewares/Third_Party/LVGL/src/libs/tjpgd']
    includes += [P/'Middlewares/Noodoe/Bluetooth/inc']
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(P/'Drivers/BSP/src/noodoe_crc32.c'),'-std=c11','-DNOODOE_PRODUCT=1','-mcpu=cortex-m4','-mthumb','-'+opt,'-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib']
    cmd+=['-I'+str(i) for i in includes]+[str(HERE/'test.c'),str(P/'Middlewares/Noodoe/Photos/src/photo_jpeg.c'),str(P/'Middlewares/Noodoe/Bluetooth/src/bluetooth_key_codec.c'),'-Wl,-T,'+str(ld),'-Wl,-e,TestJournal','-o',str(elf),'-lgcc']
    compiled=subprocess.run(cmd,capture_output=True,text=True);(out/f'compile-{opt}.log').write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:raise RuntimeError(compiled.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={w[2]:int(w[0],16) for line in nm.splitlines() if len(w:=line.split())==3}
    data=elf.read_bytes();h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    for name in (args.test or ('TestJournal','TestRideJournal','TestAudit','TestConfig','TestPhoto','TestPhotoCuts','TestRide','TestInstall','TestSettings','TestDeadline','TestPhotoMailbox','TestAppSave')):
        emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        for address,size in ((0x10000000,0x200000),(0x20000000,0x100000),(0xc0000000,0x4000000),(0x60000000,0x8000000)):emu.mem_map(address,size)
        for i in range(h[10]):
            kind,off,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,h[5]+i*h[9])
            if kind==1 and size:emu.mem_write(address,data[off:off+size])
        stop=0x101ffff0;emu.mem_write(stop,b'\x00\xbe')
        if name=='TestHistorical':
            raw=args.image.read_bytes();assert len(raw)==0x8000000
            logical=bytearray(len(raw));logical[::2]=raw[1::2];logical[1::2]=raw[::2];emu.mem_write(0x60000000,bytes(logical))
        else:emu.mem_write(0x61000000,fixture)
        emu.reg_write(UC_ARM_REG_SP,0x200ffff0);emu.reg_write(UC_ARM_REG_LR,stop|1);emu.reg_write(UC_ARM_REG_XPSR,0x01000000);emu.reg_write(UC_ARM_REG_R0,len(fixture))
        # Yield in bounded slices so the caller can report progress during a
        # full erase/page/commit power-loss matrix at unoptimized -O0.
        pc=symbols[name]|1
        for slice_number in range(300):
            emu.emu_start(pc,stop,timeout=2_000_000,count=100_000_000)
            if emu.reg_read(UC_ARM_REG_PC)==stop:break
            pc=emu.reg_read(UC_ARM_REG_PC)|1
            if slice_number % 15 == 14:
                print(json.dumps(dict(optimization=opt,test=name,progress_assertions=struct.unpack('<I',emu.mem_read(symbols['g_assertions'],4))[0])),flush=True)
        word=lambda n:struct.unpack('<I',emu.mem_read(symbols[n],4))[0]
        r=dict(optimization=opt,test=name,returned=emu.reg_read(UC_ARM_REG_PC)==stop,result=emu.reg_read(UC_ARM_REG_R0),assertions=word('g_assertions'),failure_line=word('g_failure_line'),mock_error=word('g_mock_error'),hardware_access=False)
        results.append(r);print(json.dumps(r),flush=True);(out/'results.json').write_text(json.dumps(results,indent=2))
        if not r['returned'] or r['result'] or r['failure_line'] or r['mock_error']:raise SystemExit(1)
        if name=='TestPhotoReset' and args.photo_reset_output:
            args.photo_reset_output.parent.mkdir(parents=True,exist_ok=True)
            args.photo_reset_output.write_bytes(emu.mem_read(0x60000000+0x9000+393216,1048576))
