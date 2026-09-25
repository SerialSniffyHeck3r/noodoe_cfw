"""Actual Product updater + BootStore + NDCP/SHA ARM regression, no hardware.

Only CfwFiles NOR I/O, SDRAM allocator, resource availability and MCU reset are
substituted. Each scenario starts with fresh C state; canonical file bytes are
also retained as independent evidence for Python inspection.
"""
import hashlib,json,struct,subprocess,sys,zlib,os
from pathlib import Path
HERE=Path(__file__).resolve().parent;P=HERE.parents[2]
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC,UC_ARM_REG_XPSR,UC_ARM_REG_CONTROL,UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK
out=HERE/'output';out.mkdir(exist_ok=True)
fixture=bytearray((i*13+7)&255 for i in range(0x60000))
fixture[:8]=struct.pack('<II',0x2002ff00,0x08020101)
fixture[0x200:0x22c]=struct.pack('<III',0x51534352,1,1)+bytes([0x77])*32
struct.pack_into('<4I',fixture,0x230,0x3250554e,2,2,0x60000)
sha=hashlib.sha256(fixture).digest()
no_resources=bytearray(fixture);no_resources[0x208:0x20c]=bytes(4)
(out/'fixture.h').write_text('#define FIXTURE_CRC 0x%08Xu\nstatic const uint8_t fixture_sha[32]={%s};\n#define NO_RESOURCES_CRC 0x%08Xu\nstatic const uint8_t no_resources_sha[32]={%s};\n'%(zlib.crc32(fixture),','.join(str(b) for b in sha),zlib.crc32(no_resources),','.join(str(b) for b in hashlib.sha256(no_resources).digest())))
ld=out/'test.ld';ld.write_text('''MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }
SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }''')
incs=['App_Logic/Runtime/inc','Middlewares/Noodoe/Storage/inc','Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Resources/inc','Drivers/BSP/inc','Drivers/CMSIS/Include','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Drivers/STM32F4xx_HAL_Driver/Inc','Core/Inc','RecoveryGate/include']
sources=[P/x for x in ['App_Logic/Runtime/src/RuntimeUpdate_Product.c','Middlewares/Noodoe/Storage/src/boot_store.c','Middlewares/Noodoe/Update/src/Update_Service.c','Middlewares/Noodoe/Update/src/Update_SHA256.c','Middlewares/Noodoe/Update/src/NDCP.c','Middlewares/Noodoe/Update/src/Recovery_Core.c','RecoveryGate/src/gate_policy.c','RecoveryGate/src/gate_format.c']]+[HERE/'test.c']
scenarios=[(name,0) for name in ['Test_Transfer','Test_AbortVerified','Test_BlankInactive','Test_PhysicalCorruption','Test_DisconnectDuringPage','Test_EarlyConfirmation','Test_WrongBootSequence']]
scenarios += [('Test_CommitFault',cut) for cut in (0,1,16,17,18,33,34)]
scenarios += [(name,0) for name in ('Test_InitBounds','Test_HardwareRecheck','Test_InvalidVersion')]
scenarios += [(name,mode) for name in ('Test_InitContext','Test_CallbackContext') for mode in range(5)]
scenarios += [('Test_VectorBoundary',mode) for mode in range(4)]
scenarios += [('Test_JournalGuard',mode) for mode in range(1,5)]
scenarios += [('Test_ResourcesRequired',0)]
scenarios += [('Test_Resume',mode) for mode in range(3)]
scenarios += [('Test_Uninstall',mode) for mode in range(2)]+[('Test_RetainedRestore',mode) for mode in range(2)]
scenarios += [('Test_Diagnostic',mode) for mode in range(4)]
scenarios += [('Test_RollbackEarlyAck',mode) for mode in range(2)]
if len(sys.argv)>1:scenarios=[x for x in scenarios if x[0] in sys.argv[1:]]
source_hashes={str(x.relative_to(P)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sources}
reports=[];errors=[]
for opt in ('O0','Os'):
    elf=out/(opt+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-DSTM32F429xx','-DNOODOE_PRODUCT=1','-nostdlib','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-include',str(HERE/'port.h'),*['-I'+str(P/x) for x in incs],'-I'+str(out),*map(str,sources),'-Wl,-T,'+str(ld),'-Wl,-e,Test_Transfer','-lgcc','-o',str(elf)]
    r=subprocess.run(cmd,capture_output=True,text=True);(out/('compile-'+opt+'.log')).write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3}
    data=elf.read_bytes();off=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
    for scenario,argument in scenarios:
        uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        for a,s in [(0x10000000,0x20000),(0x11000000,0x110000),(0x12000000,0x60000),(0x13000000,0x70000),(0x14000000,0x70000),(0x08000000,0x80000),(0x20000000,0x20000),(0xC0000000,0x40000),(0xE0000000,0x100000)]:uc.mem_map(a,s)
        for n in range(count):
            kind,pos,va,pa,fs,ms,flags,align=struct.unpack_from('<8I',data,off+n*size)
            if kind==1 and fs:uc.mem_write(va,data[pos:pos+fs])
        uc.mem_write(0x14000000,Path(os.environ.get('NOODOE_UNINSTALL_IMAGE',str(P.parents[1]/'analysis/uninstall-build/Release/uninstall.bin'))).read_bytes())
        uc.mem_write(0x08000000,(P.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin').read_bytes())
        uc.mem_write(0x12000000,bytes(fixture));stop=0x1001fff0
        uc.reg_write(UC_ARM_REG_SP,0x2001fff0);uc.reg_write(UC_ARM_REG_LR,stop|1);uc.reg_write(UC_ARM_REG_XPSR,0x1000000)
        if scenario=='Test_CallbackContext':
            uc.emu_start(syms['Test_PrepareContext']|1,stop,timeout=60_000_000,count=1_000_000_000)
            assert uc.reg_read(UC_ARM_REG_PC)==stop and uc.reg_read(UC_ARM_REG_R0)==0
            uc.reg_write(UC_ARM_REG_SP,0x2001fff0);uc.reg_write(UC_ARM_REG_LR,stop|1)
        if scenario in ('Test_InitContext','Test_CallbackContext'):
            register,value=[(UC_ARM_REG_XPSR,0x1000010),(UC_ARM_REG_CONTROL,1),(UC_ARM_REG_PRIMASK,1),(UC_ARM_REG_BASEPRI,0x20),(UC_ARM_REG_FAULTMASK,1)][argument]
            uc.reg_write(register,value)
        uc.reg_write(UC_ARM_REG_R0,argument)
        uc.emu_start(syms[scenario]|1,stop,timeout=60_000_000,count=1_000_000_000)
        def word(name):return struct.unpack('<I',uc.mem_read(syms[name],4))[0]
        row={'optimization':opt,'scenario':scenario,'argument':argument,'failure_line':word('failure'),'assertions':word('assertions'),'returned':uc.reg_read(UC_ARM_REG_PC)==stop,'result':uc.reg_read(UC_ARM_REG_R0),'physical_device':False,'last_result':word('last_result'),'last_offset':word('last_offset')}
        if scenario=='Test_Transfer' and not row['result']:
            raw=bytes(uc.mem_read(0x11081000,0x60000));row['canonical_payload_sha256']=hashlib.sha256(raw).hexdigest()
            assert raw==fixture
            row['resident_metadata_commits']=word('metadata_commits');assert row['resident_metadata_commits']==0
            (out/(opt+'-completed-files.bin')).write_bytes(bytes(uc.mem_read(0x11000000,0x110000)))
        if row['result'] or not row['returned']:errors.append(row)
        reports.append(row);print(json.dumps(row),flush=True)
(out/'results.json').write_text(json.dumps({'runs':reports,'sources':source_hashes,'fixture_sha256':sha.hex(),'limits':'Actual ARM policy and adapter with fake file I/O. No FAT, real NOR, BT, IRQ races or physical reset exercised.'},indent=2))
if errors:raise SystemExit(1)
