"""Execute the linked CCM startup and real LVGL allocator, without a board."""
from pathlib import Path
import argparse,json,struct,subprocess,sys,zipfile,re,hashlib
P=Path(__file__).resolve().parents[3]
sys.path[:0]=[str(P/'tools'),str(P.parents[1]/'.tools/analysis-python')]
from validate_image import Elf32
from resource_install import TC
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_XPSR
a=argparse.ArgumentParser();a.add_argument('--configuration',default='Release');a.add_argument('--elf',type=Path);a=a.parse_args()
elfpath=a.elf or P/a.configuration/'FuckNudo_Noodoe_CFW_Project.elf';e=Elf32(elfpath.read_bytes())
nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elfpath)],text=True);syms={w[2]:int(w[0],16) for l in nm.splitlines() if len(w:=l.split())==3}
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
for at,n in [(0x08000000,0x100000),(0x10000000,0x10000),(0x20000000,0x40000),(0x40023000,0x1000),(0xe000e000,0x2000)]:u.mem_map(at,n)
for s in e.sections:
    if s['flags']&2 and s['type']!=8 and s['size']:u.mem_write(s['addr'],e.section_data(s))
u.reg_write(UC_ARM_REG_XPSR,0x1000000);u.reg_write(UC_ARM_REG_SP,0x2003f000)
def call(name,*args):
    for reg,v in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2],args):u.reg_write(reg,v)
    u.reg_write(UC_ARM_REG_LR,0x080ffff1);u.emu_start(syms[name]|1,0x080ffff0,count=10000000)
    assert u.reg_read(UC_ARM_REG_PC)==0x080ffff0,name+' did not return';return u.reg_read(UC_ARM_REG_R0)
u.mem_write(0x10000000,b'\xa5'*65536);u.mem_write(0x40023830,struct.pack('<I',0x123))
call('BSP_CCM_Init');n=e.section('.ccm_bss')['size']
assert bytes(u.mem_read(0x10000000,n))==bytes(n)
assert bytes(u.mem_read(0x10000000+n,65536-n))==b'\xa5'*(65536-n)
assert struct.unpack('<I',u.mem_read(0x40023830,4))[0]==0x100123
call('lv_mem_init');assert call('GraphicsMemory_Check')==1
# Exercise the actual allocator over1000 allocation/free permutations.
for i in range(1000):
    slots=[call('lv_malloc',n) for n in [17+(i%29),512,1200,1024]]
    assert all(0x10000020<=p<0x1000c020 for p in slots)
    for idx in [1,3,0,2]:call('lv_free',slots[idx])
    assert call('GraphicsMemory_Check')==1
call('lv_mem_monitor',0x20038000);monitor=bytes(u.mem_read(0x20038000,40))
#100STOP-like retained-memory intervals: initialization is deliberately not
# rerun on wake. This tests guard/allocator preservation, not physical SDRAM.
for _ in range(100):assert call('GraphicsMemory_Check')==1
u.mem_write(0x10000000,b'\0'*4);assert call('GraphicsMemory_Check')==0
dis=subprocess.check_output([str(TC/'arm-none-eabi-objdump.exe'),'-d',str(elfpath)],text=True)
reset=dis[dis.index('<Reset_Handler>:'):];reset=reset[:reset.index('\n\n')]
assert reset.index('<BSP_CCM_Init>')<reset.index('<__libc_init_array>')
report=dict(configuration=a.configuration,ccm_bytes=n,free=65536-n,allocation_cycles=1000,retention_checks=100,guard_corruption_detected=True,monitor_hex=monitor.hex(),hardware=False)
# Mock only RTOS critical sections and the SPI bus. Resource SDRAM is never
# mapped; CCM is unmapped now, proving the ROM error view uses neither pool.
u.mem_unmap(0x10000000,0x10000)
def stub(name,fn):
    def hook(emu,address,size,context):
        result=fn(emu);emu.reg_write(UC_ARM_REG_R0,result or 0);emu.reg_write(UC_ARM_REG_PC,emu.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,hook,begin=syms[name]&~1,end=syms[name]&~1)
