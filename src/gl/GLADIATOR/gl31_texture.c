/* gl31_texture.c - texturas: objetos, almacenamiento (mutable e inmutable),
 * transferencia de pixeles, parametros y consultas.
 *
 * Targets de desktop sin equivalente directo en GLES, y como se emulan:
 *   GL_TEXTURE_1D           -> GL_TEXTURE_2D de alto 1
 *   GL_TEXTURE_RECTANGLE    -> GL_TEXTURE_2D (nivel 0, filtro/wrap restringidos; el
 *                              conversor GLSL normaliza las coordenadas)
 *   GL_TEXTURE_1D_ARRAY     -> GL_TEXTURE_2D_ARRAY con alto 1 (capas = layers)
 *   GL_TEXTURE_BUFFER       -> GL_TEXTURE_BUFFER del backend (ES 3.2 o EXT_texture_buffer)
 *
 * 1D/2D/RECT comparten el slot 2D del backend (y 1D_ARRAY/2D_ARRAY el 2D_ARRAY),
 * asi que aqui se llevan los bindings logicos por unidad y, solo cuando una
 * unidad tiene dos targets en conflicto, gl31_tex_sync_for_draw() elige antes de
 * cada draw el que corresponde al tipo de sampler que usa el programa.
 *
 * GLES 3.0 no tiene glGetTexLevelParameter ni texturas proxy, asi que se lleva
 * una tabla con las dimensiones/formato de cada nivel (hasta MAX_LV). */
#include "gl31.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif
#ifndef GL_TEXTURE_LOD_BIAS
#define GL_TEXTURE_LOD_BIAS 0x8501
#endif
#ifndef GL_DEPTH_TEXTURE_MODE
#define GL_DEPTH_TEXTURE_MODE 0x884B
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_CLAMP
#define GL_CLAMP 0x2900
#endif
#ifndef GL_DEPTH_COMPONENT32
#define GL_DEPTH_COMPONENT32 0x81A7
#endif
#ifndef GL_COMPRESSED_RGB
#define GL_COMPRESSED_RGB  0x84ED
#define GL_COMPRESSED_RGBA 0x84EE
#define GL_COMPRESSED_RED  0x8225
#define GL_COMPRESSED_RG   0x8226
#endif
#ifndef GL_UNPACK_SWAP_BYTES
#define GL_UNPACK_SWAP_BYTES 0x0CF0
#define GL_UNPACK_LSB_FIRST  0x0CF1
#define GL_PACK_SWAP_BYTES   0x0D00
#define GL_PACK_LSB_FIRST    0x0D01
#endif
#ifndef GL_TEXTURE_WIDTH
#define GL_TEXTURE_WIDTH           0x1000
#define GL_TEXTURE_HEIGHT          0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_TEXTURE_DEPTH           0x8071
#endif
#ifndef GL_TEXTURE_BORDER
#define GL_TEXTURE_BORDER 0x1005
#endif
#ifndef GL_TEXTURE_RED_SIZE
#define GL_TEXTURE_RED_SIZE   0x805C
#define GL_TEXTURE_GREEN_SIZE 0x805D
#define GL_TEXTURE_BLUE_SIZE  0x805E
#define GL_TEXTURE_ALPHA_SIZE 0x805F
#endif
#ifndef GL_TEXTURE_DEPTH_SIZE
#define GL_TEXTURE_DEPTH_SIZE 0x884A
#endif
#ifndef GL_TEXTURE_STENCIL_SIZE
#define GL_TEXTURE_STENCIL_SIZE 0x88F1
#endif
#ifndef GL_TEXTURE_SHARED_SIZE
#define GL_TEXTURE_SHARED_SIZE 0x8C3F
#endif
#ifndef GL_TEXTURE_RED_TYPE
#define GL_TEXTURE_RED_TYPE   0x8C10
#define GL_TEXTURE_GREEN_TYPE 0x8C11
#define GL_TEXTURE_BLUE_TYPE  0x8C12
#define GL_TEXTURE_ALPHA_TYPE 0x8C13
#define GL_TEXTURE_DEPTH_TYPE 0x8C16
#endif
#ifndef GL_TEXTURE_COMPRESSED
#define GL_TEXTURE_COMPRESSED            0x86A1
#endif
#ifndef GL_TEXTURE_COMPRESSED_IMAGE_SIZE
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE 0x86A0
#endif
#ifndef GL_TEXTURE_BUFFER_DATA_STORE_BINDING
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING 0x8C2D
#endif
#ifndef GL_TEXTURE_BINDING_1D
#define GL_TEXTURE_BINDING_1D 0x8068
#endif
#ifndef GL_TEXTURE_BINDING_1D_ARRAY
#define GL_TEXTURE_BINDING_1D_ARRAY 0x8C1C
#endif
#ifndef GL_TEXTURE_BINDING_RECTANGLE
#define GL_TEXTURE_BINDING_RECTANGLE 0x84F6
#endif
#ifndef GL_TEXTURE_BINDING_BUFFER
#define GL_TEXTURE_BINDING_BUFFER 0x8C2C
#endif
#ifndef GL_SAMPLER_1D_SHADOW
#define GL_SAMPLER_1D_SHADOW 0x8B61
#define GL_SAMPLER_1D_ARRAY_SHADOW 0x8DC3
#endif
#ifndef GL_MAX_ARRAY_TEXTURE_LAYERS
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF
#endif

#define MAX_LV 16
#define FACE_FIRST 0x8515u   /* GL_TEXTURE_CUBE_MAP_POSITIVE_X */
#define FACE_LAST  0x851Au   /* GL_TEXTURE_CUBE_MAP_NEGATIVE_Z */

/* slots logicos de binding por unidad */
enum { S_1D, S_2D, S_RECT, S_3D, S_CUBE, S_1DARR, S_2DARR, S_BUF, S_2DMS, S_2DMSARR, S_CUBEARR, S_COUNT };

typedef struct { GLsizei w, h, d; GLenum ifmt; } lvl_t;   /* 1D array: h = capas */
typedef struct {
    GLuint id;
    GLenum target;          /* target de desktop con el que se enlazo por primera vez (0 = ninguno) */
    lvl_t lv[MAX_LV];
    GLfloat bf[4];          /* TEXTURE_BORDER_COLOR (copia propia) */
    GLint   bi[4];
    int     bi_valid;       /* 1 = el ultimo valor se dio como entero (Iiv/Iuiv) */
} tex_t;
typedef struct { GLenum target; GLint level; lvl_t l; } proxy_t;

static tex_t* g_tex; static size_t g_nt, g_ct;           /* compartido entre contextos */
static GL31_TLS GLuint  g_bound[GL31_MAX_TEX_UNITS][S_COUNT];
static GL31_TLS GLuint  g_active_unit;
static GL31_TLS proxy_t g_proxy;
static GL31_TLS int     g_ambig;   /* alguna unidad tiene bindings en conflicto */

static int is_3d_target(GLenum t);

/* ---------- utilidades de target ---------- */
static int is_face(GLenum t) { return t >= FACE_FIRST && t <= FACE_LAST; }

static int is_proxy(GLenum t)
{
    return t == GL_PROXY_TEXTURE_1D || t == GL_PROXY_TEXTURE_2D || t == GL_PROXY_TEXTURE_3D ||
           t == GL_PROXY_TEXTURE_CUBE_MAP || t == GL_PROXY_TEXTURE_1D_ARRAY ||
           t == GL_PROXY_TEXTURE_2D_ARRAY || t == GL_PROXY_TEXTURE_RECTANGLE ||
           t == GL_PROXY_TEXTURE_2D_MULTISAMPLE || t == GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY ||
           t == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY;
}

static int slot_of(GLenum t)
{
    if (is_face(t)) t = GL_TEXTURE_CUBE_MAP;
    switch (t) {
        case GL_TEXTURE_1D:       return S_1D;
        case GL_TEXTURE_2D:       return S_2D;
        case GL_TEXTURE_RECTANGLE:return S_RECT;
        case GL_TEXTURE_3D:       return S_3D;
        case GL_TEXTURE_CUBE_MAP: return S_CUBE;
        case GL_TEXTURE_1D_ARRAY: return S_1DARR;
        case GL_TEXTURE_2D_ARRAY: return S_2DARR;
        case GL_TEXTURE_BUFFER:   return S_BUF;
        case GL_TEXTURE_2D_MULTISAMPLE:       return S_2DMS;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: return S_2DMSARR;
        case GL_TEXTURE_CUBE_MAP_ARRAY:       return S_CUBEARR;
    }
    return -1;
}

/* target del backend para un target de desktop (0 = no existe) */
GLenum gl31_tex_be_target(GLenum t)
{
    if (is_face(t)) return t;
    switch (t) {
        case GL_TEXTURE_1D: case GL_TEXTURE_2D: case GL_TEXTURE_RECTANGLE:
            return GL_TEXTURE_2D;
        case GL_TEXTURE_1D_ARRAY: case GL_TEXTURE_2D_ARRAY:
            return GL_TEXTURE_2D_ARRAY;
        case GL_TEXTURE_3D:       return GL_TEXTURE_3D;
        case GL_TEXTURE_CUBE_MAP: return GL_TEXTURE_CUBE_MAP;
        case GL_TEXTURE_BUFFER:   return gl31_caps.tex_buffer ? GL_TEXTURE_BUFFER : 0;
        case GL_TEXTURE_2D_MULTISAMPLE:
            return gl31_caps.multisample_tex ? GL_TEXTURE_2D_MULTISAMPLE : 0;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            return gl31_caps.ms_array ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : 0;
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            return gl31_caps.cube_array ? GL_TEXTURE_CUBE_MAP_ARRAY : 0;
    }
    return 0;
}

/* target valido para TexParameter / GetTexParameter / GenerateMipmap-like: sin caras ni BUFFER */
static int target_param_ok(GLenum t)
{
    int s = slot_of(t);
    if (s >= 0 && s != S_BUF && s != S_2DMS && s != S_2DMSARR && !is_face(t)) return 1;
    gl31_set_error(GL_INVALID_ENUM);
    return 0;
}

