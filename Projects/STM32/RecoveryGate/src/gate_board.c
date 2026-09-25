#include "gate_board.h"
#include "gate_policy.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx.h"
#include <string.h>
#define RAM __attribute__((section(".RamFunc.gate"),noinline,noclone))
#if !NOODOE_BOOTSTRAP
uint32_t SystemCoreClock=16000000U;
#endif
static uint32_t cycles_last,cycles_remainder,milliseconds;
static uint32_t nor_first,nor_bytes,nor_ready;
static void Pin(GPIO_TypeDef *g,uint32_t pin,uint32_t mode,uint32_t af)
{g->MODER=(g->MODER&~(3U<<(pin*2)))|(mode<<(pin*2));g->OTYPER&=~(1U<<pin);g->OSPEEDR|=3U<<(pin*2);g->PUPDR&=~(3U<<(pin*2));
 g->AFR[pin/8]=(g->AFR[pin/8]&~(15U<<((pin%8)*4)))|(af<<((pin%8)*4));}
/* DWT runs at fixed HSI16MHz; no ISR, PLL, RTOS or restored SDRAM is needed. */
RAM uint32_t GateBoard_Millis(void)
{uint32_t n=DWT->CYCCNT,d=n-cycles_last;cycles_last=n;uint32_t whole=d/16000U,tail=d%16000U;
 milliseconds+=whole;cycles_remainder+=tail;if(cycles_remainder>=16000){milliseconds++;cycles_remainder-=16000;}return milliseconds;}
uint32_t GateBoard_Init(void)
{
 SysTick->CTRL=0;SysTick->LOAD=0;SysTick->VAL=0;
 for(uint32_t i=0;i<8;i++){NVIC->ICER[i]=~0U;NVIC->ICPR[i]=~0U;}
 SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk;
 RCC->CR|=RCC_CR_HSION;uint32_t n=1000000U;while(!(RCC->CR&RCC_CR_HSIRDY)&&--n){}if(!n)return 1;
 RCC->CFGR&=~(RCC_CFGR_SW|RCC_CFGR_HPRE|RCC_CFGR_PPRE1|RCC_CFGR_PPRE2);
 n=1000000U;while((RCC->CFGR&RCC_CFGR_SWS)&&--n){}if(!n)return 2;
 /* Stop old DMA writers before reclaiming SRAM. Power latch GPIO is untouched. */
 RCC->AHB1RSTR|=RCC_AHB1RSTR_DMA1RST|RCC_AHB1RSTR_DMA2RST;
 RCC->AHB1RSTR&=~(RCC_AHB1RSTR_DMA1RST|RCC_AHB1RSTR_DMA2RST);
 CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
 cycles_last=DWT->CYCCNT;cycles_remainder=milliseconds=0;
 RCC->AHB1ENR|=RCC_AHB1ENR_GPIOAEN|RCC_AHB1ENR_GPIODEN|RCC_AHB1ENR_GPIOFEN|RCC_AHB1ENR_GPIOGEN|RCC_AHB1ENR_GPIOIEN;
 (void)RCC->AHB1ENR;Pin(GPIOA,15,0,0);Pin(GPIOG,13,0,0);Pin(GPIOI,6,0,0);Pin(GPIOD,12,0,0);
 __set_BASEPRI(0);__enable_irq();return 0;
}
uint32_t GateBoard_IgnOn(void){return !(GPIOG->IDR&(1U<<13));}
uint32_t GateBoard_Enter(void){return !(GPIOA->IDR&(1U<<15));}
uint32_t GateBoard_Keys(void){return (!(GPIOD->IDR&(1U<<12)))|(GateBoard_Enter()<<1)|((!(GPIOI->IDR&(1U<<6)))<<2);}
void GateBoard_UID(uint32_t uid[3]){for(uint32_t i=0;i<3;i++)uid[i]=*(const volatile uint32_t*)(UID_BASE+4*i);}
void GateBoard_Reset(void){__DSB();NVIC_SystemReset();for(;;){}}
/* Product has its own reset normalization. No peripheral transfer is live here. */
__attribute__((naked,noreturn)) void GateBoard_Jump(uint32_t vector __attribute__((unused)))
{__asm volatile("cpsid i\nldr r1, [r0]\nldr r2, [r0,#4]\nldr r3,=0xE000ED08\nstr r0,[r3]\nmovs r0,#0\nmsr control,r0\nmsr basepri,r0\nmsr msp,r1\ndsb\nisb\nbx r2");}
static void Select(uint32_t on){GPIOF->BSRR=on?(1U<<22):(1U<<6);}
static uint32_t Byte(uint8_t tx,uint8_t *rx)
{
 uint32_t n=100000U;while(!(SPI5->SR&SPI_SR_TXE)&&--n){}if(!n)return 1;
 *(volatile uint8_t*)&SPI5->DR=tx;n=100000U;
 while(!(SPI5->SR&SPI_SR_RXNE)&&--n){}if(!n)return 1;
 *rx=*(volatile uint8_t*)&SPI5->DR;return 0;
}
static uint32_t Status(uint8_t *v)
{uint8_t d;Select(1);uint32_t r=Byte(5,&d)||Byte(255,v);Select(0);return r;}
static uint32_t Ready(uint32_t timeout)
{uint32_t start=GateBoard_Millis();for(uint32_t n=0;n<2000000U;n++){uint8_t v;if(Status(&v))return 1;if(!(v&1))return 0;if(GateBoard_Millis()-start>timeout)return 1;}return 1;}
uint32_t GateBoard_NorInit(void)
{
 nor_first=nor_bytes=nor_ready=0;RCC->APB2ENR|=RCC_APB2ENR_SPI5EN;
 RCC->APB2RSTR|=RCC_APB2RSTR_SPI5RST;RCC->APB2RSTR&=~RCC_APB2RSTR_SPI5RST;
 Select(0);Pin(GPIOF,6,1,0);for(uint32_t i=7;i<=9;i++)Pin(GPIOF,i,2,5);
 SPI5->CR1=SPI_CR1_MSTR|SPI_CR1_SSM|SPI_CR1_SSI|SPI_CR1_BR_1|SPI_CR1_SPE;
 uint8_t d,id[3];Select(1);uint32_t r=Byte(0x9f,&d)||Byte(255,id)||Byte(255,id+1)||Byte(255,id+2);Select(0);
 if(r||id[0]!=0xc2||id[1]!=0x20||id[2]!=0x1b||Ready(1000)){return 1;}nor_ready=1;return 0;
}
static uint32_t Header(uint8_t op,uint32_t a)
{uint8_t d;return Byte(op,&d)||Byte(a>>24,&d)||Byte(a>>16,&d)||Byte(a>>8,&d)||Byte(a,&d);}
uint32_t GateBoard_RawRead(void *unused,uint32_t a,void *out,uint32_t n)
{(void)unused;uint8_t *p=out;if(!nor_ready||!p||!n||n>32768||a>=0x8000000U||n>0x8000000U-a)return 1;
 Select(1);uint32_t r=Header(0x13,a);for(uint32_t i=0;!r&&i<n;i++)r=Byte(255,p+i);Select(0);return r;}
