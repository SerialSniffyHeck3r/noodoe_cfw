#include "gate_board.h"
#include "gate_menu.h"
#include "MaintenanceCopy.h"
#include "eve_rom_text.h"
#include "bsp_board_revision.h"
#include "Recovery_Target.h"
#include "stm32f4xx.h"
/* Minimal board-specific recovery display. EVE ROM font28 only, no font asset,
 * LVGL, allocator, SDRAM, DMA or RTOS. Failure leaves recovery controls usable. */
static uint32_t ready;
static EveRomText fonts;
static uint8_t commands[2048];
__attribute__((weak)) volatile uint32_t g_gate_log_error;
volatile GateDisplayDiagnostics g_gate_display;
static uint32_t Fail(void){g_gate_display.error=g_gate_display.stage;ready=0;g_gate_display.ready=0;return 1;}
static void Delay(uint32_t ms){uint32_t start=GateBoard_Millis();while(GateBoard_Millis()-start<ms){}}
static void Pin(GPIO_TypeDef *g,uint32_t p,uint32_t mode,uint32_t af)
{g->MODER=(g->MODER&~(3U<<(p*2)))|(mode<<(p*2));g->OTYPER&=~(1U<<p);g->OSPEEDR|=3U<<(p*2);g->PUPDR&=~(3U<<(p*2));g->AFR[p/8]=(g->AFR[p/8]&~(15U<<((p%8)*4)))|(af<<((p%8)*4));}
static uint32_t Transfer(SPI_TypeDef *spi,uint32_t value,uint32_t wide,uint32_t *answer)
{uint32_t n=10000;while(!(spi->SR&SPI_SR_TXE)&&--n){}if(!n)return 1;
 if(wide)spi->DR=value;else *(volatile uint8_t*)&spi->DR=value;
 n=10000;while(!(spi->SR&SPI_SR_RXNE)&&--n){}if(!n)return 1;
 *answer=wide?(spi->DR&0xffffU):*(volatile uint8_t*)&spi->DR;return 0;}
static uint32_t Eve(uint32_t address,void *data,uint32_t n,uint32_t write)
{uint8_t *p=data;uint32_t d,r;GPIOA->BSRR=1U<<20;
 r=Transfer(SPI1,(address>>16)|(write?128:0),0,&d)||Transfer(SPI1,address>>8,0,&d)||Transfer(SPI1,address,0,&d);
 if(!write&&!r)r=Transfer(SPI1,0,0,&d);
 for(uint32_t i=0;!r&&i<n;i++){r=Transfer(SPI1,write?p[i]:0,0,&d);if(!write)p[i]=d;}
 GPIOA->BSRR=1U<<4;return r;}
static uint32_t Reg(uint32_t address,uint32_t value,uint32_t bytes){return Eve(address,&value,bytes,1);}
static uint32_t ReadRom(uint32_t a,void *b,uint32_t n){return Eve(a,b,n,0);}
/* EVE ROM startup is asynchronous. Match the proven BSP's bounded readiness
 * poll, not a single sample at 300ms. Record the actual failing register. */
static uint32_t WaitByte(uint32_t address,uint32_t expected,uint32_t timeout)
{uint32_t start=GateBoard_Millis();g_gate_display.address=address;
 do{uint32_t value=0;if(Eve(address,&value,1,0))return 1;g_gate_display.value=value;
  if(value==expected)return 0;
 }while(GateBoard_Millis()-start<timeout);return 1;}
static uint32_t Host(uint32_t cmd)
{uint32_t d;GPIOA->BSRR=1U<<20;uint32_t r=Transfer(SPI1,cmd,0,&d)||Transfer(SPI1,0,0,&d)||Transfer(SPI1,0,0,&d);GPIOA->BSRR=1U<<4;return r;}
static uint32_t PanelWord(uint32_t value)
{uint32_t d;GPIOE->BSRR=1U<<4;Delay(2);GPIOE->BSRR=1U<<20;Delay(2);if(Transfer(SPI4,value,1,&d))return 1;
 Delay(2);GPIOE->BSRR=1U<<4;Delay(2);GPIOE->BSRR=1U<<20;return 0;}
