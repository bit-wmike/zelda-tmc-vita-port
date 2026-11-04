/*
 * Tile-set swap areas in the wide views (Vita port), see port_ws_tileset.h.
 */
#include "port_ws_tileset.h"

#ifdef TMC_VITA
#include "common.h"
#include "fade.h"
#include "main.h"
#include "room.h"
#include "port_gba_mem.h"
#include "cpu/mode1.h"

#include <stdbool.h>
#include <string.h>

extern const u8* gGlobalGfxAndPalettes;

enum {
    kMaxGroups = 8,
    kMapCells = 128,       /* gMapData*Special: 128 x 128 cells */
    kGbaViewW = 240,
    kGbaViewH = 160,
    kBlockSize = 0x1000,   /* the managers load 4 KiB blocks */
    kPages = MODE1_WS_ALT_VRAM_SIZE / kBlockSize,
};

static const PortWsTileSet* sSet;
static u32 sRoomKey = 0xffffffffu; /* room of sSet / sCellGroup */
static u8 sLoadedGroup = 0xff;
static u8 sCellGroup[kMapCells * kMapCells]; /* 0xff = same in every set */

/* Per frame (BeginFrame .. PrepareRender). */
static bool sFrameActive;
static u8 sGroupCtx[kMaxGroups];
static u8 sCtxGroup[MODE1_WS_ALT_MAX];
static int sCtxCount;
static int sShadowCount;
static u8 sShadowCtx[2][MODE1_WS_SHADOW_ROWS * MODE1_WS_SHADOW_COLS];

/* Context images; a slot keeps its group's blocks between frames. */
static u8 sAltVram[MODE1_WS_ALT_MAX][MODE1_WS_ALT_VRAM_SIZE] __attribute__((aligned(64)));
static u8 sAltVramGroup[MODE1_WS_ALT_MAX] = { 0xff, 0xff, 0xff };
static u16 sAltPalette[MODE1_WS_ALT_MAX][256];

/* Palettes of each group's palette group (read once from the ROM). */
static u16 sGroupPalette[kMaxGroups][256];
static u16 sGroupPaletteBanks[kMaxGroups];
static u8 sGroupPaletteId[kMaxGroups];
static bool sGroupPaletteValid[kMaxGroups];

static u32 CurrentRoomKey(void) {
    return ((u32)gRoomControls.area << 8) | gRoomControls.room;
}

/* Which set the GBA shows each cell with. A cell inside a region rect needs
 * that region's set. Other cells may still differ between sets (rects do not
 * always cover their buildings completely): use the set(s) the manager picks
 * for 240x160 views that contain the cell, the nearest such region on ties. */
