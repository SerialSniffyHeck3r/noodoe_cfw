#include "Ui_Theme.h"
#include "Fuel_Policy.h"
#include "Phone_Indicators.h"
#include "ScreenWarningOverlay.h"
#include "RadioSelfTest.h"
#include "Ui_OffStages.h"
#include "Noodoe_Crc32.h"
#include <string.h>
unsigned assertions;
#define C(x) do{++assertions;if(!(x))return __LINE__;}while(0)
unsigned Test(void){
 UiThemeState theme={0};UiThemeConfig c={2,0,420,1140,100,300};
 C(!UiTheme_Step(&theme,&c,0,0,0,0,500000));
 C(!UiTheme_Step(&theme,&c,100,0,0,1,300000));
 C(!UiTheme_Step(&theme,&c,3099,0,0,1,300000));
 C(UiTheme_Step(&theme,&c,3100,0,0,1,300000)==1);
 C(UiTheme_Step(&theme,&c,4000,0,0,1,200000)==1);
 C(UiTheme_Step(&theme,&c,5000,0,0,0,0)==1);
 C(UiTheme_Step(&theme,&c,6000,0,0,1,100000)==1);
 C(!UiTheme_Step(&theme,&c,9000,0,0,1,100000));
 c.source=1;C(UiTheme_Step(&theme,&c,10000,1,420,0,0)==1);
 C(!UiTheme_Step(&theme,&c,10001,1,1140,0,0));
 c.day_minute=1200;c.night_minute=300;
 C(UiTheme_Step(&theme,&c,10002,1,20,0,0)==1);
 C(!UiTheme_Step(&theme,&c,10003,1,600,0,0));
 c.mode=1;C(UiTheme_Step(&theme,&c,10004,0,0,0,0)==1);
 c.mode=0;C(!UiTheme_Step(&theme,&c,10005,0,0,0,0));
 FuelPolicy fuel={0};uint32_t result;
 C(!FuelPolicy_Step(&fuel,1,0,1,0,0x51,0));
 C(!FuelPolicy_Step(&fuel,1,500,1,500,0x51,0));
 C(FuelPolicy_Step(&fuel,1,1000,1,1000,0x51,0)==FUEL_LOW);
 C(!FuelPolicy_Step(&fuel,1,1500,1,1500,0x51,0));
 C(!FuelPolicy_Step(&fuel,1,2000,1,2000,0x50,0));
 result=FuelPolicy_Step(&fuel,1,3000,1,3000,0x50,0);
 C(result==(FUEL_CRITICAL|FUEL_START_RESERVE));
 C(!FuelPolicy_Step(&fuel,1,4000,0,4000,0,1));
 C(!FuelPolicy_Step(&fuel,1,4100,1,4100,0x52,1));
 C(!FuelPolicy_Step(&fuel,1,5100,1,5100,0x52,1));
 C(!FuelPolicy_Step(&fuel,1,6100,1,6100,0x52,1));
 C(FuelPolicy_Step(&fuel,1,7100,1,7100,0x52,1)==FUEL_CLEAR_RESERVE);
 C(!FuelPolicy_Step(&fuel,2,8000,1,8000,0,0));
 C(FuelPolicy_Step(&fuel,2,9000,1,9000,0,0)==FUEL_ERROR);
 C(!FuelPolicy_Step(&fuel,2,12000,1,9000,0x50,0));
 // New session and wraparound debounce use unsigned intervals.
 C(!FuelPolicy_Step(&fuel,3,0xffffff00,1,0xffffff00,0x50,0));
 C(FuelPolicy_Step(&fuel,3,744,1,744,0x50,0)==(FUEL_CRITICAL|FUEL_START_RESERVE));
 ScreenWarningState w;
 ScreenWarningOverlay_Process(100,1);ScreenWarningOverlay(0,0xff9800,3,3,"Low Fuel");
 ScreenWarningOverlay_Process(599,1);ScreenWarningOverlay_Get(&w);C(w.active&&w.opacity==255&&w.size==432);
 ScreenWarningOverlay_Process(600,1);ScreenWarningOverlay_Get(&w);C(w.opacity==0);
 ScreenWarningOverlay_Process(3300,1);ScreenWarningOverlay_Get(&w);C(w.phase==1&&w.size<432&&w.size>336);
 ScreenWarningOverlay_Process(3500,1);C(ScreenWarningOverlay_Button(2,1));ScreenWarningOverlay_Get(&w);C(!w.active);
 C(ScreenWarningOverlay_Button(2,2));C(ScreenWarningOverlay_Button(2,3));C(!ScreenWarningOverlay_Button(2,1));
 ScreenWarningOverlay(0,0xff3030,3,3,"Fuel Level Critical");ScreenWarningOverlay_Process(3501,0);ScreenWarningOverlay_Get(&w);C(!w.active);
 PhoneIndicatorEntry e={1,2,0,0};uint32_t bt,gps;
 C(PhoneIndicators_Update(1,42,100,1,&e,1));PhoneIndicators_Colors(100,1,&bt,&gps);C(bt==0xff3030&&gps==0xf2f5f7);
 PhoneIndicators_Colors(600,1,&bt,&gps);C(bt==0xdce5e9);
 PhoneIndicators_Colors(4000,1,&bt,&gps);C(gps==0x666666);
 e.age_ms=0;C(PhoneIndicators_Update(2,42,700100,1,&e,1));C(e.age_ms==700000);PhoneIndicators_Colors(700100,1,&bt,&gps);C(bt==0xffcc33);
 PhoneIndicators_MarkRead();e.read=0;C(PhoneIndicators_Update(2,42,700200,1,&e,1));C(e.read==1);
 PhoneIndicators_Colors(700200,1,&bt,&gps);C(bt==0xdce5e9);
 PhoneIndicators_Colors(700200,0,&bt,&gps);C(bt==0x666666&&gps==0x666666);
 PhoneIndicatorEntry duplicate[2]={e,e};C(!PhoneIndicators_Update(2,42,700300,1,duplicate,2));
 // Both adapters must use this exact 8KiB echo handler; CRC checks all bytes.
 uint32_t request[131]={1,7};uint8_t reply[512];uint32_t bytes=0,crc=0xffffffff;
 C(!RadioSelfTest_Handle((uint8_t*)request,8,3,0,reply,&bytes)&&bytes==24);
 for(unsigned off=0;off<8192;off+=512){request[0]=2;request[2]=off;
  for(unsigned i=0;i<512;i++)((uint8_t*)request)[12+i]=(uint8_t)((off+i)*73+7);
  C(!RadioSelfTest_Handle((uint8_t*)request,524,3,off,reply,&bytes)&&bytes==512);
  C(!memcmp(reply,(uint8_t*)request+12,512));crc=Noodoe_Crc32Feed(crc,reply,512);
 }
 request[0]=3;C(!RadioSelfTest_Handle((uint8_t*)request,8,3,9000,reply,&bytes));
 C(g_radio_self_test.complete&&g_radio_self_test.crc==crc);
 C(RadioSelfTest_Handle((uint8_t*)request,8,4,9001,reply,&bytes)!=0);
 // Every enabled-stage combination has independent residence deadlines.
 UiConfig config={0};config.standby_ms=60000;config.bt_retention_ms=120000;
 for(unsigned mask=1;mask<8;mask++){
  config.off_stage_mask=mask;unsigned stage=Ui_OffStages_First(&config);
  C(stage==((mask&1)?OFF_DISPLAY_HOLD:(mask&2)?OFF_BT_HOLD:OFF_DEEP_SLEEP));
  if(mask&1){C(Ui_OffStages_Next(&config,stage,59999)==stage);
   stage=Ui_OffStages_Next(&config,stage,60000);C(stage==((mask&2)?OFF_BT_HOLD:(mask&4)?OFF_DEEP_SLEEP:OFF_DISPLAY_HOLD));}
  if(mask&2){C(Ui_OffStages_Next(&config,stage,119999)==stage);
   C(Ui_OffStages_Next(&config,stage,120000)==((mask&4)?OFF_DEEP_SLEEP:OFF_BT_HOLD));}
 }
 config.off_stage_mask=7;config.standby_ms=0;config.bt_retention_ms=0;
 C(Ui_OffStages_First(&config)==OFF_DEEP_SLEEP);
 return 0;
}