uint32_t GateBoard_SetWriteRange(uint32_t first,uint32_t bytes)
{nor_bytes=0;if(!nor_ready||!bytes||((first|bytes)&4095U)||first>=0x8000000U||bytes>0x8000000U-first)return 1;
 /* Container ranges below safeend, or exact original APP staging only. */
#if NOODOE_UNINSTALL
 if(first>=0x1000U&&first<=0x8000U&&bytes==4096){nor_first=first;nor_bytes=bytes;return 0;}
#endif
 if(!((first>=0x9000U&&first<0x7f70000U&&bytes<=0x7f70000U-first)||(first==0x7f90000U&&bytes==0x70000U)))return 1;
 nor_first=first;nor_bytes=bytes;return 0;}
void GateBoard_LockNor(void){nor_bytes=0;}
static uint32_t Allowed(uint32_t a,uint32_t n)
{return nor_bytes&&a>=nor_first&&a-nor_first<nor_bytes&&n<=nor_bytes-(a-nor_first)&&*(const volatile uint32_t*)0x08008010U==0;}
static uint32_t WriteEnable(void)
{uint8_t d,v;if(Ready(1000))return 1;Select(1);uint32_t r=Byte(6,&d);Select(0);return r||Status(&v)||!(v&2);}
uint32_t GateBoard_NorErase(uint32_t a)
{if((a&4095)||!Allowed(a,4096)||WriteEnable())return 1;Select(1);uint32_t r=Header(0x21,a);Select(0);if(r||Ready(1000))return 1;
 uint8_t v[128];for(uint32_t i=0;i<4096;i+=128){if(GateBoard_RawRead(0,a+i,v,128))return 1;for(uint32_t j=0;j<128;j++)if(v[j]!=255)return 1;}return 0;}
uint32_t GateBoard_NorProgram(uint32_t a,const void *data,uint32_t n)
{const uint8_t *p=data;if(!p||!n||n>256||(a&255)+n>256||!Allowed(a,n)||WriteEnable())return 1;
 uint8_t d;Select(1);uint32_t r=Header(0x12,a);for(uint32_t i=0;!r&&i<n;i++)r=Byte(p[i],&d);Select(0);
 if(r||Ready(50)){return 1;}uint8_t v[256];return GateBoard_RawRead(0,a,v,n)||memcmp(v,p,n);}
#define ERRORS (FLASH_SR_OPERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGPERR|FLASH_SR_PGSERR|FLASH_SR_RDERR)
#define MODES (FLASH_CR_PG|FLASH_CR_SER|FLASH_CR_MER1|FLASH_CR_MER2|FLASH_CR_SNB|FLASH_CR_PSIZE|FLASH_CR_STRT)
static RAM uint32_t FlashWait(void)
{for(uint32_t n=0;n<40000000U;n++){uint32_t sr=FLASH->SR;if(!(sr&FLASH_SR_BSY))return !(sr&ERRORS);
 if(!(n&1023U)&&!BSP_Watchdog_RamCheckpoint())return 0;}return 0;}