static void warn_no_target(GLenum t)
{
    if (t == GL_TEXTURE_BUFFER)
        gl31_stub_warn("GL_TEXTURE_BUFFER (backend sin texture buffers: ES 3.2 o EXT_texture_buffer)");
    else if (t == GL_TEXTURE_CUBE_MAP_ARRAY)
        gl31_stub_warn("GL_TEXTURE_CUBE_MAP_ARRAY (backend sin ES 3.2 / texture_cube_map_array)");
    else if (t == GL_TEXTURE_2D_MULTISAMPLE)
        gl31_stub_warn("GL_TEXTURE_2D_MULTISAMPLE (backend sin ES 3.1)");
    else
        gl31_stub_warn("GL_TEXTURE_2D_MULTISAMPLE_ARRAY (backend sin OES_texture_storage_multisample_2d_array)");
}

static int max_levels(GLsizei a, GLsizei b, GLsizei c)
{
    GLsizei m = a > b ? a : b;
    int n = 1;
    if (c > m) m = c;
    while (m > 1) { m >>= 1; n++; }
    return n;
}

static GLint max_tex_size(void)
{
    GLint m = 2048;
    BE(glGetIntegerv)(GL_MAX_TEXTURE_SIZE, &m);
    return m;
}

static GLint max_layers(void)
{
    GLint m = 256;
    BE(glGetIntegerv)(GL_MAX_ARRAY_TEXTURE_LAYERS, &m);
    return m;
}

/* ---------- tabla de niveles ---------- */
static tex_t* tex_find(GLuint id)
{
    size_t i;
    for (i = 0; i < g_nt; i++) if (g_tex[i].id == id) return &g_tex[i];
    return NULL;
}

static tex_t* tex_get(GLuint id)
{
    tex_t* t = tex_find(id);
    if (t) return t;
    if (g_nt == g_ct) {
        size_t c = g_ct ? g_ct * 2 : 64;
        tex_t* p = (tex_t*)realloc(g_tex, c * sizeof *p);
        if (!p) return NULL;
        g_tex = p; g_ct = c;
    }
    t = &g_tex[g_nt++];
    memset(t, 0, sizeof *t);
    t->id = id;
    return t;
}

static tex_t* cur_tex(GLenum target)
{
    int s = slot_of(target);
    GLuint id;
    if (s < 0) return NULL;
    id = g_bound[g_active_unit][s];
    return id ? tex_get(id) : NULL;
}

static void record_level(GLenum target, GLint level, GLsizei w, GLsizei h, GLsizei d, GLenum ifmt)
{
    tex_t* t;
    if (level < 0 || level >= MAX_LV) return;
    t = cur_tex(target);
    if (!t) return;
    t->lv[level].w = w; t->lv[level].h = h; t->lv[level].d = d; t->lv[level].ifmt = ifmt;
}

static void record_storage(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d)
{
    int l;
    for (l = 0; l < levels && l < MAX_LV; l++) {
        GLsizei lw = w >> l, lh = h >> l, ld = d;
        if (target == GL_TEXTURE_3D) ld = d >> l;
        if (target == GL_TEXTURE_1D) lh = 1;
        if (target == GL_TEXTURE_1D_ARRAY) lh = h;          /* capas: no se reducen */
        record_level(target, l, lw ? lw : 1, lh ? lh : 1, ld ? ld : 1, ifmt);
    }
}

/* ---------- formatos ---------- */
/* Desktop acepta internalformat sin tamano + FLOAT; ES exige formato con tamano. */
static GLenum resolve_ifmt(GLint ifmt, GLenum type)
{
    GLenum f = (GLenum)ifmt;
    switch (ifmt) {
        case 1: f = GL_RED; break;
        case 2: f = GL_RG; break;
        case 3: f = GL_RGB; break;
        case 4: f = GL_RGBA; break;
        case GL_COMPRESSED_RGB:  f = GL_RGB; break;
        case GL_COMPRESSED_RGBA: f = GL_RGBA; break;
        case GL_COMPRESSED_RED:  f = GL_RED; break;
        case GL_COMPRESSED_RG:   f = GL_RG; break;
        case GL_DEPTH_COMPONENT32: return GL_DEPTH_COMPONENT32F;
    }
    if (type == GL_FLOAT) {
        switch (f) {
            case GL_RED: return GL_R32F;
            case GL_RG:  return GL_RG32F;
            case GL_RGB: return GL_RGB32F;
            case GL_RGBA: return GL_RGBA32F;
            case GL_DEPTH_COMPONENT: return GL_DEPTH_COMPONENT32F;
        }
    } else if (type == GL_HALF_FLOAT) {
        switch (f) {
            case GL_RED: return GL_R16F;
            case GL_RG:  return GL_RG16F;
            case GL_RGB: return GL_RGB16F;
            case GL_RGBA: return GL_RGBA16F;
        }
    }
    /* ES 3.0 no admite DEPTH_COMPONENT / DEPTH_STENCIL sin tamano como internalformat */
    switch (f) {
        case GL_DEPTH_COMPONENT:
            return type == GL_UNSIGNED_SHORT ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24;
        case GL_DEPTH_STENCIL:
            return type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8;
    }
    return f;
}

static GLenum map_ifmt(GLenum f)
{
    if (f == GL_DEPTH_COMPONENT32) return GL_DEPTH_COMPONENT32F;
    return f;
}

/* ---------- caracteristicas de un formato interno (consultas de nivel sin backend ES 3.1) ---------- */
typedef struct {
    GLenum f;
    unsigned char r, g, b, a, d, s, shared;
    GLenum type;     /* tipo de los canales de color (o de profundidad) */
} fmtinfo_t;

#define T_UN GL_UNSIGNED_NORMALIZED
#define T_SN GL_SIGNED_NORMALIZED
#define T_FL GL_FLOAT
#define T_IN GL_INT
#define T_UI GL_UNSIGNED_INT

static const fmtinfo_t k_fmt[] = {
    { GL_RED, 8,0,0,0,0,0,0, T_UN }, { GL_RG, 8,8,0,0,0,0,0, T_UN },
    { GL_RGB, 8,8,8,0,0,0,0, T_UN }, { GL_RGBA, 8,8,8,8,0,0,0, T_UN },
    { GL_R8, 8,0,0,0,0,0,0, T_UN }, { GL_R8_SNORM, 8,0,0,0,0,0,0, T_SN },
    { GL_RG8, 8,8,0,0,0,0,0, T_UN }, { GL_RG8_SNORM, 8,8,0,0,0,0,0, T_SN },
    { GL_RGB8, 8,8,8,0,0,0,0, T_UN }, { GL_RGB8_SNORM, 8,8,8,0,0,0,0, T_SN },
    { GL_RGBA8, 8,8,8,8,0,0,0, T_UN }, { GL_RGBA8_SNORM, 8,8,8,8,0,0,0, T_SN },
    { GL_SRGB8, 8,8,8,0,0,0,0, T_UN }, { GL_SRGB8_ALPHA8, 8,8,8,8,0,0,0, T_UN },
    { GL_RGB565, 5,6,5,0,0,0,0, T_UN }, { GL_RGBA4, 4,4,4,4,0,0,0, T_UN },
    { GL_RGB5_A1, 5,5,5,1,0,0,0, T_UN }, { GL_RGB10_A2, 10,10,10,2,0,0,0, T_UN },
    { GL_RGB10_A2UI, 10,10,10,2,0,0,0, T_UI },
    { GL_R11F_G11F_B10F, 11,11,10,0,0,0,0, T_FL }, { GL_RGB9_E5, 9,9,9,0,0,0,5, T_FL },
    { GL_R16F, 16,0,0,0,0,0,0, T_FL }, { GL_RG16F, 16,16,0,0,0,0,0, T_FL },
    { GL_RGB16F, 16,16,16,0,0,0,0, T_FL }, { GL_RGBA16F, 16,16,16,16,0,0,0, T_FL },
    { GL_R32F, 32,0,0,0,0,0,0, T_FL }, { GL_RG32F, 32,32,0,0,0,0,0, T_FL },
    { GL_RGB32F, 32,32,32,0,0,0,0, T_FL }, { GL_RGBA32F, 32,32,32,32,0,0,0, T_FL },
    { GL_R8I, 8,0,0,0,0,0,0, T_IN }, { GL_R8UI, 8,0,0,0,0,0,0, T_UI },
    { GL_R16I, 16,0,0,0,0,0,0, T_IN }, { GL_R16UI, 16,0,0,0,0,0,0, T_UI },
    { GL_R32I, 32,0,0,0,0,0,0, T_IN }, { GL_R32UI, 32,0,0,0,0,0,0, T_UI },
    { GL_RG8I, 8,8,0,0,0,0,0, T_IN }, { GL_RG8UI, 8,8,0,0,0,0,0, T_UI },
    { GL_RG16I, 16,16,0,0,0,0,0, T_IN }, { GL_RG16UI, 16,16,0,0,0,0,0, T_UI },
    { GL_RG32I, 32,32,0,0,0,0,0, T_IN }, { GL_RG32UI, 32,32,0,0,0,0,0, T_UI },
    { GL_RGB8I, 8,8,8,0,0,0,0, T_IN }, { GL_RGB8UI, 8,8,8,0,0,0,0, T_UI },
    { GL_RGB16I, 16,16,16,0,0,0,0, T_IN }, { GL_RGB16UI, 16,16,16,0,0,0,0, T_UI },
    { GL_RGB32I, 32,32,32,0,0,0,0, T_IN }, { GL_RGB32UI, 32,32,32,0,0,0,0, T_UI },
    { GL_RGBA8I, 8,8,8,8,0,0,0, T_IN }, { GL_RGBA8UI, 8,8,8,8,0,0,0, T_UI },
    { GL_RGBA16I, 16,16,16,16,0,0,0, T_IN }, { GL_RGBA16UI, 16,16,16,16,0,0,0, T_UI },
    { GL_RGBA32I, 32,32,32,32,0,0,0, T_IN }, { GL_RGBA32UI, 32,32,32,32,0,0,0, T_UI },
    { GL_DEPTH_COMPONENT, 0,0,0,0,24,0,0, T_UN }, { GL_DEPTH_COMPONENT16, 0,0,0,0,16,0,0, T_UN },
    { GL_DEPTH_COMPONENT24, 0,0,0,0,24,0,0, T_UN }, { GL_DEPTH_COMPONENT32F, 0,0,0,0,32,0,0, T_FL },
    { GL_DEPTH24_STENCIL8, 0,0,0,0,24,8,0, T_UN }, { GL_DEPTH32F_STENCIL8, 0,0,0,0,32,8,0, T_FL },
    { GL_DEPTH_STENCIL, 0,0,0,0,24,8,0, T_UN },
};
#define N_FMT ((int)(sizeof k_fmt / sizeof k_fmt[0]))

