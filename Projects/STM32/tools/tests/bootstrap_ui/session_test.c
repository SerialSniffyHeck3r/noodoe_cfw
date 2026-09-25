#include "InstallSession.h"
#include "gate_menu.h"
#include "eve_rom_text.h"
#include <string.h>
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
int TestSession(void){
 InstallSession s={0};uint32_t w[20];InstallSession_Begin(&s,100,42);
 InstallSession_Button(&s,100,1);InstallSession_Button(&s,9999,1);CHECK(!s.cancel_requested);
 InstallSession_Button(&s,10000,0);InstallSession_Button(&s,10001,1);InstallSession_Button(&s,13000,1);CHECK(!s.cancel_requested);
 InstallSession_Button(&s,13001,1);CHECK(s.cancel_requested);
 InstallSession_Observe(&s,13002,INSTALL_TRANSFER,1,0,1,4096,393216,4096,0,0);
 CHECK(s.state==INSTALL_CANCELLING&&InstallSession_Active(&s));
 InstallSession_Observe(&s,13003,INSTALL_COMMIT,1,0,1,393216,393216,393216,0,0);
 CHECK(s.state==INSTALL_COMMIT);InstallSession_Snapshot(&s,w);CHECK(!w[17]&&w[5]==5&&w[13]==96);
 InstallSession_End(&s,INSTALL_UNKNOWN);CHECK(InstallSession_Active(&s));InstallSession_Snapshot(&s,w);CHECK(!w[17]);
 InstallSession_End(&s,INSTALL_CANCELLED);CHECK(!InstallSession_Active(&s));return 0;
}
int TestGateMenu(void){
 GateMenu m;GateMenu_Init(&m,2,0);CHECK(!GateMenu_Process(&m,2,5000)&&m.page==0);
 GateMenu_Process(&m,0,5100);GateMenu_Process(&m,2,5200);CHECK(m.page==1);
 CHECK(!GateMenu_Process(&m,2,9000));GateMenu_Process(&m,0,9100);
 CHECK(!GateMenu_Process(&m,2,9200));CHECK(!GateMenu_Process(&m,2,11199));CHECK(GateMenu_Process(&m,2,11200));
 CHECK(!GateMenu_Process(&m,2,12000));GateMenu_Process(&m,4,12100);CHECK(!m.page);
 GateMenu_Process(&m,0,12200);GateMenu_Process(&m,1,12300);CHECK(m.selection==2);
 GateMenu_Process(&m,0,12400);GateMenu_Process(&m,2,12500);CHECK(m.page==3);
 GateMenu_Process(&m,0,12600);GateMenu_Process(&m,4,12700);CHECK(m.help==1);
 GateMenu_Process(&m,0,12800);GateMenu_Process(&m,2,12900);CHECK(m.page==0);return 0;
}
static uint32_t RomRead(uint32_t a,void *p,uint32_t n){
 memset(p,0,n);if(a==0x2ffffc){*(uint32_t*)p=0x100000;return 0;}
 if(n!=148)return 1;memset(p,10,128);uint32_t f[]={2,16,16,20,0x200000};memcpy((uint8_t*)p+128,f,20);return 0;
}
int TestRomScale(void){
 EveRomText s={0};uint8_t b[1024];uint32_t n=0;CHECK(EveRomLoad(&s,RomRead,28));
 CHECK(EveRomDraw(&s,b,&n,sizeof(b),200,28,"AB"));
 uint32_t found=0;for(uint32_t i=0;i<n;i+=4){uint32_t w=EveRomWord(b+i);if((w&0xc0000000)==0x80000000){
   CHECK(((w>>21)&511)==229+found*11);CHECK(((w>>12)&511)==189);CHECK((w&127)==65+found);found++;
 }}CHECK(found==2);n=1000;CHECK(!EveRomDraw(&s,b,&n,1024,200,28,"AB")&&n==1000);
 CHECK(!EveRomDraw(&s,b,&n,1024,200,28,"\001"));return 0;
}
