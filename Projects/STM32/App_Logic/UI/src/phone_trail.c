#include "Phone_Trail.h"
#include <string.h>
/* Latitude is bounded to +/-90 degrees by Feed. A sixth-order polynomial
 * in x^2 evaluates cosine in that interval (max error <3e-7), without the
 * generic libm large-argument reducer. Projection already quantizes to Q10.
 * No unbounded angle or arbitrary math caller may use this specialization. */
static float LatitudeCosine(int32_t latitude)
{
    float x=(float)latitude*(3.14159265358979323846f/1800000000.0f);
    float z=x*x;
    return 1.0f+z*(-0.5f+z*(1.0f/24.0f+z*(-1.0f/720.0f+
        z*(1.0f/40320.0f+z*(-1.0f/3628800.0f+z/479001600.0f)))));
}
/* Integer centimetres keep calculations bounded even across the date line. */
static void Delta(PhoneTrailPoint p,PhoneTrailPoint origin,int32_t *east,int32_t *north)
{
    int64_t lon=(int64_t)p.lon-origin.lon;
    if(lon>1800000000LL)lon-=3600000000LL;
    if(lon< -1800000000LL)lon+=3600000000LL;
    /* Subtract/wrap in integers before converting. The M4F evaluates this
     * small local projection directly, avoiding three software 64-bit divides
     * per point and the old Q10 latitude-cosine quantization. */
    /* Wrapped longitude and latitude differences are both <=180 degrees,
     * so narrow before FPU conversion; no software int64-to-float routine. */
    *east=(int32_t)((float)(int32_t)lon*1.1132f*LatitudeCosine(origin.lat));
    *north=(int32_t)((float)(p.lat-origin.lat)*1.1132f);
}
/* M4F hardware norm of a local centimetre vector. Even an antipodal delta
 * fits uint32_t; float rounding is below the GPS uncertainty. No iterative
 * 64-bit square root or generic libm argument handling in the frame loop. */
static uint32_t Length(int32_t east,int32_t north)
{
    float e=east,n=north,square=e*e+n*n,length;
    __asm__("vsqrt.f32 %0, %1" : "=t"(length) : "t"(square));
    return (uint32_t)length;
}
static void Heading(PhoneTrail *s,int32_t e,int32_t n)
{
    uint32_t length=Length(e,n);if(!length)return;
    e=(int32_t)((float)e*1024.0f/length);n=(int32_t)((float)n*1024.0f/length);
    /* Smooth on the unit circle:359->0 never interpolates through180.
     * A real reversal snaps instead of passing through a zero vector. */
    if(s->heading_valid&&(e*s->east+n*s->north)>0){e+=s->east;n+=s->north;}
    length=Length(e,n);s->east=e*1024/(int32_t)length;s->north=n*1024/(int32_t)length;s->heading_valid=1;
}
static int32_t Cosine(uint32_t mdeg)
{
    mdeg%=360000;uint32_t a=mdeg>180000?360000-mdeg:mdeg;
    return (a>90000?-1:1)*(int32_t)(LatitudeCosine((a>90000?180000-a:a)*10000)*1024);
}
/* Bounded atan approximation for a unit heading, max error about0.3degree.
 * Keeps the generic libm reducer out of flash; wrap selects the short arc. */
