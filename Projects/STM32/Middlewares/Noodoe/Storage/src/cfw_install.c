#include "Noodoe_Crc32.h"
#include "Cfw_Install.h"
#include "Cfw_Store.h"
#include "StorageDisk.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include "diskio.h"
#include <string.h>
volatile CfwInstallMailbox g_cfw_install;
typedef struct  {
    uint8_t payload[CFW_INSTALL_BYTES],metadata[0x9000],read[4096];
    uint32_t first[3],entry[3],seq,phase,file,offset,position,expected_crc,read_crc;
} Installer;
static Installer *w;
static const char names[3][12]= {
    "CFWCFG  DAT","CFWRIDE DAT","CFWPIC  DAT"
};
static uint32_t U16(const uint8_t *p) {
    uint16_t value;memcpy(&value,p,2);return value;
}
static void P16(uint8_t *p,uint32_t v) {
    p[0]=v;
    p[1]=v>>8;
}
static uint32_t Fat(uint32_t c) {
    uint32_t v=U16(w->metadata+4096+c+c/2);
    return c&1?v>>4:v&4095;
}
static void SetFat(uint32_t c,uint32_t n)
{
    for(uint32_t b=4096;b<=12288;b+=8192) {
        uint8_t *p=w->metadata+b+c+c/2;
        uint32_t v=U16(p);
        P16(p,c&1?(v&15)|(n<<4):(v&0xF000)|n);
    }
}
uint32_t CfwInstall_Busy(void) {
    return (w&&w->phase)||(g_cfw_install.sequence!=g_cfw_install.ack);
}
static void Done(uint32_t e)
{
    StorageDisk_ImportScope(0);
    BSP_NOR_LockStorage();
    g_cfw_install.error=e;
    g_cfw_install.state=e?3:2;
    w->phase=0;
    __DMB();
    g_cfw_install.ack=w->seq;
}
/* Complete preflight precedes the first erase. Runtime FAT graph must already
 * be clean. Creation refuses every existing same-name file, even a valid one.
 * No installation retry can silently replace previously committed containers. */
static uint32_t Plan(void)
{
    if(CfwFiles_Status()!=CFW_OK)return CFW_CORRUPT;
    for(uint32_t f=0;f<3;++f)if(CfwFiles_Present(f))return CFW_BUSY;
    if(disk_read(0,w->metadata,0,9)!=RES_OK)return CFW_IO;
    if(Cfw_Crc(w->metadata,sizeof(w->metadata))!=g_cfw_install.metadata_crc||
    Cfw_Crc(w->payload,sizeof(w->payload))!=g_cfw_install.payload_crc)return CFW_CORRUPT;
    uint32_t payload=0,end_entry=512;
    for(uint32_t i=0;i<512;++i)if(!w->metadata[0x5000+i*32]) {
        end_entry=i;
        break;
    }
    for(uint32_t f=0;f<3;++f) {
        uint32_t first=w->first[f],entry=w->entry[f],count=CfwFiles_Size(f)/32768;
        if(first<2||first+count>4080||0x9000U+(first-2+count)*32768U>CFW_SAFE_END||entry>=512||entry>end_entry+f)return CFW_ARGUMENT;
        uint8_t *r=w->metadata+0x5000+entry*32;
        if(r[0]&&r[0]!=0xE5)return CFW_BUSY;
        if(CfwRecord_Check(w->payload+payload,f+1))return CFW_CORRUPT;
        for(uint32_t j=0;j<count;++j) {
            if(Fat(first+j))return CFW_BUSY;
            SetFat(first+j,j+1==count?0xFFF:first+j+1);
        }
        memset(r,0,32);
        memcpy(r,names[f],11);
        r[11]=0x20;
        P16(r+26,first);
        Cfw_Put32(r+28,CfwFiles_Size(f));
        payload+=CfwFiles_Size(f);
    }
    return CFW_OK;
}
/* Write exactly one sector then physically re-read it through the logical
 * adapter. The host stores every before/after sector for interruption recovery. */
