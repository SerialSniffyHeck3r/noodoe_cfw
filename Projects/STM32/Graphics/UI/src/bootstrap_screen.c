#if NOODOE_BOOTSTRAP || NOODOE_DIAGNOSTIC
#include "Bootstrap_Screen.h"
#include "bsp_eve_bus.h"
#include "eve_rom_text.h"
#include <stdio.h>
static EveRomText fonts;
static uint8_t commands[2048];
static uint32_t ReadRom(uint32_t a,void *b,uint32_t n){return BSP_EVE_BusRead(a,b,n);}
static void Word(uint8_t *b,uint32_t *n,uint32_t v)
{for(uint32_t i=0;i<4;i++)b[(*n)++]=(uint8_t)(v>>(i*8));}
static void Text(uint8_t *b,uint32_t *n,uint32_t y,uint32_t font,const char *s)
{if(EveRomDraw(&fonts,b,n,sizeof(commands),y,font,s))return;
 Word(b,n,0xffffff0c);Word(b,n,240U|(y<<16));Word(b,n,font|(1536U<<16));
 do{b[(*n)++]=(uint8_t)*s;}while(*s++);while(*n&3U)b[(*n)++]=0;}
uint32_t BootstrapScreen_Draw(const BootstrapView *v)
{
 uint8_t reg[2],*b=commands;uint32_t n=0;
 /* Frame ownership is acquired only when FIFO is empty and prior swap done. */
 if(!g_bsp_eve.ready||BSP_EVE_BusRead(0x302574,reg,2))return 2;
 if((reg[0]|((uint32_t)reg[1]<<8))!=0xffc)return 1;
 if(BSP_EVE_BusRead(0x302054,reg,1))return 2;
 if(reg[0])return 1;
 for(uint32_t font=26;font<=28;font++)(void)EveRomLoad(&fonts,ReadRom,font);
 Word(b,&n,0xffffff00);Word(b,&n,0x02000000);Word(b,&n,0x26000007);
 for(uint32_t font=26;font<=28;font++){Word(b,&n,0xffffff3f);Word(b,&n,font);Word(b,&n,font);}
 Word(b,&n,v->error?0x04ff4040:0x04ffffff);Text(b,&n,v->welcome?208:102,28,v->title);
 Word(b,&n,0x04ffffff);Text(b,&n,v->welcome?258:162,27,v->line1);Text(b,&n,200,26,v->line2);
 if(v->line3)Text(b,&n,238,26,v->line3);
 if(v->has_overall){
  Word(b,&n,0x04303030);Word(b,&n,0x1f000009);Word(b,&n,0x40000000|(96U*16U<<15)|259U*16U);Word(b,&n,0x40000000|(384U*16U<<15)|267U*16U);Word(b,&n,0x21000000);
  if(v->overall){Word(b,&n,0x0475c593);Word(b,&n,0x1f000009);Word(b,&n,0x40000000|(96U*16U<<15)|259U*16U);Word(b,&n,0x40000000|((96U+288U*v->overall/100U)*16U<<15)|267U*16U);Word(b,&n,0x21000000);}
 }
 if(v->percent<=100){
  char percent[56];snprintf(percent,sizeof(percent),"Overall %lu%% / Step %lu%%",(unsigned long)v->overall,(unsigned long)v->percent);
  Word(b,&n,0x04303030);Word(b,&n,0x1f000009);Word(b,&n,0x40000000|(96U*16U<<15)|280U*16U);Word(b,&n,0x40000000|(384U*16U<<15)|288U*16U);Word(b,&n,0x21000000);
  if(v->percent){Word(b,&n,0x044da6ff);Word(b,&n,0x1f000009);Word(b,&n,0x40000000|(96U*16U<<15)|280U*16U);Word(b,&n,0x40000000|((96U+288U*v->percent/100U)*16U<<15)|288U*16U);Word(b,&n,0x21000000);}
  Word(b,&n,0x04ffffff);Text(b,&n,315,28,percent);
 }
 Text(b,&n,363,26,v->hint);
 if(v->error){char code[28];snprintf(code,sizeof(code),"Help code: %08lX",(unsigned long)v->error);Text(b,&n,305,26,code);}
 if(v->notice){Word(b,&n,0x04ffc857);Text(b,&n,397,26,v->notice);}
 Word(b,&n,0);Word(b,&n,0xffffff01);const uint8_t prefix[3]={0xb0,0x25,0x78};
 if(BSP_EVE_BusSelect())return 2;
 uint32_t error=BSP_EVE_BusSend(prefix,3);if(!error)error=BSP_EVE_BusSend(b,n);
 uint32_t release=BSP_EVE_BusDeselect();return error||release?2:0;
}
#endif
