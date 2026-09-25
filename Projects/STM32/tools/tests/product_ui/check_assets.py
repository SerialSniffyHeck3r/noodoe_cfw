"""Check the exact compiled layout catalog, generated fonts and UI vocabulary."""
import hashlib,json,re,math
from pathlib import Path
P=Path(__file__).resolve().parents[3]
manifest=json.loads((P/'Graphics/Assets/Fonts/manifest.json').read_text())
fonts={}
for family in manifest['families'].values():
    c=(P/'Graphics/UI/src'/family['generated']).read_bytes()
    assert hashlib.sha256(c).hexdigest()==family['generated_sha256']
    fonts.update(family['fonts'])
layout=(P/'Graphics/UI/src/speed_home_layout.c').read_text()
labels=re.findall(r'\[(SH_\w+)\]\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*SH_(TEXT|NUMBER),\s*(\d+),\s*SH_(\w+)\s*\}',layout)
assert len(labels)==16,labels
quoted=lambda name: re.findall(r'"([^"\n]+)"',(P/'App_Logic/UI/src'/name).read_text())
# Full settings have their own SettingsView, not the riding-shell labels.
# Check remaining warning/offline strings against the narrow center line.
presenter=quoted('ui_dashboard_presenter.c')[2:]
card_titles=['Trip computer','Phone','Music','Remote control','Phone GPS','Settings']
text={
 'SH_CLOCK':['23:59','--:--'],
 'SH_CARD_TITLE':card_titles+['UI error','Service due','Belt service','Oil service','Low fuel'],
 'SH_CARD_LINE':presenter,
 'SH_CARD_HINT':presenter+['Preview only - not saved'],
 'SH_CARD_NUMBER':['4294967295'],
 'SH_UART':['UART OK','UART --'], 'SH_FPS_TITLE':['FPS'],
 'SH_FPS_VALUE':['60.0','--.-'],'SH_CPU_TITLE':['CPU'],
 'SH_CPU_VALUE':['100.0','--.-'],'SH_PERCENT':['%'],
 'SH_FOOTER_TITLE':['ODO','TRIP 1','TRIP 2','TRIP F','OIL','BELT','SERV'],
 'SH_FOOTER_VALUE':['0','0.0','0.1','1.0','36475','123.4','00000.0','999999','9999.9','999.9','99999','######','####.#','----.-'],
 'SH_FOOTER_UNIT':['km','mi'],
 'SH_FOOTER_AUX_TITLE':['DAYS','HOURS'],
 'SH_FOOTER_AUX_VALUE':['0','0.0','0.1','75.0','365','99999','9999.9','-----','----.-','#####','####.#'],
}
results=[];rects={}
for key,x,baseline,w,role,size,align in labels:
    x,baseline,w=map(int,(x,baseline,w));font=fonts[f'product_{role.lower()}_{size}'];h=font['line_height']+4
    chars={g['codepoint']:g for g in font['glyphs']}
    y=baseline-font['line_height']+font['base_line']-2
    if key.startswith('SH_FOOTER'):
        vocab='0123456789.-#' if role=='NUMBER' else 'kmi' if key=='SH_FOOTER_UNIT' else 'DAYSHOURS' if key=='SH_FOOTER_AUX_TITLE' else 'ODOTRIP123FBELTSV '
        above=max(chars[ord(c)]['box'][1]+chars[ord(c)]['box'][3] for c in vocab)
        below=max(-chars[ord(c)]['box'][3] for c in vocab)
        y=baseline-above;h=above+below
        assert y>=404 and y+h-1<=464,(key,y,y+h-1)
    for xx in (x,x+w-1):
        for yy in (y,y+h-1):assert (2*xx-479)**2+(2*yy-479)**2<=480**2,(key,x,y,w,h)
    widest=('',0)
    for value in text[key]:
        assert all(ord(a) in chars for a in value),(key,value)
        advance=sum(chars[ord(a)]['advance'] for a in value)
        assert advance+(0 if key=='SH_FOOTER_VALUE' and value=='00000.0' else 2)<=w,(key,value,advance,w)
        if advance>widest[1]:widest=value,advance
    if role=='NUMBER':
        assert len({chars[c]['advance'] for c in range(48,58)})==1
        assert chars[32]['advance']==chars[32]['natural_advance']
    else:
        assert chars[ord('i')]['advance']<chars[ord('W')]['advance']
        assert all(g['advance']==g['natural_advance'] for g in font['glyphs'])
    rects[key]=(x,y,x+w-1,y+h-1)
    results.append(dict(field=key,rect=[x,y,w,h],baseline=baseline,role=role,size=int(size),align=align,widest=widest))
for a,ra in rects.items():
    for b,rb in rects.items():
        if a>=b or {a,b}=={'SH_CARD_HINT','SH_CARD_NUMBER'}:continue
        assert ra[2]<rb[0] or rb[2]<ra[0] or ra[3]<rb[1] or rb[3]<ra[1],('overlap',a,b)