for name in ['vPortEnterCritical','vPortExitCritical']:stub(name,lambda emu:0)
# Named SDRAM reservation retries neither grow the arena nor rewrite metadata.
ram=syms['g_bsp_ram'];u.mem_write(ram+4,struct.pack('<I',1));u.mem_write(ram+16,struct.pack('<I',0x4000000))
reservations=[call('BSP_RAM_AllocateNamed',i+1,n) for i,n in enumerate([524288,460800,4096,552960])]
assert all(reservations);allocated=bytes(u.mem_read(ram+36,4))
for _ in range(1000):
    for i,n in enumerate([524288,460800,4096,552960]):assert call('BSP_RAM_AllocateNamed',i+1,n)==reservations[i]
assert bytes(u.mem_read(ram+36,4))==allocated
assert call('BSP_RAM_AllocateNamed',1,524320)==0
assert call('BSP_RAM_Init')==0
# The first error remains latched; only the bounded last8 history records roll.
call('SystemError_SetStage',2)
for i in range(1000):call('SystemError_Report',1,i)
assert call('SystemError_GetStatus',0x20038000)==1
assert struct.unpack('<3I',u.mem_read(0x20038000,12))==(1,0,2)
assert struct.unpack('<I',u.mem_read(syms['g_system_error']+8,4))[0]==1000
# No heap allocation is allowed while rendering the fallback view.
def no_alloc(emu):raise AssertionError('Error view allocated memory')
for name in ['lv_malloc','lv_calloc','pvPortMalloc','malloc','calloc']:
    if name in syms:stub(name,no_alloc)
settings={'space':4092,'swap':0};sent=[]
def read_bus(emu):
    at=emu.reg_read(UC_ARM_REG_R0);dst=emu.reg_read(UC_ARM_REG_R1);n=emu.reg_read(UC_ARM_REG_R2)
    assert at in [0x302574,0x302054]
    emu.mem_write(dst,(settings['space'] if at==0x302574 else settings['swap']).to_bytes(n,'little'));return 0
def send_bus(emu):
    sent.append(bytes(emu.mem_read(emu.reg_read(UC_ARM_REG_R0),emu.reg_read(UC_ARM_REG_R1))));return 0
stub('BSP_EVE_BusRead',read_bus);stub('BSP_EVE_BusSend',send_bus)
stub('BSP_EVE_BusSelect',lambda emu:0);stub('BSP_EVE_BusDeselect',lambda emu:0)
assert call('SystemErrorView_Draw',1,2,3)==0 and not sent
u.mem_write(syms['g_bsp_eve']+68,struct.pack('<I',1))
settings['space']=0;assert call('SystemErrorView_Draw',1,2,3)==0 and not sent
settings['space']=4092;settings['swap']=2;assert call('SystemErrorView_Draw',1,2,3)==0 and not sent
settings['swap']=0
assert call('SystemErrorView_Draw',1,2,3)==1
assert sent[0]==bytes.fromhex('b02578') and len(sent)==2 and len(sent[1])<=384
assert sent[1][:4]==struct.pack('<I',0xffffff00) and sent[1][-4:]==struct.pack('<I',0xffffff01)
assert sent[1].count(struct.pack('<I',0xffffff3f))==2 and b'SYSTEM ERROR' in sent[1]
assert b'aw shit :(' in sent[1] and struct.pack('<I',0x04ff4040) in sent[1]
report.update(sdram_named_retries=1000,error_records=1000,rom_error_without_ccm_or_sdram=True,rom_error_no_alloc=True,elf_sha256=hashlib.sha256(elfpath.read_bytes()).hexdigest())
out=P/'tools/tests/resources/output' /f'elf-memory-{a.configuration}.json';out.write_text(json.dumps(report,indent=2));print(json.dumps(report))
