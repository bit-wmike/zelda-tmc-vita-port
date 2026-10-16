/* Host: UI drawing is not rendered (headless); keep the API linkable. */
#include "tmc_gfx.h"
#include <string.h>
void TmcGfx_FillRect(int x, int y, int w, int h, uint32_t rgba) { (void)x; (void)y; (void)w; (void)h; (void)rgba; }
int TmcGfx_Text(int x, int y, float size, uint32_t rgba, const char* text) {
    (void)x; (void)y; (void)rgba;
    return (int)(strlen(text) * 10 * size);
}
int TmcGfx_TextWidth(float size, const char* text) { return (int)(strlen(text) * 10 * size); }
void TmcGfx_DrawPixels(const uint32_t* pixels, int width, int height, int pitch, int x, int y, float scale,
                       bool smooth, uint8_t alpha) {
    (void)pixels; (void)width; (void)height; (void)pitch; (void)x; (void)y; (void)scale; (void)smooth; (void)alpha;
}
void TmcGfx_DrawPixelsCached(const uint32_t* pixels, int width, int height, int pitch, int x, int y, float scale,
                             bool smooth, uint8_t alpha, bool changed) {
    (void)changed;
    TmcGfx_DrawPixels(pixels, width, height, pitch, x, y, scale, smooth, alpha);
}