static uint32_t Write(uint32_t sector,const uint8_t *data)
{
    return disk_write(0,data,sector,1)==RES_OK&&disk_read(0,w->read,sector,1)==RES_OK&&!memcmp(w->read,data,4096);
}
static void HashRead(void)
{w->read_crc=Noodoe_Crc32Feed(w->read_crc,w->read,4096);}
void CfwInstall_Process(void)
{
    if(!w) {
        w=BSP_RAM_AllocateNamed(BSP_RAM_CFW_INSTALL,sizeof(*w));
        if(!w)return;
        memset(w,0,sizeof(*w));
        g_cfw_install.magic=0x31494643;
        g_cfw_install.version=1;
        g_cfw_install.buffer=(uint32_t)w->payload;
        g_cfw_install.capacity=CFW_INSTALL_BYTES;
    }
    if(!w->phase) {
        uint32_t seq=g_cfw_install.sequence;
        if(!seq||seq==g_cfw_install.ack)return;
        __DMB();
        w->seq=seq;
        g_cfw_install.state=1;
        g_cfw_install.error=0;
        g_cfw_install.progress=0;
        if(g_cfw_quiesce.request) {
            Done(CFW_BUSY);
            return;
        }
        w->expected_crc=g_cfw_install.payload_crc;
        for(uint32_t f=0;f<3;++f) {
            w->first[f]=g_cfw_install.first[f];
            w->entry[f]=g_cfw_install.entry[f];
        }
        if(g_cfw_install.token!=0x42414B32||!g_bsp_power.ign_valid||!g_bsp_power.ign_on||!StorageDisk_IsStock()) {
            Done(CFW_ARGUMENT);
            return;
        }
        uint32_t e=Plan();
        if(e) {
            Done(e);
            return;
        }
        BSP_NOR_ClearContainers();
        if(BSP_NOR_UnlockStorage(0x42414B32)!=BSP_NOR_OK) {
            Done(CFW_IO);
            return;
        }
        StorageDisk_ImportScope(1);
        w->file=w->offset=w->position=0;
        w->phase=1;
        return;
    }
    if(w->phase==1) {
        uint32_t sector=(0x9000+(w->first[w->file]-2)*32768+w->offset)/4096;
        if(!Write(sector,w->payload+w->position)) {
            Done(CFW_IO);
            return;
        }
        w->offset+=4096;
        w->position+=4096;
        g_cfw_install.progress=w->position;
        if(w->offset==CfwFiles_Size(w->file)) {
            w->offset=0;
            if(++w->file==3) {
                w->phase=2;
                w->file=0;
                w->read_crc=0xFFFFFFFFU;
            }
        }
        return;
    }
    /* Re-read the complete physical payload before publishing FAT ownership.
         * Per-page comparison alone could accept a host-mutated SDRAM source. */
    if(w->phase==2) {
        uint32_t sector=(0x9000+(w->first[w->file]-2)*32768+w->offset)/4096;
        if(disk_read(0,w->read,sector,1)!=RES_OK) {
            Done(CFW_IO);
            return;
        }
        HashRead();
        w->offset+=4096;
        if(w->offset==CfwFiles_Size(w->file)) {
            w->offset=0;
            if(++w->file==3) {
                if((w->read_crc^0xFFFFFFFFU)!=w->expected_crc) {
                    Done(CFW_CORRUPT);
                    return;
                }
                w->phase=3;
                w->position=1;
            }
        }
        return;
    }
    /* Publish both FAT copies before the directory. A power failure here is
         * explicitly non-atomic and requires the host's before/after journal. */
    if(w->phase==3) {
        if(disk_read(0,w->read,w->position,1)!=RES_OK) {
            Done(CFW_IO);
            return;
        }
        if(memcmp(w->read,w->metadata+w->position*4096,4096)&&
        !Write(w->position,w->metadata+w->position*4096)) {
            Done(CFW_IO);
            return;
        }
        if(++w->position==9)Done(0);
    }
}
