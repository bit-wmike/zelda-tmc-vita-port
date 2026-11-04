/*
 * Pixel memo for the second-screen / overlay compositor — see the header for
 * the contract each primitive has to meet to use it.
 */
#include "port_second_screen_memo.h"

#include <stdlib.h>

#ifndef SS_MEMO_BUDGET_BYTES
#if defined(TMC_3DS) && !defined(TMC_VITA)
/* The 3DS panel is 320x240 (300 KiB a full surface). */
#define SS_MEMO_BUDGET_BYTES (3u * 1024u * 1024u)
#else
/* Vita: 960x544 is ~2 MiB a full surface; one map/items paint keeps ~5 MiB. */
#define SS_MEMO_BUDGET_BYTES (16u * 1024u * 1024u)
#endif
#endif

#define SS_MEMO_MAX_ENTRIES 64

struct SsMemoEntry {
    SsMemoKey key;
    int used;
    int sprite;
    uint32_t lastUse;
    int32_t x0, y0, w, h; /* captured rect (x0/y0 unused for sprites) */
    uint32_t* px;
    uint32_t* rowSpan;  /* sprites: per row, index of its first span; [h] = total */
    uint16_t* spans;    /* sprites: (start, length) pairs of non-zero runs */
    size_t bytes;
};

static SsMemoEntry sEntries[SS_MEMO_MAX_ENTRIES];
static size_t sBytes;
static uint32_t sClock;
static uint32_t sHits, sMisses;
static SsMemoEntry sPending; /* sprite being rendered (scratch, not in the pool) */

static int KeyEq(const SsMemoKey* a, const SsMemoKey* b) {
    return a->n == b->n && memcmp(a->w, b->w, sizeof(uint32_t) * (size_t)a->n) == 0;
}

static void FreeEntry(SsMemoEntry* e) {
    free(e->px);
    free(e->rowSpan);
    free(e->spans);
    sBytes -= e->bytes;
    memset(e, 0, sizeof(*e));
}

static SsMemoEntry* Find(const SsMemoKey* k, int sprite) {
    int i;
    if (k->overflow) {
        return NULL;
    }
    for (i = 0; i < SS_MEMO_MAX_ENTRIES; i++) {
        SsMemoEntry* e = &sEntries[i];
        if (e->used && e->sprite == sprite && e->px != NULL && KeyEq(&e->key, k)) {
            e->lastUse = ++sClock;
            ++sHits;
            return e;
        }
    }
    ++sMisses;
    return NULL;
}

/* A free slot with room for `bytes`, evicting least-recently-used entries;
 * NULL when even an empty pool could not hold it. */
static SsMemoEntry* Reserve(size_t bytes) {
    if (bytes > SS_MEMO_BUDGET_BYTES) {
        return NULL;
    }
    for (;;) {
        SsMemoEntry* freeSlot = NULL;
        SsMemoEntry* oldest = NULL;
        int i;
        for (i = 0; i < SS_MEMO_MAX_ENTRIES; i++) {
            SsMemoEntry* e = &sEntries[i];
            if (!e->used) {
                if (freeSlot == NULL) freeSlot = e;
            } else if (oldest == NULL || (int32_t)(e->lastUse - oldest->lastUse) < 0) {
                oldest = e;
            }
        }
        if (freeSlot != NULL && sBytes + bytes <= SS_MEMO_BUDGET_BYTES) {
            return freeSlot;
        }
        if (oldest == NULL) {
            return NULL;
        }
        FreeEntry(oldest);
    }
}

int Port_SecondScreenMemo_Restore(const SsMemoKey* k, uint32_t* pixels, int32_t stride) {
    const SsMemoEntry* e = Find(k, 0);
    int32_t y;
    if (e == NULL) {
        return 0;
    }
    for (y = 0; y < e->h; y++) {
        memcpy(pixels + (size_t)(e->y0 + y) * (size_t)stride + e->x0, e->px + (size_t)y * (size_t)e->w,
               sizeof(uint32_t) * (size_t)e->w);
    }
    return 1;
}

void Port_SecondScreenMemo_Capture(const SsMemoKey* k, const uint32_t* pixels, int32_t stride, int32_t x0,
                                   int32_t y0, int32_t x1, int32_t y1) {
    SsMemoEntry* e;
    size_t bytes;
    int32_t y, w = x1 - x0, h = y1 - y0;
    if (k->overflow || w <= 0 || h <= 0) {
        return;
    }
    bytes = sizeof(uint32_t) * (size_t)w * (size_t)h;
    e = Reserve(bytes);
    if (e == NULL) {
        return;
    }
    e->px = (uint32_t*)malloc(bytes);
    if (e->px == NULL) {
        return;
    }
    for (y = 0; y < h; y++) {
        memcpy(e->px + (size_t)y * (size_t)w, pixels + (size_t)(y0 + y) * (size_t)stride + x0,
               sizeof(uint32_t) * (size_t)w);
    }
    e->key = *k;
    e->used = 1;
    e->sprite = 0;
    e->lastUse = ++sClock;
    e->x0 = x0;
    e->y0 = y0;
    e->w = w;
    e->h = h;
    e->bytes = bytes;
    sBytes += bytes;
}

const SsMemoEntry* Port_SecondScreenMemo_FindSprite(const SsMemoKey* k) {
    return Find(k, 1);
}

uint32_t* Port_SecondScreenMemo_BeginSprite(const SsMemoKey* k, int32_t w, int32_t h, int32_t originX,
                                            int32_t originY) {
    /* The render buffer is scratch outside the pool: only the cropped result
     * is charged against the budget (and only it can evict anything). */
    free(sPending.px);
    memset(&sPending, 0, sizeof(sPending));
    if (k->overflow || w <= 0 || h <= 0 || (size_t)w * (size_t)h > SS_MEMO_BUDGET_BYTES / sizeof(uint32_t)) {
        return NULL;
    }
    sPending.px = (uint32_t*)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (sPending.px == NULL) {
        return NULL;
    }
    sPending.key = *k;
    sPending.sprite = 1;
    sPending.x0 = -originX;
    sPending.y0 = -originY;
    sPending.w = w;
    sPending.h = h;
    return sPending.px;
}

