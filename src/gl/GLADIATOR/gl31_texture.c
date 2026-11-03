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

/* ---------- sombra de texturas comprimidas (glGetCompressedTexImage) ----------
 * ES no puede leer una textura comprimida. Cada nivel comprimido (por cara en cubos) guarda una copia de sus
 * bloques en un buffer de GPU, que se actualiza en cada glCompressedTexImage/SubImage y glCopyImageSubData
 * sin pasar por la CPU (copias GPU->GPU cuando el origen es un PBO). La lectura es otra copia a un PBO o a
 * la memoria de la app. Presupuesto total: LORICA_COMPRESSED_SHADOW_MB (256 por defecto); pasado el limite
 * las texturas nuevas no se copian y su lectura da error. */
typedef struct { GLuint tex; GLint level; int face; GLuint buf; size_t size; GLsizei w, h, d; GLenum ifmt; } cs_t;
static cs_t* g_cs; static size_t g_ncs, g_ccs, g_cs_bytes;
static int g_cs_warned;

static size_t cs_budget(void)
{
    static size_t b;
    if (!b) { const char* e = getenv("LORICA_COMPRESSED_SHADOW_MB"); long mb = e ? atol(e) : 256; b = (size_t)(mb < 0 ? 0 : mb) << 20; if (!b) b = 1; }
    return b;
}

static int cs_face(GLenum target) { return is_face(target) ? (int)(target - FACE_FIRST) + 1 : 0; }

static cs_t* cs_find(GLuint tex, GLint level, int face)
{
    size_t k;
    for (k = 0; k < g_ncs; k++) if (g_cs[k].tex == tex && g_cs[k].level == level && g_cs[k].face == face) return &g_cs[k];
    return NULL;
}

static void cs_drop_at(size_t k)
{
    if (g_cs[k].buf) BE(glDeleteBuffers)(1, &g_cs[k].buf);
    g_cs_bytes -= g_cs[k].size;
    g_cs[k] = g_cs[--g_ncs];
}

static void cs_drop_tex(GLuint tex)
{
    size_t k = 0;
    while (k < g_ncs) { if (g_cs[k].tex == tex) cs_drop_at(k); else k++; }
}

static size_t cs_layer_bytes(GLsizei w, GLsizei h, GLenum ifmt, int* cols, int* rows, int* bb)
{
    int bw, bh;
    if (!gl31_compressed_block(ifmt, &bw, &bh, bb)) return 0;
    *cols = (w + bw - 1) / bw; *rows = (h + bh - 1) / bh;
    return (size_t)*cols * (size_t)*rows * (size_t)*bb;
}

/* crea o ajusta la sombra de un nivel; un formato sin comprimir la elimina */
static void cs_ensure(GLuint tex, int face, GLint level, GLsizei w, GLsizei h, GLsizei d, GLenum ifmt)
{
    gl31_state_t* s = gl31_state();
    cs_t* c = cs_find(tex, level, face);
    int cols, rows, bb;
    size_t lb, need;
    GLuint prev;
    if (!gl31_compressed_block(ifmt, NULL, NULL, NULL)) { if (c) cs_drop_at((size_t)(c - g_cs)); return; }
    lb = cs_layer_bytes(w, h, ifmt, &cols, &rows, &bb);
    need = lb * (size_t)(d > 0 ? d : 1);
    if (c && c->size == need && c->ifmt == ifmt && c->w == w && c->h == h && c->d == d) return;
    if (c) cs_drop_at((size_t)(c - g_cs));
    if (!need) return;
    if (g_cs_bytes + need > cs_budget()) {
        if (!g_cs_warned) { g_cs_warned = 1; gl31_stub_warn("glGetCompressedTexImage: presupuesto de copias de texturas comprimidas agotado (LORICA_COMPRESSED_SHADOW_MB)"); }
        return;
    }
    if (g_ncs == g_ccs) {
        size_t nc = g_ccs ? g_ccs * 2 : 32;
        cs_t* p = (cs_t*)realloc(g_cs, nc * sizeof *p);
        if (!p) return;
        g_cs = p; g_ccs = nc;
    }
    c = &g_cs[g_ncs];
    memset(c, 0, sizeof *c);
    c->tex = tex; c->level = level; c->face = face; c->size = need; c->w = w; c->h = h; c->d = d; c->ifmt = ifmt;
    BE(glGenBuffers)(1, &c->buf);
    prev = s->copy_write_buffer;
    BE(glBindBuffer)(GL_COPY_WRITE_BUFFER, c->buf);
    BE(glBufferData)(GL_COPY_WRITE_BUFFER, (GLsizeiptr)need, NULL, GL_STATIC_COPY);
    BE(glBindBuffer)(GL_COPY_WRITE_BUFFER, prev);
    g_ncs++; g_cs_bytes += need;
}

/* copia los bloques de una region a la sombra. Origen: PBO de desempaquetado (offset = data) o memoria */
static void cs_copy_in(cs_t* c, GLint xo, GLint yo, GLint zo, GLsizei w, GLsizei h, GLsizei d, GLenum ifmt,
                       GLuint src_buf, const void* data, size_t src_base)
{
    gl31_state_t* s = gl31_state();
    int bw, bh, bb, cols, rows, tcols, trows;
    size_t layer, rowb, z, r;
    GLuint pr = s->copy_read_buffer, pw = s->copy_write_buffer;
    if (!gl31_compressed_block(ifmt, &bw, &bh, &bb) || w <= 0 || h <= 0 || d <= 0) return;
    if (xo % bw || yo % bh) return;                                 /* ES exige regiones alineadas a bloques */
    cols = (w + bw - 1) / bw; rows = (h + bh - 1) / bh;
    layer = cs_layer_bytes(c->w, c->h, c->ifmt, &tcols, &trows, &bb);
    rowb = (size_t)cols * (size_t)bb;
    BE(glBindBuffer)(GL_COPY_WRITE_BUFFER, c->buf);
    if (src_buf) BE(glBindBuffer)(GL_COPY_READ_BUFFER, src_buf);
    for (z = 0; z < (size_t)d; z++) {
        size_t doff0 = (size_t)(zo + (GLint)z) * layer + ((size_t)(yo / bh) * (size_t)tcols + (size_t)(xo / bw)) * (size_t)bb;
        if ((size_t)(zo + (GLint)z) >= (size_t)(c->d > 0 ? c->d : 1)) break;
        if (cols == tcols) {                                         /* filas contiguas: una sola copia */
            size_t len = rowb * (size_t)rows, so = src_base + (z * (size_t)rows) * rowb;
            if (doff0 + len > c->size) continue;
            if (src_buf) BE(glCopyBufferSubData)(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLintptr)so, (GLintptr)doff0, (GLsizeiptr)len);
            else if (data) BE(glBufferSubData)(GL_COPY_WRITE_BUFFER, (GLintptr)doff0, (GLsizeiptr)len, (const char*)data + so);
        } else {
            for (r = 0; r < (size_t)rows; r++) {
                size_t so = src_base + (z * (size_t)rows + r) * rowb, doff = doff0 + r * (size_t)tcols * (size_t)bb;
                if (doff + rowb > c->size) continue;
                if (src_buf) BE(glCopyBufferSubData)(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLintptr)so, (GLintptr)doff, (GLsizeiptr)rowb);
                else if (data) BE(glBufferSubData)(GL_COPY_WRITE_BUFFER, (GLintptr)doff, (GLsizeiptr)rowb, (const char*)data + so);
            }
        }
    }
    BE(glBindBuffer)(GL_COPY_READ_BUFFER, pr);
    BE(glBindBuffer)(GL_COPY_WRITE_BUFFER, pw);
}


/* ==================================================================
 * glGetTexImage sobre texturas comprimidas: se descomprimen en GPU
 * Un kernel de compute lee los bloques de la copia (sombra) y escribe floats RGBA32F en una textura
 * temporal; despues se lee esa textura con el camino normal de glGetTexImage/glReadPixels (cualquier
 * formato/tipo). Formatos: S3TC (BC1/2/3), RGTC (BC4/5, con y sin signo) y EAC R11/RG11.
 * ================================================================== */