static void BuildCellGroups(const u16* regions, int roomW, int roomH) {
    static u8 viewGroup[kMapCells][kMapCells];
    static s32 cover[kMapCells + 1][kMapCells + 1];
    static u8 mask[kMapCells][kMapCells];
    const int cellsW = roomW / 8 < kMapCells ? roomW / 8 : kMapCells;
    const int cellsH = roomH / 8 < kMapCells ? roomH / 8 : kMapCells;
    const int viewsW = roomW > kGbaViewW ? (roomW - kGbaViewW) / 8 + 1 : 1;
    const int viewsH = roomH > kGbaViewH ? (roomH - kGbaViewH) / 8 + 1 : 1;
    const u16* r;
    int vx, vy, x, y, g;

    memset(sCellGroup, 0xff, sizeof(sCellGroup));
    memset(mask, 0, sizeof(mask));
    /* Same test as CheckRegionOnScreen with the GBA screen size. */
    for (vy = 0; vy < viewsH && vy < kMapCells; ++vy) {
        for (vx = 0; vx < viewsW && vx < kMapCells; ++vx) {
            viewGroup[vy][vx] = 0xff;
            for (r = regions; r[0] != 0xff; r += 5) {
                const u32 px = (u32)(vx * 8) - r[1] + kGbaViewW;
                const u32 py = (u32)(vy * 8) - r[2] + kGbaViewH;
                if (px < (u32)r[3] + kGbaViewW && py < (u32)r[4] + kGbaViewH) {
                    viewGroup[vy][vx] = (u8)r[0];
                    break;
                }
            }
        }
    }
    for (g = 0; g < kMaxGroups; ++g) {
        memset(cover, 0, sizeof(cover));
        int any = 0;
        for (vy = 0; vy < viewsH && vy < kMapCells; ++vy) {
            for (vx = 0; vx < viewsW && vx < kMapCells; ++vx) {
                if (viewGroup[vy][vx] != g) continue;
                const int x1 = vx + kGbaViewW / 8 < kMapCells ? vx + kGbaViewW / 8 : kMapCells;
                const int y1 = vy + kGbaViewH / 8 < kMapCells ? vy + kGbaViewH / 8 : kMapCells;
                ++cover[vy][vx];
                --cover[vy][x1];
                --cover[y1][vx];
                ++cover[y1][x1];
                any = 1;
            }
        }
        if (!any) continue;
        for (y = 0; y < kMapCells; ++y) {
            s32 run = 0;
            for (x = 0; x < kMapCells; ++x) {
                run += cover[y][x];
                cover[y][x] = run + (y > 0 ? cover[y - 1][x] : 0);
                if (cover[y][x] > 0) mask[y][x] |= (u8)(1u << g);
            }
        }
    }
    for (y = 0; y < cellsH; ++y) {
        for (x = 0; x < cellsW; ++x) {
            const int cx = x * 8 + 4, cy = y * 8 + 4;
            int best = 0xff;
            s32 bestDist = 0x7fffffff;
            for (r = regions; r[0] != 0xff; r += 5) {
                if (r[0] >= kMaxGroups) continue;
                const s32 dx = cx < r[1] ? r[1] - cx : (cx >= r[1] + r[3] ? cx - (r[1] + r[3] - 1) : 0);
                const s32 dy = cy < r[2] ? r[2] - cy : (cy >= r[2] + r[4] ? cy - (r[2] + r[4] - 1) : 0);
                if (dx == 0 && dy == 0) { /* inside: first region wins */
                    best = r[0];
                    break;
                }
                if (((mask[y][x] >> r[0]) & 1u) == 0) continue;
                if (dx + dy < bestDist) {
                    bestDist = dx + dy;
                    best = r[0];
                }
            }
            sCellGroup[y * kMapCells + x] = (u8)best;
        }
    }
}

void Port_WsTileSet_Update(const PortWsTileSet* set, u32 loadedGroup) {
    const u32 key = CurrentRoomKey();
    if (set == NULL || set->groupCount > kMaxGroups) {
        sSet = NULL;
        return;
    }
    if (sSet != set || sRoomKey != key) {
        sSet = set;
        sRoomKey = key;
        BuildCellGroups(set->regions, gRoomControls.width, gRoomControls.height);
    }
    sLoadedGroup = loadedGroup < set->groupCount ? (u8)loadedGroup : 0xff;
}

void Port_WsTileSet_BeginFrame(void) {
    for (int bg = 0; bg < MODE1_GBA_BG_COUNT; ++bg)
        virtuappu_mode1_ws_alt_ctx[bg] = NULL;
    memset(sGroupCtx, 0, sizeof(sGroupCtx));
    sCtxCount = 0;
    sShadowCount = 0;
    sFrameActive = sSet != NULL && gMain.task == TASK_GAME && sRoomKey == CurrentRoomKey();
}

u8* Port_WsTileSet_BeginShadow(int scrolling) {
    if (!sFrameActive || scrolling || sShadowCount >= 2)
        return NULL;
    return sShadowCtx[sShadowCount++];
}

u8 Port_WsTileSet_Ctx(s32 worldCol, s32 worldRow) {
    if ((u32)worldCol >= kMapCells || (u32)worldRow >= kMapCells)
        return 0;
    const u8 g = sCellGroup[worldRow * kMapCells + worldCol];
    if (g >= kMaxGroups || g == sLoadedGroup)
        return 0;
    if (sGroupCtx[g] == 0) {
        if (sCtxCount >= MODE1_WS_ALT_MAX)
            return 0; /* more sets on screen than contexts: VRAM as loaded */
        sCtxGroup[sCtxCount] = g;
        sGroupCtx[g] = (u8)++sCtxCount;
    }
    return sGroupCtx[g];
}

