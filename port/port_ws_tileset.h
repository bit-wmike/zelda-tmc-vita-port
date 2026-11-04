#ifndef PORT_WS_TILESET_H
#define PORT_WS_TILESET_H

/*
 * Tile-set swap areas in the wide views (Vita port).
 *
 * The Minish village swaps its whole BG tile set (and palettes) depending on
 * which part of the map touches the screen (minishVillageTileSetManager.c).
 * A 240x160 view never shows two parts with different sets; Breitbild and
 * Full View can, and the part whose set is not loaded showed foreign tiles.
 *
 * The manager reports its region table and which set VRAM holds. Each shadow
 * cell then gets a tile-set context for the renderer (cpu/mode1.h,
 * virtuappu_mode1_ws_alt_ctx): 0 = VRAM as loaded, otherwise an image of BG
 * VRAM with that cell's set (ROM data) plus the matching palettes.
 */

#include "global.h"

typedef struct PortWsTileSet {
    const u16* regions;        /* {group, x, y, w, h}..., terminated by 0xff */
    const u32* gfx;            /* per group 16 words: 8 x {gfx offset, VRAM address} */
    const u8* paletteGroups;   /* palette group per gfx group */
    u8 groupCount;
} PortWsTileSet;

#ifdef TMC_VITA
/* Manager, once per update. loadedGroup = set completely in VRAM, or 0xff. */
void Port_WsTileSet_Update(const PortWsTileSet* set, u32 loadedGroup);

/* Shadow producer (port_linked_stubs.c), once per VBlank. */
void Port_WsTileSet_BeginFrame(void);
u8* Port_WsTileSet_BeginShadow(int scrolling);
u8 Port_WsTileSet_Ctx(s32 worldCol, s32 worldRow);
void Port_WsTileSet_EndShadow(int bgIndex, u8* ctx);

/* Presenter, right before the frame is rendered. */
void Port_WsTileSet_PrepareRender(void);
#endif

#endif