static RAM void CacheFlush(void)
{uint32_t old=FLASH->ACR;FLASH->ACR=old&~(FLASH_ACR_ICEN|FLASH_ACR_DCEN);FLASH->ACR=(old&~(FLASH_ACR_ICEN|FLASH_ACR_DCEN))|FLASH_ACR_ICRST|FLASH_ACR_DCRST;
 FLASH->ACR=old&~(FLASH_ACR_ICRST|FLASH_ACR_DCRST);__DSB();__ISB();}
static RAM uint32_t FlashWorker(uint32_t sector,uint32_t address,const uint32_t *p,uint32_t words)
{
 if((FLASH->SR&FLASH_SR_BSY)||!(FLASH->CR&FLASH_CR_LOCK))return 1;
#if NOODOE_UNINSTALL
 /* This binary can program its append-only S4 journal, never erase S4,
  * rewrite entry vectors, original metadata or factory data through this API. */
 if(sector||address<0x08011000U||address>=0x08020000U||words>(0x08020000U-address)/4)return 1;
#else
 if(sector?sector<5||sector>7:address<GATE_PRODUCT_BASE||address>GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES-4*words)return 1;
#endif
 FLASH->KEYR=0x45670123;FLASH->KEYR=0xcdef89ab;if(FLASH->CR&FLASH_CR_LOCK)return 1;
 uint32_t ok=1;FLASH->SR=ERRORS|FLASH_SR_EOP;
 if(sector){FLASH->CR=(FLASH->CR&~MODES)|FLASH_CR_PSIZE_1|FLASH_CR_SER|(sector<<FLASH_CR_SNB_Pos);FLASH->CR|=FLASH_CR_STRT;ok=FlashWait();}
 else for(uint32_t i=0;i<words&&ok;i++){FLASH->CR=(FLASH->CR&~MODES)|FLASH_CR_PSIZE_1|FLASH_CR_PG;*(volatile uint32_t*)(address+4*i)=p[i];__DSB();ok=FlashWait();}
 /* Never fetch FLASH code or constants while BSY remains set. The bounded
  * lease has expired; stop feeding and let IWDG reset into the gate. */
 if(FLASH->SR&FLASH_SR_BSY){for(;;){__NOP();}}
 FLASH->CR=(FLASH->CR&~MODES)|FLASH_CR_LOCK;CacheFlush();return !ok;
}
static uint32_t FlashAction(uint32_t sector,uint32_t address,const void *p,uint32_t bytes)
{
 if((DBGMCU->IDCODE&4095)!=0x419||*(const volatile uint16_t*)0x1fff7a22!=512||
  ((RCC->APB1ENR&RCC_APB1ENR_WWDGEN)&&(WWDG->CR&WWDG_CR_WDGA)))return 1;
 if(!BSP_Watchdog_BeginFlash(GateBoard_Millis(),16000000))return 1;
 uint32_t irq=__get_PRIMASK();__disable_irq();uint32_t r=FlashWorker(sector,address,p,bytes/4);
 uint32_t w=BSP_Watchdog_EndFlash();__set_PRIMASK(irq);if(r||!w)return 1;
 if(p&&memcmp((const void*)address,p,bytes)){return 1;}return 0;
}
uint32_t GateBoard_FlashErase(uint32_t sector)
{if(sector<5||sector>7)return 1;if(FlashAction(sector,0,0,0))return 1;
 uint32_t address=GATE_PRODUCT_BASE+(sector-5)*0x20000U;for(uint32_t i=0;i<0x20000;i+=4)if(*(const volatile uint32_t*)(address+i)!=~0U)return 1;return 0;}
uint32_t GateBoard_FlashProgram(uint32_t a,const void *p,uint32_t n)
{if(!p||!n||n>4096||((a|n|(uintptr_t)p)&3U)||a<GATE_PRODUCT_BASE||a>=GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES||n>GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES-a)return 1;return FlashAction(0,a,p,n);}
uint32_t GateBoard_FlashRead(void *unused,uint32_t offset,void *p,uint32_t n)
{(void)unused;if(!p||!n||offset>=GATE_PRODUCT_BYTES||n>GATE_PRODUCT_BYTES-offset)return 1;memcpy(p,(const void*)(GATE_PRODUCT_BASE+offset),n);return 0;}

#if NOODOE_UNINSTALL
uint32_t GateBoard_JournalProgram(uint32_t off,const void *p,uint32_t n)
{if(!p||!n||n>256||((off|n|(uintptr_t)p)&3U)||off>=0xf000U||n>0xf000U-off)return 1;
 return FlashAction(0,0x08011000U+off,p,n);}
#endif
