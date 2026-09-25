MEMORY {
 FLASH(rx): ORIGIN = 0x10000000, LENGTH = 128K
 RAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
