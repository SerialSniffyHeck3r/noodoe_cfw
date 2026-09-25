"""Execute real stock FAT adapter, FatFs and incremental ChaN photo service on ARM.
NOR comes from the verified full donor backup. All write stubs must stay unused.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
(O/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 512K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 256K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n . = ALIGN(8); end = .;\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
incs=[H/'stubs',P/'Drivers/BSP/inc',P/'Middlewares/Noodoe/Storage/inc',P/'Middlewares/Noodoe/Photos/inc',P/'Middlewares/Third_Party/FatFs/src',P/'Middlewares/Third_Party/LVGL/src/libs/tjpgd']
sources=[P/f for f in ['Middlewares/Noodoe/Storage/src/storage_disk.c','Middlewares/Noodoe/Storage/src/StorageService.c','Middlewares/Noodoe/Photos/src/PhotoService.c','Middlewares/Third_Party/FatFs/src/ff.c','Middlewares/Noodoe/Photos/src/photo_jpeg.c']]
backup=P.parents[1]/'analysis/2026-09-12-integrated-bringup/nor-full-backup-02/A.bin'
reports=[]
for opt in ['O0','Os','Oz']:
 elf=O/(opt+'.elf')
 cmd=[str(TC/'arm-none-eabi-gcc.exe'),str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/src/noodoe_crc32.c'),'-I'+str(next(q for q in Path(__file__).resolve().parents if (q/'tools/build.ps1').is_file())/'Drivers/BSP/inc'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,*(['-flto'] if opt=='Oz' else []),'-DLV_CONF_SKIP','-DLV_USE_TJPGD=1','-Wall','-Wextra','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*['-I'+str(x) for x in incs],str(H/'test.c'),*map(str,sources),'-Wl,-T,'+str(O/'test.ld'),'-Wl,--gc-sections','-Wl,-e,TestMain','-Wl,--undefined=TestReload','-o',str(elf),'-lgcc']
 r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True);symbols={s[2]:int(s[0],16) for l in nm.splitlines() if len(s:=l.split())==3}
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 for address,size in [(0x10000000,0x80000),(0x20000000,0x40000),(0x90000000,0x8000000),(0xA0000000,0x40000),(0xC0000000,0x4000000)]:u.mem_map(address,size)
 u.mem_write(0x90000000,backup.read_bytes())
 files=[next((P.parents[1]/'evidence/usb/2026-09-09-noodoe-NOODOE_USB_SERIAL_REQUIRED/files/album'/str(i)).glob('*.jpg')) for i in range(2)]
 lengths=[]
 for i,f in enumerate(files):
  jpeg=f.read_bytes();u.mem_write(0xA0000000+i*0x20000,jpeg);lengths.append(len(jpeg))
 data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
 for i in range(header[10]):
  kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
  if kind==1 and size:u.mem_write(address,data[offset:offset+size])
 u.mem_write(symbols['input_lengths'],struct.pack('<2I',*lengths))
 stop=0x1007fff0;u.reg_write(UC_ARM_REG_SP,0x2003fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
 u.emu_start(symbols['TestMain']|1,stop,count=800000000)
 word=lambda name:struct.unpack('<I',u.mem_read(symbols[name],4))[0]
 r={'optimization':opt,'returned':u.reg_read(UC_ARM_REG_PC)==stop,'failure_line':u.reg_read(UC_ARM_REG_R0),'assertions':word('assertions'),'writes':word('writes'),'photo_diag':struct.unpack('<18I',u.mem_read(symbols['g_photos'],72))}
 reports.append(r);print(json.dumps(r),flush=True);(O/'results.json').write_text(json.dumps(reports,indent=2))
 if not r['returned'] or r['failure_line']:raise SystemExit(1)
 for i in range(3):
  address,w,h,n,revision=struct.unpack('<5I',u.mem_read(symbols['result_images']+i*20,20));pixels=bytes(u.mem_read(address,n));(O/f'{opt}-photo-{i}.rgb565').write_bytes(pixels)

 # Save the emulated NOR for a fresh-CPU persistent reload; no real device I/O.
 if opt=='Os':
  media=bytes(u.mem_read(0x90000000,0x8000000))
  before=backup.read_bytes();changed={i for i in range(0,len(before),4096) if before[i:i+4096]!=media[i:i+4096]}
  # FAT/BPB/root plus album parent and folders0..2 must also be unchanged before
  # replay. This binds allocation and path traversal to the tested donor image.
  watched=set(range(0,0x9000,4096))
  for cluster in [2610,2612,2613,2614]:
   at=0x9000+(cluster-2)*32768;watched.update(range(at,at+32768,4096))
  allblocks=sorted(changed|watched);regions=[]
  for at in allblocks:
   if regions and regions[-1]['offset']+regions[-1]['length']==at:regions[-1]['length']+=4096
   else:regions.append({'offset':at,'length':4096})
  for i,r in enumerate(regions):
   at,n=r['offset'],r['length'];a=before[at:at+n];b=media[at:at+n]
   (O/f'plan-{i}-before.bin').write_bytes(a);(O/f'plan-{i}-after.bin').write_bytes(b)
   r.update(before_sha256=hashlib.sha256(a).hexdigest(),after_sha256=hashlib.sha256(b).hexdigest())
  plan={'schema':1,'backup_sha256':hashlib.sha256(before).hexdigest(),'uid':[3735583,875974927,892810041],
        'regions':regions,'images':[{'slot':i,'path':str(f),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for i,f in enumerate(files)]}
  (O/'import-plan.json').write_text(json.dumps(plan,indent=2))
  u.mem_write(0x20000000,bytes(0x40000));u.mem_write(0xC0000000,bytes(0x4000000))
  for i in range(header[10]):
   kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
   if kind==1 and size:u.mem_write(address,data[offset:offset+size])
  u.mem_write(0x90000000,media);u.reg_write(UC_ARM_REG_SP,0x2003fff0);u.reg_write(UC_ARM_REG_LR,stop|1)
  u.emu_start(symbols['TestReload']|1,stop,count=800000000)
  assert u.reg_read(UC_ARM_REG_PC)==stop and u.reg_read(UC_ARM_REG_R0)==0, 'Reload failed at '+str(u.reg_read(UC_ARM_REG_R0))
  print('Fresh CPU / persistent FAT reload: PASS',flush=True)
