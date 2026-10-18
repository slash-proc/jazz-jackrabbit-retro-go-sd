/* Pre-decoded pictures in the pack (jj_bake.c) */
#ifndef JJ_BAKE_H
#define JJ_BAKE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* smaller pictures stay in RAM (not worth an index entry) */
#define JJ_BAKE_MIN_PIXELS 64

uint64_t jj_bake_hash(const uint8_t *pixels, int w, int h, int pitch);
/* the same, piece by piece (pixels row after row) */
uint64_t jj_bake_hash_begin(void);
uint64_t jj_bake_hash_add(uint64_t x, const uint8_t *p, size_t n);
uint64_t jj_bake_hash_end(uint64_t x, int w, int h);
const uint8_t *jj_bake_lookup(uint64_t hash, int w, int h);
/* the same pixels in the pack (read only), NULL if not there */
const uint8_t *jj_bake_find(const uint8_t *pixels, int w, int h, int pitch);

#ifndef JJ_DEVICE
void jj_bake_start_recording(void);
bool jj_bake_recording(void);
void jj_bake_record(const uint8_t *pixels, int w, int h, int pitch);
int  jj_bake_build_memory(uint8_t **data, uint32_t *size);
int  jj_bake_write(const char *path);
#endif

#ifdef __cplusplus
}
#endif

#endif