static const fmtinfo_t* fmt_lookup(GLenum f)
{
    int i;
    for (i = 0; i < N_FMT; i++) if (k_fmt[i].f == f) return &k_fmt[i];
    return NULL;
}

/* formato de transferencia valido para reservar un nivel con datos NULL */
static int pair_for_ifmt(GLenum rf, GLenum* format, GLenum* type)
{
    const fmtinfo_t* fi = fmt_lookup(rf);
    int nc;
    if (!fi || fi->d || fi->shared) return 0;
    nc = (fi->r != 0) + (fi->g != 0) + (fi->b != 0) + (fi->a != 0);
    *type = GL_UNSIGNED_BYTE;
    if (fi->type == T_FL) *type = fi->r > 16 ? GL_FLOAT : GL_HALF_FLOAT;
    if (fi->type == T_IN) *type = GL_INT;
    if (fi->type == T_UI) *type = GL_UNSIGNED_INT;
    if (fi->type == T_SN) *type = GL_BYTE;
    if (fi->type == T_IN || fi->type == T_UI)
        *format = nc == 1 ? GL_RED_INTEGER : nc == 2 ? GL_RG_INTEGER : nc == 3 ? GL_RGB_INTEGER : GL_RGBA_INTEGER;
    else
        *format = nc == 1 ? GL_RED : nc == 2 ? GL_RG : nc == 3 ? GL_RGB : GL_RGBA;
    if (rf == GL_RGB565) *type = GL_UNSIGNED_SHORT_5_6_5;
    else if (rf == GL_RGBA4) *type = GL_UNSIGNED_SHORT_4_4_4_4;
    else if (rf == GL_RGB5_A1) *type = GL_UNSIGNED_SHORT_5_5_5_1;
    else if (rf == GL_RGB10_A2) *type = GL_UNSIGNED_INT_2_10_10_10_REV;
    else if (rf == GL_RGB10_A2UI) *type = GL_UNSIGNED_INT_2_10_10_10_REV;
    return 1;
}

/* ---------- ambiguedad entre targets que comparten el slot del backend ---------- */
static void note_ambiguity(int unit, int slot)
{
    const GLuint* b = g_bound[unit];
    if (!b[slot]) return;
    if (slot == S_1D || slot == S_2D || slot == S_RECT) {
        if ((slot != S_1D && b[S_1D]) || (slot != S_2D && b[S_2D]) || (slot != S_RECT && b[S_RECT]))
            g_ambig = 1;
    } else if (slot == S_1DARR || slot == S_2DARR) {
        if (b[S_1DARR] && b[S_2DARR]) g_ambig = 1;
    }
}

/* slot logico al que corresponde un tipo de uniform sampler (tipo desktop), o -1 */
static int sampler_slot(GLenum ty)
{
    switch (ty) {
        case GL_SAMPLER_1D: case GL_INT_SAMPLER_1D: case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_SAMPLER_1D_SHADOW:
            return S_1D;
        case GL_SAMPLER_2D_RECT: case GL_INT_SAMPLER_2D_RECT: case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
            return S_RECT;
        case GL_SAMPLER_2D: case GL_INT_SAMPLER_2D: case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW:
            return S_2D;
        case GL_SAMPLER_1D_ARRAY: case GL_INT_SAMPLER_1D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_1D_ARRAY_SHADOW:
            return S_1DARR;
        case GL_SAMPLER_2D_ARRAY: case GL_INT_SAMPLER_2D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
            return S_2DARR;
    }
    return -1;
}

/* para cada unidad, el slot logico que el programa muestrea (-1 = ninguno) */
static void program_unit_slots(GLuint prog, signed char sel[GL31_MAX_TEX_UNITS])
{
    GLint n = 0, i;
    memset(sel, -1, GL31_MAX_TEX_UNITS);
    BE(glGetProgramiv)(prog, GL_ACTIVE_UNIFORMS, &n);
    for (i = 0; i < n; i++) {
        char name[256], base[256];
        GLsizei len = 0;
        GLint sz = 0, k;
        GLenum ty = 0;
        int slot;
        size_t bl;
        name[0] = 0;
        BE(glGetActiveUniform)(prog, (GLuint)i, (GLsizei)sizeof name, &len, &sz, &ty, name);
        slot = sampler_slot(gl31_program_sampler_type(prog, name, ty));
        if (slot < 0) continue;
        bl = strlen(name);
        memcpy(base, name, bl + 1);
        if (bl > 3 && strcmp(base + bl - 3, "[0]") == 0) base[bl - 3] = 0;
        for (k = 0; k < (sz > 0 ? sz : 1); k++) {
            char un[300];
            GLint loc, v = -1;
            if (sz > 1) { snprintf(un, sizeof un, "%s[%d]", base, (int)k); loc = BE(glGetUniformLocation)(prog, un); }
            else        loc = BE(glGetUniformLocation)(prog, base);
            if (loc < 0) continue;
            BE(glGetUniformiv)(prog, loc, &v);
            if (v >= 0 && v < GL31_MAX_TEX_UNITS) sel[v] = (signed char)slot;
        }
    }
}

void gl31_tex_sync_for_draw(void)
{
    gl31_state_t* st;
    signed char sel[GL31_MAX_TEX_UNITS];
    int u, any = 0, have_sel = 0, touched = 0;

    if (!g_ambig) return;
    st = gl31_state();
    for (u = 0; u < GL31_MAX_TEX_UNITS; u++) {
        const GLuint* b = g_bound[u];
        int ga = (b[S_1D] != 0) + (b[S_2D] != 0) + (b[S_RECT] != 0);
        int gb = (b[S_1DARR] != 0) + (b[S_2DARR] != 0);
        int sl;
        if (ga < 2 && gb < 2) continue;
        any = 1;
        if (!st->program) continue;
        if (!have_sel) { program_unit_slots(st->program, sel); have_sel = 1; }
        sl = sel[u];
        if (sl < 0 || !b[sl]) continue;
        touched = 1;
        BE(glActiveTexture)(GL_TEXTURE0 + (GLenum)u);
        BE(glBindTexture)((sl == S_1DARR || sl == S_2DARR) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D, b[sl]);
    }
    if (touched) BE(glActiveTexture)(GL_TEXTURE0 + g_active_unit);
    g_ambig = any;
}

/* TEXTURE_BINDING_* segun los bindings logicos de la app (1 = servido aqui) */
int gl31_tex_get_binding(GLenum pname, GLint* out)
{
    int s;
    switch (pname) {
        case GL_TEXTURE_BINDING_1D:           s = S_1D; break;
        case GL_TEXTURE_BINDING_2D:           s = S_2D; break;
        case GL_TEXTURE_BINDING_RECTANGLE:    s = S_RECT; break;
        case GL_TEXTURE_BINDING_3D:           s = S_3D; break;
        case GL_TEXTURE_BINDING_CUBE_MAP:     s = S_CUBE; break;
        case GL_TEXTURE_BINDING_1D_ARRAY:     s = S_1DARR; break;
        case GL_TEXTURE_BINDING_2D_ARRAY:     s = S_2DARR; break;
        case GL_TEXTURE_BINDING_BUFFER:       s = S_BUF; break;
        case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:       s = S_CUBEARR; break;
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE:       s = S_2DMS; break;
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY: s = S_2DMSARR; break;
        default: return 0;
    }
    *out = (GLint)g_bound[g_active_unit][s];
    return 1;
}

/* ---------- objetos ---------- */
void gl31_glGenTextures(GLsizei n, GLuint* t)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenTextures)(n, t);
}

void gl31_glDeleteTextures(GLsizei n, const GLuint* t)
{
    GLsizei i; int u, s;
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        size_t k;
        if (!t[i]) continue;
        for (k = 0; k < g_nt; k++)
            if (g_tex[k].id == t[i]) { g_tex[k] = g_tex[--g_nt]; break; }
        for (u = 0; u < GL31_MAX_TEX_UNITS; u++)
            for (s = 0; s < S_COUNT; s++) if (g_bound[u][s] == t[i]) g_bound[u][s] = 0;
    }
    BE(glDeleteTextures)(n, t);
}