void Port_WsTileSet_EndShadow(int bgIndex, u8* ctx) {
    if (ctx != NULL && bgIndex >= 0 && bgIndex < MODE1_GBA_BG_COUNT)
        virtuappu_mode1_ws_alt_ctx[bgIndex] = ctx;
}

/* The palette RAM banks a palette group writes, and their colours. */
static bool LoadGroupPalette(u8 group) {
    const u8 id = sSet->paletteGroups[group];
    if (sGroupPaletteValid[group] && sGroupPaletteId[group] == id)
        return sGroupPaletteBanks[group] != 0;
    u16 saved[512]; /* BG and OBJ halves of the buffer */
    const u32 savedUsed = gUsedPalettes;
    memcpy(saved, gPaletteBuffer, sizeof(saved));
    gUsedPalettes = 0;
    LoadPaletteGroup(id);
    memcpy(sGroupPalette[group], gPaletteBuffer, sizeof(sGroupPalette[group]));
    sGroupPaletteBanks[group] = (u16)gUsedPalettes;
    memcpy(gPaletteBuffer, saved, sizeof(saved));
    gUsedPalettes = savedUsed;
    sGroupPaletteId[group] = id;
    sGroupPaletteValid[group] = true;
    return sGroupPaletteBanks[group] != 0;
}

void Port_WsTileSet_PrepareRender(void) {
    int k;
    bool used = false;
    for (int bg = 0; bg < MODE1_GBA_BG_COUNT; ++bg)
        used |= virtuappu_mode1_ws_alt_ctx[bg] != NULL;
    if (!used || sSet == NULL)
        sCtxCount = 0;
    for (k = 0; k < sCtxCount; ++k) {
        const u8 g = sCtxGroup[k];
        const u32* blocks = &sSet->gfx[g * 16];
        u16 covered = 0;
        int i;
        for (i = 0; i < 8; ++i) {
            const u32 off = blocks[i * 2 + 1] - 0x06000000u;
            if (off < MODE1_WS_ALT_VRAM_SIZE) covered |= (u16)(1u << (off / kBlockSize));
        }
        if (sAltVramGroup[k] != g) {
            for (i = 0; i < 8; ++i) {
                const u32 off = blocks[i * 2 + 1] - 0x06000000u;
                if (off + kBlockSize <= MODE1_WS_ALT_VRAM_SIZE)
                    memcpy(&sAltVram[k][off], &gGlobalGfxAndPalettes[blocks[i * 2]], kBlockSize);
            }
            sAltVramGroup[k] = g;
        }
        /* Everything the set does not replace comes from VRAM as it is now. */
        for (i = 0; i < kPages; ++i) {
            if (!((covered >> i) & 1u))
                memcpy(&sAltVram[k][i * kBlockSize], &gVram[i * kBlockSize], kBlockSize);
        }
        /* Palettes: RAM, with the set's banks replaced. A bank that differs
         * from the engine's buffer is being faded; keep RAM there. */
        memcpy(sAltPalette[k], gBgPltt, sizeof(sAltPalette[k]));
        if (LoadGroupPalette(g)) {
            for (int bank = 0; bank < 16; ++bank) {
                if (!((sGroupPaletteBanks[g] >> bank) & 1u)) continue;
                if (memcmp(&gBgPltt[bank * 16], &gPaletteBuffer[bank * 16], 32) != 0) continue;
                memcpy(&sAltPalette[k][bank * 16], &sGroupPalette[g][bank * 16], 32);
            }
        }
        virtuappu_mode1_ws_alt_vram[k] = sAltVram[k];
        virtuappu_mode1_ws_alt_bg_palette[k] = sAltPalette[k];
    }
    for (; k < MODE1_WS_ALT_MAX; ++k) {
        virtuappu_mode1_ws_alt_vram[k] = NULL;
        virtuappu_mode1_ws_alt_bg_palette[k] = NULL;
    }
    if (sCtxCount == 0) {
        for (int bg = 0; bg < MODE1_GBA_BG_COUNT; ++bg)
            virtuappu_mode1_ws_alt_ctx[bg] = NULL;
    }
}
#endif
