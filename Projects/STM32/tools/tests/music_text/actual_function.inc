static uint32_t BindText(MusicGraphic *v,uint32_t key,uint32_t panel)
{
    if(v->text_key!=key||v->panel!=panel){
        v->panel=panel;v->text_image.header.w=panel?288:304;v->text_image.header.h=panel?128:72;v->text_image.header.stride=panel?144:152;
        /* Retire a previously scanned texture before reusing its fixed bank.
         * LVGL object hiding alone is not an EVE display-list swap fence. */
        v->text_key=key;v->text_ready=0;v->text_wait=v->text_revision!=0;
        v->text_fence=g_graphics.render_count;lv_obj_invalidate(v->root);
    }
    if(v->text_wait){if(!Graphics_FramePresentedAfter(v->text_fence))return 0;v->text_wait=0;}
    uint32_t before=v->text_revision,ready=v->text_ready;
    v->text_ready=(panel?PhoneVisual_CopyPanel:PhoneVisual_CopyMask)(key,(void*)v->text_image.data,&v->text_revision);
    if(v->text_ready&&before!=v->text_revision)Graphics_ImageChanged(&v->text_image);
    if(ready!=v->text_ready||before!=v->text_revision)lv_obj_invalidate(v->root);
    return v->text_ready;
}
uint32_t MusicGraphic_Text(MusicGraphic *v,uint32_t key){return BindText(v,key,0);}
uint32_t MusicGraphic_Panel(MusicGraphic *v,uint32_t key,uint32_t selection,uint32_t count)
{v->reply_index=selection;v->reply_count=count;return BindText(v,key,1);}