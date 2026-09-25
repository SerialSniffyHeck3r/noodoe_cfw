ENTRY(Gate_Reset)
MEMORY {
 VECTORS (rx) : ORIGIN = 0x08010000, LENGTH = 4K
 JOURNAL (r) : ORIGIN = 0x08011000, LENGTH = 60K
 FLASH (rx) : ORIGIN = 0x08020000, LENGTH = 384K
 RAM (rwx) : ORIGIN = 0x20007000, LENGTH = 0x28F00
 RETAIN (rw) : ORIGIN = 0x2002FF00, LENGTH = 256
}
_estack = ORIGIN(RETAIN);
SECTIONS {
 .isr_vector : { KEEP(*(.isr_vector)) . = 0x200; KEEP(*(.uninstall_descriptor)) } > VECTORS
 .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
 .ARM.extab : { *(.ARM.extab*) } > FLASH
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
 _sidata = LOADADDR(.data);
 .data : { . = ALIGN(4); _sdata = .; *(.data*) *(.RamFunc*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH
 .bss (NOLOAD) : { . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } > RAM
 /DISCARD/ : { *(.comment*) *(.note*) }
}
ASSERT(SIZEOF(.isr_vector) <= 4096, "Uninstall entry exceeds4KiB")
ASSERT(_ebss + 8192 <= _estack, "Uninstall stack reserve below8KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x08080000, "Uninstall code + stock exceed384KiB")