static const char k_dec_src[] =
"#version 310 es\n"
"precision highp float; precision highp int;\n"
"layout(local_size_x = 8, local_size_y = 8) in;\n"
"layout(std430, binding = 0) readonly buffer Src { uint src[]; };\n"
"layout(rgba32f, binding = 0) writeonly uniform highp image2D u_out;\n"
"uniform uint u_fmt, u_w, u_h, u_bcols, u_base;   /* u_base: offset en palabras de la capa */\n"
"uint byteat(uint o, uint i) { return (src[o + (i >> 2)] >> ((i & 3u) * 8u)) & 255u; }\n"
"vec3 rgb565(uint c) { return vec3(float((c >> 11) & 31u) / 31.0, float((c >> 5) & 63u) / 63.0, float(c & 31u) / 31.0); }\n"
"vec4 bc1(uint o, uint lx, uint ly, bool force4, bool a1) {\n"
"  uint w0 = src[o], w1 = src[o + 1u];\n"
"  uint c0 = w0 & 0xFFFFu, c1 = w0 >> 16;\n"
"  vec3 e0 = rgb565(c0), e1 = rgb565(c1);\n"
"  uint idx = (w1 >> (2u * (ly * 4u + lx))) & 3u;\n"
"  vec3 c; float a = 1.0;\n"
"  if (c0 > c1 || force4) {\n"
"    if (idx == 0u) c = e0; else if (idx == 1u) c = e1; else if (idx == 2u) c = (2.0 * e0 + e1) / 3.0; else c = (e0 + 2.0 * e1) / 3.0;\n"
"  } else {\n"
"    if (idx == 0u) c = e0; else if (idx == 1u) c = e1; else if (idx == 2u) c = (e0 + e1) * 0.5; else { c = vec3(0.0); if (a1) a = 0.0; }\n"
"  }\n"
"  return vec4(c, a);\n"
"}\n"
"float bc4(uint o, uint i, bool sgn) {            /* 8 bytes: 2 extremos y 16 indices de 3 bits */\n"
"  uint w0 = src[o], w1 = src[o + 1u];\n"
"  float e0, e1;\n"
"  if (sgn) { e0 = max(float(int(byteat(o, 0u) << 24) >> 24) / 127.0, -1.0); e1 = max(float(int(byteat(o, 1u) << 24) >> 24) / 127.0, -1.0); }\n"
"  else { e0 = float(byteat(o, 0u)) / 255.0; e1 = float(byteat(o, 1u)) / 255.0; }\n"
"  uint bp = 16u + 3u * i, k;\n"
"  if (bp + 3u <= 32u) k = (w0 >> bp) & 7u;\n"
"  else if (bp >= 32u) k = (w1 >> (bp - 32u)) & 7u;\n"
"  else k = ((w0 >> bp) | (w1 << (32u - bp))) & 7u;\n"
"  if (k == 0u) return e0; if (k == 1u) return e1;\n"
"  float kf = float(k);\n"
"  if (e0 > e1) return ((8.0 - kf) * e0 + (kf - 1.0) * e1) / 7.0;\n"
"  if (k < 6u) return ((6.0 - kf) * e0 + (kf - 1.0) * e1) / 5.0;\n"
"  return k == 6u ? (sgn ? -1.0 : 0.0) : 1.0;\n"
"}\n"
"const int EAC[128] = int[128](\n"
"  -3,-6,-9,-15,2,5,8,14,\n"
"  -3,-7,-10,-13,2,6,9,12,\n"
"  -2,-5,-8,-13,1,4,7,12,\n"
"  -2,-4,-6,-13,1,3,5,12,\n"
"  -3,-6,-8,-12,2,5,7,11,\n"
"  -3,-7,-9,-11,2,6,8,10,\n"
"  -4,-7,-8,-11,3,6,7,10,\n"
"  -3,-5,-8,-11,2,4,7,10,\n"
"  -2,-6,-8,-10,1,5,7,9,\n"
"  -2,-5,-8,-10,1,4,7,9,\n"
"  -2,-4,-8,-10,1,3,7,9,\n"
"  -2,-5,-7,-10,1,4,6,9,\n"
"  -3,-4,-7,-10,2,3,6,9,\n"
"  -1,-2,-3,-10,0,1,2,9,\n"
"  -4,-6,-8,-9,3,5,7,8,\n"
"  -3,-5,-7,-9,2,4,6,8);\n"
"float eac(uint o, uint x, uint y, bool sgn) {\n"
"  uint b0 = byteat(o, 0u), b1 = byteat(o, 1u);\n"
"  uint mult = b1 >> 4, tab = b1 & 15u;\n"
"  uint p = x * 4u + y;                              /* los indices van por columnas, 48 bits big-endian */\n"
"  uint bit = 45u - 3u * p;\n"
"  uint hi = (byteat(o, 2u) << 8) | byteat(o, 3u), lo = (byteat(o, 4u) << 24) | (byteat(o, 5u) << 16) | (byteat(o, 6u) << 8) | byteat(o, 7u);\n"
"  uint idx;\n"
"  if (bit >= 32u) idx = (hi >> (bit - 32u)) & 7u;\n"
"  else if (bit + 3u <= 32u) idx = (lo >> bit) & 7u;\n"
"  else idx = ((lo >> bit) | (hi << (32u - bit))) & 7u;\n"
"  int mod = EAC[tab * 8u + idx];\n"
"  float m = mult == 0u ? 0.125 : float(mult);      /* multiplicador 0 = 1/8 */\n"
"  if (sgn) {\n"
"    float base = float(max(int(b0 << 24) >> 24, -127));\n"
"    return clamp((base * 8.0 + float(mod) * m * 8.0) / 1023.0, -1.0, 1.0);\n"
"  }\n"
"  return clamp((float(b0) * 8.0 + 4.0 + float(mod) * m * 8.0) / 2047.0, 0.0, 1.0);\n"
"}\n"
"uint be32(uint o, uint i) { return (byteat(o, i) << 24) | (byteat(o, i + 1u) << 16) | (byteat(o, i + 2u) << 8) | byteat(o, i + 3u); }\n"
"int sext3(uint v) { return int(v << 29) >> 29; }\n"
"const int ETCM[32] = int[32](2,8,-2,-8, 5,17,-5,-17, 9,29,-9,-29, 13,42,-13,-42, 18,60,-18,-60, 24,80,-24,-80, 33,106,-33,-106, 47,183,-47,-183);\n"
"const int ETCD[8] = int[8](3,6,11,16,23,32,41,64);\n"
"ivec3 ex4(uint r, uint g, uint b) { return ivec3(r, g, b) * 17; }\n"
"ivec3 ex5(int r, int g, int b) { return ivec3((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)); }\n"
"vec4 etc2(uint o, uint x, uint y, bool punch) {\n"
"  uint hi = be32(o, 0u), lo = be32(o, 4u);\n"
"  uint p = x * 4u + y;\n"
"  uint idx = (((lo >> (16u + p)) & 1u) << 1) | ((lo >> p) & 1u);\n"
"  bool bit33 = ((hi >> 1) & 1u) != 0u;\n"
"  bool opaque = !punch || bit33;\n"
"  bool diff = punch ? true : bit33;\n"
"  bool transp = !opaque && idx == 2u;\n"
"  ivec3 c0, c1, col;\n"
"  int mode = 0;                                      /* 0 ETC1, 1 T, 2 H, 3 planar */\n"
"  int R = 0, G = 0, B = 0, R2 = 0, G2 = 0, B2 = 0;\n"
"  if (!diff) {\n"
"    c0 = ex4((hi >> 28) & 15u, (hi >> 20) & 15u, (hi >> 12) & 15u);\n"
"    c1 = ex4((hi >> 24) & 15u, (hi >> 16) & 15u, (hi >> 8) & 15u);\n"
"  } else {\n"
"    R = int((hi >> 27) & 31u); G = int((hi >> 19) & 31u); B = int((hi >> 11) & 31u);\n"
"    R2 = R + sext3((hi >> 24) & 7u); G2 = G + sext3((hi >> 16) & 7u); B2 = B + sext3((hi >> 8) & 7u);\n"
"    if (R2 < 0 || R2 > 31) mode = 1; else if (G2 < 0 || G2 > 31) mode = 2; else if (B2 < 0 || B2 > 31) mode = 3;\n"
"    else { c0 = ex5(R, G, B); c1 = ex5(R2, G2, B2); }\n"
"  }\n"
"  if (mode == 1) {                                   /* T */\n"
"    uint r0 = (((hi >> 27) & 3u) << 2) | ((hi >> 24) & 3u);\n"
"    ivec3 a = ex4(r0, (hi >> 20) & 15u, (hi >> 16) & 15u), b = ex4((hi >> 12) & 15u, (hi >> 8) & 15u, (hi >> 4) & 15u);\n"
"    int d = ETCD[(((hi >> 2) & 3u) << 1) | (hi & 1u)];\n"
"    if (idx == 0u) col = a; else if (idx == 1u) col = clamp(b + d, 0, 255); else if (idx == 2u) col = b; else col = clamp(b - d, 0, 255);\n"
"  } else if (mode == 2) {                            /* H */\n"
"    uint r0 = (hi >> 27) & 15u, g0 = ((hi >> 24) & 7u) << 1 | ((hi >> 20) & 1u);\n"
"    uint b0 = (((hi >> 19) & 1u) << 3) | (((hi >> 16) & 3u) << 1) | ((hi >> 15) & 1u);\n"
"    uint r1 = (hi >> 11) & 15u, g1 = ((hi >> 8) & 7u) << 1 | ((hi >> 7) & 1u), b1 = (hi >> 3) & 15u;\n"
"    ivec3 a = ex4(r0, g0, b0), b = ex4(r1, g1, b1);\n"
"    uint k = (((hi >> 2) & 1u) << 2) | ((hi & 1u) << 1) | ((((r0 << 8) | (g0 << 4) | b0) >= ((r1 << 8) | (g1 << 4) | b1)) ? 1u : 0u);\n"
"    int d = ETCD[k];\n"
"    if (idx == 0u) col = clamp(a + d, 0, 255); else if (idx == 1u) col = clamp(a - d, 0, 255); else if (idx == 2u) col = clamp(b + d, 0, 255); else col = clamp(b - d, 0, 255);\n"
"  } else if (mode == 3) {                            /* planar */\n"
"    int ro = int((hi >> 25) & 63u), go = int((((hi >> 24) & 1u) << 6) | ((hi >> 17) & 63u));\n"
"    int bo = int((((hi >> 16) & 1u) << 5) | (((hi >> 11) & 3u) << 3) | (((hi >> 8) & 3u) << 1) | ((hi >> 7) & 1u));\n"
"    int rh = int((((hi >> 2) & 31u) << 1) | (hi & 1u)), gh = int((lo >> 25) & 127u);\n"
"    int bh = int((((lo >> 24) & 1u) << 5) | ((lo >> 19) & 31u));\n"
"    int rv = int((lo >> 13) & 63u), gv = int((lo >> 6) & 127u), bv = int(lo & 63u);\n"
"    ro = (ro << 2) | (ro >> 4); go = (go << 1) | (go >> 6); bo = (bo << 2) | (bo >> 4);\n"
"    rh = (rh << 2) | (rh >> 4); gh = (gh << 1) | (gh >> 6); bh = (bh << 2) | (bh >> 4);\n"
"    rv = (rv << 2) | (rv >> 4); gv = (gv << 1) | (gv >> 6); bv = (bv << 2) | (bv >> 4);\n"
"    int xi = int(x), yi = int(y);\n"
"    col = clamp(ivec3((xi * (rh - ro) + yi * (rv - ro) + 4 * ro + 2) >> 2, (xi * (gh - go) + yi * (gv - go) + 4 * go + 2) >> 2, (xi * (bh - bo) + yi * (bv - bo) + 4 * bo + 2) >> 2), 0, 255);\n"
"  } else {                                           /* ETC1 */\n"
"    bool flip = (hi & 1u) != 0u;\n"
"    bool sub = flip ? (y >= 2u) : (x >= 2u);\n"
"    uint cw = sub ? ((hi >> 2) & 7u) : ((hi >> 5) & 7u);\n"
"    int m = ETCM[cw * 4u + idx];\n"
"    if (!opaque && (idx == 0u || idx == 2u)) m = 0;         /* alpha: modificadores {0, +b, transparente, -b} */\n"
"    col = clamp((sub ? c1 : c0) + m, 0, 255);\n"
"  }\n"
"  if (transp && mode != 3) return vec4(0.0);\n"
"  return vec4(vec3(col) / 255.0, 1.0);\n"
"}\n"
"void main() {\n"
"  uvec2 p = gl_GlobalInvocationID.xy;\n"
"  if (p.x >= u_w || p.y >= u_h) return;\n"
"  uint bx = p.x >> 2, by = p.y >> 2, lx = p.x & 3u, ly = p.y & 3u, i = ly * 4u + lx;\n"
"  uint bidx = by * u_bcols + bx;\n"
"  vec4 v = vec4(0.0, 0.0, 0.0, 1.0);\n"
"  if (u_fmt <= 1u) { v = bc1(u_base + bidx * 2u, lx, ly, false, u_fmt == 1u); }\n"
"  else if (u_fmt == 2u) {\n"
"    uint o = u_base + bidx * 4u;\n"
"    v = bc1(o + 2u, lx, ly, true, false);\n"
"    v.a = float((src[o + (i >> 3)] >> ((i & 7u) * 4u)) & 15u) / 15.0;\n"
"  } else if (u_fmt == 3u) {\n"
"    uint o = u_base + bidx * 4u;\n"
"    v = bc1(o + 2u, lx, ly, true, false);\n"
"    v.a = bc4(o, i, false);\n"
"  } else if (u_fmt <= 5u) { uint o = u_base + bidx * 2u; v = vec4(bc4(o, i, u_fmt == 5u), 0.0, 0.0, 1.0); }\n"
"  else if (u_fmt <= 7u) { uint o = u_base + bidx * 4u; v = vec4(bc4(o, i, u_fmt == 7u), bc4(o + 2u, i, u_fmt == 7u), 0.0, 1.0); }\n"
"  else if (u_fmt <= 9u) { uint o = u_base + bidx * 2u; v = vec4(eac(o, lx, ly, u_fmt == 9u), 0.0, 0.0, 1.0); }\n"
"  else if (u_fmt == 10u || u_fmt == 11u) { uint o = u_base + bidx * 4u; bool s = u_fmt == 11u; v = vec4(eac(o, lx, ly, s), eac(o + 2u, lx, ly, s), 0.0, 1.0); }\n"
"  else if (u_fmt == 12u) v = etc2(u_base + bidx * 2u, lx, ly, false);\n"
"  else if (u_fmt == 13u) v = etc2(u_base + bidx * 2u, lx, ly, true);\n"
"  else { uint o = u_base + bidx * 4u; v = etc2(o + 2u, lx, ly, false); v.a = eac(o, lx, ly, false); }\n"
"  imageStore(u_out, ivec2(p), v);\n"
"}\n";