# Raised arms may enter the clock's empty side margins, but no stroke touches
# actual clock glyph ink. Test stroke samples against the widest clock string.
def separator(name):
    body=re.search(rf'{name}\[(?:2|SPEED_HOME_TRAPEZOID_POINTS)\]\[2\]\s*=\s*\{{(.*?)\}};',layout,re.S).group(1)
    return [tuple(map(int,p)) for p in re.findall(r'\{(\d+),(\d+)\}',body)]
points=separator('speed_home_separator')
lower=separator('speed_home_footer_separator')
bottom=separator('speed_home_footer_bottom_separator')
assert bottom==[(144,448),(336,448)],bottom
assert points==[(127,51),(172,87),(308,87),(353,51)],points
assert lower==[(87,424),(116,401),(364,401),(392,424)],lower
assert lower[1][1]==lower[2][1]==round(240+(240-18/2)/math.sqrt(2))-2
assert bottom[0][1]-lower[1][1]==47 # Four extra pixels around unchanged text.
# Integer endpoints remain within1pixel of the original24:19 inclined rays.
for path in (points,lower):
    for outer,inner in ((path[0],path[1]),(path[-1],path[-2])):
        dx=abs(outer[0]-inner[0]);dy=abs(outer[1]-inner[1])
        assert abs(dx*19-dy*24)/math.hypot(24,19)<1
header=(P/'Graphics/UI/inc/SpeedHome_Layout.h').read_text()
constants={k:int(v) for k,v in re.findall(r'#define (SPEED_HOME_\w+) (\d+)',header)}
stroke=constants['SPEED_HOME_SEPARATOR_STROKE'];assert stroke==3
margin=stroke/2
cx,cy,cw,ch=[constants['SPEED_HOME_CONTENT_'+k] for k in ('X','Y','WIDTH','HEIGHT')]
center=(cx,cy,cx+cw-1,cy+ch-1)
assert center==(72,105,407,384)
assert cy>max(y for x,y in points)+2 and center[3]<min(y for x,y in lower)-2
# Every corner stays inside the ring's inner edge; descendants cannot paint
# the permanent ring even when they cover the entire clipped content parent.
for x in (center[0],center[2]):
    for y in (center[1],center[3]):assert (2*x-479)**2+(2*y-479)**2<(480-2*constants['SPEED_HOME_RING_STROKE'])**2
for key in ('SH_CARD_TITLE','SH_CARD_LINE','SH_CARD_HINT','SH_CARD_NUMBER'):
    box=rects[key]
    assert center[0]<=box[0]<=box[2]<=center[2] and center[1]<=box[1]<=box[3]<=center[3]
# Sample every separator path with its actual half-stroke clearance.
for path in (points,lower,bottom):
    for edge,((ax,ay),(bx,by)) in enumerate(zip(path,path[1:])):
        boundary_arm=path==lower and edge in (0,2)
        if boundary_arm:
            # Only lower inclined arms end at the circle through the ring's
            # open bottom sector. Their half caps are stencil-clipped. No
            # separator is allowed into the actual ring, including these arms.
            ex,ey=path[0 if edge==0 else -1]
            assert (2*ex-479)**2+(2*ey-479)**2<=480**2
            assert math.hypot(ex-239.5,ey-239.5)+margin>=240
        for n in range(101):
            x=ax+(bx-ax)*n/100;y=ay+(by-ay)*n/100
            # The lower arms sit in the open bottom sector of the speed ring.
            # Validate physical circle margin and the actual rounded arc ends,
            # rather than restricting that empty sector to the inner radius.
            assert (2*x-479)**2+(2*y-479)**2<=(480 if boundary_arm else 480-stroke)**2
            angle=math.degrees(math.atan2(y-240,x-240))%360
            radius=240-constants['SPEED_HOME_RING_STROKE']/2
            if 135<=angle or angle<=45:
                assert math.hypot(x-240,y-240)<radius-constants['SPEED_HOME_RING_STROKE']/2-margin
            else:
                for endpoint in (45,135):
                    ex=240+radius*math.cos(math.radians(endpoint))
                    ey=240+radius*math.sin(math.radians(endpoint))
                    assert math.hypot(x-ex,y-ey)>constants['SPEED_HOME_RING_STROKE']/2+margin
            for key,box in rects.items():
                if key=='SH_CLOCK':continue # Exact clock glyph check below.
                assert x<box[0]-margin or x>box[2]+margin or y<box[1]-margin or y>box[3]+margin,('separator/label',key,box,x,y)
