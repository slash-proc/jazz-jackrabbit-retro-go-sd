/*
 * Pre-decoded pictures ("baked" surfaces).
 *
 * OpenJazz decodes its pictures (tiles, sprites, fonts, menu screens) from
 * the compressed game files into RAM: about 700 KB in a level, more than the
 * console has. They never change once made, so tools/jj_pack.py runs the
 * game once on the PC (harness, option -B: every level, bonus level, planet,
 * scene and menu is loaded) and stores every picture made in SURFACES.BAK,
 * indexed by a hash of its pixels. On the console, a picture whose pixels
 * are found there is used in place from the flash and its RAM copy freed.
 * A picture that is not found simply stays in RAM.
 *
 * SURFACES.BAK (little endian):
 *   0x00 char magic[4] "GWBK"
 *   0x04 u32  count
 *   0x08 entries[count], sorted by hash:
 *          u64 hash, u16 w, u16 h, u32 offset (from the start of the file)
 *   ...  pixels (w x h bytes each, 4 byte aligned)
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "jj_platform.h"
#include "jj_bake.h"

#define BAKE_FILE "SURFACES.BAK"
#define ENTRY_SIZE 16

static const uint8_t *g_bake;
static uint32_t g_bake_size, g_count;
static bool g_loaded;

/* FNV-1a 64 of the pixels (row after row), then the size */
uint64_t jj_bake_hash_begin(void)
{
    return 1469598103934665603ull;
}

uint64_t jj_bake_hash_add(uint64_t x, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        x ^= p[i];
        x *= 1099511628211ull;
    }
    return x;
}

uint64_t jj_bake_hash_end(uint64_t x, int w, int h)
{
    x ^= ((uint64_t)(uint32_t)w << 32) | (uint32_t)h;
    x *= 1099511628211ull;
    return x;
}

uint64_t jj_bake_hash(const uint8_t *pixels, int w, int h, int pitch)
{
    uint64_t x = jj_bake_hash_begin();
    for (int y = 0; y < h; y++)
        x = jj_bake_hash_add(x, pixels + (size_t)y * pitch, (size_t)w);
    return jj_bake_hash_end(x, w, h);
}

static void load(void)
{
    g_loaded = true;
    const uint8_t *d;
    uint32_t s;
    if (!jj_plat_find_file(BAKE_FILE, &d, &s) || s < 8 || memcmp(d, "GWBK", 4) != 0)
        return;
    uint32_t count;
    memcpy(&count, d + 4, 4);
    if (8 + (uint64_t)count * ENTRY_SIZE > s)
        return;
    g_bake = d;
    g_bake_size = s;
    g_count = count;
}

const uint8_t *jj_bake_find(const uint8_t *pixels, int w, int h, int pitch)
{
    if (!g_loaded)
        load();
    if (!g_count || w <= 0 || h <= 0 || (uint32_t)w * h < JJ_BAKE_MIN_PIXELS)
        return NULL;
    return jj_bake_lookup(jj_bake_hash(pixels, w, h, pitch), w, h);
}

const uint8_t *jj_bake_lookup(uint64_t hash, int w, int h)
{
    if (!g_loaded)
        load();
    if (!g_count)
        return NULL;
    uint32_t lo = 0, hi = g_count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        const uint8_t *e = g_bake + 8 + mid * ENTRY_SIZE;
        uint64_t eh;
        memcpy(&eh, e, 8);
        if (eh < hash)
            lo = mid + 1;
        else
            hi = mid;
    }
    for (uint32_t i = lo; i < g_count; i++) {
        const uint8_t *e = g_bake + 8 + i * ENTRY_SIZE;
        uint64_t eh;
        memcpy(&eh, e, 8);
        if (eh != hash)
            break;
        uint16_t ew, eh16;
        uint32_t off;
        memcpy(&ew, e + 8, 2);
        memcpy(&eh16, e + 10, 2);
        memcpy(&off, e + 12, 4);
        if (ew == w && eh16 == h && off + (uint32_t)w * h <= g_bake_size)
            return g_bake + off;
    }
    return NULL;
}

