// GWRG processor ABI for the in-memory OpenJazz asset pipeline.
#include <algorithm>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <wasi/api.h>

#include "jj_pack.h"
#include "SDL.h"
#include "jj_sdl.h"
#include "jj_sound.h"

int oj_gnw_bake_memory(uint8_t **data, uint32_t *size);
extern "C" int jj_render_music_memory(void (*output)(const char *, const uint8_t *, uint32_t), int rate, bool adpcm);
extern "C" void jj_plat_fatal(const char *);
extern "C" void jj_heap_init(void *, size_t);
extern "C" void jj_heap_ignore_range(const void *, const void *);

struct CatalogEntry { uint8_t sha1[20]; const char *name; };
#include "catalog.inc"

extern "C" int jj_gwrg_printf(const char *, ...) { return 0; }
extern "C" int jj_gwrg_fprintf(FILE *, const char *, ...) { return 0; }
extern "C" int jj_gwrg_puts(const char *) { return 0; }
extern "C" __wasi_errno_t __wasi_fd_write(__wasi_fd_t, const __wasi_ciovec_t *, __wasi_size_t, __wasi_size_t *written) {
    if (written) *written=0;
    return 0;
}
extern "C" __wasi_errno_t __wasi_fd_read(__wasi_fd_t, const __wasi_iovec_t *, __wasi_size_t, __wasi_size_t *read) {
    if (read) *read=0;
    return 8;
}
extern "C" __wasi_errno_t __wasi_fd_close(__wasi_fd_t) { return 0; }
extern "C" __wasi_errno_t __wasi_fd_seek(__wasi_fd_t, __wasi_filedelta_t, __wasi_whence_t, __wasi_filesize_t *) { return 8; }
extern "C" time_t jj_gwrg_time(time_t *out) { if (out) *out=0; return 0; }
extern "C" struct tm *jj_gwrg_localtime(const time_t *) { static struct tm t={}; return &t; }
extern "C" size_t jj_gwrg_strftime(char *, size_t, const char *, const struct tm *) { return 0; }

extern "C" uint32_t jj_plat_time_ms(void) { static uint32_t ticks; return ++ticks; }
extern "C" void jj_plat_service(int) {}
extern "C" void jj_plat_present(const uint8_t *, int, int, int, const uint8_t *, uint32_t) {}
extern "C" int jj_plat_audio_rate(void) { return 22050; }
extern "C" bool jj_plat_find_file(const char *name, const uint8_t **data, uint32_t *size) { return jj_pack_find(name,data,size); }
extern "C" const char *jj_plat_save_dir(void) { return ""; }
extern "C" void *jj_plat_canvas_memory(size_t size) { return malloc(size); }
extern "C" void jj_plat_quit(void) { __builtin_trap(); }

struct Input { uint8_t *data; uint32_t size; };
struct File { std::string name; std::vector<uint8_t> data; };
struct Output { std::string name; std::vector<uint8_t> data; };

static std::vector<Input> inputs;
static std::vector<File> files;
static std::vector<Output> outputs;
static std::string error_text, warning_text;
static uint32_t stage = 0;
static uint8_t *base_pack = nullptr;
static uint32_t base_pack_size = 0;
static uint8_t *baked = nullptr;
static uint32_t baked_size = 0;
static constexpr const char *STAGES[] = {"Bake decoded pictures", "Render music", "Build Jazz.jj"};
extern "C" void jj_plat_fatal(const char *message) { error_text = message ? message : "OpenJazz fatal error"; __builtin_trap(); }
extern "C" void __wasm_call_ctors(void);
static void ensure_initialized() {
    static bool initialized=false;
    if (!initialized) { initialized=true; __wasm_call_ctors(); }
}

// Compact SHA-1 is used only to select a filename from the reviewed catalog;
// file contents still come directly from the browser's input buffers.
static uint32_t rol(uint32_t x, unsigned n) { return (x << n) | (x >> (32 - n)); }
static void sha1(const uint8_t *src, uint32_t size, uint8_t digest[20]) {
    uint32_t h[5] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    uint64_t bits = (uint64_t)size * 8;
    uint64_t padded = ((uint64_t)size + 9 + 63) & ~63ULL;
    std::vector<uint8_t> msg((size_t)padded, 0);
    if (size) memcpy(msg.data(), src, size);
    msg[size] = 0x80;
    for (int i=0;i<8;i++) msg[padded-1-i] = (uint8_t)(bits >> (i*8));
    for (uint64_t off=0; off<padded; off+=64) {
        uint32_t w[80];
        for (int i=0;i<16;i++) w[i]=((uint32_t)msg[off+i*4]<<24)|((uint32_t)msg[off+i*4+1]<<16)|((uint32_t)msg[off+i*4+2]<<8)|msg[off+i*4+3];
        for (int i=16;i<80;i++) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for (int i=0;i<80;i++) {
            uint32_t f,k;
            if(i<20){f=(b&c)|(~b&d);k=0x5a827999;}
            else if(i<40){f=b^c^d;k=0x6ed9eba1;}
            else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}
            else {f=b^c^d;k=0xca62c1d6;}
            uint32_t t=rol(a,5)+f+e+k+w[i];e=d;d=c;c=rol(b,30);b=a;a=t;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
    }
    for(int i=0;i<5;i++) for(int j=0;j<4;j++) digest[i*4+j]=(uint8_t)(h[i]>>(24-j*8));
}