static GLuint g_dec_prog;
static int g_dec_state;
static GLint dec_u[5];

static int dec_code(GLenum f)
{
    switch (f) {
        case 0x83F0: case 0x8C4C: return 0;   case 0x83F1: case 0x8C4D: return 1;
        case 0x83F2: case 0x8C4E: return 2;   case 0x83F3: case 0x8C4F: return 3;
        case 0x8DBB: return 4; case 0x8DBC: return 5; case 0x8DBD: return 6; case 0x8DBE: return 7;
        case 0x9270: return 8; case 0x9271: return 9; case 0x9272: return 10; case 0x9273: return 11;
        case 0x9274: case 0x9275: return 12;  case 0x9276: case 0x9277: return 13;  case 0x9278: case 0x9279: return 14;
    }
    return -1;
}

static int dec_prog(void)
{
    static const char* const nm[] = {"u_fmt","u_w","u_h","u_bcols","u_base"};
    GLuint sh; GLint ok = 0; const char* src = k_dec_src; int k;
    if (g_dec_state) return g_dec_state == 1;
    g_dec_state = -1;
    if (!gl31_caps.es31) return 0;
    sh = BE(glCreateShader)(GL_COMPUTE_SHADER);
    BE(glShaderSource)(sh, 1, &src, NULL);
    BE(glCompileShader)(sh);
    BE(glGetShaderiv)(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = "";
        BE(glGetShaderInfoLog)(sh, sizeof log, NULL, log);
        fprintf(stderr, "LORICA: kernel de descompresion no compila: %s\n", log);
        BE(glDeleteShader)(sh);
        return 0;
    }
    g_dec_prog = BE(glCreateProgram)();
    BE(glAttachShader)(g_dec_prog, sh);
    BE(glLinkProgram)(g_dec_prog);
    BE(glDeleteShader)(sh);
    BE(glGetProgramiv)(g_dec_prog, GL_LINK_STATUS, &ok);
    if (!ok) { BE(glDeleteProgram)(g_dec_prog); g_dec_prog = 0; return 0; }
    for (k = 0; k < 5; k++) dec_u[k] = BE(glGetUniformLocation)(g_dec_prog, nm[k]);
    g_dec_state = 1;
    return 1;
}

