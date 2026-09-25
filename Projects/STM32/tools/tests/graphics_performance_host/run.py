"""Execute production pacing/CPU/geometry C on ARM; mock only hardware/LVGL edges."""
import hashlib, json, struct, subprocess, sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
PROJECT=HERE.parents[2]
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'output'
OUT.mkdir(parents=True,exist_ok=True)
headers={
'stm32f4xx.h':'''#pragma once
#include <stdint.h>
typedef struct { uint32_t CTRL,CYCCNT; } FakeDwt;
typedef struct { uint32_t DEMCR; } FakeDebug;
extern FakeDwt dwt; extern FakeDebug dbg; extern uint32_t SystemCoreClock;
#define DWT (&dwt)
#define CoreDebug (&dbg)
#define CoreDebug_DEMCR_TRCENA_Msk (1U<<24)
#define DWT_CTRL_CYCCNTENA_Msk 1U
''',
'stm32f4xx_hal.h':'#include "stm32f4xx.h"\nuint32_t HAL_GetTick(void);\n',
'FreeRTOS.h':'#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n',
'task.h':'void *xTaskGetIdleTaskHandle(void);\n',
'Graphics.h':'''#pragma once
#include <stdint.h>
#include "Graphics_Viewport.h"
typedef struct {int32_t x1,y1,x2,y2;} lv_area_t;
typedef struct {int dummy;} lv_timer_t;
typedef struct {int dummy;} lv_display_t;
typedef struct {uint32_t render_count;} Graphics_Diagnostics;
extern volatile Graphics_Diagnostics g_graphics;
typedef struct {uint32_t magic,version,valid,fps_tenths,cpu_tenths,target_fps_milli,
 continuous,window_ms,window_frames,frame_slots_missed,cpu_window_cycles,idle_window_cycles;} Graphics_Performance;
extern volatile Graphics_Performance g_graphics_performance;
lv_timer_t *lv_display_get_refr_timer(lv_display_t *d);
void lv_timer_pause(lv_timer_t *t);
void lv_timer_set_cb(lv_timer_t *t,void(*cb)(lv_timer_t*));
void *lv_display_get_screen_active(lv_display_t *d);
void lv_obj_invalidate(void *o);
void lv_refr_now(lv_display_t *d);
uint32_t Graphics_IsAreaVisible(const lv_area_t*);
uint32_t Graphics_IsPointVisible(int32_t,int32_t);
uint32_t Graphics_IsCircleVisible(int32_t,int32_t,int32_t);
uint32_t Graphics_AreaIntersectsVisible(const lv_area_t*);
uint32_t Graphics_GetSafeArea(int32_t,int32_t,lv_area_t*);
void Graphics_PerformanceInit(lv_display_t*);
void Graphics_PerformanceRender(lv_display_t*);
void Graphics_PerformanceSample(void);
'''}
for name,content in headers.items(): (OUT/name).write_text(content,encoding='utf-8')
(OUT/'test.c').write_text(r'''
#include "Graphics.h"
#include "bsp_cpu_load.h"
#include "stm32f4xx.h"
FakeDwt dwt; FakeDebug dbg; uint32_t SystemCoreClock=168000000U;
volatile Graphics_Diagnostics g_graphics;
static uint32_t ms; static lv_timer_t timer; static lv_display_t display;
static uint32_t assertions;
void *memset(void *p,int value,unsigned count){uint8_t *b=p;for(unsigned i=0;i<count;++i)b[i]=(uint8_t)value;return p;}
#define CHECK(x) do {++assertions; if(!(x)) return __LINE__;} while(0)
void *xTaskGetIdleTaskHandle(void){return (void*)0x1111;}
uint32_t HAL_GetTick(void){return ms;}
lv_timer_t *lv_display_get_refr_timer(lv_display_t *d){(void)d;return &timer;}
void lv_timer_pause(lv_timer_t *t){(void)t;}
void lv_timer_set_cb(lv_timer_t *t,void(*cb)(lv_timer_t*)){(void)t;(void)cb;}
void *lv_display_get_screen_active(lv_display_t *d){return d;}
void lv_obj_invalidate(void *o){(void)o;}
void lv_refr_now(lv_display_t *d){(void)d;++g_graphics.render_count;}
static void cpu_reset(uint32_t cycles){dwt.CYCCNT=cycles;BSP_CPU_LoadInit();}
uint32_t test_main(void)
{
    /* Known schedule: 250ms busy,750ms idle, including counter wrap. */
    for(uint32_t wrap=0;wrap<2;++wrap){
        uint32_t start=wrap?0xfff00000U:0U;
        cpu_reset(start);
        CHECK(g_bsp_cpu_load.ready==0);
        dwt.CYCCNT+=42000000U;BSP_CPU_TraceSwitch((void*)0x1111);
        dwt.CYCCNT+=126000000U;BSP_CPU_TraceSwitch((void*)0x2222);
        BSP_CPU_LoadSample();
        CHECK(g_bsp_cpu_load.ready==1 && g_bsp_cpu_load.busy_tenths==250);
        CHECK(g_bsp_cpu_load.total_cycles==168000000U && g_bsp_cpu_load.idle_cycles==126000000U);
    }
    cpu_reset(0);BSP_CPU_TraceSwitch((void*)0x1111);
    dwt.CYCCNT=168000000U;BSP_CPU_TraceSwitch((void*)0x2222);BSP_CPU_LoadSample();
    CHECK(g_bsp_cpu_load.busy_tenths==0);
    cpu_reset(0);dwt.CYCCNT=168000000U;BSP_CPU_LoadSample();
    CHECK(g_bsp_cpu_load.busy_tenths==1000);
    cpu_reset(0);BSP_CPU_LoadSample();CHECK(!g_bsp_cpu_load.ready);
    /* 5ms service quantization must still submit exactly30, not28/31 frames/s. */
    ms=0;g_graphics.render_count=0;Graphics_PerformanceInit(&display);
    g_graphics_performance.continuous=1;
    for(uint32_t i=0;i<200;++i){ms+=5;dwt.CYCCNT+=840000U;Graphics_PerformanceRender(&display);Graphics_PerformanceSample();}
    CHECK(g_graphics.render_count==30 && g_graphics_performance.frame_slots_missed==0);
    CHECK(g_graphics_performance.valid==1 && g_graphics_performance.fps_tenths==300);
    CHECK(g_graphics_performance.cpu_tenths==1000);
    /* A delayed service records skipped slots and never emits a burst of3 frames. */
    ms+=100;Graphics_PerformanceRender(&display);
    CHECK(g_graphics.render_count==31 && g_graphics_performance.frame_slots_missed==2);
    /* Circular safe regions must contain all corners, including y-band extrema. */
    lv_area_t area={66,112,413,336};CHECK(Graphics_IsAreaVisible(&area));
    area=(lv_area_t){150,438,329,455};CHECK(Graphics_IsAreaVisible(&area));
    area=(lv_area_t){0,0,479,479};CHECK(!Graphics_IsAreaVisible(&area));
    CHECK(Graphics_GetSafeArea(0,40,&area) && Graphics_IsAreaVisible(&area));
    CHECK(!Graphics_GetSafeArea(40,20,&area));
    for(int y=2;y<450;y+=7){CHECK(Graphics_GetSafeArea(y,y+20,&area));CHECK(Graphics_IsAreaVisible(&area));}
    /* Full480 diameter, half-pixel center. The aperture is still a disk. */
    CHECK(GRAPHICS_ACTIVE_CENTER_X2==479 && GRAPHICS_ACTIVE_CENTER_Y2==479 && GRAPHICS_ACTIVE_RADIUS_X2==480);
    CHECK(Graphics_IsPointVisible(239,239) && Graphics_IsPointVisible(240,240));
    CHECK(Graphics_IsPointVisible(0,239) && Graphics_IsPointVisible(479,240));
    CHECK(Graphics_IsPointVisible(239,0) && Graphics_IsPointVisible(240,479));
    CHECK(Graphics_IsPointVisible(1,239) && Graphics_IsPointVisible(478,240));
    CHECK(Graphics_IsPointVisible(239,1) && Graphics_IsPointVisible(240,478));
    CHECK(!Graphics_IsPointVisible(69,70) && Graphics_IsPointVisible(70,70));
    CHECK(!Graphics_IsPointVisible(410,409) && Graphics_IsPointVisible(409,409));
    CHECK(!Graphics_IsPointVisible(70,410) && Graphics_IsPointVisible(70,409));
    CHECK(!Graphics_IsPointVisible(409,69) && Graphics_IsPointVisible(409,70));
    CHECK(!Graphics_IsPointVisible(0,0) && !Graphics_IsPointVisible(479,0));
    CHECK(!Graphics_IsPointVisible(0,479) && !Graphics_IsPointVisible(479,479));
    CHECK(!Graphics_IsPointVisible(-1,240) && !Graphics_IsPointVisible(480,240));
    CHECK(!Graphics_IsPointVisible(INT32_MIN,INT32_MAX));
    CHECK(!Graphics_IsAreaVisible(0));
    area=(lv_area_t){20,20,10,10};CHECK(!Graphics_IsAreaVisible(&area));
    area=(lv_area_t){0,0,69,70};CHECK(!Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){0,0,70,70};CHECK(Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){410,409,479,479};CHECK(!Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){409,409,479,479};CHECK(Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){0,0,479,479};CHECK(Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX};CHECK(Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){480,0,500,479};CHECK(!Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){0,0,0,479};CHECK(Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){-100,239,0,240};CHECK(Graphics_AreaIntersectsVisible(&area));
    area=(lv_area_t){479,239,INT32_MAX,240};CHECK(Graphics_AreaIntersectsVisible(&area));
    CHECK(!Graphics_AreaIntersectsVisible(0));
    CHECK(Graphics_IsCircleVisible(239,239,238));
    CHECK(Graphics_IsCircleVisible(240,240,238));
    CHECK(Graphics_IsCircleVisible(239,239,239));
    CHECK(Graphics_IsCircleVisible(240,240,239));
    CHECK(!Graphics_IsCircleVisible(239,239,240));
    CHECK(!Graphics_IsCircleVisible(240,240,240));
    CHECK(!Graphics_IsCircleVisible(239,239,-1));
    CHECK(!Graphics_IsCircleVisible(239,239,INT32_MAX));
    CHECK(Graphics_IsCircleVisible(0,239,0));
    CHECK(!Graphics_IsCircleVisible(0,239,1));
    CHECK(Graphics_GetSafeArea(239,240,&area));
    CHECK(area.x1==0 && area.x2==479);
    CHECK(Graphics_GetSafeArea(1,478,&area));
    CHECK(area.x1==213 && area.x2==266);
    CHECK(Graphics_GetSafeArea(479,479,&area));
    CHECK(area.x1==225 && area.x2==254);
    CHECK(Graphics_GetSafeArea(0,479,&area));
    CHECK(area.x1==225 && area.x2==254 && Graphics_IsAreaVisible(&area));
    CHECK(!Graphics_GetSafeArea(INT32_MIN,INT32_MAX,&area));
    CHECK(!Graphics_GetSafeArea(1,478,0));
    /* Every row has maximal symmetric bounds. Each central row/column uses
     * all480 pixels, including endpoints; edge corners remain outside. */
    for(int y=0;y<480;y++){
        CHECK(Graphics_IsPointVisible(239,y) && Graphics_IsPointVisible(240,y));
        CHECK(Graphics_IsPointVisible(y,239) && Graphics_IsPointVisible(y,240));
        CHECK(Graphics_GetSafeArea(y,y,&area));
        CHECK(Graphics_IsAreaVisible(&area) && area.x1+area.x2==479);
        CHECK(!Graphics_IsPointVisible(area.x1-1,y) && !Graphics_IsPointVisible(area.x2+1,y));
        lv_area_t mirrored;
        CHECK(Graphics_GetSafeArea(479-y,479-y,&mirrored));
        CHECK(mirrored.x1==area.x1 && mirrored.x2==area.x2);
        CHECK(Graphics_IsPointVisible(y,area.x1) && Graphics_IsPointVisible(y,area.x2));
    }
    return 0;
}
uint32_t get_assertions(void){return assertions;}
''',encoding='utf-8')
(OUT/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x10000000, LENGTH = 64K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 64K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n}\n')
sources=[PROJECT/'Drivers/BSP/src/bsp_cpu_load.c',PROJECT/'Graphics/Port/src/graphics_geometry.c',PROJECT/'Graphics/Port/src/graphics_performance.c']
results=[]
for opt in ('-O0','-Os'):
    elf=OUT/(opt[1:]+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=c11',opt,'-ffreestanding','-fno-builtin','-nostdlib','-I',str(OUT),'-I',str(PROJECT/'Drivers/BSP/inc'),'-I',str(PROJECT/'Graphics/Port/inc'),*[str(p) for p in sources],str(OUT/'test.c'),'-T',str(OUT/'test.ld'),'-Wl,-e,test_main','-lgcc','-o',str(elf)]
    build=subprocess.run(cmd,capture_output=True,text=True)
    if build.returncode: raise RuntimeError(build.stdout+build.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={line.split()[2]:int(line.split()[0],16) for line in nm.splitlines() if len(line.split())==3}
    # Load ELF PT_LOAD segments at virtual addresses; no startup or host substitute algorithms.
    data=elf.read_bytes(); phoff=struct.unpack_from('<I',data,28)[0]; phsize,phnum=struct.unpack_from('<HH',data,42)
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    uc.mem_map(0x10000000,0x10000);uc.mem_map(0x20000000,0x10000)
    for i in range(phnum):
        kind,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,phoff+i*phsize)
        if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
    uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2000f000)
    uc.reg_write(UC_ARM_REG_LR,0x1000fff1)
    uc.emu_start(symbols['test_main']|1,0x1000fff0,timeout=20000000,count=15000000)
    if uc.reg_read(UC_ARM_REG_PC)!=0x1000fff0:raise RuntimeError(f'{opt}: fixture did not return within execution budget')
    failure=uc.reg_read(UC_ARM_REG_R0)
    if failure:raise RuntimeError(f'{opt}: production C assertion failed at harness line{failure}')
    uc.reg_write(UC_ARM_REG_LR,0x1000fff1);uc.emu_start(symbols['get_assertions']|1,0x1000fff0,count=1000)
    results.append(dict(optimization=opt,assertions=uc.reg_read(UC_ARM_REG_R0),status='pass'))
report=dict(results=results,sources={str(p.relative_to(PROJECT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[PROJECT/'Graphics/Port/inc/Graphics_Viewport.h']})
(OUT/'results.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