const SsMemoEntry* Port_SecondScreenMemo_EndSprite(void) {
    SsMemoEntry t = sPending;
    SsMemoEntry* e;
    size_t nSpans = 0;
    int32_t x, y, bx0 = t.w, by0 = t.h, bx1 = 0, by1 = 0, cw, ch;
    memset(&sPending, 0, sizeof(sPending));
    if (t.px == NULL) {
        return NULL;
    }
    /* Crop to the drawn pixels: callers pad generously around what their
     * primitive can reach, and the padding has no business in the pool. */
    for (y = 0; y < t.h; y++) {
        const uint32_t* row = t.px + (size_t)y * (size_t)t.w;
        for (x = 0; x < t.w; x++) {
            if (row[x] != 0) {
                if (x < bx0) bx0 = x;
                if (x >= bx1) bx1 = x + 1;
                if (y < by0) by0 = y;
                by1 = y + 1;
            }
        }
    }
    if (bx1 <= bx0 || by1 <= by0) {
        bx0 = by0 = 0;
        bx1 = by1 = 1; /* nothing drawn: keep one (zero) pixel */
    }
    cw = bx1 - bx0;
    ch = by1 - by0;
    if (cw > 65535) {
        free(t.px);
        return NULL;
    }
    for (y = 0; y < ch; y++) {
        const uint32_t* row = t.px + (size_t)(by0 + y) * (size_t)t.w + bx0;
        for (x = 0; x < cw; x++) {
            if (row[x] != 0 && (x == 0 || row[x - 1] == 0)) {
                nSpans++;
            }
        }
    }
    e = Reserve(sizeof(uint32_t) * ((size_t)cw * (size_t)ch + (size_t)ch + 1) + sizeof(uint16_t) * 2 * nSpans);
    if (e == NULL) {
        free(t.px);
        return NULL;
    }
    e->px = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)cw * (size_t)ch);
    e->rowSpan = (uint32_t*)malloc(sizeof(uint32_t) * ((size_t)ch + 1));
    e->spans = (uint16_t*)malloc(sizeof(uint16_t) * 2 * (nSpans > 0 ? nSpans : 1));
    if (e->px == NULL || e->rowSpan == NULL || e->spans == NULL) {
        free(e->px);
        free(e->rowSpan);
        free(e->spans);
        memset(e, 0, sizeof(*e));
        free(t.px);
        return NULL;
    }
    nSpans = 0;
    for (y = 0; y < ch; y++) {
        const uint32_t* row = t.px + (size_t)(by0 + y) * (size_t)t.w + bx0;
        memcpy(e->px + (size_t)y * (size_t)cw, row, sizeof(uint32_t) * (size_t)cw);
        e->rowSpan[y] = (uint32_t)nSpans;
        for (x = 0; x < cw;) {
            int32_t start;
            if (row[x] == 0) {
                x++;
                continue;
            }
            start = x;
            while (x < cw && row[x] != 0) {
                x++;
            }
            e->spans[2 * nSpans] = (uint16_t)start;
            e->spans[2 * nSpans + 1] = (uint16_t)(x - start);
            nSpans++;
        }
    }
    e->rowSpan[ch] = (uint32_t)nSpans;
    free(t.px);
    e->key = t.key;
    e->used = 1;
    e->sprite = 1;
    e->lastUse = ++sClock;
    e->x0 = t.x0 + bx0;
    e->y0 = t.y0 + by0;
    e->w = cw;
    e->h = ch;
    e->bytes = sizeof(uint32_t) * ((size_t)cw * (size_t)ch + (size_t)ch + 1) + sizeof(uint16_t) * 2 * nSpans;
    sBytes += e->bytes;
    return e;
}

void Port_SecondScreenMemo_BlitSprite(const SsMemoEntry* e, uint32_t* pixels, int32_t bufW, int32_t bufH,
                                      int32_t stride, int32_t x, int32_t y) {
    int32_t r, r0, r1;
    if (e == NULL) {
        return;
    }
    x += e->x0;
    y += e->y0;
    r0 = y < 0 ? -y : 0;
    r1 = e->h;
    if (y + r1 > bufH) {
        r1 = bufH - y;
    }
    for (r = r0; r < r1; r++) {
        uint32_t* dst = pixels + (size_t)(y + r) * (size_t)stride;
        const uint32_t* src = e->px + (size_t)r * (size_t)e->w;
        uint32_t i;
        for (i = e->rowSpan[r]; i < e->rowSpan[r + 1]; i++) {
            int32_t a = x + e->spans[2 * i];
            int32_t b = a + e->spans[2 * i + 1];
            if (a < 0) a = 0;
            if (b > bufW) b = bufW;
            if (a < b) {
                memcpy(dst + a, src + (a - x), sizeof(uint32_t) * (size_t)(b - a));
            }
        }
    }
}

void Port_SecondScreenMemo_Clear(void) {
    int i;
    for (i = 0; i < SS_MEMO_MAX_ENTRIES; i++) {
        if (sEntries[i].used) {
            FreeEntry(&sEntries[i]);
        }
    }
}

void Port_SecondScreenMemo_Stats(uint32_t* hits, uint32_t* misses, uint32_t* bytes) {
    if (hits) *hits = sHits;
    if (misses) *misses = sMisses;
    if (bytes) *bytes = (uint32_t)sBytes;
}
