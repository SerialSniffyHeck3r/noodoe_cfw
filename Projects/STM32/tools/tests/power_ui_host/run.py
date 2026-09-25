"""Actual power UI/state/ride/scene ARM code with a1s hold/commit boundary.
Hardware/graphics transports are stubs; no silicon wake or real screenshot claim.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys
H=Path(__file__).resolve().parent;P=H.parents[2];O=H/'output';O.mkdir(exist_ok=True)
S=O/'stubs';S.mkdir(exist_ok=True)
for name in ['NoodoeRuntime','PowerService','Button_Hints','App_Settings','SpeedHome_View','Graphics','Graphics_Background','Wallpaper','Wallpaper_Runtime','Settings_View','Settings_UI']:
    (S/(name+'.h')).write_text('#include "platform.h"\n')
(S/'lvgl.h').write_text('#include <stdint.h>\ntypedef void lv_obj_t;\n')
(S/'platform.h').write_text('''#ifndef TEST_PLATFORM_H
#define TEST_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#include "Ui_State.h"
#include "SpeedHome_Model.h"
#include "Ui_DashboardPresentation.h"
#include "Scene_Transition.h"
#include "Power_View.h"
typedef struct {uint32_t ign_valid,ign_on,clock_valid,hour,minute;} NoodoeSystemSnapshot;
typedef struct {uint32_t valid_fields,speed_kph,odometer_km;} VehicleSnapshot;
#define VEHICLE_VALID_SPEED 1U
#define VEHICLE_VALID_ODOMETER 2U
enum {POWER_RUN,POWER_ECONOMY,POWER_DISPLAY_SLEEP,POWER_DEEP,POWER_OWNER_GRAPHICS};
typedef struct {uint32_t render_count;} FakeGraphics;
extern FakeGraphics g_graphics;
typedef struct {uint32_t loading,phase,display_percent,center_percent;} FakeBackground;
extern FakeBackground g_background;
typedef struct {uint32_t effective_brightness;} FakeSettings;
extern FakeSettings *g_app_settings;
uint32_t PowerService_RunRequired(void);
void PowerService_Request(uint32_t policy);
void PowerService_Acknowledge(uint32_t owner,uint32_t value);
uint32_t Graphics_SetDisplaySleeping(uint32_t sleep);
uint32_t Graphics_DisplaySleeping(void);
uint32_t Graphics_SetContinuousRendering(uint32_t active);
uint32_t Graphics_Invalidate(void);
uint32_t Graphics_GetBrightnessPercent(void);
uint32_t Graphics_SetBrightnessPercent(uint32_t percent);
uint32_t Graphics_FramePresentedAfter(uint32_t frame);
void *Graphics_GetScreen(void);
void SpeedHome_GetScene(ScenePose *pose);
void SpeedHome_ApplyScene(const ScenePose *pose,uint32_t quick);
void SpeedHome_Render(const SpeedHomeModel *m,const UiDashboardPresentation *p,uint32_t now);
void ButtonHints_Update(const void *bindings,uint32_t scope,uint32_t alpha,uint32_t now);
void SettingsView_Hide(void);
uint32_t GraphicsBackground_SetImage(const void *image);
uint32_t GraphicsBackground_SetCenterBrightness(uint32_t value);
void GraphicsBackground_Process(void);
uint32_t GraphicsBackground_Settled(void);
void WallpaperRuntime_HoldShutdown(uint32_t hold);
#endif
''')
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
sys.path.insert(0,str(P.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_XPSR
link=O/'test.ld'
link.write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 256K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM AT>FLASH\n.bss(NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n . = ALIGN(8); end = .;\n/DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) } }\n')
sources=[*sorted((P/'App_Logic/UI/src').glob('ui_*.c')),P/'App_Logic/UI/src/phone_calls.c',P/'App_Logic/UI/src/power_ui.c',P/'App_Logic/UI/src/ignition_session.c',*[P/'Graphics/UI/src'/f for f in ['page_transition.c','scene_transition.c','power_scene.c']]]
reports=[]
for opt in ['O0','Os']:
    elf=O/(opt+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-nostartfiles','--specs=nano.specs','--specs=nosys.specs',*['-I'+str(x) for x in [S,P/'App_Logic/UI/inc',P/'App_Logic/Settings/inc',P/'Graphics/UI/inc']],str(H/'test.c'),*map(str,sources),'-Wl,-T,'+str(link),'-Wl,--gc-sections','-Wl,-e,TestMain','-o',str(elf),'-lgcc']
    r=subprocess.run(cmd,capture_output=True,text=True);(O/(opt+'.log')).write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stderr)
    listing=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={p[2]:int(p[0],16) for line in listing.splitlines() if len(p:=line.split())==3}
    u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000000,0x40000);u.mem_map(0x20000000,0x20000)
    data=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    for i in range(header[10]):
        kind,offset,address,_,size,_,_,_=struct.unpack_from('<IIIIIIII',data,header[5]+i*header[9])
        if kind==1 and size:u.mem_write(address,data[offset:offset+size])
    stop=0x1003fff0;u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,stop|1);u.reg_write(UC_ARM_REG_XPSR,0x1000000)
    u.emu_start(symbols['TestMain']|1,stop,count=50000000)
    result={'opt':opt,'result':u.reg_read(UC_ARM_REG_R0),'returned':u.reg_read(UC_ARM_REG_PC)==stop,'assertions':struct.unpack('<I',u.mem_read(symbols['assertions'],4))[0]}
    reports.append(result);print(result,flush=True)
    assert result['returned'] and result['result']==0,result
(O/'results.json').write_text(json.dumps({'runs':reports,'hardware_access':False,'sources':{str(f.relative_to(P)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}},indent=2))
