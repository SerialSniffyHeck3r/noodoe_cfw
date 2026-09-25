"""Offline original BL GPIO intent trace. No target access or register writes.

Executes the original BL's common GPIO routine in an ARM emulator, with GPIO
IDR strap bits set to the previously observed board revision6. HAL functions
execute their real instructions against inert MMIO memory. BSRR writes are
recorded as intended levels, not treated as evidence of physical voltage.
"""
from pathlib import Path
import hashlib, importlib.util, json, struct, sys

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
REVERSING = PROJECT.parents[1]
sys.path.insert(0, str(REVERSING / '.tools/analysis-python'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_PC, UC_ARM_REG_XPSR

spec = importlib.util.spec_from_file_location('original_boot', REVERSING / 'analysis/2026-09-11-bootloader-re/startup/inspect_boot.py')
boot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boot)
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
for address, size in ((0x08000000, 0x80000), (0x20000000, 0x30000),
                      (0x40000000, 0x40000), (0xE000E000, 0x2000)):
    uc.mem_map(address, size)
uc.mem_write(0x08000000, boot.FLASH)
uc.mem_write(boot.RAM_BASE, boot.RAM)
# PA3=0, PH3=1, PH2=1, PB10=0. These are emulated inputs, not fresh measurements.
uc.mem_write(0x40021C10, struct.pack('<I', 0x0C))
trace = []

def pin_names(port, mask):
    assert 0x40020000 <= port <= 0x40022000 and (port - 0x40020000) % 0x400 == 0
    letter = chr(ord('A') + (port - 0x40020000) // 0x400)
    return [f'P{letter}{pin}' for pin in range(16) if mask & (1 << pin)]

def code(u, pc, size, _):
    caller = (u.reg_read(UC_ARM_REG_LR) & ~1) - 4
    if pc == 0x20001638:  # actual BL HAL_GPIO_Init
        port, config = u.reg_read(UC_ARM_REG_R0), u.reg_read(UC_ARM_REG_R1)
        mask, mode, pull, speed, alternate = struct.unpack('<5I', u.mem_read(config, 20))
        trace.append(dict(operation='init', call=f'0x{caller:08X}', pins=pin_names(port, mask),
                          mode=mode, pull=pull, speed=speed, alternate=alternate))
    elif pc == 0x200019C0:  # actual BL HAL_GPIO_WritePin
        port, mask, value = (u.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
        trace.append(dict(operation='write', call=f'0x{caller:08X}', pins=pin_names(port, mask), value=bool(value)))

uc.hook_add(UC_HOOK_CODE, code)
uc.reg_write(UC_ARM_REG_XPSR, 0x1000000)
uc.reg_write(UC_ARM_REG_SP, 0x2002F000)
uc.reg_write(UC_ARM_REG_LR, 0x0807FFF1)
uc.emu_start(0x200002B1, 0x0807FFF0, count=1000000)
assert uc.reg_read(UC_ARM_REG_PC) == 0x0807FFF0, 'Original GPIO routine did not return'
final = {}
for operation in trace:
    for pin in operation['pins']:
        state = final.setdefault(pin, {})
        if operation['operation'] == 'init':
            state.update({k: operation[k] for k in ('mode', 'pull', 'speed', 'alternate')})
            state['init_call'] = operation['call']
        else:
            state.update(level=int(operation['value']), write_call=operation['call'])
report = dict(
    original_flash_sha256=hashlib.sha256(boot.FLASH).hexdigest(),
    common_gpio_entry='0x200002B0', board_revision=uc.mem_read(0x20006A01, 1)[0],
    emulated_strap_inputs=dict(PA3=0, PH3=1, PH2=1, PB10=0),
    output_pins={pin: state for pin, state in final.items() if state.get('mode') == 1},
    trace=trace,
    limits='Offline original instructions and inert MMIO. This establishes GPIO intent only; no controller supply, clock waveform, physical pin or full-stock startup result is measured.')
target = HERE / 'boot-gpio-intent.json'
target.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({k: v for k, v in report.items() if k != 'trace'}, indent=2))
