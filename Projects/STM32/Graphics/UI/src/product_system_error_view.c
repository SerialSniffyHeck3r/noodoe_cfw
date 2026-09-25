#include "SystemError_View.h"
#include "bsp_eve_bus.h"
#include <stdio.h>
#include <string.h>
/* An error cannot allocate another object. This stack command block uses ROM
 * handles28/30; it is independent of the external glyph/texture cache. */
static void Word(uint8_t *b,uint32_t *n,uint32_t v)
{for(uint32_t i=0;i<4;++i)b[(*n)++]=(uint8_t)(v>>(8*i));}
static void Text(uint8_t *b,uint32_t *n,uint32_t y,uint32_t font,const char *s)
{
    Word(b,n,0xFFFFFF0C);Word(b,n,240U|(y<<16));Word(b,n,font|(1536U<<16));
    do{b[(*n)++]=(uint8_t)*s;}while(*s++);
    while(*n&3)b[(*n)++]=0;
}
uint32_t SystemErrorView_Draw(uint32_t code,uint32_t detail,uint32_t stage,uint32_t recovery)
{
    uint8_t reg[2];
    if(!g_bsp_eve.ready||BSP_EVE_BusRead(0x302574,reg,2)!=BSP_EVE_OK)return 0;
    if((reg[0]|((uint32_t)reg[1]<<8))!=0xFFC)return 0;
    if(BSP_EVE_BusRead(0x302054,reg,1)!=BSP_EVE_OK||reg[0])return 0;
    uint8_t data[384];uint32_t n=0;char line[64];
    Word(data,&n,0xFFFFFF00);Word(data,&n,0x02000000);Word(data,&n,0x26000007);Word(data,&n,0x04FFFFFF);
    Word(data,&n,0xFFFFFF3F);Word(data,&n,28);Word(data,&n,28);
    Word(data,&n,0xFFFFFF3F);Word(data,&n,30);Word(data,&n,30);
    /* Keep only the headline red; recovery details retain maximum contrast. */
    Word(data,&n,0x04FF4040);Text(data,&n,150,30,"SYSTEM ERROR");
    Word(data,&n,0x04FFFFFF);Text(data,&n,186,28,"aw shit :(");
    snprintf(line,sizeof(line),"Code %lu / Detail %lu",(unsigned long)code,(unsigned long)detail);Text(data,&n,230,28,line);
    snprintf(line,sizeof(line),"Boot stage %lu",(unsigned long)stage);Text(data,&n,264,28,line);
    Text(data,&n,309,28,recovery==2?"Verified - restart required":recovery==1?"Checking resources...":"Recovery interface available");
    Word(data,&n,0);Word(data,&n,0xFFFFFF01);
    const uint8_t prefix[3]={0xB0,0x25,0x78};
    BSP_EVE_Status s=BSP_EVE_BusSelect();if(s!=BSP_EVE_OK)return 0;
    s=BSP_EVE_BusSend(prefix,3);if(s==BSP_EVE_OK)s=BSP_EVE_BusSend(data,n);
    BSP_EVE_Status end=BSP_EVE_BusDeselect();return s==BSP_EVE_OK&&end==BSP_EVE_OK;
}