/* descomprime la capa `layer` de la sombra `c` en una textura RGBA32F nueva (0 si no se puede) */
static GLuint dec_layer(cs_t* c, GLsizei layer)
{
    gl31_state_t* s = gl31_state();
    int code = dec_code(c->ifmt), cols, rows, bb;
    size_t lb;
    GLuint tex = 0;
    GLint sv_ssbo = 0, sv_id = 0, sv_name = 0, sv_level = 0, sv_layered = 0, sv_layer = 0, sv_acc = 0, sv_fmt = 0, sv_act = 0, sv_tex = 0;
    GLint64 sv_start = 0, sv_size = 0;
    if (code < 0 || !dec_prog()) return 0;
    lb = cs_layer_bytes(c->w, c->h, c->ifmt, &cols, &rows, &bb);
    BE(glGetIntegerv)(GL_ACTIVE_TEXTURE, &sv_act);
    BE(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &sv_tex);
    BE(glGenTextures)(1, &tex);
    BE(glBindTexture)(GL_TEXTURE_2D, tex);
    BE(glTexStorage2D)(GL_TEXTURE_2D, 1, GL_RGBA32F, c->w, c->h);
    BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    BE(glBindTexture)(GL_TEXTURE_2D, (GLuint)sv_tex);
    BE(glGetIntegerv)(GL_SHADER_STORAGE_BUFFER_BINDING, &sv_ssbo);
    BE(glGetIntegeri_v)(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &sv_id);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_START, 0, &sv_start);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &sv_size);
    BE(glGetIntegeri_v)(GL_IMAGE_BINDING_NAME, 0, &sv_name);
    BE(glGetIntegeri_v)(GL_IMAGE_BINDING_LEVEL, 0, &sv_level);
    BE(glGetIntegeri_v)(GL_IMAGE_BINDING_LAYERED, 0, &sv_layered);
    BE(glGetIntegeri_v)(GL_IMAGE_BINDING_LAYER, 0, &sv_layer);
    BE(glGetIntegeri_v)(GL_IMAGE_BINDING_ACCESS, 0, &sv_acc);
    BE(glGetIntegeri_v)(GL_IMAGE_BINDING_FORMAT, 0, &sv_fmt);
    BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, c->buf);
    BE(glBindImageTexture)(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    BE(glUseProgram)(g_dec_prog);
    BE(glUniform1ui)(dec_u[0], (GLuint)code);
    BE(glUniform1ui)(dec_u[1], (GLuint)c->w);
    BE(glUniform1ui)(dec_u[2], (GLuint)c->h);
    BE(glUniform1ui)(dec_u[3], (GLuint)cols);
    BE(glUniform1ui)(dec_u[4], (GLuint)((size_t)layer * lb / 4u));
    BE(glDispatchCompute)((GLuint)((c->w + 7) / 8), (GLuint)((c->h + 7) / 8), 1);
    BE(glMemoryBarrier)(0xFFFFFFFFu);
    if (sv_id && sv_size > 0) BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id, (GLintptr)sv_start, (GLsizeiptr)sv_size);
    else BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, (GLuint)sv_ssbo);
    if (sv_name) BE(glBindImageTexture)(0, (GLuint)sv_name, sv_level, (GLboolean)sv_layered, sv_layer, (GLenum)sv_acc, (GLenum)sv_fmt);
    else BE(glBindImageTexture)(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    BE(glUseProgram)(s->program);
    (void)sv_act;
    return tex;
}

/* tras una subida comprimida de la app */
static void cs_upload(GLenum target, GLint level, GLint xo, GLint yo, GLint zo, GLsizei w, GLsizei h, GLsizei d,
                      GLenum ifmt, GLsizei size, const void* data)
{
    gl31_state_t* s = gl31_state();
    tex_t* tx = cur_tex(target);
    cs_t* c;
    (void)size;
    if (!tx) return;
    c = cs_find(tx->id, level, cs_face(target));
    if (!c) return;
    if (s->pixel_unpack_buffer) cs_copy_in(c, xo, yo, zo, w, h, d, ifmt, s->pixel_unpack_buffer, NULL, (size_t)(intptr_t)data);
    else if (data) cs_copy_in(c, xo, yo, zo, w, h, d, ifmt, 0, data, 0);
}

/* tras glCopyImageSubData entre texturas: bloques de la sombra origen a la sombra destino */
static void cs_copy_image(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz, GLuint dn, GLenum dt, GLint dl,
                          GLint dx, GLint dy, GLint dz, GLsizei w, GLsizei h, GLsizei d)
{
    GLsizei k;
    int sc = (st == GL_TEXTURE_CUBE_MAP), dc = (dt == GL_TEXTURE_CUBE_MAP);
    for (k = 0; k < (sc || dc ? d : 1); k++) {
        cs_t *a = cs_find(sn, sl, sc ? (int)(sz + k) + 1 : 0), *b = cs_find(dn, dl, dc ? (int)(dz + k) + 1 : 0);
        int bw, bh, bb, tcols, trows, dcols, drows, cols, rows;
        size_t sl_bytes, dl_bytes, z, r, rowb;
        GLint nlayers = (sc || dc) ? 1 : d;
        gl31_state_t* s = gl31_state();
        GLuint pr = s->copy_read_buffer, pw = s->copy_write_buffer;
        if (!b) continue;
        if (!a || !gl31_compressed_block(a->ifmt, &bw, &bh, &bb) || sx % bw || sy % bh || dx % bw || dy % bh) { cs_drop_at((size_t)(b - g_cs)); continue; }
        sl_bytes = cs_layer_bytes(a->w, a->h, a->ifmt, &tcols, &trows, &bb);
        dl_bytes = cs_layer_bytes(b->w, b->h, b->ifmt, &dcols, &drows, &bb);
        cols = (w + bw - 1) / bw; rows = (h + bh - 1) / bh; rowb = (size_t)cols * (size_t)bb;
        BE(glBindBuffer)(GL_COPY_READ_BUFFER, a->buf);
        BE(glBindBuffer)(GL_COPY_WRITE_BUFFER, b->buf);
        for (z = 0; z < (size_t)nlayers; z++)
            for (r = 0; r < (size_t)rows; r++) {
                size_t so = (sc ? 0 : (size_t)(sz + (GLint)z)) * sl_bytes + (((size_t)(sy / bh) + r) * (size_t)tcols + (size_t)(sx / bw)) * (size_t)bb;
                size_t doff = (dc ? 0 : (size_t)(dz + (GLint)z)) * dl_bytes + (((size_t)(dy / bh) + r) * (size_t)dcols + (size_t)(dx / bw)) * (size_t)bb;
                if (so + rowb > a->size || doff + rowb > b->size) continue;
                BE(glCopyBufferSubData)(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLintptr)so, (GLintptr)doff, (GLsizeiptr)rowb);
            }
        BE(glBindBuffer)(GL_COPY_READ_BUFFER, pr);
        BE(glBindBuffer)(GL_COPY_WRITE_BUFFER, pw);
    }
}