static bool has_name(const std::string &name) {
    for (const auto &f : files) if (f.name == name) return true;
    return false;
}
static bool required_files() {
    const char *names[] = {"PANEL.000", "FONTMN1.0FN", "MENU.000", "SOUNDS.000"};
    for (auto n : names) if (!has_name(n)) return false;
    return true;
}
static void append32(std::vector<uint8_t> &v, uint32_t n) {
    for (int i=0;i<4;i++) v.push_back((uint8_t)(n >> (i*8)));
}
static std::vector<uint8_t> make_pack(std::vector<File> list) {
    std::sort(list.begin(), list.end(), [](const File &a, const File &b) { return a.name < b.name; });
    const uint32_t header = 16 + (uint32_t)list.size() * 32;
    uint64_t total = (header + 3u) & ~3u;
    for (const auto &f : list) total = (total + f.data.size() + 3u) & ~3ULL;
    if (total > UINT32_MAX) return {};
    std::vector<uint8_t> out((size_t)total, 0);
    memcpy(out.data(), JJ_PACK_MAGIC, 8);
    uint32_t count = (uint32_t)list.size(), whole = (uint32_t)total;
    memcpy(out.data()+8, &count, 4); memcpy(out.data()+12, &whole, 4);
    uint32_t pos = (header + 3u) & ~3u;
    for (uint32_t i=0;i<count;i++) {
        uint8_t *entry=out.data()+16+i*32;
        memcpy(entry,list[i].name.c_str(),std::min<size_t>(23,list[i].name.size()));
        memcpy(entry+24,&pos,4); uint32_t sz=(uint32_t)list[i].data.size(); memcpy(entry+28,&sz,4);
        if(sz) memcpy(out.data()+pos,list[i].data.data(),sz);
        pos=(pos+sz+3u)&~3u;
    }
    return out;
}
static void set_error(const char *s) { error_text = s; }
static void add_music(const char *name, const uint8_t *data, uint32_t size) {
    if (!has_name(name)) files.push_back({name, std::vector<uint8_t>(data, data + size)});
}