void gl31_glBindTexture(GLenum target, GLuint texture)
{
    int s = slot_of(target);
    GLenum be;
    int first = 0;
    if (s < 0 || is_face(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    be = gl31_tex_be_target(target);
    if (!be) { warn_no_target(target); gl31_set_error(GL_INVALID_ENUM); return; }
    if (texture) {
        tex_t* t = tex_get(texture);
        if (!t) { gl31_set_error(GL_OUT_OF_MEMORY); return; }
        if (t->target && t->target != target) { gl31_set_error(GL_INVALID_OPERATION); return; }
        if (!t->target) { t->target = target; first = 1; }
    }
    g_bound[g_active_unit][s] = texture;
    note_ambiguity((int)g_active_unit, s);
    BE(glBindTexture)(be, texture);
    if (first && target == GL_TEXTURE_RECTANGLE) {
        /* valores iniciales de GL para texturas rectangulo */
        BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void gl31_glActiveTexture(GLenum unit)
{
    gl31_state_load_limits();
    if (unit < GL_TEXTURE0 || (GLint)(unit - GL_TEXTURE0) >= gl31_state()->max_tex_units) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    g_active_unit = unit - GL_TEXTURE0;
    BE(glActiveTexture)(unit);
}

GLboolean gl31_glIsTexture(GLuint texture) { return texture ? BE(glIsTexture)(texture) : GL_FALSE; }

/* ---------- parametros ---------- */
static int is_lod_param(GLenum p) { return p == GL_TEXTURE_MIN_LOD || p == GL_TEXTURE_MAX_LOD; }

/* sin equivalente en ES (BORDER_COLOR se maneja aparte) */
static int ignored_param(GLenum p)
{
    return p == GL_TEXTURE_LOD_BIAS || p == GL_DEPTH_TEXTURE_MODE || p == GL_GENERATE_MIPMAP;
}

/* restricciones de GL_TEXTURE_RECTANGLE; 1 = valido */
static int rect_param_ok(GLenum pname, GLint param)
{
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            if (param != GL_NEAREST && param != GL_LINEAR) { gl31_set_error(GL_INVALID_ENUM); return 0; }
            break;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            if (param != GL_CLAMP_TO_EDGE && param != GL_CLAMP_TO_BORDER && param != GL_CLAMP) {
                gl31_set_error(GL_INVALID_ENUM);
                return 0;
            }
            break;
        case GL_TEXTURE_BASE_LEVEL:
            if (param != 0) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
            break;
        default:
            break;
    }
    return 1;
}

static void param_i(GLenum target, GLenum pname, GLint param)
{
    if (ignored_param(pname)) return;
    if (target == GL_TEXTURE_RECTANGLE && !rect_param_ok(pname, param)) return;
    switch (pname) {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
            if (param == GL_CLAMP) param = GL_CLAMP_TO_EDGE;
            else if (param == GL_CLAMP_TO_BORDER && !gl31_caps.border_clamp) param = GL_CLAMP_TO_EDGE;
            break;
        default:
            break;
    }
    BE(glTexParameteri)(gl31_tex_be_target(target), pname, param);
}

/* ---- color de borde: copia propia + backend si soporta clamp-to-border ---- */
static void set_border_f(GLenum target, const GLfloat* v)
{
    tex_t* t = cur_tex(target);
    if (t) { memcpy(t->bf, v, sizeof t->bf); t->bi_valid = 0; }
    if (gl31_caps.border_clamp) BE(glTexParameterfv)(gl31_tex_be_target(target), GL_TEXTURE_BORDER_COLOR, v);
}

static void set_border_i(GLenum target, const GLint* v, int is_unsigned)
{
    tex_t* t = cur_tex(target);
    if (t) { memcpy(t->bi, v, sizeof t->bi); t->bi_valid = 1; }
    if (gl31_caps.border_clamp) {
        GLenum be = gl31_tex_be_target(target);
        if (is_unsigned) BE(glTexParameterIuiv)(be, GL_TEXTURE_BORDER_COLOR, (const GLuint*)v);
        else             BE(glTexParameterIiv)(be, GL_TEXTURE_BORDER_COLOR, v);
    }
}

/* glTexParameteriv: los enteros se convierten linealmente a [-1,1] */
static void set_border_iv(GLenum target, const GLint* v)
{
    GLfloat f[4];
    int k;
    for (k = 0; k < 4; k++) {
        double c = (2.0 * (double)v[k] + 1.0) / 4294967295.0;
        f[k] = (GLfloat)(c < -1.0 ? -1.0 : c > 1.0 ? 1.0 : c);
    }
    set_border_f(target, f);
}

static void border_as_float(const tex_t* t, GLfloat out[4])
{
    int k;
    for (k = 0; k < 4; k++) out[k] = !t ? 0.f : (t->bi_valid ? (GLfloat)t->bi[k] : t->bf[k]);
}

static void border_as_int(const tex_t* t, GLint out[4])
{
    int k;
    for (k = 0; k < 4; k++) {
        if (!t) out[k] = 0;
        else if (t->bi_valid) out[k] = t->bi[k];
        else {
            double c = ((double)t->bf[k] * 4294967295.0 - 1.0) / 2.0;
            out[k] = (GLint)(c < -2147483648.0 ? -2147483648.0 : c > 2147483647.0 ? 2147483647.0 : c);
        }
    }
}

void gl31_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    if (!target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { gl31_set_error(GL_INVALID_ENUM); return; }   /* parametro vectorial */
    param_i(target, pname, param);
}

void gl31_glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    if (!target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (is_lod_param(pname)) { BE(glTexParameterf)(gl31_tex_be_target(target), pname, param); return; }
    param_i(target, pname, (GLint)param);
}

void gl31_glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    if (!params) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { set_border_f(target, params); return; }
    gl31_glTexParameterf(target, pname, params[0]);
}

/* GL_TEXTURE_SWIZZLE_RGBA (ARB_texture_swizzle) no existe en ES: son cuatro parametros */
static void swizzle_rgba(GLenum target, const GLint* v)
{
    static const GLenum pn[4] = { GL_TEXTURE_SWIZZLE_R, GL_TEXTURE_SWIZZLE_G, GL_TEXTURE_SWIZZLE_B, GL_TEXTURE_SWIZZLE_A };
    int k;
    for (k = 0; k < 4; k++) param_i(target, pn[k], v[k]);
}

void gl31_glTexParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    if (!params) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { set_border_iv(target, params); return; }
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) { swizzle_rgba(target, params); return; }
    if (is_lod_param(pname)) gl31_glTexParameterf(target, pname, (GLfloat)params[0]);
    else                     param_i(target, pname, params[0]);
}

void gl31_glTexParameterIiv(GLenum target, GLenum pname, const GLint* params)
{
    if (!params) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { set_border_i(target, params, 0); return; }
    gl31_glTexParameteriv(target, pname, params);
}

void gl31_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint* params)
{
    GLint v;
    if (!params) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { set_border_i(target, (const GLint*)params, 1); return; }
    v = (GLint)params[0];
    gl31_glTexParameteriv(target, pname, &v);
}

void gl31_glGetTexParameteriv(GLenum target, GLenum pname, GLint* params)
{
    if (!params || !target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { border_as_int(cur_tex(target), params); return; }
    if (ignored_param(pname)) { params[0] = 0; return; }      /* valores por defecto de GL */
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        static const GLenum pn[4] = { GL_TEXTURE_SWIZZLE_R, GL_TEXTURE_SWIZZLE_G, GL_TEXTURE_SWIZZLE_B, GL_TEXTURE_SWIZZLE_A };
        int k;
        for (k = 0; k < 4; k++) BE(glGetTexParameteriv)(gl31_tex_be_target(target), pn[k], &params[k]);
        return;
    }
    BE(glGetTexParameteriv)(gl31_tex_be_target(target), pname, params);
}

void gl31_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
{
    if (!params || !target_param_ok(target)) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) { border_as_float(cur_tex(target), params); return; }
    if (ignored_param(pname)) { params[0] = 0.f; return; }
    BE(glGetTexParameterfv)(gl31_tex_be_target(target), pname, params);
}

void gl31_glGetTexParameterIiv(GLenum target, GLenum pname, GLint* params)
{
    gl31_glGetTexParameteriv(target, pname, params);
}

void gl31_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint* params)
{
    GLint v[4] = {0, 0, 0, 0};
    int k, n = (pname == GL_TEXTURE_BORDER_COLOR) ? 4 : 1;
    gl31_glGetTexParameteriv(target, pname, v);
    if (params) for (k = 0; k < n; k++) params[k] = (GLuint)v[k];
}

/* ---------- texturas proxy ---------- */
/* Solo comprueban si las dimensiones caben. El resultado se consulta con
 * glGetTexLevelParameter(PROXY_..., WIDTH). */
static void proxy_set(GLenum target, GLint level, GLsizei w, GLsizei h, GLsizei d, GLenum ifmt)
{
    GLint m = max_tex_size();
    int ok = level >= 0 && level < MAX_LV && w >= 0 && h >= 0 && d >= 0;
    if (ok) {
        GLint lim = m >> level;
        ok = w <= lim;
        switch (target) {
            case GL_PROXY_TEXTURE_1D:        break;
            case GL_PROXY_TEXTURE_1D_ARRAY:  ok = ok && h <= max_layers(); break;
            case GL_PROXY_TEXTURE_2D:        ok = ok && h <= lim; break;
            case GL_PROXY_TEXTURE_RECTANGLE: ok = ok && level == 0 && h <= m; break;
            case GL_PROXY_TEXTURE_CUBE_MAP:  ok = ok && h <= lim && w == h; break;
            case GL_PROXY_TEXTURE_3D:        ok = ok && h <= lim && d <= lim; break;
            case GL_PROXY_TEXTURE_2D_ARRAY:  ok = ok && h <= lim && d <= max_layers(); break;
            case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY: ok = ok && h <= lim && w == h && d <= max_layers() && d % 6 == 0; break;
            case GL_PROXY_TEXTURE_2D_MULTISAMPLE:       ok = ok && level == 0 && h <= m; break;
            case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY: ok = ok && level == 0 && h <= m && d <= max_layers(); break;
            default:                         ok = 0;
        }
    }
    memset(&g_proxy, 0, sizeof g_proxy);
    g_proxy.target = target; g_proxy.level = level;
    if (ok) { g_proxy.l.w = w; g_proxy.l.h = h; g_proxy.l.d = d; g_proxy.l.ifmt = ifmt; }
}

