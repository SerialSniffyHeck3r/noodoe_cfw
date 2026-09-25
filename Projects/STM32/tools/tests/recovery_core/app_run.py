"""Actual AppRecovery control/fault/identity ARM code; no target access."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
PROJECT=HERE.parents[2]
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_XPSR,UC_ARM_REG_PC
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'app_output';OUT.mkdir(exist_ok=True)
(OUT/'test.ld').write_text("""MEMORY {
 FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 192K
}
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 .noinit (NOLOAD) : { *(.noinit*) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
""")
inc=['App_Logic/Recovery/inc','Middlewares/Noodoe/Update/inc','Middlewares/Noodoe/Storage/inc','Drivers/BSP/inc','Core/Inc','Drivers/CMSIS/Include','Drivers/CMSIS/Device/ST/STM32F4xx/Include','Drivers/STM32F4xx_HAL_Driver/Inc','Middlewares/Third_Party/FreeRTOS/Source/include','Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2','Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F']
sources=[PROJECT/'App_Logic/Recovery/src/App_Recovery.c',PROJECT/'App_Logic/Recovery/src/Recovery_Buttons.c',PROJECT/'Middlewares/Noodoe/Update/src/Update_SHA256.c',HERE/'test_app_recovery.c',PROJECT/'Middlewares/Noodoe/Update/src/Recovery_Core.c']
functions=['app_recovery_reset_test','AppRecovery_FaultReset','AppRecovery_IsFaultBoot','BSP_FaultRecoveryRequested','app_recovery_identity_test','AppRecovery_Identity','AppRecovery_IdentityProcess','RecoveryResident_Verify','AppRecovery_RuntimeButtons']
results=[]
for opt in ('-O0','-Os'):
 elf=OUT/f'{opt[1:]}.elf'
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-std=c11','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft',opt,'-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-fno-unwind-tables','-DNOODOE_PRODUCT=1','-DconfigAPPLICATION_ALLOCATED_HEAP=1','-DSTM32F429xx','-DUSE_HAL_DRIVER']
 cmd += [v for p in inc for v in ('-I',str(PROJECT/p))]
 cmd += ['-nostdlib',*map(str,sources),'-T',str(OUT/'test.ld'),'-Wl,--gc-sections','-Wl,-e,app_recovery_test_main']
 cmd += [f'-Wl,-u,{f}' for f in functions]+['-lgcc','-o',str(elf)]
 r=subprocess.run(cmd,capture_output=True,text=True)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 symbols={l.split()[2]:int(l.split()[0],16) for l in subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines() if len(l.split())==3}
 data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];psz,pnum=struct.unpack_from('<HH',data,42)
 uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 for a,n in [(0x10000000,0x20000),(0x20000000,0x30000),(0xE000E000,0x2000),(0x08000000,0x80000),(0x40020000,0x3000)]:uc.mem_map(a,n)
 for i in range(pnum):
  k,o,a,_,sz,_,_,_=struct.unpack_from('<8I',data,phoff+i*psz)
  if k==1 and sz:uc.mem_write(a,data[o:o+sz])
 initial_ram=bytes(uc.mem_read(0x20000000,0x30000))
 flash=bytes((i*17+3)&255 for i in range(0x80000));uc.mem_write(0x08000000,flash)
 resets=[]
 def reset_hook(uc,access,address,size,value,user):
  if address==0xE000ED0C and value&4:resets.append(value);uc.emu_stop()
 uc.hook_add(UC_HOOK_MEM_WRITE,reset_hook)
 def call(name,r0=0,r1=0):
  uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2002FFF0);uc.reg_write(UC_ARM_REG_LR,0x1001FFF1);uc.reg_write(UC_ARM_REG_R0,r0);uc.reg_write(UC_ARM_REG_R1,r1)
  uc.emu_start(symbols[name]|1,0x1001FFF0,count=1000000000,timeout=50000000)
  return uc.reg_read(UC_ARM_REG_R0)
 assert call('app_recovery_test_main')==0 and not resets
 assert call('RecoveryResident_Verify',0x08000000)==0
 resident=(PROJECT.parents[1]/'VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin').read_bytes()[:0x8000]
 uc.mem_write(0x08000000,resident);assert call('RecoveryResident_Verify',0x08000000)==1
 uc.mem_write(0x08000000,flash[:0x8000])
 call('app_recovery_reset_test');assert len(resets)==1
 intent=struct.unpack('<4I',uc.mem_read(symbols['intent'],16));assert intent[0]==0x32564352 and intent[1]==2
 assert call('AppRecovery_IsFaultBoot')==0
 call('AppRecovery_FaultReset',0x303);assert len(resets)==2 and call('AppRecovery_IsFaultBoot')==1
 uc.mem_write(symbols['recovery_active'],struct.pack('<I',1));call('BSP_FaultRecoveryRequested',0x301);assert len(resets)==2
 assert bytes(uc.mem_read(symbols['identity_ready'],4))==bytes(4)
 for _ in range(120):
  call('AppRecovery_IdentityProcess')
  assert uc.reg_read(UC_ARM_REG_PC)==0x1001FFF0
 assert struct.unpack('<I',uc.mem_read(symbols['identity_ready'],4))[0]==1
 digest=bytes(uc.mem_read(symbols['identity_digests'],64))
 assert digest==hashlib.sha256(flash[0x10000:]).digest()+hashlib.sha256(flash[:0x8000]).digest()
 count=struct.unpack('<I',uc.mem_read(symbols['app_recovery_assertions'],4))[0]
 # Exercise the real runtime entry long after startup. Resetting the emulated
 # fixture occurs only BETWEEN cases: no power removal/reset within a chord.
 # GPIOG is raw IGN (active low); the recovery policy must never change it or
 # GPIO power outputs. Product task scheduling after IGN wake is audited
 # separately, not simulated by setting this input register.
 policy_resets=len(resets);runtime_cases=[]
 for ign_off in (0,1):
  uc.mem_write(0x20000000,initial_ram)
  uc.mem_write(0x40021810,struct.pack('<I',ign_off<<13))
  def buttons(raw,now):
   uc.mem_write(0x40020010,struct.pack('<I',0 if raw&2 else 1<<15))
   uc.mem_write(0x40022010,struct.pack('<I',0 if raw&1 else 1<<6))
   call('AppRecovery_RuntimeButtons',now)
  base=86400000;before_resets=len(resets)
  buttons(0,base);buttons(0,base+80)
  buttons(1,base+100);buttons(1,base+180);buttons(1,base+6000)
  assert call('AppRecovery_RuntimeRequested')==0 # DOWN alone is harmless.
  buttons(3,base+6010);buttons(0,base+6070);buttons(0,base+6150)
  assert call('AppRecovery_RuntimeRequested')==0 # Sub-debounce chord ignored.
  buttons(3,base+6200);buttons(3,base+6280)
  buttons(3,base+9279);assert call('AppRecovery_RuntimeRequested')==0
  buttons(3,base+9280);assert call('AppRecovery_RuntimeRequested')==1
  assert len(resets)==before_resets
  call('AppRecovery_RuntimeProcess',base+10779,1)
  call('AppRecovery_RuntimeProcess',base+10780,0)
  assert len(resets)==before_resets # Delay and storage drain are mandatory.
  call('AppRecovery_ControlDisconnected')
  assert call('AppRecovery_RuntimeRequested')==1 # Physical entry needs no BT.
  gpio_before=bytes(uc.mem_read(0x40020000,0x3000))
  call('AppRecovery_RuntimeProcess',base+10780,1)
  assert len(resets)==before_resets+1
  intent_words=struct.unpack('<4I',uc.mem_read(symbols['intent'],16))
  assert intent_words==(0x32564352,1,0xfffffffe,1^0xD615A73B)
  assert bytes(uc.mem_read(0x40020000,0x3000))==gpio_before
  runtime_cases.append({'raw_ign_off':bool(ign_off),'started_after_ms':base,'software_reset_to_waiting_recovery':True,'direct_install_requested':False,'gpio_unchanged':True,'power_cycle_performed':False,'storage_drain_required':True,'short_chord_rejected':True})
 row={'optimization':opt,'status':'pass','assertions':count,'actual_cmsis_reset_intercepts':len(resets),'control_fault_reset_intercepts':policy_resets,'fault_reentry_stops_reset':True,'identity_full_flash_match':True,'runtime_button_cases':runtime_cases};results.append(row);print(json.dumps(row),flush=True)
(OUT/'results.json').write_text(json.dumps({'results':results,'limits':'Actual ARM app policy/runtime chord/CMSIS AIRCR writes with simulated GPIO/SRAM/internal flash. No physical button/display/NOR/RTOS scheduling or IGN STOP-wake proof. Runtime chord is skipped by the real IoTask during POWER_DEEP; the vehicle procedure first sets IGN ON.'},indent=2))


