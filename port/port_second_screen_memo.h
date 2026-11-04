#ifndef PORT_SECOND_SCREEN_MEMO_H
#define PORT_SECOND_SCREEN_MEMO_H

/*
 * Pixel memo for the second-screen / overlay compositor.
 *
 * The panel is repainted from scratch every time, but most of what it paints
 * (backdrop lattice, stone slabs and wells, button plates, the stitched map at
 * a given zoom and scroll) comes out the same paint after paint. This module
 * keeps the finished pixels of such primitives, keyed on EVERY input that
 * decides them, and hands them back on the next paint instead of redrawing.
 *
 * Two kinds of entry, both exact (the key is compared word for word, so a hit
 * reproduces the original pixels byte for byte):
 *
 *  - Captured rect: the primitive was drawn into the surface as usual and then
 *    the rect it fully covers was copied out. Only for primitives that write
 *    EVERY pixel of that rect (their output never depends on what was under
 *    them). A hit copies the rect back. Keys carry absolute positions.
 *
 *  - Sprite: the primitive was rendered at the origin of a private zeroed
 *    buffer. Only for primitives whose every write is "store a non-zero value
 *    or leave the pixel alone", and whose output only depends on positions
 *    relative to its own origin (translation invariant, clipping only by the
 *    surface bounds). A hit stores the non-zero pixels at the requested origin,
 *    clipped to the surface — the same pixels the primitive would have written.
 *
 * Entries live in an LRU pool with a byte budget; anything that does not fit
 * simply isn't cached and the caller draws directly. Single painter thread.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SS_MEMO_KEY_WORDS 28

typedef struct {
    uint32_t w[SS_MEMO_KEY_WORDS];
    int32_t n;
    int32_t overflow; /* too many words: the key cannot be exact, never cache */
} SsMemoKey;

enum {
    SS_MEMO_KIND_BACKDROP = 1,
    SS_MEMO_KIND_SLICED,
    SS_MEMO_KIND_MENU_BUTTON,
    SS_MEMO_KIND_RING,
    SS_MEMO_KIND_DETAIL_MAP,
    SS_MEMO_KIND_WORLD_MAP,
};

static inline void SsMemoKey_Init(SsMemoKey* k, uint32_t kind) {
    memset(k, 0, sizeof(*k));
    k->w[0] = kind;
    k->n = 1;
}

static inline void SsMemoKey_U32(SsMemoKey* k, uint32_t v) {
    if (k->n < SS_MEMO_KEY_WORDS) {
        k->w[k->n++] = v;
    } else {
        k->overflow = 1;
    }
}

static inline void SsMemoKey_I32(SsMemoKey* k, int32_t v) {
    SsMemoKey_U32(k, (uint32_t)v);
}

/* Floats go in by bit pattern: equal bits <=> identical arithmetic later. */
static inline void SsMemoKey_F32(SsMemoKey* k, float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    SsMemoKey_U32(k, u);
}

static inline void SsMemoKey_Ptr(SsMemoKey* k, const void* p) {
    uint64_t v = (uint64_t)(uintptr_t)p;
    SsMemoKey_U32(k, (uint32_t)v);
    SsMemoKey_U32(k, (uint32_t)(v >> 32));
}

/* Whole string including its terminator, so "AB" and "ABC" never collide. */
static inline void SsMemoKey_Str(SsMemoKey* k, const char* s) {
    if (s == NULL) {
        SsMemoKey_U32(k, 0xFFFFFFFFu);
        return;
    }
    for (;;) {
        uint32_t v = 0;
        int i, end = 0;
        for (i = 0; i < 4; i++) {
            uint8_t c = (uint8_t)s[0];
            v |= (uint32_t)c << (8 * i);
            if (c == 0) {
                end = 1;
                break;
            }
            s++;
        }
        SsMemoKey_U32(k, v);
        if (end || k->overflow) {
            return;
        }
    }
}

typedef struct SsMemoEntry SsMemoEntry;

/* Captured rect. Restore returns 1 (and has written the pixels) on a hit.
 * Capture copies [x0,x1)x[y0,y1) — already clipped to the surface — out of
 * the surface; it is a no-op when the key overflowed or the pool can't fit it. */
int Port_SecondScreenMemo_Restore(const SsMemoKey* k, uint32_t* pixels, int32_t stride);
void Port_SecondScreenMemo_Capture(const SsMemoKey* k, const uint32_t* pixels, int32_t stride, int32_t x0,
                                   int32_t y0, int32_t x1, int32_t y1);

/* Sprite. Find returns the cached sprite or NULL. On NULL, Begin hands out a
 * zeroed w x h buffer (stride w) to render into with the primitive's origin at
 * (originX, originY) — the buffer must hold everything the primitive can draw
 * — or NULL when it can't be cached; End seals it (cropped to its non-zero
 * pixels). Blit stores the non-zero pixels with the primitive's origin at
 * (x, y), clipped to [0,bufW)x[0,bufH). */
const SsMemoEntry* Port_SecondScreenMemo_FindSprite(const SsMemoKey* k);
uint32_t* Port_SecondScreenMemo_BeginSprite(const SsMemoKey* k, int32_t w, int32_t h, int32_t originX,
                                            int32_t originY);
const SsMemoEntry* Port_SecondScreenMemo_EndSprite(void);
void Port_SecondScreenMemo_BlitSprite(const SsMemoEntry* e, uint32_t* pixels, int32_t bufW, int32_t bufH,
                                      int32_t stride, int32_t x, int32_t y);

/* Drops everything (e.g. if the art a key only names by pointer changed). */
void Port_SecondScreenMemo_Clear(void);

/* Hit/miss counters, for logs and tests. */
void Port_SecondScreenMemo_Stats(uint32_t* hits, uint32_t* misses, uint32_t* bytes);

#endif
