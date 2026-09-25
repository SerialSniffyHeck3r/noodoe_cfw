/*
 * 순정 BL에서 받은 실행 상태를 C 런타임보다 먼저 정리하는 APP 진입점.
 * Cube startup의 Reset_Handler는 weak이므로 이 strong 정의가 벡터에 연결된다.
 * 생성 startup 파일은 그대로 보존하고 실제 ELF 벡터 주소를 CLI에서 검증한다.
 */
.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
  /* SRAM의 BL 벡터를 덮기 전에 일반 IRQ부터 막는다. */
  cpsid i
  mrs r0, control
  mrs r1, basepri
  mrs r2, ipsr
  ldr r3, =0xE000ED08
  ldr r3, [r3]

  /* BL의 Thread/privileged 진입을 전제로 MSP를 명시적으로 선택한다. */
  movs r4, #0
  msr control, r4
  isb
  ldr sp, =_estack
  msr basepri, r4
  msr faultmask, r4

  /* C 함수 prologue의 첫 stack 접근보다 먼저 이전 MPU 제한을 해제한다.
   * 진단 구조체의 mpu_ctrl_before_c_cleanup은 이 해제 이후의 값이다.
   * 순정 BL의 privileged Thread 진입을 전제로 한다.
   */
  ldr r5, =0xE000ED94
  str r4, [r5]
  dsb
  isb

  /* vector table은 FLASH의 APP 벡터다. DMA/IRQ 정리보다도 먼저 설치한다. */
  ldr r4, =g_pfnVectors
  ldr r5, =0xE000ED08
  str r4, [r5]
  dsb
  isb
  /* Optional profile hook starts IWDG before any peripheral normalization.
   * Preserve the original boot-entry arguments across the call. */
  push {r0-r3}
  bl BSP_BootSafetyStart
  pop {r0-r3}
  bl BSP_BootEarly
  bl SystemInit

  /* 자체 C 초기화: .noinit은 아래 두 범위 밖에 있으므로 진입 기록이 보존된다. */
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  bcs 2f
  ldr r3, [r0], #4
  str r3, [r1], #4
  b 1b
2:
  ldr r1, =_sbss
  ldr r2, =_ebss
  movs r3, #0
3:
  cmp r1, r2
  bcs 4f
  str r3, [r1], #4
  b 3b
4:
  bl BSP_CCM_Init
  /* Profile-owned rescue runs before main/HAL/PLL/RTOS. Default is empty. */
  bl BSP_ApplicationEarly
  /* 전역 생성자도 표준 startup과 같이 실행한다. IRQ 해제는 main의 BSP에 맡긴다. */
  bl __libc_init_array
  bl main
5:
  /* main 반환은 지원하지 않는다. 디버거에서 식별할 수 있도록 정지한다. */
  b 5b
.size Reset_Handler, .-Reset_Handler

/* The peripheral bus cannot see CCM. Enable its CPU data clock before clearing
 * the explicit NOLOAD range. No retained diagnostics or SRAM DMA data is touched. */
.section .text.BSP_CCM_Init,"ax",%progbits
.global BSP_CCM_Init
.type BSP_CCM_Init,%function
.thumb_func
BSP_CCM_Init:
  ldr r0, =0x40023830
  ldr r1, [r0]
  orr r1, r1, #0x00100000
  str r1, [r0]
  dsb
  ldr r0, =_sccmbss
  ldr r1, =_eccmbss
  movs r2, #0
6:
  cmp r0, r1
  bcs 7f
  str r2, [r0], #4
  b 6b
7:
  bx lr
.size BSP_CCM_Init, .-BSP_CCM_Init
