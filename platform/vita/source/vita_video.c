/*
 * PS Vita video backend (vita2d / GXM).
 *
 * The finished game frame is uploaded into a linear texture and scaled to the
 * 960x544 screen with the original pixel aspect ratio. Three display styles:
 *   0 Smooth       - plain bilinear scale
 *   1 Sharp (def.) - nearest-neighbour 3x into an off-screen target, then a
 *                    bilinear scale to full height: crisp pixels, no shimmer
 *   2 Pixel        - integer 3x, nearest-neighbour (small black border)
 * UI (overlay, menus) is drawn on top through the TmcGfx_* helpers.
 */
#include "tmc_platform.h"
#include "tmc_gfx.h"
#include "tmc_diag.h"

#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>

#include <arm_neon.h>
#include <stdio.h>
#include <string.h>

#define TEX_W 400
#define TEX_H 240

static vita2d_texture* sTex[2];
static vita2d_texture* sSharpTarget;
static vita2d_pgf* sFont;
static int sTexIndex;
static bool sReady;

extern int TmcVita_DisplayStyle(void);
extern void TmcVita_DrawUi(void);

bool TmcVitaVideo_Init(void) {
    const int rc = vita2d_init();
    TmcDiag_Logf("[video] vita2d_init=%d\n", rc);
    if (rc < 0) return false;
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));
    vita2d_set_vblank_wait(1);
    for (int i = 0; i < 2; ++i) {
        sTex[i] = vita2d_create_empty_texture_format(TEX_W, TEX_H, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
        if (!sTex[i]) return false;
    }
    /* vita2d always projects to 960x544, so the off-screen target must have
     * exactly that size or everything drawn into it ends up scaled/offset. */
    sSharpTarget = vita2d_create_empty_texture_rendertarget(960, 544, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
    sFont = vita2d_load_default_pgf();
    TmcDiag_Logf("[video] textures=%p,%p sharp=%p font=%p\n", (void*)sTex[0], (void*)sTex[1], (void*)sSharpTarget,
                 (void*)sFont);
    sReady = true;
    return true;
}

/* Copies one row and forces opaque alpha, four pixels at a time. */
static inline void CopyRowOpaque(uint32_t* dst, const uint32_t* src, int width) {
    const uint32x4_t opaque = vdupq_n_u32(0xFF000000u);
    int x = 0;
    for (; x + 4 <= width; x += 4) vst1q_u32(dst + x, vorrq_u32(vld1q_u32(src + x), opaque));
    for (; x < width; ++x) dst[x] = src[x] | 0xFF000000u;
}

static void Upload(vita2d_texture* tex, const uint32_t* pixels, int width, int height, int pitch) {
    uint8_t* dst = (uint8_t*)vita2d_texture_get_datap(tex);
    const unsigned stride = vita2d_texture_get_stride(tex);
    for (int y = 0; y < height; ++y) {
        /* Force opaque alpha: the PPU leaves A undefined. */
        CopyRowOpaque((uint32_t*)(dst + (size_t)y * stride), pixels + (size_t)y * pitch, width);
    }
}

extern int TmcVita_KeepZoom(void);

static void GameRect(int width, int height, int style, float* scale, float* dx, float* dy) {
    /* Full View: one zoom for every gameplay frame (the 240-line view fills
     * the screen height; smaller frames get a border). */
    const int refHeight = TmcVita_KeepZoom() ? 240 : height;
    float sc = 544.0f / (float)refHeight;
    if (style == 2) {
        sc = (float)(int)(544 / refHeight);
        if (sc < 1.0f) sc = 1.0f;
    }
    *scale = sc;
    *dx = (960.0f - width * sc) * 0.5f;
    *dy = (544.0f - height * sc) * 0.5f;
}

void TmcVideo_Present(const uint32_t* pixels, int width, int height, int pitch, bool vsync) {
    if (!sReady) return;
    if (width > TEX_W) width = TEX_W;
    if (height > TEX_H) height = TEX_H;
    const uint64_t tWait = TmcSys_TimeUs();
    vita2d_wait_rendering_done();
    const uint64_t tDraw = TmcSys_TimeUs();
    TmcDiag_Stage(TMC_DIAG_VSYNC, tDraw - tWait);
    vita2d_texture* tex = sTex[sTexIndex];
    sTexIndex ^= 1;
    Upload(tex, pixels, width, height, pitch);
    vita2d_set_vblank_wait(vsync ? 1 : 0);

    const int style = TmcVita_DisplayStyle();
    float scale, dx, dy;
    GameRect(width, height, style, &scale, &dx, &dy);
    vita2d_pool_reset();
    if (style == 1 && sSharpTarget) {
        /* Largest whole-number upscale that still fits the 960x544 target
         * (3x for the normal views, 2x for 400x240 Full View). */
        int k = 960 / width;
        if (544 / height < k) k = 544 / height;
        if (k < 1) k = 1;
        /* Pass 1: nearest-neighbour k-times into the off-screen target. */
        vita2d_start_drawing_advanced(sSharpTarget, SCE_GXM_SCENE_FRAGMENT_SET_DEPENDENCY);
        vita2d_texture_set_filters(tex, SCE_GXM_TEXTURE_FILTER_POINT, SCE_GXM_TEXTURE_FILTER_POINT);
        vita2d_draw_texture_part_scale(tex, 0, 0, 0, 0, width, height, (float)k, (float)k);
        vita2d_end_drawing();
        /* Pass 2: bilinear scale of that block to full screen height. */
        vita2d_start_drawing_advanced(NULL, SCE_GXM_SCENE_VERTEX_WAIT_FOR_DEPENDENCY);
        vita2d_clear_screen();
        vita2d_texture_set_filters(sSharpTarget, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);
        const float s2 = scale / (float)k;
        vita2d_draw_texture_part_scale(sSharpTarget, dx, dy, 0, 0, (float)(width * k), (float)(height * k), s2, s2);
    } else {
        vita2d_start_drawing_advanced(NULL, 0);
        vita2d_clear_screen();
        const SceGxmTextureFilter f = style == 0 ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
        vita2d_texture_set_filters(tex, f, f);
        vita2d_draw_texture_part_scale(tex, dx, dy, 0, 0, width, height, scale, scale);
    }
    TmcVita_DrawUi();
    vita2d_end_drawing();
    {
        /* 30 fps mode: submit at most one frame per two display refreshes.
         * vita2d hands the flip to its display-queue thread, so swap returns
         * at once and the queue alone would accept a frame every refresh
         * (two logic frames per refresh = double speed). Waiting here for the
         * second refresh since the last submission keeps exactly 60 logic
         * frames per second; a frame that took longer needs no extra wait. */
        extern bool Port_Config_VitaHalfRate(void);
        static unsigned sLastPresentVcount;
        if (Port_Config_VitaHalfRate() && vsync) {
            while ((unsigned)sceDisplayGetVcount() - sLastPresentVcount < 2u) sceDisplayWaitVblankStart();
        }
        vita2d_swap_buffers();
        sLastPresentVcount = (unsigned)sceDisplayGetVcount();
    }
    TmcDiag_Stage(TMC_DIAG_DRAW, TmcSys_TimeUs() - tDraw);
}

/* ---- UI drawing helpers ---- */
void TmcGfx_FillRect(int x, int y, int w, int h, uint32_t rgba) {
    vita2d_draw_rectangle((float)x, (float)y, (float)w, (float)h,
                          RGBA8((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF));
}

static unsigned ToVita(uint32_t rgba) {
    return RGBA8((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF);
}

int TmcGfx_Text(int x, int y, float size, uint32_t rgba, const char* text) {
    if (!sFont) return 0;
    /* vita2d_pgf positions text by its baseline. */
    vita2d_pgf_draw_text(sFont, x, y + (int)(17.0f * size), ToVita(rgba), size, text);
    return vita2d_pgf_text_width(sFont, size, text);
}

int TmcGfx_TextWidth(float size, const char* text) {
    return sFont ? vita2d_pgf_text_width(sFont, size, text) : 0;
}

/* Arbitrary RGBA panel (overlay). One reusable texture per size. */
static vita2d_texture* sPanelTex;
static int sPanelW, sPanelH;

void TmcGfx_DrawPixels(const uint32_t* pixels, int width, int height, int pitch, int x, int y, float scale,
                       bool smooth, uint8_t alpha) {
    TmcGfx_DrawPixelsCached(pixels, width, height, pitch, x, y, scale, smooth, alpha, true);
}

void TmcGfx_DrawPixelsCached(const uint32_t* pixels, int width, int height, int pitch, int x, int y, float scale,
                             bool smooth, uint8_t alpha, bool changed) {
    if (!sPanelTex || sPanelW != width || sPanelH != height) {
        changed = true;
        if (sPanelTex) {
            vita2d_wait_rendering_done();
            vita2d_free_texture(sPanelTex);
        }
        sPanelTex = vita2d_create_empty_texture_format(width, height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
        sPanelW = width;
        sPanelH = height;
        if (!sPanelTex) return;
    }
    if (changed) {
        uint8_t* dst = (uint8_t*)vita2d_texture_get_datap(sPanelTex);
        const unsigned stride = vita2d_texture_get_stride(sPanelTex);
        for (int row = 0; row < height; ++row)
            CopyRowOpaque((uint32_t*)(dst + (size_t)row * stride), pixels + (size_t)row * pitch, width);
    }
    const SceGxmTextureFilter f = smooth ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_filters(sPanelTex, f, f);
    vita2d_draw_texture_tint_scale(sPanelTex, (float)x, (float)y, scale, scale, RGBA8(255, 255, 255, alpha));
}

/* Blocking message screen used for fatal errors (ROM missing, etc.). */
void TmcVitaVideo_ShowMessage(const char* title, const char* message) {
    if (!sReady && !TmcVitaVideo_Init()) return;
    for (int frame = 0; frame < 60 * 60; ++frame) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        TmcGfx_FillRect(0, 0, 960, 544, 0x101828FF);
        TmcGfx_Text(40, 40, 1.4f, 0xFFD060FF, title);
        int y = 100;
        const char* p = message;
        char line[256];
        while (*p) {
            int n = 0;
            while (p[n] && p[n] != '\n' && n < 255) ++n;
            memcpy(line, p, (size_t)n);
            line[n] = 0;
            TmcGfx_Text(40, y, 1.0f, 0xFFFFFFFF, line);
            y += 28;
            p += n;
            if (*p == '\n') ++p;
        }
        TmcGfx_Text(40, 500, 0.9f, 0xA0A0A0FF, "Das Programm beendet sich in einer Minute. / Exiting in one minute.");
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
}
