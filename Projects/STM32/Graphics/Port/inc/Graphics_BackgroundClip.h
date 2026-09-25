#ifndef GRAPHICS_BACKGROUND_CLIP_H
#define GRAPHICS_BACKGROUND_CLIP_H
/* Called only inside the background draw's saved EVE context. Uses its fixed
 * shell separator geometry and leaves no stencil values for later widgets. */
void GraphicsBackground_ClipBegin(void);
void GraphicsBackground_ClipEnd(void);
#endif
