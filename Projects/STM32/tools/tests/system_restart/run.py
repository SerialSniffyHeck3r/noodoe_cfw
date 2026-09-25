"""Production asynchronous restart, ARM O0/Os; mock reset/storage only."""
from pathlib import Path
H=Path(__file__).resolve().parent
s=(H.parent/'companion_control/run.py').read_text()
a=s.index(' cmd=');b=s.index(' r=subprocess.run',a)
s=s[:a]+" cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-'+opt,'-g','-Wall','-Wextra','-Werror','-DNOODOE_PRODUCT=1','-I'+str(H),*['-I'+str(x) for x in incs],'-I'+str(P/'RecoveryGate/include'),str(H/'test.c'),str(P/'App_Logic/Settings/src/settings_system.c'),'-nostartfiles','--specs=nosys.specs','--specs=nano.specs','-Wl,-T,'+str(ld),'-o',str(elf)]\n"+s[b:]
exec(compile(s,str(__file__),'exec'))