/* ---------- consultas por nivel (emuladas; nativas si el backend es ES 3.1+) ---------- */
void gl31_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params)
{
    const lvl_t* l = NULL;
    const fmtinfo_t* fi;
    lvl_t zero;
    GLenum qt = 0;
    int proxy;
    memset(&zero, 0, sizeof zero);
    if (!params) return;
    if (level < 0) { gl31_set_error(GL_INVALID_VALUE); return; }

    proxy = is_proxy(target);
    if (proxy) {
        if (g_proxy.target == target && g_proxy.level == level) l = &g_proxy.l;
    } else {
        tex_t* t;
        if (slot_of(target) < 0) { gl31_set_error(GL_INVALID_ENUM); return; }
        t = cur_tex(target);
        if (t && level < MAX_LV) l = &t->lv[level];
        qt = gl31_tex_be_target(target);
    }
    if (!l) l = &zero;

    switch (pname) {
        case GL_TEXTURE_WIDTH:           *params = l->w; return;
        case GL_TEXTURE_HEIGHT:          *params = l->h; return;
        case GL_TEXTURE_DEPTH:           *params = l->d; return;
        case GL_TEXTURE_INTERNAL_FORMAT: *params = (GLint)l->ifmt; return;
        case GL_TEXTURE_BORDER:          *params = 0; return;
        default: break;
    }

    if (gl31_caps.level_query && !proxy && qt &&
        (l->w > 0 || target == GL_TEXTURE_BUFFER)) {
        BE(glGetTexLevelParameteriv)(qt, level, pname, params);
        return;
    }

    fi = fmt_lookup(l->ifmt);
    switch (pname) {
        case GL_TEXTURE_RED_SIZE:     *params = fi ? fi->r : 0; return;
        case GL_TEXTURE_GREEN_SIZE:   *params = fi ? fi->g : 0; return;
        case GL_TEXTURE_BLUE_SIZE:    *params = fi ? fi->b : 0; return;
        case GL_TEXTURE_ALPHA_SIZE:   *params = fi ? fi->a : 0; return;
        case GL_TEXTURE_DEPTH_SIZE:   *params = fi ? fi->d : 0; return;
        case GL_TEXTURE_STENCIL_SIZE: *params = fi ? fi->s : 0; return;
        case GL_TEXTURE_SHARED_SIZE:  *params = fi ? fi->shared : 0; return;
        case GL_TEXTURE_RED_TYPE:     *params = (fi && fi->r) ? (GLint)fi->type : (GLint)GL_NONE; return;
        case GL_TEXTURE_GREEN_TYPE:   *params = (fi && fi->g) ? (GLint)fi->type : (GLint)GL_NONE; return;
        case GL_TEXTURE_BLUE_TYPE:    *params = (fi && fi->b) ? (GLint)fi->type : (GLint)GL_NONE; return;
        case GL_TEXTURE_ALPHA_TYPE:   *params = (fi && fi->a) ? (GLint)fi->type : (GLint)GL_NONE; return;
        case GL_TEXTURE_DEPTH_TYPE:   *params = (fi && fi->d) ? (GLint)fi->type : (GLint)GL_NONE; return;
        case GL_TEXTURE_COMPRESSED:
            /* un formato que no es de la tabla de formatos sin comprimir se toma como comprimido */
            *params = (l->ifmt && !fi) ? GL_TRUE : GL_FALSE;
            return;
        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
            gl31_stub_warn("glGetTexLevelParameter(COMPRESSED_IMAGE_SIZE) sin backend ES 3.1");
            gl31_set_error(GL_INVALID_OPERATION);
            return;
        default:
            gl31_set_error(GL_INVALID_ENUM);
    }
}

void gl31_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params)
{
    GLint v = 0;
    if (!params) return;
    gl31_glGetTexLevelParameteriv(target, level, pname, &v);
    *params = (GLfloat)v;
}

/* ---------- almacenamiento inmutable ---------- */
void gl31_glTexStorage1D(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w)
{
    if (target == GL_PROXY_TEXTURE_1D) { proxy_set(target, 0, w, 1, 1, map_ifmt(ifmt)); return; }
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (levels < 1 || w < 1 || levels > max_levels(w, 1, 1)) { gl31_set_error(GL_INVALID_VALUE); return; }
    ifmt = map_ifmt(ifmt);
    BE(glTexStorage2D)(GL_TEXTURE_2D, levels, ifmt, w, 1);
    record_storage(GL_TEXTURE_1D, levels, ifmt, w, 1, 1);
}

void gl31_glTexStorage2D(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h)
{
    if (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_CUBE_MAP ||
        target == GL_PROXY_TEXTURE_1D_ARRAY || target == GL_PROXY_TEXTURE_RECTANGLE) {
        proxy_set(target, 0, w, h, 1, map_ifmt(ifmt));
        return;
    }
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP &&
        target != GL_TEXTURE_RECTANGLE && target != GL_TEXTURE_1D_ARRAY) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (levels < 1 || w < 1 || h < 1 ||
        levels > (target == GL_TEXTURE_1D_ARRAY ? max_levels(w, 1, 1) : max_levels(w, h, 1)) ||
        (target == GL_TEXTURE_RECTANGLE && levels != 1)) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    ifmt = map_ifmt(ifmt);
    if (target == GL_TEXTURE_1D_ARRAY) BE(glTexStorage3D)(GL_TEXTURE_2D_ARRAY, levels, ifmt, w, 1, h);
    else                               BE(glTexStorage2D)(gl31_tex_be_target(target), levels, ifmt, w, h);
    record_storage(target, levels, ifmt, w, h, 1);
}