static int32_t Course(int32_t e,int32_t n)
{
    float x=e<0?-e:e,y=n<0?-n:n;
    if(x+y==0)return 0;
    float z=x<y?x/y:y/x;
    int32_t a=(int32_t)((45.0f*z+15.64f*z*(1.0f-z))*1000.0f);
    if(x>y)a=90000-a;
    if(n<0)a=180000-a;
    if(e<0)a=360000-a;
    return a%360000;
}
void PhoneTrail_RenderStep(PhoneTrail *s,uint32_t now)
{
    if(!s||!s->camera_ready)return;
    uint32_t dt=now-s->camera_ms,q=dt>=300U?300U:dt;
    int64_t lon=(int64_t)s->target.lon-s->from.lon;
    if(lon>1800000000LL)lon-=3600000000LL;
    if(lon< -1800000000LL)lon+=3600000000LL;
    lon=s->from.lon+lon*q/300;
    if(lon>1800000000LL)lon-=3600000000LL;
    if(lon< -1800000000LL)lon+=3600000000LL;
    s->display.lon=(int32_t)lon;
    s->display.lat=s->from.lat+(int64_t)(s->target.lat-s->from.lat)*q/300;
    int32_t angle=s->target_course-s->from_course;
    if(angle>180000)angle-=360000;
    if(angle< -180000)angle+=360000;
    s->display_course=(s->from_course+angle*(int32_t)q/300+360000)%360000;
    s->display_east=Cosine((s->display_course+270000)%360000);
    s->display_north=Cosine(s->display_course);
}
static void CameraTarget(PhoneTrail *s,PhoneTrailPoint point,uint32_t now,uint32_t gap)
{
    int32_t course=Course(s->east,s->north);
    if(!s->camera_ready){s->anchor=point;s->display=point;s->display_course=course;s->camera_ready=1;}
    else PhoneTrail_RenderStep(s,now);
    if(gap){s->display=point;s->display_course=course;} /* Never fly across an outage. */
    s->from=s->display;s->target=point;s->from_course=s->display_course;s->target_course=course;s->camera_ms=now;
    PhoneTrail_RenderStep(s,now);
}
void PhoneTrail_Init(PhoneTrail *s){if(s){memset(s,0,sizeof(*s));s->north=1024;}}
void PhoneTrail_FeedHeading(PhoneTrail *s,uint32_t now,uint32_t valid,int32_t lat,int32_t lon,uint32_t bearing,uint32_t course)
{
    if(!s)return;
    if(!valid||lat< -900000000||lat>900000000||lon< -1800000000||lon>1800000000){s->live=0;return;}
    uint32_t sample_dt=now-s->sample_ms;
    if(s->have_sample&&(!sample_dt||sample_dt>0x80000000U))return;
    uint32_t gap=s->have_sample&&sample_dt>10000;
    s->sample_ms=now;s->have_sample=1;
    if(s->count){
        uint32_t dt=now-s->last_ms;
        int32_t e,n;Delta((PhoneTrailPoint){lat,lon,0},s->points[s->count-1],&e,&n);
        uint32_t cm=Length(e,n);
        /* 100m/s plus20m uncertainty rejects isolated teleport fixes. After
         * a long outage reconnect with a break, never draw a false straight leg. */
        if(!gap&&cm>2000U+(uint64_t)dt*10U){++s->rejected;return;}
        s->live=1;
        if(bearing&&course<360000)Heading(s,Cosine((course+270000)%360000),Cosine(course));
        if(cm<500){CameraTarget(s,s->points[s->count-1],now,0);return;} /* Keep stationary coordinates. */
        if(!bearing&&!gap)Heading(s,e,n);
    }else if(bearing&&course<360000)Heading(s,Cosine((course+270000)%360000),Cosine(course));
    if(s->count==PHONE_TRAIL_POINTS){
        for(uint32_t i=0;i<PHONE_TRAIL_POINTS/2;i++){PhoneTrailPoint p=s->points[i*2];if(i)p.gap|=s->points[i*2-1].gap;s->points[i]=p;}
        s->count=PHONE_TRAIL_POINTS/2;
    }
    s->points[s->count++]=(PhoneTrailPoint){lat,lon,gap};s->last_ms=now;s->live=1;
    CameraTarget(s,s->points[s->count-1],now,gap);
}
void PhoneTrail_Feed(PhoneTrail *s,uint32_t now,uint32_t valid,int32_t lat,int32_t lon)
{PhoneTrail_FeedHeading(s,now,valid,lat,lon,0,0);}
uint32_t PhoneTrail_Scale(uint32_t zoom)
{static const uint16_t meters[]={20,50,100,200,500,1000};return meters[zoom<6?zoom:5];}
uint32_t PhoneTrail_ProjectView(const PhoneTrail *s,int16_t xy[PHONE_TRAIL_POINTS][2],uint32_t w,uint32_t h,uint32_t up,uint32_t zoom)
{
    if(!s||!xy||!s->count||w<3||h<3)return 0;
    int32_t east=up?s->display_east:0,north=up?s->display_north:1024;
    float scale=PhoneTrail_Scale(zoom)*2048.0f; /* Q10 heading * cm/pixel */
    for(uint32_t i=0;i<s->count;i++){
        int32_t e,n;Delta(s->points[i],s->camera_ready?s->display:s->points[s->count-1],&e,&n);
        float x=w/2+((float)e*north-(float)n*east)/scale;
        float y=h/2-((float)n*north+(float)e*east)/scale;
        xy[i][0]=x< -30000?-30000:x>30000?30000:(int16_t)x;
        xy[i][1]=y< -30000?-30000:y>30000?30000:(int16_t)y;
    }
    return s->count;
}
/* The modulo is in earth centimetres, before rotation. The first fix is the
 * immutable origin; zoom changes spacing, never replaces grid with screen art. */
uint32_t PhoneTrail_ProjectGrid(const PhoneTrail *s,int16_t lines[PHONE_TRAIL_GRID_LINES][4],uint32_t w,uint32_t h,uint32_t up,uint32_t zoom)
{
    if(!s||!s->camera_ready||!lines)return 0;
    int32_t e,n;Delta(s->display,s->anchor,&e,&n);
    /* A grid cell spans two scale bars (100px). The earlier50px cells plus
     * four fade passes exhausted the8KiB EVE list during two-page slides.
     * Reduce redundant grid geometry, not route samples or driver limits.
     * Both the modulo and screen pitch change together: cells remain fixed
     * to real positions through movement, rotation and zoom. */
    int32_t spacing=(int32_t)PhoneTrail_Scale(zoom)*200;
    float ox=-(float)(e%spacing)*100/spacing,oy=-(float)(n%spacing)*100/spacing;
    float he=up?s->display_east/1024.0f:0,hn=up?s->display_north/1024.0f:1;
    uint32_t count=0;
    for(int i=-5;i<=5;i++)for(int axis=0;axis<2;axis++){
        float x=ox+i*100,y=oy+i*100;
        float x1=axis?-500:x,x2=axis?500:x,y1=axis?y:-500,y2=axis?y:500;
        lines[count][0]=(int16_t)(w/2+x1*hn-y1*he);lines[count][1]=(int16_t)(h/2-y1*hn-x1*he);
        lines[count][2]=(int16_t)(w/2+x2*hn-y2*he);lines[count][3]=(int16_t)(h/2-y2*hn-x2*he);count++;
    }return count;
}
uint32_t PhoneTrail_Project(const PhoneTrail *s,int16_t xy[PHONE_TRAIL_POINTS][2],uint32_t w,uint32_t h)
{return PhoneTrail_ProjectView(s,xy,w,h,0,2);}
