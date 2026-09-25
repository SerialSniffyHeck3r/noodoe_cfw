.syntax unified
.cpu cortex-m4
.thumb
.section .isr_vector,"a",%progbits
.global gate_vectors
gate_vectors:
 .word _estack
 .word Gate_Reset
 .rept 14
 .word Gate_Fault
 .endr
 .rept 91
 .word Gate_Fault
 .endr
.section .text.Gate_Reset,"ax",%progbits
.thumb_func
.global Gate_Reset
Gate_Reset:
 cpsid i
 movs r0,#0
 msr control,r0
 msr basepri,r0
 msr faultmask,r0
 ldr sp,=_estack
 ldr r1,=0xE000ED08
 ldr r2,=gate_vectors
 str r2,[r1]
 dsb
 isb
 /* A direct original-BL branch can inherit DMA. Stop both masters before
  * reclaiming stack/data, rather than waiting until after C initialization. */
 ldr r1,=0x40023810
 ldr r2,[r1]
 ldr r3,=0x00600000
 orrs r2,r3
 str r2,[r1]
 bics r2,r3
 str r2,[r1]
 dsb
 /* This entry is deliberately before C data initialization and HSE/PLL. */
 bl BSP_Watchdog_StartEarly
 cmp r0,#0
 beq Gate_Fault
 ldr r0,=_sidata
 ldr r1,=_sdata
 ldr r2,=_edata
1: cmp r1,r2
 bcs 2f
 ldr r3,[r0],#4
 str r3,[r1],#4
 b 1b
2: ldr r1,=_sbss
 ldr r2,=_ebss
 movs r3,#0
3: cmp r1,r2
 bcs 4f
 str r3,[r1],#4
 b 3b
4: bl Uninstall_Main
 b Gate_Fault
.thumb_func
.global Gate_Fault
Gate_Fault:
 /* A fault cannot feed forever. IWDG returns through the original BL. */
 cpsid i
1: b 1b