void gl31_glTexStorage3D(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d)
{
    if (target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_2D_ARRAY || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY) {
        proxy_set(target, 0, w, h, d, map_ifmt(ifmt));
        return;
    }
    if (!is_3d_target(target)) {
        if (target == GL_TEXTURE_CUBE_MAP_ARRAY) warn_no_target(target);
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY && (w != h || d % 6)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (levels < 1 || w < 1 || h < 1 || d < 1 ||
        levels > max_levels(w, h, target == GL_TEXTURE_3D ? d : 1)) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    ifmt = map_ifmt(ifmt);
    BE(glTexStorage3D)(target, levels, ifmt, w, h, d);
    record_storage(target, levels, ifmt, w, h, d);
}


/* ==================================================================
 * glPixelStore
 * ================================================================== */
#ifndef GL_PACK_IMAGE_HEIGHT
#define GL_PACK_IMAGE_HEIGHT 0x806C
#define GL_PACK_SKIP_IMAGES  0x806B
#endif

void gl31_glPixelStorei(GLenum pname, GLint param)
{
    gl31_state_t* s = gl31_state();
    switch (pname) {
        case GL_PACK_ALIGNMENT:
        case GL_UNPACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8) {
                gl31_set_error(GL_INVALID_VALUE);
                return;
            }
            if (pname == GL_PACK_ALIGNMENT) s->pack_alignment = param;
            else s->unpack_alignment = param;
            break;
        case GL_PACK_ROW_LENGTH:   case GL_PACK_SKIP_ROWS:   case GL_PACK_SKIP_PIXELS:
        case GL_UNPACK_ROW_LENGTH: case GL_UNPACK_SKIP_ROWS: case GL_UNPACK_SKIP_PIXELS:
        case GL_UNPACK_IMAGE_HEIGHT: case GL_UNPACK_SKIP_IMAGES:
            if (param < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
            if (pname == GL_PACK_ROW_LENGTH)       s->pack_row_length = param;
            else if (pname == GL_PACK_SKIP_ROWS)   s->pack_skip_rows = param;
            else if (pname == GL_PACK_SKIP_PIXELS) s->pack_skip_pixels = param;
            else if (pname == GL_UNPACK_ROW_LENGTH)   s->unpack_row_length = param;
            else if (pname == GL_UNPACK_SKIP_ROWS)    s->unpack_skip_rows = param;
            else if (pname == GL_UNPACK_SKIP_PIXELS)  s->unpack_skip_pixels = param;
            else if (pname == GL_UNPACK_IMAGE_HEIGHT) s->unpack_image_height = param;
            else if (pname == GL_UNPACK_SKIP_IMAGES)  s->unpack_skip_images = param;
            break;
        case GL_PACK_IMAGE_HEIGHT:
        case GL_PACK_SKIP_IMAGES:                    /* solo lectura 3D: ES no los tiene */
            if (param < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
            if (param) gl31_stub_warn("glPixelStorei(GL_PACK_IMAGE_HEIGHT/SKIP_IMAGES)");
            return;
        case GL_PACK_SWAP_BYTES: case GL_PACK_LSB_FIRST:
        case GL_UNPACK_SWAP_BYTES: case GL_UNPACK_LSB_FIRST:
            if (param) gl31_stub_warn("glPixelStorei(SWAP_BYTES/LSB_FIRST)");
            return;                                  /* ES no los tiene: se ignoran */
        default:
            gl31_set_error(GL_INVALID_ENUM);
            return;
    }
    BE(glPixelStorei)(pname, param);
}

void gl31_glPixelStoref(GLenum pname, GLfloat param)
{
    gl31_glPixelStorei(pname, (GLint)(param < 0.f ? param - 0.5f : param + 0.5f));
}

/* ==================================================================
 * transferencia de pixeles: TexImage / TexSubImage / CopyTex / Compressed
 *
 * Los targets de desktop sin equivalente se traducen aqui:
 *   RECTANGLE   -> 2D (solo nivel 0)
 *   1D_ARRAY    -> 2D_ARRAY con alto 1 y las capas en la tercera dimension
 * Las dimensiones maximas y la coherencia formato/tipo las valida el backend.
 * ================================================================== */
static int level_ok(GLint level)
{
    if (level < 0 || level >= MAX_LV) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

static int is_2d_target(GLenum t)
{
    return t == GL_TEXTURE_2D || is_face(t) || t == GL_TEXTURE_RECTANGLE || t == GL_TEXTURE_1D_ARRAY;
}

static int is_3d_target(GLenum t)
{
    return t == GL_TEXTURE_3D || t == GL_TEXTURE_2D_ARRAY || (t == GL_TEXTURE_CUBE_MAP_ARRAY && gl31_caps.cube_array);
}

/* 1 = proxy valido para la familia 2D (y ya resuelto) */
static int proxy_2d_family(GLenum t)
{
    return t == GL_PROXY_TEXTURE_2D || t == GL_PROXY_TEXTURE_CUBE_MAP ||
           t == GL_PROXY_TEXTURE_1D_ARRAY || t == GL_PROXY_TEXTURE_RECTANGLE;
}

/* ---------- BGRA ----------
 * GLES 3.0 no admite GL_BGRA como formato de transferencia. Para subidas de
 * 4 bytes por pixel se intercambian R y B en CPU y se sube como RGBA. */
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
typedef struct {
    void* buf;
    int   fail, restore;
    GLint a, rl, sr, sp, ih, si;
} bgra_t;

static const void* bgra_prepare(GLenum* format, GLenum* type, GLsizei w, GLsizei h, GLsizei d,
                                const void* pixels, bgra_t* b)
{
    gl31_state_t* s = gl31_state();
    size_t rl, ih, stride, row, k, j, i;
    const unsigned char* src;
    unsigned char* dst;

    memset(b, 0, sizeof *b);
    if (*format != GL_BGRA) return pixels;
    if (*type == GL_UNSIGNED_INT_8_8_8_8_REV) *type = GL_UNSIGNED_BYTE;
    if (*type != GL_UNSIGNED_BYTE || s->pixel_unpack_buffer) {
        gl31_stub_warn(s->pixel_unpack_buffer ? "GL_BGRA con PIXEL_UNPACK_BUFFER"
                                              : "GL_BGRA con tipo distinto de UNSIGNED_BYTE");
        gl31_set_error(GL_INVALID_OPERATION);
        b->fail = 1;
        return NULL;
    }
    *format = GL_RGBA;
    if (!pixels || w <= 0 || h <= 0 || d <= 0) return pixels;

    rl = s->unpack_row_length > 0 ? (size_t)s->unpack_row_length : (size_t)w;
    ih = s->unpack_image_height > 0 ? (size_t)s->unpack_image_height : (size_t)h;
    row = rl * 4u;
    stride = s->unpack_alignment > 4 ? ((row + 7u) / 8u) * 8u : row;
    b->buf = malloc((size_t)w * (size_t)h * (size_t)d * 4u);
    if (!b->buf) { gl31_set_error(GL_OUT_OF_MEMORY); b->fail = 1; return NULL; }
    src = (const unsigned char*)pixels;
    dst = (unsigned char*)b->buf;
    for (k = 0; k < (size_t)d; k++)
        for (j = 0; j < (size_t)h; j++) {
            const unsigned char* r = src + (((size_t)s->unpack_skip_images + k) * ih +
                                            (size_t)s->unpack_skip_rows + j) * stride +
                                     (size_t)s->unpack_skip_pixels * 4u;
            for (i = 0; i < (size_t)w; i++, r += 4, dst += 4) {
                dst[0] = r[2]; dst[1] = r[1]; dst[2] = r[0]; dst[3] = r[3];
            }
        }
    /* el bloque ya esta empaquetado: desactivar el desempaquetado del backend */
    b->a = s->unpack_alignment; b->rl = s->unpack_row_length; b->sr = s->unpack_skip_rows;
    b->sp = s->unpack_skip_pixels; b->ih = s->unpack_image_height; b->si = s->unpack_skip_images;
    BE(glPixelStorei)(GL_UNPACK_ALIGNMENT, 4);
    BE(glPixelStorei)(GL_UNPACK_ROW_LENGTH, 0);
    BE(glPixelStorei)(GL_UNPACK_SKIP_ROWS, 0);
    BE(glPixelStorei)(GL_UNPACK_SKIP_PIXELS, 0);
    BE(glPixelStorei)(GL_UNPACK_IMAGE_HEIGHT, 0);
    BE(glPixelStorei)(GL_UNPACK_SKIP_IMAGES, 0);
    b->restore = 1;
    return b->buf;
}

static void bgra_done(bgra_t* b)
{
    if (b->restore) {
        BE(glPixelStorei)(GL_UNPACK_ALIGNMENT, b->a);
        BE(glPixelStorei)(GL_UNPACK_ROW_LENGTH, b->rl);
        BE(glPixelStorei)(GL_UNPACK_SKIP_ROWS, b->sr);
        BE(glPixelStorei)(GL_UNPACK_SKIP_PIXELS, b->sp);
        BE(glPixelStorei)(GL_UNPACK_IMAGE_HEIGHT, b->ih);
        BE(glPixelStorei)(GL_UNPACK_SKIP_IMAGES, b->si);
    }
    free(b->buf);
}

void gl31_glTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border,
                       GLenum format, GLenum type, const void* pixels)
{
    GLenum rf = resolve_ifmt(ifmt, type);
    if (proxy_2d_family(target)) {
        if (border != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, level, w, h, 1, rf);
        return;
    }
    if (!is_2d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || h < 0 || !level_ok(level)) {
        if (border != 0 || w < 0 || h < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (target == GL_TEXTURE_RECTANGLE && level != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (is_face(target) && w != h) { gl31_set_error(GL_INVALID_VALUE); return; }

    {
        bgra_t bg;
        const void* px = bgra_prepare(&format, &type, w, h, 1, pixels, &bg);
        if (bg.fail) return;
        if (target == GL_TEXTURE_1D_ARRAY)
            BE(glTexImage3D)(GL_TEXTURE_2D_ARRAY, level, (GLint)rf, w, 1, h, 0, format, type, px);
        else
            BE(glTexImage2D)(gl31_tex_be_target(target), level, (GLint)rf, w, h, 0, format, type, px);
        bgra_done(&bg);
    }
    record_level(target, level, w, h, 1, rf);
}

void gl31_glTexImage3D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLsizei d,
                       GLint border, GLenum format, GLenum type, const void* pixels)
{
    GLenum rf = resolve_ifmt(ifmt, type);
    if (target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_2D_ARRAY || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY) {
        if (border != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, level, w, h, d, rf);
        return;
    }
    if (!is_3d_target(target)) { if (target == GL_TEXTURE_CUBE_MAP_ARRAY) warn_no_target(target); gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || h < 0 || d < 0 || !level_ok(level) ||
        (target == GL_TEXTURE_CUBE_MAP_ARRAY && (w != h || d % 6))) {
        if (border != 0 || w < 0 || h < 0 || d < 0 || target == GL_TEXTURE_CUBE_MAP_ARRAY) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    {
        bgra_t bg;
        const void* px = bgra_prepare(&format, &type, w, h, d, pixels, &bg);
        if (bg.fail) return;
        BE(glTexImage3D)(target, level, (GLint)rf, w, h, d, 0, format, type, px);
        bgra_done(&bg);
    }
    record_level(target, level, w, h, d, rf);
}

void gl31_glTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLsizei w, GLsizei h,
                          GLenum format, GLenum type, const void* pixels)
{
    if (!is_2d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || yo < 0 || w < 0 || h < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (target == GL_TEXTURE_RECTANGLE && level != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    {
        bgra_t bg;
        const void* px = bgra_prepare(&format, &type, w, h, 1, pixels, &bg);
        if (bg.fail) return;
        if (target == GL_TEXTURE_1D_ARRAY)       /* yo/h son capa/numero de capas */
            BE(glTexSubImage3D)(GL_TEXTURE_2D_ARRAY, level, xo, 0, yo, w, 1, h, format, type, px);
        else
            BE(glTexSubImage2D)(gl31_tex_be_target(target), level, xo, yo, w, h, format, type, px);
        bgra_done(&bg);
    }
}

void gl31_glTexSubImage3D(GLenum target, GLint level, GLint xo, GLint yo, GLint zo, GLsizei w,
                          GLsizei h, GLsizei d, GLenum format, GLenum type, const void* pixels)
{
    if (!is_3d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || yo < 0 || zo < 0 || w < 0 || h < 0 || d < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    {
        bgra_t bg;
        const void* px = bgra_prepare(&format, &type, w, h, d, pixels, &bg);
        if (bg.fail) return;
        BE(glTexSubImage3D)(target, level, xo, yo, zo, w, h, d, format, type, px);
        bgra_done(&bg);
    }
}

/* ---- copia desde el framebuffer de lectura ---- */
void gl31_glCopyTexImage2D(GLenum target, GLint level, GLenum ifmt, GLint x, GLint y,
                           GLsizei w, GLsizei h, GLint border)
{
    GLenum rf;
    if (!is_2d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || h < 0 || !level_ok(level)) {
        if (border != 0 || w < 0 || h < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (target == GL_TEXTURE_RECTANGLE && level != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (is_face(target) && w != h) { gl31_set_error(GL_INVALID_VALUE); return; }
    rf = resolve_ifmt((GLint)ifmt, GL_UNSIGNED_BYTE);

    if (target == GL_TEXTURE_1D_ARRAY) {
        /* reservar el nivel y copiar fila a fila: cada fila del framebuffer es una capa */
        GLenum fmt, ty;
        GLsizei l;
        if (!pair_for_ifmt(rf, &fmt, &ty)) {
            gl31_stub_warn("glCopyTexImage2D(1D_ARRAY, formato sin par de transferencia)");
            gl31_set_error(GL_INVALID_OPERATION);
            return;
        }
        BE(glTexImage3D)(GL_TEXTURE_2D_ARRAY, level, (GLint)rf, w, 1, h, 0, fmt, ty, NULL);
        for (l = 0; l < h; l++)
            BE(glCopyTexSubImage3D)(GL_TEXTURE_2D_ARRAY, level, 0, 0, l, x, y + l, w, 1);
    } else {
        BE(glCopyTexImage2D)(gl31_tex_be_target(target), level, rf, x, y, w, h, 0);
    }
    record_level(target, level, w, h, 1, rf);
}

void gl31_glCopyTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLint x, GLint y,
                              GLsizei w, GLsizei h)
{
    if (!is_2d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || yo < 0 || w < 0 || h < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (target == GL_TEXTURE_RECTANGLE && level != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (target == GL_TEXTURE_1D_ARRAY) {
        GLsizei l;
        for (l = 0; l < h; l++)
            BE(glCopyTexSubImage3D)(GL_TEXTURE_2D_ARRAY, level, xo, 0, yo + l, x, y + l, w, 1);
    } else {
        BE(glCopyTexSubImage2D)(gl31_tex_be_target(target), level, xo, yo, x, y, w, h);
    }
}

void gl31_glCopyTexSubImage3D(GLenum target, GLint level, GLint xo, GLint yo, GLint zo,
                              GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!is_3d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || yo < 0 || zo < 0 || w < 0 || h < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glCopyTexSubImage3D)(target, level, xo, yo, zo, x, y, w, h);
}

/* ---- comprimidas ---- */
/* 1D_ARRAY y RECTANGLE no se admiten comprimidas aqui */
static int compressed_2d_target(GLenum t) { return t == GL_TEXTURE_2D || is_face(t); }

void gl31_glCompressedTexImage2D(GLenum target, GLint level, GLenum ifmt, GLsizei w, GLsizei h,
                                 GLint border, GLsizei size, const void* data)
{
    if (proxy_2d_family(target)) {
        if (border != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, level, w, h, 1, ifmt);
        return;
    }
    if (!compressed_2d_target(target)) {
        if (is_2d_target(target)) gl31_stub_warn("glCompressedTexImage2D(RECTANGLE/1D_ARRAY)");
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (border != 0 || w < 0 || h < 0 || size < 0 || !level_ok(level)) {
        if (border != 0 || w < 0 || h < 0 || size < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glCompressedTexImage2D)(gl31_tex_be_target(target), level, ifmt, w, h, 0, size, data);
    record_level(target, level, w, h, 1, ifmt);
}

void gl31_glCompressedTexImage3D(GLenum target, GLint level, GLenum ifmt, GLsizei w, GLsizei h,
                                 GLsizei d, GLint border, GLsizei size, const void* data)
{
    if (target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_2D_ARRAY) {
        if (border != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, level, w, h, d, ifmt);
        return;
    }
    if (!is_3d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || h < 0 || d < 0 || size < 0 || !level_ok(level)) {
        if (border != 0 || w < 0 || h < 0 || d < 0 || size < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glCompressedTexImage3D)(target, level, ifmt, w, h, d, 0, size, data);
    record_level(target, level, w, h, d, ifmt);
}

void gl31_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLsizei w,
                                    GLsizei h, GLenum format, GLsizei size, const void* data)
{
    if (!compressed_2d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || yo < 0 || w < 0 || h < 0 || size < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glCompressedTexSubImage2D)(gl31_tex_be_target(target), level, xo, yo, w, h, format, size, data);
}

void gl31_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xo, GLint yo, GLint zo,
                                    GLsizei w, GLsizei h, GLsizei d, GLenum format, GLsizei size,
                                    const void* data)
{
    if (!is_3d_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || yo < 0 || zo < 0 || w < 0 || h < 0 || d < 0 || size < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glCompressedTexSubImage3D)(target, level, xo, yo, zo, w, h, d, format, size, data);
}

/* ---- mipmaps ---- */
void gl31_glGenerateMipmap(GLenum target)
{
    tex_t* t;
    int l, halves_h, halves_d;
    switch (target) {
        case GL_TEXTURE_1D: case GL_TEXTURE_2D: case GL_TEXTURE_3D: case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_1D_ARRAY: case GL_TEXTURE_2D_ARRAY: case GL_TEXTURE_CUBE_MAP_ARRAY:
            if (target == GL_TEXTURE_CUBE_MAP_ARRAY && !gl31_caps.cube_array) { warn_no_target(target); gl31_set_error(GL_INVALID_ENUM); return; }
            break;
        default:                                /* RECTANGLE y BUFFER no generan mipmaps */
            gl31_set_error(GL_INVALID_ENUM);
            return;
    }
    BE(glGenerateMipmap)(gl31_tex_be_target(target));

    /* anotar la cadena resultante a partir del nivel 0 */
    t = cur_tex(target);
    if (!t || t->lv[0].w <= 0) return;
    halves_h = (target != GL_TEXTURE_1D && target != GL_TEXTURE_1D_ARRAY);
    halves_d = (target == GL_TEXTURE_3D);
    for (l = 1; l < MAX_LV; l++) {
        const lvl_t p = t->lv[l - 1];
        GLsizei w = p.w > 1 ? p.w >> 1 : 1;
        GLsizei h = halves_h ? (p.h > 1 ? p.h >> 1 : 1) : p.h;
        GLsizei d = halves_d ? (p.d > 1 ? p.d >> 1 : 1) : p.d;
        if (w == p.w && h == p.h && d == p.d) break;       /* ya es 1x1(x1) */
        record_level(target, l, w, h, d, p.ifmt);
    }
}

/* ==================================================================
 * Texturas 1D: se almacenan como 2D de alto 1 en el backend.
 * ================================================================== */
void gl31_glTexImage1D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLint border,
                       GLenum format, GLenum type, const void* pixels)
{
    GLenum rf = resolve_ifmt(ifmt, type);
    bgra_t bg;
    const void* px;
    if (target == GL_PROXY_TEXTURE_1D) {
        if (border != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, level, w, 1, 1, rf);
        return;
    }
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || !level_ok(level)) {
        if (border != 0 || w < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    px = bgra_prepare(&format, &type, w, 1, 1, pixels, &bg);
    if (bg.fail) return;
    BE(glTexImage2D)(GL_TEXTURE_2D, level, (GLint)rf, w, 1, 0, format, type, px);
    bgra_done(&bg);
    record_level(GL_TEXTURE_1D, level, w, 1, 1, rf);
}

void gl31_glTexSubImage1D(GLenum target, GLint level, GLint xo, GLsizei w, GLenum format,
                          GLenum type, const void* pixels)
{
    bgra_t bg;
    const void* px;
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || w < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    px = bgra_prepare(&format, &type, w, 1, 1, pixels, &bg);
    if (bg.fail) return;
    BE(glTexSubImage2D)(GL_TEXTURE_2D, level, xo, 0, w, 1, format, type, px);
    bgra_done(&bg);
}

void gl31_glCopyTexImage1D(GLenum target, GLint level, GLenum ifmt, GLint x, GLint y,
                           GLsizei w, GLint border)
{
    GLenum rf;
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || !level_ok(level)) {
        if (border != 0 || w < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    rf = resolve_ifmt((GLint)ifmt, GL_UNSIGNED_BYTE);
    BE(glCopyTexImage2D)(GL_TEXTURE_2D, level, rf, x, y, w, 1, 0);
    record_level(GL_TEXTURE_1D, level, w, 1, 1, rf);
}

void gl31_glCopyTexSubImage1D(GLenum target, GLint level, GLint xo, GLint x, GLint y, GLsizei w)
{
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || w < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glCopyTexSubImage2D)(GL_TEXTURE_2D, level, xo, 0, x, y, w, 1);
}

void gl31_glCompressedTexImage1D(GLenum target, GLint level, GLenum ifmt, GLsizei w, GLint border,
                                 GLsizei size, const void* data)
{
    if (target == GL_PROXY_TEXTURE_1D) {
        if (border != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, level, w, 1, 1, ifmt);
        return;
    }
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (border != 0 || w < 0 || size < 0 || !level_ok(level)) {
        if (border != 0 || w < 0 || size < 0) gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glCompressedTexImage2D)(GL_TEXTURE_2D, level, ifmt, w, 1, 0, size, data);
    record_level(GL_TEXTURE_1D, level, w, 1, 1, ifmt);
}

void gl31_glCompressedTexSubImage1D(GLenum target, GLint level, GLint xo, GLsizei w, GLenum format,
                                    GLsizei size, const void* data)
{
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || w < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glCompressedTexSubImage2D)(GL_TEXTURE_2D, level, xo, 0, w, 1, format, size, data);
}

/* ==================================================================
 * Texturas multisample (ES 3.1+; el array requiere OES_texture_storage_multisample_2d_array)
 * El almacenamiento es inmutable: volver a especificar la misma textura
 * da GL_INVALID_OPERATION en el backend (en GL de escritorio seria legal).
 * ================================================================== */
static int ms_samples_ok(GLsizei samples)
{
    GLint m = 4;
    if (samples < 1) return 0;
    BE(glGetIntegerv)(GL_MAX_SAMPLES, &m);
    return samples <= m;
}

void gl31_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h,
                                  GLboolean fixed)
{
    GLenum rf = resolve_ifmt((GLint)ifmt, GL_UNSIGNED_BYTE);
    if (target == GL_PROXY_TEXTURE_2D_MULTISAMPLE) {
        if (samples < 1) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, 0, w, h, 1, ms_samples_ok(samples) ? rf : 0);
        if (!ms_samples_ok(samples)) memset(&g_proxy.l, 0, sizeof g_proxy.l);
        return;
    }
    if (target != GL_TEXTURE_2D_MULTISAMPLE) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!gl31_caps.multisample_tex) {
        warn_no_target(target);
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (w < 1 || h < 1 || samples < 1) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glTexStorage2DMultisample)(GL_TEXTURE_2D_MULTISAMPLE, samples, rf, w, h, fixed);
    record_level(GL_TEXTURE_2D_MULTISAMPLE, 0, w, h, 1, rf);
}

void gl31_glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h,
                                  GLsizei d, GLboolean fixed)
{
    GLenum rf = resolve_ifmt((GLint)ifmt, GL_UNSIGNED_BYTE);
    if (target == GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        if (samples < 1) { gl31_set_error(GL_INVALID_VALUE); return; }
        proxy_set(target, 0, w, h, d, rf);
        if (!ms_samples_ok(samples)) memset(&g_proxy.l, 0, sizeof g_proxy.l);
        return;
    }
    if (target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!gl31_caps.ms_array) {
        warn_no_target(target);
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (w < 1 || h < 1 || d < 1 || samples < 1) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glTexStorage3DMultisample)(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples, rf, w, h, d, fixed);
    record_level(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0, w, h, d, rf);
}

void gl31_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat* val)
{
    if (pname != GL_SAMPLE_POSITION) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!val) return;
    if (!gl31_caps.multisample_tex || !gl31_be.glGetMultisamplefv) {
        gl31_stub_warn("glGetMultisamplefv (backend sin ES 3.1)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    BE(glGetMultisamplefv)(pname, index, val);
}

void gl31_glSampleMaski(GLuint index, GLbitfield mask)
{
    if (index >= 1) { gl31_set_error(GL_INVALID_VALUE); return; }    /* MAX_SAMPLE_MASK_WORDS = 1 */
    if (!gl31_caps.multisample_tex || !gl31_be.glSampleMaski) {
        gl31_stub_warn("glSampleMaski (backend sin ES 3.1)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    BE(glSampleMaski)(index, mask);
}

/* ==================================================================
 * Texture buffer objects
 * ================================================================== */
void gl31_glTexBuffer(GLenum target, GLenum ifmt, GLuint buffer)
{
    tex_t* t;
    if (target != GL_TEXTURE_BUFFER) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!gl31_caps.tex_buffer || !gl31_be.glTexBuffer) {
        warn_no_target(target);
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    t = cur_tex(GL_TEXTURE_BUFFER);
    if (!t) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glTexBuffer)(GL_TEXTURE_BUFFER, ifmt, buffer);
    memset(&t->lv[0], 0, sizeof t->lv[0]);
    if (buffer) {
        GLint w = 0;
        t->lv[0].ifmt = ifmt;
        if (gl31_caps.level_query)
            BE(glGetTexLevelParameteriv)(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &w);
        t->lv[0].w = w; t->lv[0].h = 1; t->lv[0].d = 1;
    }
}

/* ==================================================================
 * glGetTexImage: framebuffer temporal + lectura (con conversion de formato)
 * ================================================================== */
static size_t type_bytes(GLenum type)
{
    switch (type) {
        case GL_UNSIGNED_BYTE: case GL_BYTE: return 1;
        case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: return 2;
        case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT: return 4;
    }
    return 0;
}

static size_t format_comps(GLenum f)
{
    switch (f) {
        case GL_RED: case GL_RED_INTEGER: case GL_ALPHA: case GL_LUMINANCE: return 1;
        case GL_RG: case GL_RG_INTEGER: case GL_LUMINANCE_ALPHA: return 2;
        case GL_RGB: case GL_RGB_INTEGER: return 3;
        case GL_RGBA: case GL_RGBA_INTEGER: return 4;
    }
    return 0;
}

void gl31_glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels)
{
    gl31_state_t* st = gl31_state();
    tex_t* t;
    lvl_t l;
    GLint prev_read = 0, prev_tex = 0;
    GLuint fbo = 0;
    GLsizei layers, z;
    size_t pix, rowlen, rowbytes, slice;
    int s = slot_of(target);

    if (s < 0 || s == S_BUF || s == S_2DMS || s == S_2DMSARR) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || level >= MAX_LV) { gl31_set_error(GL_INVALID_VALUE); return; }
    t = cur_tex(target);
    if (!t || t->lv[level].w <= 0) {
        if (!t) gl31_set_error(GL_INVALID_OPERATION);
        else gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    l = t->lv[level];
    if (!pixels && !st->pixel_pack_buffer) return;
    if (!fmt_lookup(l.ifmt)) {
        gl31_stub_warn("glGetTexImage(textura comprimida)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }

    pix = type_bytes(type) * format_comps(format);
    if (!pix) pix = 4;                               /* tipos empaquetados / no estandar */
    rowlen = st->pack_row_length > 0 ? (size_t)st->pack_row_length : (size_t)l.w;
    rowbytes = rowlen * pix;
    if (st->pack_alignment > 1) {
        size_t a = (size_t)st->pack_alignment;
        rowbytes = ((rowbytes + a - 1) / a) * a;
    }
    layers = 1;
    slice = rowbytes * (size_t)(s == S_1DARR ? 1 : l.h);
    if (s == S_3D || s == S_2DARR) layers = l.d;
    if (s == S_1DARR) layers = l.h;

    BE(glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    BE(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &prev_tex);   /* (no se altera, solo documenta) */
    (void)prev_tex;
    BE(glGenFramebuffers)(1, &fbo);
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, fbo);

    for (z = 0; z < layers; z++) {
        unsigned char* dst = (unsigned char*)pixels + (size_t)z * slice;
        GLsizei rw = l.w, rh = (s == S_1DARR) ? 1 : l.h;
        if (s == S_3D || s == S_2DARR || s == S_1DARR)
            BE(glFramebufferTextureLayer)(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t->id, level, z);
        else
            BE(glFramebufferTexture2D)(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       is_face(target) ? target : GL_TEXTURE_2D, t->id, level);
        if (z == 0) {
            BE(glReadBuffer)(GL_COLOR_ATTACHMENT0);
            if (BE(glCheckFramebufferStatus)(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                gl31_stub_warn("glGetTexImage(formato no renderizable)");
                gl31_set_error(GL_INVALID_OPERATION);
                break;
            }
        }
        if (!gl31_read_pixels_ex(0, 0, rw, rh, format, type, dst)) break;
    }

    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
    BE(glDeleteFramebuffers)(1, &fbo);
}

/* La lectura de texturas comprimidas no tiene equivalente en ES. */
void gl31_glGetCompressedTexImage(GLenum target, GLint level, void* img)
{
    (void)target; (void)level; (void)img;
    gl31_stub_warn("glGetCompressedTexImage");
    gl31_set_error(GL_INVALID_OPERATION);
}

void gl31_tex_shutdown(void)
{
    free(g_tex);
    g_tex = NULL; g_nt = g_ct = 0;
}


/* ==================================================================
 * GL 4.x: texture views, copy image, storage multisample, tex buffer range, bind multiples
 * ================================================================== */
GLenum gl31_tex_level0_format(GLuint id)
{
    tex_t* t = tex_find(id);
    return t ? t->lv[0].ifmt : 0;
}

static int is_array_target(GLenum t)
{
    return t == GL_TEXTURE_1D_ARRAY || t == GL_TEXTURE_2D_ARRAY || t == GL_TEXTURE_CUBE_MAP_ARRAY ||
           t == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}

void gl31_glTextureView(GLuint texture, GLenum target, GLuint orig, GLenum ifmt, GLuint minlevel,
                        GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
    tex_t *v, *o;
    GLenum be;
    GLuint i;
    if (!gl31_caps.texture_view) {
        gl31_stub_warn("glTextureView (backend sin ES 3.2 / EXT_texture_view)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    be = gl31_tex_be_target(target);
    if (slot_of(target) < 0 || is_face(target) || !be) { gl31_set_error(GL_INVALID_ENUM); return; }
    o = tex_find(orig);
    if (!texture || texture == orig || !o || !o->target || numlevels < 1 || numlayers < 1 ||
        minlevel + numlevels > MAX_LV) {
        gl31_set_error(texture == orig || !o ? GL_INVALID_VALUE : GL_INVALID_OPERATION);
        return;
    }
    v = tex_get(texture);
    if (!v) { gl31_set_error(GL_OUT_OF_MEMORY); return; }
    if (v->target || v->lv[0].w) { gl31_set_error(GL_INVALID_OPERATION); return; }
    ifmt = map_ifmt(ifmt);
    BE(glTextureView)(texture, be, orig, ifmt, minlevel, numlevels, minlayer, numlayers);
    v->target = target;
    for (i = 0; i < numlevels; i++) {
        v->lv[i] = o->lv[minlevel + i];
        v->lv[i].ifmt = ifmt;
        if (is_array_target(target)) v->lv[i].d = (GLsizei)numlayers;
    }
}

/* destino de un target de desktop para glCopyImageSubData: renderbuffers tal cual */
static int ci_target(GLenum t, GLenum* be, int* is1da)
{
    *is1da = (t == GL_TEXTURE_1D_ARRAY);
    if (t == GL_RENDERBUFFER) { *be = t; return 1; }
    if (slot_of(t) < 0 || is_face(t) || t == GL_TEXTURE_BUFFER) return 0;
    *be = gl31_tex_be_target(t);
    return *be != 0;
}

void gl31_glCopyImageSubData(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz,
                             GLuint dn, GLenum dt, GLint dl, GLint dx, GLint dy, GLint dz,
                             GLsizei w, GLsizei h, GLsizei d)
{
    GLenum sbe = 0, dbe = 0;
    int s1 = 0, d1 = 0;
    if (!gl31_caps.copy_image) {
        gl31_stub_warn("glCopyImageSubData (backend sin ES 3.2 / EXT_copy_image)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (!ci_target(st, &sbe, &s1) || !ci_target(dt, &dbe, &d1)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (w < 0 || h < 0 || d < 0 || sl < 0 || dl < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    /* 1D array: la coordenada Y de GL es la capa; en el backend la capa va en Z (con alto 1) */
    if (s1) { sz = sy; sy = 0; }
    if (d1) { dz = dy; dy = 0; }
    if (s1 || d1) {
        if (s1 && d1) { d = h; h = 1; }
        else { gl31_set_error(GL_INVALID_OPERATION); return; }
    }
    BE(glCopyImageSubData)(sn, sbe, sl, sx, sy, sz, dn, dbe, dl, dx, dy, dz, w, h, d);
}

void gl31_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLboolean fixed)
{
    gl31_glTexImage2DMultisample(target, samples, ifmt, w, h, fixed);
}

void gl31_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d, GLboolean fixed)
{
    gl31_glTexImage3DMultisample(target, samples, ifmt, w, h, d, fixed);
}

void gl31_glTexBufferRange(GLenum target, GLenum ifmt, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    tex_t* t;
    if (target != GL_TEXTURE_BUFFER) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!gl31_caps.tex_buffer || !gl31_be.glTexBufferRange) {
        warn_no_target(target);
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    t = cur_tex(GL_TEXTURE_BUFFER);
    if (!t) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glTexBufferRange)(GL_TEXTURE_BUFFER, ifmt, buffer, offset, size);
    memset(&t->lv[0], 0, sizeof t->lv[0]);
    if (buffer) {
        GLint w = 0;
        t->lv[0].ifmt = ifmt;
        if (gl31_caps.level_query) BE(glGetTexLevelParameteriv)(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &w);
        t->lv[0].w = w; t->lv[0].h = 1; t->lv[0].d = 1;
    }
}

/* GL 4.4: glBindTextures - cada textura se enlaza a su propio target; NULL desenlaza todo */
void gl31_glBindTextures(GLuint first, GLsizei count, const GLuint* textures)
{
    GLuint saved = g_active_unit;
    GLsizei i;
    int k;
    gl31_state_load_limits();
    if (count < 0 || (GLint)(first + (GLuint)count) > gl31_state()->max_tex_units) {
        gl31_set_error(count < 0 ? GL_INVALID_OPERATION : GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; i++) {
        GLuint unit = first + (GLuint)i;
        GLuint id = textures ? textures[i] : 0;
        tex_t* t = id ? tex_find(id) : NULL;
        if (id && (!t || !t->target)) { gl31_set_error(GL_INVALID_OPERATION); break; }
        gl31_glActiveTexture(GL_TEXTURE0 + unit);
        if (id) {
            gl31_glBindTexture(t->target, id);
        } else {
            static const GLenum all[] = { GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE, GL_TEXTURE_3D,
                                          GL_TEXTURE_CUBE_MAP, GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY };
            for (k = 0; k < (int)(sizeof all / sizeof all[0]); k++) gl31_glBindTexture(all[k], 0);
        }
    }
    gl31_glActiveTexture(GL_TEXTURE0 + saved);
}

/* ---------- accesores para DSA (gl31_dsa.c) ---------- */
/* target de desktop con el que se creo/enlazo la textura (0 = todavia sin tipo) */
GLenum gl31_tex_target_of(GLuint id)
{
    tex_t* t = id ? tex_find(id) : NULL;
    return t ? t->target : 0;
}

/* textura enlazada en la unidad activa para `target`; devuelve 0 si el target no existe */
int gl31_tex_bound_for(GLenum target, GLuint* out)
{
    int s = slot_of(target);
    if (s < 0) return 0;
    *out = g_bound[g_active_unit][s];
    return 1;
}

GLuint gl31_tex_active_unit(void) { return g_active_unit; }