static uint32_t PanelCommand(uint32_t cmd,uint32_t op){return PanelWord(0x2000|(cmd>>8))||PanelWord(cmd&255)||PanelWord(op);}
uint32_t GateBoard_DisplayInit(void)
{ready=0;g_gate_display=(GateDisplayDiagnostics){.magic=0x47444931,.stage=1};
 if(!RECOVERY_PANEL_SUPPORTED(*(const volatile uint16_t*)0x0800c080)||BSP_BoardRevision()<3)return Fail();
 g_gate_display.stage=2;
 RCC->AHB1ENR|=RCC_AHB1ENR_GPIOAEN|RCC_AHB1ENR_GPIOBEN|RCC_AHB1ENR_GPIOCEN|RCC_AHB1ENR_GPIOEEN|RCC_AHB1ENR_GPIOIEN;
 RCC->APB2ENR|=RCC_APB2ENR_SPI1EN|RCC_APB2ENR_SPI4EN;RCC->APB1ENR|=RCC_APB1ENR_TIM5EN;(void)RCC->APB2ENR;
 RCC->APB2RSTR|=RCC_APB2RSTR_SPI1RST|RCC_APB2RSTR_SPI4RST;RCC->APB2RSTR&=~(RCC_APB2RSTR_SPI1RST|RCC_APB2RSTR_SPI4RST);
 GPIOA->BSRR=1U<<4;GPIOB->BSRR=1U<<17;GPIOC->BSRR=(1U<<29)|(1U<<8);GPIOI->BSRR=1U<<11;GPIOE->BSRR=1U<<20;
 Pin(GPIOA,4,1,0);Pin(GPIOB,1,1,0);Pin(GPIOC,13,1,0);Pin(GPIOC,8,1,0);Pin(GPIOI,11,1,0);Pin(GPIOE,4,1,0);
 Pin(GPIOA,6,2,5);Pin(GPIOB,3,2,5);Pin(GPIOB,5,2,5);Pin(GPIOE,2,2,5);Pin(GPIOE,5,2,5);Pin(GPIOE,6,2,5);
 SPI1->CR1=SPI_CR1_MSTR|SPI_CR1_SSM|SPI_CR1_SSI|SPI_CR1_BR_1|SPI_CR1_SPE;
 SPI4->CR1=SPI_CR1_MSTR|SPI_CR1_SSM|SPI_CR1_SSI|SPI_CR1_BR_1|SPI_CR1_DFF|SPI_CR1_SPE;
 Delay(20);GPIOB->BSRR=1U<<1;Delay(20);if(Host(0x44)||Host(0x62)||Host(0))return Fail();Delay(300);
 g_gate_display.stage=3;if(WaitByte(0x302000,0x7c,500))return Fail();
 g_gate_display.stage=4;if(WaitByte(0x302020,0,500))return Fail();
 g_gate_display.stage=5;
 const uint16_t timing[10]={550,37,480,0,4,505,18,480,0,2};
 if(Reg(0x302070,0,1)){return Fail();}for(uint32_t i=0;i<10;i++)if(Reg(0x30202c+i*4,timing[i],2))return Fail();
 uint32_t gpio=0;const uint32_t blank[3]={0x02000000,0x26000007,0};
 if(Eve(0x302094,&gpio,1,0)||Reg(0x302094,gpio&~128U,1)||Eve(0x300000,(void*)blank,sizeof(blank),1)||Reg(0x302054,2,1))return Fail();
 if(Reg(0x302060,1,1)||Reg(0x302064,0,1)||Reg(0x302068,0,1)||Reg(0x30206c,0,1)||Reg(0x302070,3,1))return Fail();
 g_gate_display.stage=6;if(WaitByte(0x302054,0,250))return Fail();
 g_gate_display.stage=7;
 GPIOI->BSRR=1U<<27;Delay(10);GPIOC->BSRR=1U<<13;Delay(10);GPIOC->BSRR=1U<<29;Delay(20);GPIOC->BSRR=1U<<13;Delay(50);
 if(PanelCommand(0x1100,0x4000)){return Fail();}Delay(120);if(PanelCommand(0x2900,0x4000)||PanelCommand(0x0a00,0xc000))return Fail();
 g_gate_display.stage=8;uint32_t mode=0;if(Transfer(SPI4,0xa5a5,1,&mode))return Fail();
 g_gate_display.value=mode;if(mode!=0x9c)return Fail();
 g_gate_display.stage=9;
 Pin(GPIOI,0,2,2);Pin(GPIOI,8,1,0);TIM5->CR1=0;TIM5->PSC=319;TIM5->ARR=99;TIM5->CCR4=25;TIM5->CCMR2=(6U<<12)|(1U<<11);TIM5->CCER=TIM_CCER_CC4E;TIM5->EGR=TIM_EGR_UG;TIM5->CR1=TIM_CR1_CEN;
 GPIOI->BSRR=1U<<8;GPIOC->BSRR=1U<<24;ready=1;g_gate_display.ready=1;return 0;}
