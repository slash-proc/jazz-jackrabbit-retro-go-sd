/* Files of OpenJazz on the Game & Watch: see jj_file.h */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "jj_file.h"
#include "jj_platform.h"

/* "path/of/NAME.EXT" -> "NAME.EXT" in upper case */
static void base_name(const char *path, char *out, size_t out_size)
{
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\')
            b = p + 1;
    size_t i = 0;
    for (; b[i] && i + 1 < out_size; i++)
        out[i] = (char)toupper((unsigned char)b[i]);
    out[i] = '\0';
}

static void real_path(const char *path, char *out, size_t out_size)
{
    char name[64];
    base_name(path, name, sizeof(name));
    snprintf(out, out_size, "%s/%s", jj_plat_save_dir(), name);
}

JJFILE *jjf_open(const char *path, const char *mode)
{
    char name[64];
    base_name(path, name, sizeof(name));
    bool write = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
    const uint8_t *data;
    uint32_t size;
    if (!write && jj_plat_find_file(name, &data, &size)) {
        JJFILE *f = (JJFILE *)jj_calloc(1, sizeof(JJFILE));
        if (!f)
            return NULL;
        f->mem = data;
        f->size = size;
        return f;
    }
#ifndef JJ_GWRG_WASM
    char full[256];
    real_path(path, full, sizeof(full));
    FILE *fp = fopen(full, mode);
    if (!fp)
        return NULL;
    JJFILE *f = (JJFILE *)jj_calloc(1, sizeof(JJFILE));
    if (!f) {
        fclose(fp);
        return NULL;
    }
    f->fp = fp;
    return f;
#else
    (void)path;
    return NULL;
#endif
}

int jjf_close(JJFILE *f)
{
    if (!f)
        return 0;
    if (f->fp)
        fclose(f->fp);
    jj_free(f);
    return 0;
}

long jjf_tell(JJFILE *f)
{
    return f->fp ? ftell(f->fp) : (long)f->pos;
}

int jjf_seek(JJFILE *f, long offset, int whence)
{
    if (f->fp)
        return fseek(f->fp, offset, whence);
    long p = whence == SEEK_SET ? offset : whence == SEEK_CUR ? (long)f->pos + offset : (long)f->size + offset;
    if (p < 0)
        return -1;
    f->pos = (uint32_t)p;
    return 0;
}

size_t jjf_read(void *buf, size_t size, size_t n, JJFILE *f)
{
    if (f->fp)
        return fread(buf, size, n, f->fp);
    if (size == 0 || f->pos >= f->size)
        return 0;
    size_t avail = (f->size - f->pos) / size;
    if (n > avail)
        n = avail;
    memcpy(buf, f->mem + f->pos, n * size);
    f->pos += (uint32_t)(n * size);
    return n;
}

size_t jjf_write(const void *buf, size_t size, size_t n, JJFILE *f)
{
    return f->fp ? fwrite(buf, size, n, f->fp) : 0;
}

int jjf_getc(JJFILE *f)
{
    if (f->fp)
        return fgetc(f->fp);
    if (f->pos >= f->size) {
        f->pos++; /* like a read past the end: tell() moves on */
        return EOF;
    }
    return f->mem[f->pos++];
}

int jjf_putc(int c, JJFILE *f)
{
    return f->fp ? fputc(c, f->fp) : EOF;
}

int jjf_access(const char *path, int mode)
{
    (void)mode;
    char name[64];
    base_name(path, name, sizeof(name));
    const uint8_t *data;
    uint32_t size;
    if (jj_plat_find_file(name, &data, &size))
        return 0;
#ifndef JJ_GWRG_WASM
    char full[256];
    real_path(path, full, sizeof(full));
    FILE *fp = fopen(full, "rb");
    if (!fp)
        return -1;
    fclose(fp);
    return 0;
#else
    return -1;
#endif
}

const uint8_t *jjf_mapped(JJFILE *f)
{
    return (f && f->mem) ? f->mem + f->pos : NULL;
}