static void record_level(GLenum target, GLint level, GLsizei w, GLsizei h, GLsizei d, GLenum ifmt)
{
    tex_t* t;
    if (level < 0 || level >= MAX_LV) return;
    t = cur_tex(target);
    if (!t) return;
    t->lv[level].w = w; t->lv[level].h = h; t->lv[level].d = d; t->lv[level].ifmt = ifmt;
    cs_ensure(t->id, cs_face(target), level, w, h, d, ifmt);
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
        cs_drop_tex(t[i]);
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
            {   /* tamano en bytes de la imagen comprimida: bloques que cubren el nivel por bytes de bloque */
                int bw, bh, bb;
                if (!l->ifmt || !gl31_compressed_block(l->ifmt, &bw, &bh, &bb)) { gl31_set_error(GL_INVALID_OPERATION); return; }
                *params = (GLint)((((l->w + bw - 1) / bw) * ((l->h + bh - 1) / bh)) * bb * (l->d ? l->d : 1));
            }
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
        case GL_PACK_SKIP_IMAGES:                    /* ES no los tiene: los aplica la conversion propia */
            if (param < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
            if (pname == GL_PACK_IMAGE_HEIGHT) s->pack_image_height = param;
            else s->pack_skip_images = param;
            return;
        case GL_PACK_SWAP_BYTES:   s->pack_swap_bytes = param ? GL_TRUE : GL_FALSE; return;
        case GL_UNPACK_SWAP_BYTES: s->unpack_swap_bytes = param ? GL_TRUE : GL_FALSE; return;
        case GL_PACK_LSB_FIRST:    s->pack_lsb_first = param ? GL_TRUE : GL_FALSE; return;   /* solo afecta a GL_BITMAP */
        case GL_UNPACK_LSB_FIRST:  s->unpack_lsb_first = param ? GL_TRUE : GL_FALSE; return;
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
    int   fail, restore, gpu;
    GLint a, rl, sr, sp, ih, si;
} bgra_t;

static const void* bgra_prepare_cpu(GLenum* format, GLenum* type, GLsizei w, GLsizei h, GLsizei d,
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

/* ---- BGRA en GPU ----
 * Un kernel de compute lee los pixeles de la app (de un PBO o subidos a un buffer de GPU), intercambia R y B,
 * quita el relleno de GL_UNPACK_* y deja el bloque empaquetado en otro buffer que se usa como
 * PIXEL_UNPACK_BUFFER del backend. El intercambio ya no pasa por la CPU. */
static const char k_bgra_src[] =
"#version 310 es\n"
"precision highp float; precision highp int;\n"
"layout(local_size_x = 64) in;\n"
"layout(std430, binding = 0) readonly buffer SrcB { uint src[]; };\n"
"layout(std430, binding = 1) writeonly buffer DstB { uint dst[]; };\n"
"uniform uint u_w, u_h, u_n, u_ih, u_sp, u_sr, u_si, u_stride, u_base, u_gx;\n"
"uint ld(uint off) {\n"
"  uint wd = off >> 2; uint sh = (off & 3u) * 8u;\n"
"  uint lo = src[wd] >> sh;\n"
"  if (sh == 0u) return lo;\n"
"  return lo | (src[wd + 1u] << (32u - sh));\n"
"}\n"
"void main() {\n"
"  uint i = gl_GlobalInvocationID.y * u_gx + gl_GlobalInvocationID.x;\n"
"  if (i >= u_n) return;\n"
"  uint wh = u_w * u_h; uint k = i / wh; uint rem = i - k * wh; uint j = rem / u_w; uint x = rem - j * u_w;\n"
"  uint off = u_base + ((u_si + k) * u_ih + u_sr + j) * u_stride + (u_sp + x) * 4u;\n"
"  uint v = ld(off);\n"
"  dst[i] = (v & 0xFF00FF00u) | ((v & 0xFFu) << 16) | ((v >> 16) & 0xFFu);\n"
"}\n";

static GLuint g_bg_prog, g_bg_in, g_bg_out;
static size_t g_bg_in_cap, g_bg_out_cap;
static int g_bg_state;
static GLint bg_u[10];

static int bgra_gpu_prog(void)
{
    static const char* const nm[] = {"u_w","u_h","u_n","u_ih","u_sp","u_sr","u_si","u_stride","u_base","u_gx"};
    GLuint sh; GLint ok = 0; const char* src = k_bgra_src; size_t k;
    if (g_bg_state) return g_bg_state == 1;
    g_bg_state = -1;
    if (!gl31_caps.es31) return 0;
    sh = BE(glCreateShader)(GL_COMPUTE_SHADER);
    BE(glShaderSource)(sh, 1, &src, NULL);
    BE(glCompileShader)(sh);
    BE(glGetShaderiv)(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { BE(glDeleteShader)(sh); return 0; }
    g_bg_prog = BE(glCreateProgram)();
    BE(glAttachShader)(g_bg_prog, sh);
    BE(glLinkProgram)(g_bg_prog);
    BE(glDeleteShader)(sh);
    BE(glGetProgramiv)(g_bg_prog, GL_LINK_STATUS, &ok);
    if (!ok) { BE(glDeleteProgram)(g_bg_prog); g_bg_prog = 0; return 0; }
    for (k = 0; k < 10; k++) bg_u[k] = BE(glGetUniformLocation)(g_bg_prog, nm[k]);
    g_bg_state = 1;
    return 1;
}

void gl31_texture_gpu_shutdown(void) { g_bg_prog = g_bg_in = g_bg_out = 0; g_bg_in_cap = g_bg_out_cap = 0; g_bg_state = 0; }

static const void* bgra_prepare(GLenum* format, GLenum* type, GLsizei w, GLsizei h, GLsizei d,
                                const void* pixels, bgra_t* b)
{
    gl31_state_t* s = gl31_state();
    size_t rl, ih, stride, need, n, outbytes;
    GLuint src_buf, groups, gx, gy;
    GLintptr src_off = 0;
    GLint sv_ssbo = 0, sv_id0 = 0, sv_id1 = 0;
    GLint64 sv_s0 = 0, sv_z0 = 0, sv_s1 = 0, sv_z1 = 0;

    if (*format != GL_BGRA || !bgra_gpu_prog()) return bgra_prepare_cpu(format, type, w, h, d, pixels, b);
    if (*type == GL_UNSIGNED_INT_8_8_8_8_REV) *type = GL_UNSIGNED_BYTE;
    if (*type != GL_UNSIGNED_BYTE) return bgra_prepare_cpu(format, type, w, h, d, pixels, b);
    memset(b, 0, sizeof *b);
    *format = GL_RGBA;
    if (w <= 0 || h <= 0 || d <= 0) return pixels;
    if (!s->pixel_unpack_buffer && !pixels) return pixels;

    rl = s->unpack_row_length > 0 ? (size_t)s->unpack_row_length : (size_t)w;
    ih = s->unpack_image_height > 0 ? (size_t)s->unpack_image_height : (size_t)h;
    stride = s->unpack_alignment > 4 ? ((rl * 4u + 7u) / 8u) * 8u : rl * 4u;
    n = (size_t)w * (size_t)h * (size_t)d;
    outbytes = n * 4u;
    need = (((size_t)s->unpack_skip_images + (size_t)d - 1u) * ih + (size_t)s->unpack_skip_rows + (size_t)h - 1u) * stride +
           ((size_t)s->unpack_skip_pixels + (size_t)w) * 4u;

    BE(glGetIntegerv)(GL_SHADER_STORAGE_BUFFER_BINDING, &sv_ssbo);
    BE(glGetIntegeri_v)(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &sv_id0);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_START, 0, &sv_s0);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &sv_z0);
    BE(glGetIntegeri_v)(GL_SHADER_STORAGE_BUFFER_BINDING, 1, &sv_id1);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_START, 1, &sv_s1);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_SIZE, 1, &sv_z1);

    if (s->pixel_unpack_buffer) {
        src_buf = s->pixel_unpack_buffer;
        src_off = (GLintptr)(intptr_t)pixels;
    } else {                                       /* memoria de la app: una copia a un buffer de GPU */
        size_t cap = (need + 16u + 255u) & ~(size_t)255u;
        if (!g_bg_in) BE(glGenBuffers)(1, &g_bg_in);
        BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, g_bg_in);
        if (g_bg_in_cap < cap) { BE(glBufferData)(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)cap, NULL, GL_STREAM_DRAW); g_bg_in_cap = cap; }
        BE(glBufferSubData)(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)need, pixels);
        src_buf = g_bg_in;
    }
    if (!g_bg_out) BE(glGenBuffers)(1, &g_bg_out);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, g_bg_out);
    if (g_bg_out_cap < outbytes) {
        size_t cap = (outbytes + 255u) & ~(size_t)255u;
        BE(glBufferData)(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)cap, NULL, GL_DYNAMIC_COPY);
        g_bg_out_cap = cap;
    }
    BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, src_buf);
    BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 1, g_bg_out);
    BE(glUseProgram)(g_bg_prog);
    BE(glUniform1ui)(bg_u[0], (GLuint)w); BE(glUniform1ui)(bg_u[1], (GLuint)h); BE(glUniform1ui)(bg_u[2], (GLuint)n);
    BE(glUniform1ui)(bg_u[3], (GLuint)ih); BE(glUniform1ui)(bg_u[4], (GLuint)s->unpack_skip_pixels);
    BE(glUniform1ui)(bg_u[5], (GLuint)s->unpack_skip_rows); BE(glUniform1ui)(bg_u[6], (GLuint)s->unpack_skip_images);
    BE(glUniform1ui)(bg_u[7], (GLuint)stride); BE(glUniform1ui)(bg_u[8], (GLuint)src_off);
    groups = (GLuint)((n + 63) / 64);
    gx = groups > 65535u ? 65535u : groups;
    gy = (groups + gx - 1) / gx;
    BE(glUniform1ui)(bg_u[9], gx * 64u);
    BE(glDispatchCompute)(gx, gy, 1);
    BE(glMemoryBarrier)(0xFFFFFFFFu);
    src_buf = 0;

    /* restaurar enlaces de SSBO y programa */
    if (sv_id0 && sv_z0 > 0) BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id0, (GLintptr)sv_s0, (GLsizeiptr)sv_z0);
    else BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id0);
    if (sv_id1 && sv_z1 > 0) BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 1, (GLuint)sv_id1, (GLintptr)sv_s1, (GLsizeiptr)sv_z1);
    else BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 1, (GLuint)sv_id1);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, (GLuint)sv_ssbo);
    BE(glUseProgram)(s->program);

    /* el bloque limpio se entrega como PBO del backend, con el desempaquetado a valores por defecto */
    b->a = s->unpack_alignment; b->rl = s->unpack_row_length; b->sr = s->unpack_skip_rows;
    b->sp = s->unpack_skip_pixels; b->ih = s->unpack_image_height; b->si = s->unpack_skip_images;
    BE(glBindBuffer)(GL_PIXEL_UNPACK_BUFFER, g_bg_out);
    BE(glPixelStorei)(GL_UNPACK_ALIGNMENT, 4);
    BE(glPixelStorei)(GL_UNPACK_ROW_LENGTH, 0);
    BE(glPixelStorei)(GL_UNPACK_SKIP_ROWS, 0);
    BE(glPixelStorei)(GL_UNPACK_SKIP_PIXELS, 0);
    BE(glPixelStorei)(GL_UNPACK_IMAGE_HEIGHT, 0);
    BE(glPixelStorei)(GL_UNPACK_SKIP_IMAGES, 0);
    b->restore = 1;
    b->gpu = 1;
    return (const void*)0;
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
    if (b->gpu) BE(glBindBuffer)(GL_PIXEL_UNPACK_BUFFER, gl31_state()->pixel_unpack_buffer);
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
    cs_upload(target, level, 0, 0, 0, w, h, 1, ifmt, size, data);
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
    cs_upload(target, level, 0, 0, 0, w, h, d, ifmt, size, data);
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
    cs_upload(target, level, xo, yo, 0, w, h, 1, format, size, data);
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
    cs_upload(target, level, xo, yo, zo, w, h, d, format, size, data);
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
    cs_upload(GL_TEXTURE_1D, level, 0, 0, 0, w, 1, 1, ifmt, size, data);
}

