#include "Graphics.h"

/* 범위 확인 후2배 좌표 차이는 최대479라 signed 곱셈이 overflow하지 않는다.
 * 중심239.5px/반경240px의 원과 고정480x480 raster를 함께 검사한다. */
uint32_t Graphics_IsPointVisible(int32_t x, int32_t y)
{
    if(x<0 || y<0 || x>=(int32_t)GRAPHICS_WIDTH || y>=(int32_t)GRAPHICS_HEIGHT) return 0U;
    const int32_t dx=2*x-GRAPHICS_ACTIVE_CENTER_X2;
    const int32_t dy=2*y-GRAPHICS_ACTIVE_CENTER_Y2;
    return dx*dx+dy*dy<=GRAPHICS_ACTIVE_RADIUS_X2*GRAPHICS_ACTIVE_RADIUS_X2;
}

/* 원은 볼록하므로 네 꼭짓점의 포함 여부로 사각형 전체를 검사한다. */
uint32_t Graphics_IsAreaVisible(const lv_area_t *a)
{
    return a && a->x1<=a->x2 && a->y1<=a->y2 &&
        Graphics_IsPointVisible(a->x1,a->y1) && Graphics_IsPointVisible(a->x2,a->y1) &&
        Graphics_IsPointVisible(a->x1,a->y2) && Graphics_IsPointVisible(a->x2,a->y2);
}

/* 원 중심에 가장 가까운 사각 내부pixel도 바깥이면GPU task를 생략한다.
 * 일부가 보이는 task는 최종EVE 원형 제외 단계가 경계에서 자른다. */
uint32_t Graphics_AreaIntersectsVisible(const lv_area_t *a)
{
    if(!a || a->x1>a->x2 || a->y1>a->y2 || a->x2<0 || a->y2<0 ||
       a->x1>=(int32_t)GRAPHICS_WIDTH || a->y1>=(int32_t)GRAPHICS_HEIGHT) return 0U;
    int32_t x=GRAPHICS_ACTIVE_CENTER_X2/2, y=GRAPHICS_ACTIVE_CENTER_Y2/2;
    if(x<a->x1) x=a->x1; else if(x>a->x2) x=a->x2;
    if(y<a->y1) y=a->y1; else if(y>a->y2) y=a->y2;
    return Graphics_IsPointVisible(x,y);
}

/* 원형 도형은 bounding square 대신 중심 거리+반경으로 포함 여부를 확인한다.
 * radius에stroke/AA 여유를 포함한다. 먼저240으로 제한해 곱셈overflow를 막는다.
 * 정수 중심240,240과 반경240인 원은 half-pixel 활성 원을 일부 벗어난다.
 * 전체viewport를 채우고 최종mask로 자르는 시험은 이 완전포함API와 구별한다. */
uint32_t Graphics_IsCircleVisible(int32_t x, int32_t y, int32_t radius)
{
    if(!Graphics_IsPointVisible(x,y) || radius<0 || radius>GRAPHICS_ACTIVE_RADIUS_X2/2) return 0U;
    int32_t dx=2*x-GRAPHICS_ACTIVE_CENTER_X2, dy=2*y-GRAPHICS_ACTIVE_CENTER_Y2;
    int32_t left=GRAPHICS_ACTIVE_RADIUS_X2-2*radius;
    return dx*dx+dy*dy<=left*left;
}

/* y band 전체에서 안전한 최대 폭을 구한다. 초기 배치 때480px 이진탐색을
 * 두 번 수행한다. 반경240에서는 최상단/최하단에도 중앙의 좁은 band가 있다.
 * 유효한 전체480x480 영역이라고 해서 사각 모서리까지 허용하는 것은 아니다. */
uint32_t Graphics_GetSafeArea(int32_t top, int32_t bottom, lv_area_t *area)
{
    if(!area || top<0 || bottom>=(int32_t)GRAPHICS_HEIGHT || top>bottom) return 0U;
    int32_t middle=GRAPHICS_ACTIVE_CENTER_X2/2;
    if(!Graphics_IsPointVisible(middle,top) || !Graphics_IsPointVisible(middle,bottom)) return 0U;
    int32_t lo=0, hi=middle;
    while(lo<hi) {
        int32_t mid=(lo+hi)/2;
        if(Graphics_IsPointVisible(mid,top) && Graphics_IsPointVisible(mid,bottom)) hi=mid;
        else lo=mid+1;
    }
    const int32_t left=lo;
    lo=middle; hi=(int32_t)GRAPHICS_WIDTH-1;
    while(lo<hi) {
        int32_t mid=(lo+hi+1)/2;
        if(Graphics_IsPointVisible(mid,top) && Graphics_IsPointVisible(mid,bottom)) lo=mid;
        else hi=mid-1;
    }
    *area=(lv_area_t){left,top,lo,bottom};
    return 1U;
}
