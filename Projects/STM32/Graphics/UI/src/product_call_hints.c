#include "Product_CallIcon.h"
#include "Product_MusicIcons.h"
#include "Product_TripIcons.h"
#include "Button_Hints.h"
#include "BSP_Buttons.h"
#include "Phone_Calls.h"
/* Bind semantics only; geometry, PH9 suppression and release feedback remain
 * owned by the existing generic hint renderer/middleware. */
void ProductCall_Hints(uint32_t state,uint32_t scope,uint32_t alpha,uint32_t now)
{
 const ButtonHintBinding rows[3]={
  {BSP_BUTTON_UP,Product_TripIcon(PRODUCT_TRIP_MAXIMUM),NULL,NULL},
  {BSP_BUTTON_ENTER,Product_MusicIcon(MUSIC_ICON_CIRCLE),NULL,state==CALL_IDLE||state==CALL_RINGING?Product_CallIcon():NULL},
  {BSP_BUTTON_DOWN,Product_MusicIcon(MUSIC_ICON_DOWN),NULL,state!=CALL_IDLE?Product_CallIcon():NULL}};
 ButtonHints_Update(rows,scope,alpha,now);
}