void gl31_glCompressedTexSubImage1D(GLenum target, GLint level, GLint xo, GLsizei w, GLenum format,
                                    GLsizei size, const void* data)
{
    if (target != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (level < 0 || xo < 0 || w < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glCompressedTexSubImage2D)(GL_TEXTURE_2D, level, xo, 0, w, 1, format, size, data);
    cs_upload(GL_TEXTURE_1D, level, xo, 0, 0, w, 1, 1, format, size, data);
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

/* posiciones de muestra estandar (las mismas que fija Vulkan) para backends sin glGetMultisamplefv */
static const GLfloat k_sp1[1][2]   = {{0.5f,0.5f}};
static const GLfloat k_sp2[2][2]   = {{0.75f,0.75f},{0.25f,0.25f}};
static const GLfloat k_sp4[4][2]   = {{0.375f,0.125f},{0.875f,0.375f},{0.125f,0.625f},{0.625f,0.875f}};
static const GLfloat k_sp8[8][2]   = {{0.5625f,0.3125f},{0.4375f,0.6875f},{0.8125f,0.5625f},{0.3125f,0.1875f},
                                      {0.1875f,0.8125f},{0.0625f,0.4375f},{0.6875f,0.9375f},{0.9375f,0.0625f}};
static const GLfloat k_sp16[16][2] = {{0.5625f,0.5625f},{0.4375f,0.3125f},{0.3125f,0.625f},{0.75f,0.4375f},
                                      {0.1875f,0.375f},{0.625f,0.8125f},{0.8125f,0.6875f},{0.6875f,0.1875f},
                                      {0.375f,0.875f},{0.5f,0.0625f},{0.25f,0.125f},{0.125f,0.75f},
                                      {0.0f,0.5f},{0.9375f,0.25f},{0.875f,0.9375f},{0.0625f,0.0f}};

void gl31_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat* val)
{
    GLint samples = 0;
    const GLfloat (*tab)[2];
    if (pname != GL_SAMPLE_POSITION) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!val) return;
    if (gl31_caps.multisample_tex && gl31_be.glGetMultisamplefv) { BE(glGetMultisamplefv)(pname, index, val); return; }
    BE(glGetIntegerv)(GL_SAMPLES, &samples);
    if (samples <= 1) { samples = 1; tab = k_sp1; }
    else if (samples == 2) tab = k_sp2;
    else if (samples <= 4) { samples = 4; tab = k_sp4; }
    else if (samples <= 8) { samples = 8; tab = k_sp8; }
    else { samples = 16; tab = k_sp16; }
    if ((GLint)index >= samples) { gl31_set_error(GL_INVALID_VALUE); return; }
    val[0] = tab[index][0]; val[1] = tab[index][1];
}