static void Word(uint8_t *p,uint32_t *n,uint32_t value){for(uint32_t i=0;i<4;i++)p[(*n)++]=value>>(8*i);}
static void Text(uint8_t *p,uint32_t *n,uint32_t y,const char *s)
{if(EveRomDraw(&fonts,p,n,sizeof(commands),y,28,s))return;
 Word(p,n,0xffffff0c);Word(p,n,240|(y<<16));Word(p,n,28|(1536U<<16));do{p[(*n)++]=*s;}while(*s++);while(*n&3)p[(*n)++]=0;}
static void Number(char *out,uint32_t value)
{char rev[10];uint32_t n=0;do{rev[n++]='0'+value%10;value/=10;}while(value);while(n)*out++=rev[--n];*out=0;}
/* Fixed diagnostics need no printf/newlib heap in the independent Gate. */
static char *Copy(char *p,const char *s){while(*s)*p++=*s++;*p=0;return p;}
static void Pair(char *p,const char *a,uint32_t x,const char *b,uint32_t y)
{p=Copy(p,a);Number(p,x);while(*p)++p;p=Copy(p,b);Number(p,y);}
void GateBoard_Display(uint32_t state,uint32_t percent,uint32_t error)
{if(!ready)return;uint32_t free=0,swap=0;if(Eve(0x302574,&free,2,0)||free!=0xffc||Eve(0x302054,&swap,1,0)||swap){++g_gate_display.skipped;return;}
 (void)EveRomLoad(&fonts,ReadRom,28);
 uint8_t *data=commands;uint32_t n=0;char value[16];Word(data,&n,0xffffff00);Word(data,&n,0x02000000);Word(data,&n,0x26000007);
 Word(data,&n,0xffffff3f);Word(data,&n,28);Word(data,&n,28);Word(data,&n,error?0x04ff4040:0x04ffffff);
 Text(data,&n,130,error?"SYSTEM ERROR":state==6||state==7?"BACK TO STOCK":COPY_INSTALLER_TITLE);Word(data,&n,0x04ffffff);
 if(state==6||state==7){Text(data,&n,185,error?COPY_FATAL:"Time to get back to stock.");
  if(error){Number(value,error);Text(data,&n,220,value);}
  if(state==7){Text(data,&n,265,"Release O, then hold for 2 sec.");Number(value,percent);Text(data,&n,300,value);}
  else{Text(data,&n,265,"Key OFF. Hold O for 1 sec.");Text(data,&n,300,"Key ON; hold O for 2 sec.");}
  Text(data,&n,340,"Restore original V5.16.");}
 else{const char *title=state==1?"Checking storage":state==2?"Saving boot record":state==3?"Checking resources":state==4?"Checking / installing CFW":state==5?"Restoring original V5.16":state==8?"Checking the original image":state==9?"Writing NOR staging":state==10?"Verifying NOR staging":state==11?"Starting the stock installer":"Starting firmware";
  Text(data,&n,210,title);Number(value,percent);Text(data,&n,265,value);Text(data,&n,320,"Please keep main power on.");}
 if(g_gate_log_error){Word(data,&n,0x04ffb040);Text(data,&n,385,"Log unavailable. Recovery still works.");}
 Word(data,&n,0);Word(data,&n,0xffffff01);if(Eve(0x302578,data,n,1)){g_gate_display.stage=10;(void)Fail();}else ++g_gate_display.frames;}

/* Same ROM-only FIFO contract as early recovery. Drawing failure never
 * changes menu approval, watchdog progression, or the recovery state. */
