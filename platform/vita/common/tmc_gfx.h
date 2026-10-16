/* Minimal 2D drawing API for the Vita UI layer (overlay, menus, toasts).
 * Colors are 0xRRGGBBAA. Coordinates are 960x544 screen pixels. */
#ifndef TMC_GFX_H
#define TMC_GFX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TmcGfx_FillRect(int x, int y, int w, int h, uint32_t rgba);
/* Draws UTF-8 text with its top-left at (x,y); size 1.0 is ~20 px tall.
 * Returns the width in pixels. */
int TmcGfx_Text(int x, int y, float size, uint32_t rgba, const char* text);
int TmcGfx_TextWidth(float size, const char* text);
/* Draws an ABGR8888 pixel buffer scaled by `scale` at (x,y). */
void TmcGfx_DrawPixels(const uint32_t* pixels, int width, int height, int pitch, int x, int y, float scale,
                       bool smooth, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif
