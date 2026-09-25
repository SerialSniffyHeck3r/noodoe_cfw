#ifndef BSP_EVE_H
#define BSP_EVE_H

#include <stdint.h>

/* EVE의 정상 응답과 MCU 전송 실패를 구분한다. 0만 성공이며 재시도/전원 정책은
 * 호출하는 LCD 시험 태스크가 결정한다. 이 모듈은 패널 SPI4나 백라이트를 제어하지 않는다. */
typedef enum {
    BSP_EVE_OK = 0,
    BSP_EVE_ERROR_ARGUMENT = 1,
    BSP_EVE_ERROR_CONTEXT = 2,
    BSP_EVE_ERROR_SPI = 3,
    BSP_EVE_ERROR_ID_TIMEOUT = 4,
    BSP_EVE_ERROR_CPU_TIMEOUT = 5,
    BSP_EVE_ERROR_SWAP_TIMEOUT = 6,
    BSP_EVE_ERROR_VERIFY = 7,
    BSP_EVE_ERROR_NOT_READY = 8
} BSP_EVE_Status;

/* debugger에서 18개의 32bit word로 읽을 수 있는 RAM 진단이다. phase는 마지막
 * 진행 단계, result는 실패 원인이다. frames/frequency_hz는 EVE에서 읽은 값이며
 * 화면을 눈으로 확인했다는 증거가 아니다. 장치 접근 없이 이 구조체를 조회할 수 있다. */
typedef struct {
    uint32_t magic, version, phase, result;
    uint32_t spi_status, spi_error, last_address, last_value;
    uint32_t reg_id, cpu_reset, chip_id, frequency_hz;
    uint32_t frames, dl_swap, dl_words, pclk;
    uint32_t transfer_count, ready;
} BSP_EVE_Diagnostics;

extern volatile BSP_EVE_Diagnostics g_bsp_eve;
/* Graphics owner:1 busy,0 complete,other error. SLEEP retains RAM_G/cache;
 * wake phases are split so the caller can wait without blocking its task. */
uint32_t BSP_EVE_Sleep(void);
uint32_t BSP_EVE_WakeBegin(void);
uint32_t BSP_EVE_WakeFinish(void);

/* HAL_Init/SystemClock_Config와 HAL millisecond tick이 동작한 뒤, IRQ가 열린
 * Thread mode에서 단일 소유자가 호출한다. PA4/PB1/SPI1과 EVE RAM_DL/화면 timing을
 * 초기화하고 빈 화면의 PCLK까지 시작한다. 실패하면 상세 진단을 남기고 반환한다. */
BSP_EVE_Status BSP_EVE_Init(void);

/* Init 성공 후 480x480 창 테두리·제목 띠·교차선을 raw display list로 쓴다.
 * 폰트, command FIFO, RTOS object, 외부 framebuffer, 일반 graphics framework는 없다. */
BSP_EVE_Status BSP_EVE_DrawTestWindow(void);

/*
 * 보드 내부 display facade가 작성한 신뢰하는 raw DL을 RAM_DL에 제출한다.
 * count는 byte가 아닌 uint32_t word 수이며 1..2048, 마지막 word는 DISPLAY(0)여야 한다.
 * 이전 swap 완료 확인 -> RAM 쓰기/readback -> FRAME swap 완료 -> 진단 조회 순서다.
 * 동기 호출이므로 반환 뒤 buffer를 재사용할 수 있다. 일반 앱은 BSP_Display를 사용한다.
 */
BSP_EVE_Status BSP_EVE_SubmitDisplayList(const uint32_t *words, uint32_t count);

/* SPI1이 이미 준비된 상태에서 REG_ID 한 byte를 읽는다. id=NULL이면 오류이며,
 * 전송 실패 시 호출자 버퍼는 바꾸지 않는다. EVE를 reset하거나 재초기화하지 않는다. */
BSP_EVE_Status BSP_EVE_ReadId(uint8_t *id);

/* Init 성공 후 REG_FRAMES 32bit를 읽는다. LCD 태스크는 blink 반주기마다 호출하여
 * 실제 EVE scanout 카운터 증가를 관측할 수 있다. NULL/실패 시 *frames는 보존한다. */
BSP_EVE_Status BSP_EVE_ReadFrames(uint32_t *frames);

/* Init 성공 후 ID/CPURESET/FRAMES/FREQUENCY/PCLK/DLSWAP를 읽어 진단을 갱신한다.
 * 화면이나 레지스터를 쓰지 않으며, 다른 API와 동시에 호출하지 않는다. */
BSP_EVE_Status BSP_EVE_RefreshDiagnostics(void);

/* SPI 접근 없이 진단 RAM의 읽기 전용 주소를 돌려준다. 반환값의 수명은 펌웨어 전체다. */
const volatile BSP_EVE_Diagnostics *BSP_EVE_GetDiagnostics(void);

#endif