/* ---- recording (harness, option -B) ---- */
#ifndef JJ_DEVICE
typedef struct { uint64_t hash; uint16_t w, h; uint8_t *pixels; } rec_t;
static rec_t *g_recs;
static uint32_t g_nrecs, g_caprecs;
static bool g_recording;

void jj_bake_start_recording(void)
{
    g_recording = true;
}

bool jj_bake_recording(void)
{
    return g_recording;
}

void jj_bake_record(const uint8_t *pixels, int w, int h, int pitch)
{
    if (!g_recording || w <= 0 || h <= 0 || (uint32_t)w * h < JJ_BAKE_MIN_PIXELS)
        return;
    uint64_t hash = jj_bake_hash(pixels, w, h, pitch);
    for (uint32_t i = 0; i < g_nrecs; i++)
        if (g_recs[i].hash == hash && g_recs[i].w == w && g_recs[i].h == h)
            return;
    if (g_nrecs == g_caprecs) {
        g_caprecs = g_caprecs ? g_caprecs * 2 : 256;
        g_recs = (rec_t *)(realloc)(g_recs, g_caprecs * sizeof(rec_t));
    }
    rec_t *r = &g_recs[g_nrecs++];
    r->hash = hash;
    r->w = (uint16_t)w;
    r->h = (uint16_t)h;
    r->pixels = (uint8_t *)(malloc)((size_t)w * h);
    for (int y = 0; y < h; y++)
        memcpy(r->pixels + (size_t)y * w, pixels + (size_t)y * pitch, w);
}

static int cmp_rec(const void *a, const void *b)
{
    const rec_t *x = (const rec_t *)a, *y = (const rec_t *)b;
    return x->hash < y->hash ? -1 : x->hash > y->hash;
}

int jj_bake_build_memory(uint8_t **data, uint32_t *size)
{
    if (!data || !size)
        return -1;
    qsort(g_recs, g_nrecs, sizeof(rec_t), cmp_rec);
    uint64_t total = 8 + (uint64_t)g_nrecs * ENTRY_SIZE;
    for (uint32_t i = 0; i < g_nrecs; i++) {
        uint32_t n = (uint32_t)g_recs[i].w * g_recs[i].h;
        total += (n + 3u) & ~3u;
    }
    if (total > UINT32_MAX)
        return -1;
    uint8_t *out = (uint8_t *)malloc((size_t)total);
    if (!out)
        return -1;
    memcpy(out, "GWBK", 4);
    memcpy(out + 4, &g_nrecs, 4);
    uint32_t offset = 8 + g_nrecs * ENTRY_SIZE;
    uint32_t cursor = offset;
    for (uint32_t i = 0; i < g_nrecs; i++) {
        uint8_t *e = out + 8 + i * ENTRY_SIZE;
        memcpy(e, &g_recs[i].hash, 8);
        memcpy(e + 8, &g_recs[i].w, 2);
        memcpy(e + 10, &g_recs[i].h, 2);
        memcpy(e + 12, &cursor, 4);
        uint32_t n = (uint32_t)g_recs[i].w * g_recs[i].h;
        cursor += (n + 3u) & ~3u;
    }
    cursor = offset;
    uint64_t pixels = 0;
    for (uint32_t i = 0; i < g_nrecs; i++) {
        uint32_t n = (uint32_t)g_recs[i].w * g_recs[i].h;
        memcpy(out + cursor, g_recs[i].pixels, n);
        cursor += n;
        while (cursor & 3u)
            out[cursor++] = 0;
        pixels += n;
    }
    *data = out;
    *size = (uint32_t)total;
#ifndef JJ_GWRG_WASM
    printf("bake: %u pictures, %llu bytes\n", g_nrecs, (unsigned long long)pixels);
#endif
    return 0;
}

#ifndef JJ_GWRG_WASM
int jj_bake_write(const char *path)
{
    uint8_t *data = NULL;
    uint32_t size = 0;
    if (jj_bake_build_memory(&data, &size) != 0)
        return -1;
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(data);
        return -1;
    }
    fwrite(data, 1, size, f);
    fclose(f);
    free(data);
    return 0;
}
#endif
#endif