assert constants['SPEED_HOME_ICON_X']==rects['SH_FOOTER_TITLE'][0]==118
clock=fonts['product_number_48'];glyphs={g['codepoint']:g for g in clock['glyphs']}
clock_baseline=next(r['baseline'] for r in results if r['field']=='SH_CLOCK')
assert clock_baseline==71
for value in text['SH_CLOCK']:
    width=sum(glyphs[ord(c)]['advance'] for c in value);left=144+(192-width)//2
    for c in value:
        g=glyphs[ord(c)];w,h,ox,oy=g['box'];box=(left+ox,clock_baseline-h-oy,left+ox+w-1,clock_baseline-oy-1)
        for (ax,ay),(bx,by) in zip(points,points[1:]):
            for n in range(101):
                x=ax+(bx-ax)*n/100;y=ay+(by-ay)*n/100
                assert x<box[0]-margin or x>box[2]+margin or y<box[1]-margin or y>box[3]+margin,('clock/line',value,box,x,y)
        left+=g['advance']
assert rects['SH_CARD_TITLE'][1]>max(y for x,y in points)+1
assert rects['SH_FOOTER_UNIT'][0]>rects['SH_FOOTER_VALUE'][2]
assert rects['SH_FOOTER_VALUE'][0]>rects['SH_FOOTER_TITLE'][2]
# Restore the approved36px main row; keep the new time row. Both numeric
# fields use the same fixed right edge, with auxiliary units before the value.
assert rects['SH_FOOTER_VALUE'][1:4:2]==(410,436)
assert rects['SH_FOOTER_UNIT'][3]==rects['SH_FOOTER_VALUE'][3]==436
assert rects['SH_FOOTER_AUX_VALUE'][2]==rects['SH_FOOTER_VALUE'][2]==309
assert rects['SH_FOOTER_AUX_TITLE'][2]<rects['SH_FOOTER_AUX_VALUE'][0]
assert rects['SH_FOOTER_AUX_VALUE'][1]>rects['SH_FOOTER_VALUE'][3]
icons=json.loads((P/'Graphics/Assets/Icons/manifest.json').read_text())
assert icons['family']=='Google Material Icons Round'
assert icons['generated_sha256']==hashlib.sha256((P/'Graphics/UI/src/product_icon_fonts.c').read_bytes()).hexdigest()
iconmap={i['icon']:i for i in icons['icons']}
assert iconmap['build']['codepoint']==0xE869 and iconmap['build']['ink_size']==[22,22]
assert iconmap['local_gas_station']['codepoint']==0xE546 and iconmap['local_gas_station']['ink_size']==[18,18]
assert icons['bitmap_bytes']==404
# Both icons substitute the title: their actual ink starts with footer numbers.
for icon in icons['icons']:
    y=410-icon['ink_offset_top']
    assert y>=404 and y+23<=444
    for x in (118,141):
        for yy in (y,y+23):assert (2*x-479)**2+(2*yy-479)**2<=480**2
# Lower straight border clears the main ink and the retained calendar DAYS.
assert bottom[0][1]==bottom[1][1]==448
assert rects['SH_FOOTER_VALUE'][3]+2<448
assert 448+margin<rects['SH_FOOTER_AUX_VALUE'][1]
# Validate the alternate straight OIL line as a whole, never a circle approximation.
bar_header=(P/'Graphics/UI/inc/Product_MaintenanceBar.h').read_text()
bar_constants={k:int(v) for k,v in re.findall(r'#define PRODUCT_MAINTENANCE_LINE_(\w+) (\d+)',bar_header)}
gx,gy,gw,gh=[bar_constants[k] for k in ('X','Y','WIDTH','HEIGHT')]
assert (gx,gy,gw,gh)==(160,454,160,6)
assert gy>max(y for x,y in bottom)+2
for x in (gx,gx+gw-1):
    for y in (gy,gy+gh-1):assert (2*x-479)**2+(2*y-479)**2<=480**2
# Visible numeric/icon fields may not intersect any of the three strokes.
for icon in icons['icons']:
    box=(118,410,118+icon['ink_size'][0]-1,410+icon['ink_size'][1]-1)
    for path in (points,lower,bottom):
        for (ax,ay),(bx,by) in zip(path,path[1:]):
            for n in range(101):
                x=ax+(bx-ax)*n/100;y=ay+(by-ay)*n/100
                assert x<box[0]-margin or x>box[2]+margin or y<box[1]-margin or y>box[3]+margin,('separator/icon',icon['icon'],x,y)
result={'status':'PASS','labels':results,'separator_stroke':stroke,'separator':points,'footer_separator':lower,'footer_bottom_separator':bottom,'central_content':center,'footer_top':404,'footer_bottom':464,'footer_ink_top':410,'icon':icons,'oil_line':{'rect':[gx,gy,gw,gh],'oil_hours_visible':False,'calendar_modes_hide_line':True},
        'scope':'Actual font/catalog bounds and no-overlap checks; not panel viewing distance or GPU rendering.'}
out=Path(__file__).parent/'output';out.mkdir(exist_ok=True)
(out/'assets.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