void GateBoard_Menu(const GateMenu *m,const GateMenuFacts *v)
{
 if(!ready)return;
 uint32_t free=0,swap=0;
 if(Eve(0x302574,&free,2,0)||free!=0xffc||Eve(0x302054,&swap,1,0)||swap)return;
 (void)EveRomLoad(&fonts,ReadRom,28);
 uint8_t *b=commands;uint32_t n=0;char detail[80];
 Word(b,&n,0xffffff00);Word(b,&n,0x02000000);Word(b,&n,0x26000007);
 Word(b,&n,0xffffff3f);Word(b,&n,28);Word(b,&n,28);Word(b,&n,0x04ffffff);
 Text(b,&n,88,COPY_RECOVERY);Text(b,&n,128,COPY_RESCUE);
 if(!m->page){
  Text(b,&n,181,"Stock firmware: V5.16");
  Text(b,&n,216,v->verified?"Recovery image: Verified":"Recovery image: Not verified");
  Text(b,&n,273,m->selection==0?"> Back to stock <":"Back to stock");
  Text(b,&n,312,m->selection==1?"> Details <":"Details");
  Text(b,&n,351,m->selection==2?"> Help <":"Help");
 }else if(m->page==1){
  Text(b,&n,196,"Release O, then hold for 2 sec.");
  Text(b,&n,238,"No extra key cycle needed.");
  Pair(detail,"Hold: ",m->hold_ms>=2000?100:m->hold_ms/20," / ",100);Text(b,&n,285,detail);
  Text(b,&n,334,"UP / DOWN: back");
 }else if(m->page==2){
  Pair(detail,"Reason ",v->reason," / Code ",v->code);Text(b,&n,185,detail);
  Copy(detail,"Reset cause (decimal): ");Number(detail+23,v->reset);Text(b,&n,224,detail);
  Pair(detail,"Fault ",v->fault," / Build ",v->failed);Text(b,&n,263,detail);
  Copy(detail,"Log status: ");Number(detail+12,v->log);Text(b,&n,310,detail);Text(b,&n,350,"O: back");
 }else{
  static const char *const a[]={"Recovery works without Bluetooth.","Restore checks the complete image.","Keep main power during writes.","If restoration fails, save the code."};
  static const char *const c[]={"UP/DOWN: menu. O: choose.","Then stages it for the stock BL.","IGN OFF does not cut main power.","Phone logs help after reconnecting."};
  Text(b,&n,198,a[m->help]);Text(b,&n,242,c[m->help]);Text(b,&n,322,"UP/DOWN: scroll. O: back.");
 }
 if(v->code){Word(b,&n,0x04ff4040);Text(b,&n,389,COPY_ERROR);}
 Word(b,&n,0);Word(b,&n,0xffffff01);
 if(Eve(0x302578,b,n,1)){g_gate_display.stage=10;(void)Fail();}else ++g_gate_display.frames;
}

#if NOODOE_UNINSTALL
/* Shared EVE transport/ROM text; uninstall has no resource or GPU bitmap dependency. */
void GateBoard_Uninstall(uint32_t state,uint32_t percent,uint32_t bytes,uint32_t error)
{
 if(!ready)return;
 uint32_t free=0,swap=0;
 if(Eve(0x302574,&free,2,0)||free!=0xffc||Eve(0x302054,&swap,1,0)||swap)return;
 (void)EveRomLoad(&fonts,ReadRom,28);
 uint8_t *b=commands;uint32_t n=0;char detail[80];
 Word(b,&n,0xffffff00);Word(b,&n,0x02000000);Word(b,&n,0x26000007);
 Word(b,&n,0xffffff3f);Word(b,&n,28);Word(b,&n,28);Word(b,&n,error?0x04ff4040:0x04ffffff);
 Text(b,&n,96,error?"SYSTEM ERROR":"BACK TO STOCK");Word(b,&n,0x04ffffff);
 const char *title=state==0?"Checking before cleanup":state==1?"Keep CFW data":state==2?"Erase CFW data":
  state==3?"Erasing CFW contents":state==4?"Returning free space":state==5?"Preparing original V5.16":
  state==6?"Verifying original V5.16":state==7?"Starting the stock installer":state==10?"Saving the cleanup plan":"Cleanup needs a retry";
 Text(b,&n,155,title);
 if(state==1||state==2){Text(b,&n,203,"Stock files and factory data stay.");
  Pair(detail,"CFW data: ",bytes/1024," KiB / ",percent>100?100:percent);Text(b,&n,245,detail);
  Text(b,&n,290,"UP / DOWN to choose");Text(b,&n,329,"Release O, then hold for 2 sec.");}
 else if(state==9){Text(b,&n,207,"Cleanup blocked. Data unchanged.");
  Pair(detail,"Help code: ",error," / stage ",state);Text(b,&n,250,detail);
  Text(b,&n,301,"Keep data and return to stock:");Text(b,&n,340,"Release O, then hold for 2 sec.");}
 else if(error){Text(b,&n,207,"Well, shit. Cleanup stopped.");Pair(detail,"Help code: ",error," / stage ",state);Text(b,&n,250,detail);
  Text(b,&n,301,"Save this code before retrying.");Text(b,&n,340,"Release O; hold 2 sec to retry.");}
 else {Pair(detail,"Progress: ",percent>100?100:percent,"% / CFW KiB: ",bytes/1024);Text(b,&n,247,detail);
  Text(b,&n,301,"Working on the device.");Text(b,&n,340,"Please keep main power on.");}
 Word(b,&n,0);Word(b,&n,0xffffff01);if(Eve(0x302578,b,n,1)){g_gate_display.stage=10;(void)Fail();}else ++g_gate_display.frames;
}
#endif