/* glSampleMaski: ES 3.0 no tiene mascara de muestras. Una mascara que deja pasar todas las muestras no
 * cambia nada y se acepta; una restrictiva no se puede expresar y se avisa. */
void gl31_glSampleMaski(GLuint index, GLbitfield mask)
{
    if (index >= 1) { gl31_set_error(GL_INVALID_VALUE); return; }    /* MAX_SAMPLE_MASK_WORDS = 1 */
    if (gl31_caps.multisample_tex && gl31_be.glSampleMaski) { BE(glSampleMaski)(index, mask); return; }
    if (mask != 0xFFFFFFFFu) {
        GLint samples = 0;
        BE(glGetIntegerv)(GL_SAMPLES, &samples);
        if (samples < 1) samples = 1;
        if (samples > 32 || (mask & ((samples == 32) ? 0xFFFFFFFFu : ((1u << samples) - 1u))) != ((samples == 32) ? 0xFFFFFFFFu : ((1u << samples) - 1u)))
            gl31_stub_warn("glSampleMaski(mascara restrictiva) sin backend ES 3.1");
    }
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
        /* comprimida: se descomprime cada capa en GPU a una textura RGBA32F y se lee con el camino normal */
        cs_t* c = cs_find(t->id, level, cs_face(target));
        GLsizei nl, k;
        size_t pixc, rb, sl;
        if (!c || dec_code(c->ifmt) < 0 || !dec_prog()) {
            gl31_stub_warn("glGetTexImage(textura comprimida): formato sin decodificador (solo S3TC, RGTC, EAC y ETC2; no ASTC ni BPTC)");
            gl31_set_error(GL_INVALID_OPERATION);
            return;
        }
        pixc = type_bytes(type) * format_comps(format); if (!pixc) pixc = 4;
        rb = (st->pack_row_length > 0 ? (size_t)st->pack_row_length : (size_t)l.w) * pixc;
        if (st->pack_alignment > 1) rb = ((rb + (size_t)st->pack_alignment - 1) / (size_t)st->pack_alignment) * (size_t)st->pack_alignment;
        sl = rb * (size_t)(st->pack_image_height > 0 ? st->pack_image_height : l.h);
        nl = (s == S_3D || s == S_2DARR) ? (l.d > 0 ? l.d : 1) : 1;
        BE(glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
        BE(glGenFramebuffers)(1, &fbo);
        for (k = 0; k < nl; k++) {
            GLuint dt = dec_layer(c, k);
            if (!dt) { gl31_set_error(GL_INVALID_OPERATION); break; }
            BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, fbo);
            BE(glFramebufferTexture2D)(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dt, 0);
            BE(glReadBuffer)(GL_COLOR_ATTACHMENT0);
            if (!gl31_read_pixels_ex(0, 0, l.w, l.h, format, type, (unsigned char*)pixels + ((size_t)st->pack_skip_images + (size_t)k) * sl)) {
                BE(glDeleteTextures)(1, &dt);
                break;
            }
            BE(glDeleteTextures)(1, &dt);
        }
        BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
        BE(glDeleteFramebuffers)(1, &fbo);
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
    {   /* GL_PACK_IMAGE_HEIGHT / GL_PACK_SKIP_IMAGES: separacion entre capas y capas que se saltan al empezar */
        size_t ih = st->pack_image_height > 0 ? (size_t)st->pack_image_height : (size_t)(s == S_1DARR ? 1 : l.h);
        slice = rowbytes * ih;
    }
    if (s == S_3D || s == S_2DARR) layers = l.d;
    if (s == S_1DARR) layers = l.h;

    BE(glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    BE(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &prev_tex);   /* (no se altera, solo documenta) */
    (void)prev_tex;
    BE(glGenFramebuffers)(1, &fbo);
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, fbo);

    {   /* profundidad / stencil: se adjunta a su punto y se lee con el camino de GPU de glReadPixels */
        GLenum dsatt = GL_COLOR_ATTACHMENT0;
        int dsf = (format == GL_DEPTH_COMPONENT || format == GL_STENCIL_INDEX || format == GL_DEPTH_STENCIL);
        if (dsf) dsatt = (l.ifmt == GL_DEPTH24_STENCIL8 || l.ifmt == GL_DEPTH32F_STENCIL8 || l.ifmt == GL_DEPTH_STENCIL)
                         ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        for (z = 0; z < layers; z++) {
            unsigned char* dst = (unsigned char*)pixels + ((size_t)st->pack_skip_images + (size_t)z) * slice;
            GLsizei rw = l.w, rh = (s == S_1DARR) ? 1 : l.h;
            if (s == S_3D || s == S_2DARR || s == S_1DARR)
                BE(glFramebufferTextureLayer)(GL_READ_FRAMEBUFFER, dsatt, t->id, level, z);
            else
                BE(glFramebufferTexture2D)(GL_READ_FRAMEBUFFER, dsatt,
                                           is_face(target) ? target : GL_TEXTURE_2D, t->id, level);
            if (z == 0) {
                BE(glReadBuffer)(dsf ? GL_NONE : GL_COLOR_ATTACHMENT0);
                if (BE(glCheckFramebufferStatus)(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    gl31_stub_warn("glGetTexImage(formato no renderizable)");
                    gl31_set_error(GL_INVALID_OPERATION);
                    break;
                }
            }
            if (!gl31_read_pixels_ex(0, 0, rw, rh, format, type, dst)) break;
        }
    }

    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
    BE(glDeleteFramebuffers)(1, &fbo);
}

/* La lectura de texturas comprimidas no tiene equivalente en ES. */
void gl31_glGetCompressedTexImage(GLenum target, GLint level, void* img)
{
    gl31_state_t* s = gl31_state();
    tex_t* tx;
    cs_t* c;
    GLuint pr;
    if (level < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!(is_face(target) || target == GL_TEXTURE_1D || target == GL_TEXTURE_2D || target == GL_TEXTURE_3D ||
          target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_1D_ARRAY)) { gl31_set_error(GL_INVALID_ENUM); return; }
    tx = cur_tex(target);
    c = tx ? cs_find(tx->id, level, cs_face(target)) : NULL;
    if (!c) { gl31_set_error(GL_INVALID_OPERATION); return; }   /* no es comprimida (o sin copia por presupuesto) */
    pr = s->copy_read_buffer;
    BE(glBindBuffer)(GL_COPY_READ_BUFFER, c->buf);
    if (s->pixel_pack_buffer) {
        BE(glCopyBufferSubData)(GL_COPY_READ_BUFFER, GL_PIXEL_PACK_BUFFER, 0, (GLintptr)(intptr_t)img, (GLsizeiptr)c->size);
    } else if (img) {
        const void* m = BE(glMapBufferRange)(GL_COPY_READ_BUFFER, 0, (GLsizeiptr)c->size, GL_MAP_READ_BIT);
        if (m) { memcpy(img, m, c->size); BE(glUnmapBuffer)(GL_COPY_READ_BUFFER); }
        else gl31_set_error(GL_OUT_OF_MEMORY);
    }
    BE(glBindBuffer)(GL_COPY_READ_BUFFER, pr);
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

/* target del backend de una textura (para reconstruir attachments) */
GLenum gl31_tex_be_target_of(GLuint id)
{
    tex_t* tx = tex_find(id);
    GLenum be = (tx && tx->target) ? gl31_tex_be_target(tx->target) : 0;
    return be ? be : GL_TEXTURE_2D;
}

/* ---- formatos comprimidos: tamano de bloque ---- */
int gl31_compressed_block(GLenum f, int* bw, int* bh, int* bytes)
{
    int w = 4, h = 4, b = 0;
    if ((f >= 0x83F0 && f <= 0x83F3) || (f >= 0x8C4C && f <= 0x8C4F)) b = (f == 0x83F0 || f == 0x83F1 || f == 0x8C4C || f == 0x8C4D) ? 8 : 16;
    else if (f >= 0x8DBB && f <= 0x8DBE) b = (f == 0x8DBB || f == 0x8DBC) ? 8 : 16;
    else if (f >= 0x8E8C && f <= 0x8E8F) b = 16;
    else if (f >= 0x9270 && f <= 0x9279) b = (f == 0x9272 || f == 0x9273 || f == 0x9278 || f == 0x9279) ? 16 : 8;
    else if ((f >= 0x93B0 && f <= 0x93BD) || (f >= 0x93D0 && f <= 0x93DD)) {
        static const unsigned char dim[14][2] = {{4,4},{5,4},{5,5},{6,5},{6,6},{8,5},{8,6},{8,8},{10,5},{10,6},{10,8},{10,10},{12,10},{12,12}};
        unsigned i = (f >= 0x93D0) ? f - 0x93D0 : f - 0x93B0;
        w = dim[i][0]; h = dim[i][1]; b = 16;
    }
    if (!b) return 0;
    if (bw) *bw = w;
    if (bh) *bh = h;
    if (bytes) *bytes = b;
    return 1;
}
#define gl31_tex_is_compressed_fmt(f) gl31_compressed_block((f), NULL, NULL, NULL)

/* ---- glCopyImageSubData sin ES 3.2 / EXT_copy_image: copia GPU con glBlitFramebuffer ----
 * Cada capa/cara se adjunta a un FBO de lectura y a otro de dibujo y se copia con un blit 1:1. Cubre
 * color y profundidad/stencil (el blit exige formatos compatibles), texturas 2D / arrays / 3D / cubos y
 * renderbuffers. Los formatos comprimidos no son renderizables: no se pueden copiar sin la extension. */
static int ci_is_depth(GLenum f)
{
    return f == GL_DEPTH_COMPONENT16 || f == GL_DEPTH_COMPONENT24 || f == GL_DEPTH_COMPONENT32F || f == GL_DEPTH_COMPONENT ||
           f == GL_DEPTH24_STENCIL8 || f == GL_DEPTH32F_STENCIL8 || f == GL_DEPTH_STENCIL;
}

static int ci_has_stencil(GLenum f)
{
    return f == GL_DEPTH24_STENCIL8 || f == GL_DEPTH32F_STENCIL8 || f == GL_DEPTH_STENCIL || f == GL_STENCIL_INDEX8;
}

static GLenum ci_format_of(GLuint name, GLenum target, GLint level)
{
    if (target == GL_RENDERBUFFER) {
        GLint f = 0, prev = 0;
        BE(glGetIntegerv)(GL_RENDERBUFFER_BINDING, &prev);
        BE(glBindRenderbuffer)(GL_RENDERBUFFER, name);
        BE(glGetRenderbufferParameteriv)(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &f);
        BE(glBindRenderbuffer)(GL_RENDERBUFFER, (GLuint)prev);
        return (GLenum)f;
    } else {
        tex_t* tx = tex_find(name);
        return (tx && level >= 0 && level < MAX_LV) ? tx->lv[level].ifmt : 0;
    }
}

static void ci_attach(GLenum fbt, GLenum att, GLuint name, GLenum desk_target, GLenum be_target, GLint level, GLint layer)
{
    if (desk_target == GL_RENDERBUFFER)
        BE(glFramebufferRenderbuffer)(fbt, att, GL_RENDERBUFFER, name);
    else if (be_target == GL_TEXTURE_2D_ARRAY || be_target == GL_TEXTURE_3D)
        BE(glFramebufferTextureLayer)(fbt, att, name, level, layer);
    else if (be_target == GL_TEXTURE_CUBE_MAP)
        BE(glFramebufferTexture2D)(fbt, att, FACE_FIRST + (GLenum)layer, name, level);
    else
        BE(glFramebufferTexture2D)(fbt, att, GL_TEXTURE_2D, name, level);
}

static void ci_blit_fallback(GLuint sn, GLenum st, GLenum sbe, GLint sl, GLint sx, GLint sy, GLint sz,
                             GLuint dn, GLenum dt, GLenum dbe, GLint dl, GLint dx, GLint dy, GLint dz,
                             GLsizei w, GLsizei h, GLsizei d, int s1, int d1)
{
    gl31_state_t* gs = gl31_state();
    GLenum sf = ci_format_of(sn, st, sl), df = ci_format_of(dn, dt, dl);
    GLuint fb[2] = {0, 0};
    GLsizei k;
    GLboolean scissor;
    GLenum att;
    GLbitfield mask;
    int ok = 1;
    if (s1 || d1) { gl31_stub_warn("glCopyImageSubData(1D_ARRAY) sin backend ES 3.2"); gl31_set_error(GL_INVALID_OPERATION); return; }
    if (!sf || !df || ci_is_depth(sf) != ci_is_depth(df) || sf == 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (gl31_tex_is_compressed_fmt(sf) || gl31_tex_is_compressed_fmt(df)) {
        gl31_stub_warn("glCopyImageSubData(formato comprimido) sin backend ES 3.2 / EXT_copy_image");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (ci_is_depth(sf)) {
        att = ci_has_stencil(sf) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        mask = GL_DEPTH_BUFFER_BIT | (ci_has_stencil(sf) ? GL_STENCIL_BUFFER_BIT : 0);
    } else { att = GL_COLOR_ATTACHMENT0; mask = GL_COLOR_BUFFER_BIT; }
    BE(glGenFramebuffers)(2, fb);
    scissor = BE(glIsEnabled)(GL_SCISSOR_TEST);
    if (scissor) BE(glDisable)(GL_SCISSOR_TEST);
    for (k = 0; k < d && ok; k++) {
        GLenum rs;
        BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, fb[0]);
        BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fb[1]);
        ci_attach(GL_READ_FRAMEBUFFER, att, sn, st, sbe, sl, sz + k);
        ci_attach(GL_DRAW_FRAMEBUFFER, att, dn, dt, dbe, dl, dz + k);
        if (att == GL_COLOR_ATTACHMENT0) { BE(glReadBuffer)(GL_COLOR_ATTACHMENT0); BE(glDrawBuffers)(1, &(GLenum){GL_COLOR_ATTACHMENT0}); }
        rs = BE(glCheckFramebufferStatus)(GL_READ_FRAMEBUFFER);
        if (rs != GL_FRAMEBUFFER_COMPLETE || BE(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            gl31_set_error(GL_INVALID_OPERATION); ok = 0; break;
        }
        BE(glBlitFramebuffer)(sx, sy, sx + w, sy + h, dx, dy, dx + w, dy + h, mask, GL_NEAREST);
    }
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, gs->read_fbo);
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, gl31_fbo_be_draw(gs->draw_fbo));
    if (scissor) BE(glEnable)(GL_SCISSOR_TEST);
    BE(glDeleteFramebuffers)(2, fb);
}

void gl31_glCopyImageSubData(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz,
                             GLuint dn, GLenum dt, GLint dl, GLint dx, GLint dy, GLint dz,
                             GLsizei w, GLsizei h, GLsizei d)
{
    GLenum sbe = 0, dbe = 0;
    int s1 = 0, d1 = 0;
    if (!ci_target(st, &sbe, &s1) || !ci_target(dt, &dbe, &d1)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (w < 0 || h < 0 || d < 0 || sl < 0 || dl < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (st != GL_RENDERBUFFER && dt != GL_RENDERBUFFER && !s1 && !d1)
        cs_copy_image(sn, st, sl, sx, sy, sz, dn, dt, dl, dx, dy, dz, w, h, d);   /* sombra de comprimidas */
    if (!gl31_caps.copy_image) {
        ci_blit_fallback(sn, st, sbe, sl, sx, sy, sz, dn, dt, dbe, dl, dx, dy, dz, w, h, d, s1, d1);
        return;
    }
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