extern "C" uint32_t abi_version() { ensure_initialized(); return 1; }
extern "C" uint32_t alloc(uint32_t size) { ensure_initialized(); return (uint32_t)(uintptr_t)malloc(size ? size : 1); }
extern "C" void input_clear() {
    ensure_initialized();
    for (auto &i : inputs) free(i.data);
    inputs.clear(); files.clear(); outputs.clear(); error_text.clear(); warning_text.clear();
    free(base_pack); base_pack=nullptr; base_pack_size=0; free(baked); baked=nullptr; baked_size=0; stage=0;
}
extern "C" uint32_t input_add(uint8_t *data, uint32_t size) {
    ensure_initialized();
    inputs.push_back({data,size}); return (uint32_t)(inputs.size()-1);
}
extern "C" uint32_t run_begin(uint32_t flags) {
    ensure_initialized();
    outputs.clear(); files.clear(); error_text.clear(); warning_text.clear(); stage=0;
    free(base_pack); base_pack=nullptr; base_pack_size=0; free(baked); baked=nullptr; baked_size=0;
    if (flags) { set_error("Unknown conversion options."); return 2; }
    size_t unknown=0;
    for (const auto &input : inputs) {
        uint8_t hash[20]; sha1(input.data,input.size,hash); bool matched=false;
        for (const auto &entry : CATALOG) if (!memcmp(hash,entry.sha1,20)) {
            matched=true;
            if (!has_name(entry.name)) files.push_back({entry.name,std::vector<uint8_t>(input.data,input.data+input.size)});
        }
        if (!matched) unknown++;
    }
    if (unknown) warning_text = std::to_string(unknown) + " input file(s) did not match the known Jazz data catalog and were skipped.";
    if (!required_files()) { set_error("Required game data is missing: PANEL.000, FONTMN1.0FN, MENU.000, or SOUNDS.000."); return 4; }
    auto pack=make_pack(files);
    if(pack.empty()){set_error("Could not create the input pack.");return 1;}
    base_pack_size=(uint32_t)pack.size(); base_pack=(uint8_t*)malloc(pack.size());
    if(!base_pack){set_error("Out of memory while preparing game files.");return 1;}
    memcpy(base_pack,pack.data(),pack.size());
    if(jj_pack_open(base_pack,base_pack_size)!=JJ_PACK_OK){set_error("The game data pack is invalid.");return 1;}
    jj_heap_ignore_range(base_pack,base_pack+base_pack_size);
    return 0;
}
extern "C" uint32_t run_step() {
    ensure_initialized();
    if (!error_text.empty()) return 2;
    if (stage == 0) {
        jj_heap_init(nullptr, 0);
        jj_sound_enable(false, false);
        jj_sdl_map_button(0, SDLK_LEFT, SDLK_UNKNOWN);
        jj_sdl_map_button(1, SDLK_RIGHT, SDLK_UNKNOWN);
        jj_sdl_map_button(2, SDLK_UP, SDLK_UNKNOWN);
        jj_sdl_map_button(3, SDLK_DOWN, SDLK_UNKNOWN);
        jj_sdl_map_button(4, SDLK_LCTRL, SDLK_UNKNOWN);
        jj_sdl_map_button(5, SDLK_LALT, SDLK_UNKNOWN);
        jj_sdl_map_button(6, SDLK_ESCAPE, SDLK_UNKNOWN);
        jj_sdl_map_button(7, SDLK_LSHIFT, SDLK_UNKNOWN);
        if (oj_gnw_bake_memory(&baked,&baked_size)!=0 || !baked) { set_error("OpenJazz could not decode the game pictures."); return 2; }
        files.push_back({"SURFACES.BAK",std::vector<uint8_t>(baked,baked+baked_size)});
    } else if (stage == 1) {
        if (jj_render_music_memory(add_music,11025,true)<0) { set_error("OpenJazz could not render game music."); return 2; }
    } else if (stage == 2) {
        std::vector<File> final_files;
        for (const auto &f : files) if (f.name.size()<4 || f.name.substr(f.name.size()-4)!=".PSM") final_files.push_back(f);
        auto pack=make_pack(final_files);
        if(pack.empty()){set_error("Could not create Jazz.jj.");return 2;}
        outputs.push_back({"jazz-pack",std::move(pack)});
    } else return 0;
    stage++;
    return stage < 3 ? 1 : 0;
}
extern "C" uint32_t run(uint32_t flags) { uint32_t r=run_begin(flags);if(r)return r;do{r=run_step();}while(r==1);return r; }
extern "C" uint32_t stage_count(){ensure_initialized();return 3;}
extern "C" uint32_t stage_index(){ensure_initialized();return stage;}
extern "C" uint32_t stage_name_ptr(uint32_t i){ensure_initialized();return i<3?(uint32_t)(uintptr_t)STAGES[i]:0;}
extern "C" uint32_t stage_name_len(uint32_t i){ensure_initialized();return i<3?(uint32_t)strlen(STAGES[i]):0;}
extern "C" uint32_t output_count(){ensure_initialized();return (uint32_t)outputs.size();}
extern "C" uint32_t output_name_ptr(uint32_t i){ensure_initialized();return i<outputs.size()?(uint32_t)(uintptr_t)outputs[i].name.data():0;}
extern "C" uint32_t output_name_len(uint32_t i){ensure_initialized();return i<outputs.size()?(uint32_t)outputs[i].name.size():0;}
extern "C" uint32_t output_ptr(uint32_t i){ensure_initialized();return i<outputs.size()?(uint32_t)(uintptr_t)outputs[i].data.data():0;}
extern "C" uint32_t output_len(uint32_t i){ensure_initialized();return i<outputs.size()?(uint32_t)outputs[i].data.size():0;}
extern "C" uint32_t error_ptr(){ensure_initialized();return (uint32_t)(uintptr_t)error_text.data();}
extern "C" uint32_t error_len(){ensure_initialized();return (uint32_t)error_text.size();}
extern "C" uint32_t warnings_ptr(){ensure_initialized();return (uint32_t)(uintptr_t)warning_text.data();}
extern "C" uint32_t warnings_len(){ensure_initialized();return (uint32_t)warning_text.size();}
