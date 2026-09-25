ENTRY(Gate_Reset)
MEMORY {
 FLASH (rx) : ORIGIN = 0x08010000, LENGTH = 64K
 RAM (rwx) : ORIGIN = 0x20007000, LENGTH = 0x28F00
 RETAIN (rw) : ORIGIN = 0x2002FF00, LENGTH = 256
}
_estack = ORIGIN(RETAIN);
SECTIONS {
 .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
 .gate_features ORIGIN(FLASH) + 0x200 : { KEEP(*(.gate_features)) } > FLASH
 .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
 .ARM.extab : { *(.ARM.extab*) } > FLASH
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
 _sidata = LOADADDR(.data);
 .data : { . = ALIGN(4); _sdata = .; *(.data*) *(.RamFunc*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH
 .bss (NOLOAD) : { . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } > RAM
 .gate_mailbox (NOLOAD) : { KEEP(*(.gate_mailbox)) } > RETAIN
 /DISCARD/ : { *(.comment*) *(.note*) }
}
ASSERT(_ebss + 8192 <= _estack, "RecoveryGate stack reserve below8KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x08020000, "RecoveryGate exceeds S4")
