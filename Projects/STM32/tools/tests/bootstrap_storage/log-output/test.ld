MEMORY { FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 256K
 RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 192K }
 SECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }