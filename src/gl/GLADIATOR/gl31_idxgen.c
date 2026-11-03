/* gl31_idxgen.c - reescritura de indices en la GPU (compute de GLES 3.1) para lo que ES no tiene:
 *   - glPrimitiveRestartIndex con un indice distinto del fijo (ES solo admite 0xFF/0xFFFF/0xFFFFFFFF)
 *   - glPolygonMode(LINE / POINT)             (los triangulos se dibujan como lineas / puntos)
 *   - glProvokingVertex(FIRST_VERTEX_CONVENTION)  (se rota cada primitiva para que el provocador sea el ultimo)
 * Un unico kernel genera, en un buffer de GPU, una lista de indices UNSIGNED_INT con 0xFFFFFFFF como
 * "ranura vacia"; el draw se hace entonces con el restart de indice fijo de ES. Sin CPU en el camino. */
#include "gl31.h"
#include "gl31_idx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_ELEMENT_ARRAY_BARRIER_BIT
#define GL_ELEMENT_ARRAY_BARRIER_BIT 0x00000002
#endif

static const char k_src[] =
"#version 310 es\n"
"precision highp float; precision highp int;\n"
"layout(local_size_x = 64) in;\n"
"layout(std430, binding = 0) readonly buffer SrcB { uint src[]; };\n"
"layout(std430, binding = 1) writeonly buffer DstB { uint dst[]; };\n"
"uniform uint u_op;        /* 0 restart, 1 poly lineas, 2 poly puntos, 3 provoking */\n"
"uniform uint u_prim;      /* 0 pts 1 lines 2 lstrip 3 lloop 4 tris 5 strip 6 fan */\n"
"uniform uint u_type;      /* 0 arrays, 1 ubyte, 2 ushort, 3 uint */\n"
"uniform uint u_count, u_first, u_srcoff, u_restart, u_hasr, u_threads, u_k, u_gx;\n"
"const uint R = 0xFFFFFFFFu;\n"
"uint fetch(uint p) {\n"
"  if (u_type == 0u) return u_first + p;\n"
"  if (u_type == 1u) { uint b = u_srcoff + p; return (src[b >> 2] >> ((b & 3u) * 8u)) & 0xFFu; }\n"
"  if (u_type == 2u) { uint b = u_srcoff + p * 2u; return (src[b >> 2] >> ((b & 2u) * 8u)) & 0xFFFFu; }\n"
"  return src[(u_srcoff >> 2) + p];\n"
"}\n"
"bool isr(uint v) { return u_hasr != 0u && v == u_restart; }\n"
"bool rp(uint p) { return isr(fetch(p)); }\n"
"/* primera posicion del segmento (entre restarts) que contiene p; tope de busqueda 8192 */\n"
"uint segstart(uint p) {\n"
"  if (u_hasr == 0u) return 0u;\n"
"  uint lim = p > 8192u ? p - 8192u : 0u;\n"
"  for (uint q = p; q > lim; q--) { if (rp(q - 1u)) return q; }\n"
"  return lim;\n"
"}\n"
"/* triangulo de la hebra g: t = orden de giro correcto; rf = rotado para que el provocador (primero) quede ultimo */\n"
"bool gettri(uint g, out uvec3 t, out uvec3 rf) {\n"
"  t = uvec3(R); rf = uvec3(R);\n"
"  uint n = u_count;\n"
"  if (u_prim == 4u) {\n"
"    uint p = u_hasr != 0u ? g : g * 3u;\n"
"    if (p + 2u >= n) return false;\n"
"    if (u_hasr != 0u) {\n"
"      if (rp(p) || rp(p + 1u) || rp(p + 2u)) return false;\n"
"      if (((p - segstart(p)) % 3u) != 0u) return false;\n"
"    }\n"
"    uint a = fetch(p), b = fetch(p + 1u), c = fetch(p + 2u);\n"
"    t = uvec3(a, b, c); rf = uvec3(b, c, a); return true;\n"
"  }\n"
"  if (u_prim == 5u) {\n"
"    uint p = g;\n"
"    if (p + 2u >= n) return false;\n"
"    uint par = p & 1u;\n"
"    if (u_hasr != 0u) { if (rp(p) || rp(p + 1u) || rp(p + 2u)) return false; par = (p - segstart(p)) & 1u; }\n"
"    uint a = fetch(p), b = fetch(p + 1u), c = fetch(p + 2u);\n"
"    if (par == 0u) { t = uvec3(a, b, c); rf = uvec3(b, c, a); }\n"
"    else           { t = uvec3(b, a, c); rf = uvec3(c, b, a); }\n"
"    return true;\n"
"  }\n"
"  /* abanico: g es la posicion del segundo vertice */\n"
"  uint p = u_hasr != 0u ? g : g + 1u;\n"
"  if (p + 1u >= n) return false;\n"
"  uint c0 = 0u;\n"
"  if (u_hasr != 0u) { if (rp(p) || rp(p + 1u)) return false; c0 = segstart(p); if (c0 >= p) return false; }\n"
"  uint v0 = fetch(c0), a = fetch(p), b = fetch(p + 1u);\n"
"  t = uvec3(v0, a, b); rf = uvec3(b, v0, a); return true;\n"
"}\n"
"/* linea de la hebra g / ranura s */\n"
"bool getline(uint g, uint s, out uvec2 l, out uvec2 rl) {\n"
"  l = uvec2(R); rl = uvec2(R);\n"
"  uint n = u_count;\n"
"  if (u_prim == 1u) {\n"
"    uint p = u_hasr != 0u ? g : g * 2u;\n"
"    if (p + 1u >= n) return false;\n"
"    if (u_hasr != 0u) { if (rp(p) || rp(p + 1u)) return false; if (((p - segstart(p)) & 1u) != 0u) return false; }\n"
"    uint a = fetch(p), b = fetch(p + 1u); l = uvec2(a, b); rl = uvec2(b, a); return true;\n"
"  }\n"
"  if (u_prim == 2u || (u_prim == 3u && s == 0u && u_hasr != 0u)) {\n"
"    uint p = g;\n"
"    if (p + 1u >= n) return false;\n"
"    if (u_hasr != 0u && (rp(p) || rp(p + 1u))) return false;\n"
"    uint a = fetch(p), b = fetch(p + 1u); l = uvec2(a, b); rl = uvec2(b, a); return true;\n"
"  }\n"
"  if (u_prim == 3u && u_hasr == 0u) {\n"
"    uint a = fetch(g), b = fetch((g + 1u) % n); l = uvec2(a, b); rl = uvec2(b, a); return true;\n"
"  }\n"
"  /* lazo con restart, ranura 1: linea de cierre al final de cada segmento */\n"
"  uint p = g;\n"
"  if (rp(p)) return false;\n"
"  if (p + 1u < n && !rp(p + 1u)) return false;\n"
"  uint c0 = segstart(p);\n"
"  if (c0 >= p) return false;\n"
"  uint a = fetch(p), b = fetch(c0); l = uvec2(a, b); rl = uvec2(b, a); return true;\n"
"}\n"
"void main() {\n"
"  uint g = gl_GlobalInvocationID.y * u_gx + gl_GlobalInvocationID.x;\n"
"  if (g >= u_threads) return;\n"
"  uint o = g * u_k;\n"
"  if (u_op == 0u) { uint v = fetch(g); dst[o] = isr(v) ? R : v; return; }\n"
"  if (u_prim >= 4u) {\n"
"    uvec3 t, rf;\n"
"    bool ok = gettri(g, t, rf);\n"
"    if (u_op == 1u) {\n"
"      if (!ok) { for (uint i = 0u; i < 6u; i++) dst[o + i] = R; return; }\n"
"      dst[o] = t.x; dst[o+1u] = t.y; dst[o+2u] = t.y; dst[o+3u] = t.z; dst[o+4u] = t.z; dst[o+5u] = t.x;\n"
"    } else if (u_op == 2u) {\n"
"      if (!ok) { for (uint i = 0u; i < 3u; i++) dst[o + i] = R; return; }\n"
"      dst[o] = t.x; dst[o+1u] = t.y; dst[o+2u] = t.z;\n"
"    } else {\n"
"      if (!ok) { for (uint i = 0u; i < 3u; i++) dst[o + i] = R; return; }\n"
"      dst[o] = rf.x; dst[o+1u] = rf.y; dst[o+2u] = rf.z;\n"
"    }\n"
"    return;\n"
"  }\n"
"  /* lineas, solo provoking */\n"
"  uint slots = (u_prim == 3u && u_hasr != 0u) ? 2u : 1u;\n"
"  for (uint s = 0u; s < slots; s++) {\n"
"    uvec2 l, rl;\n"
"    bool ok = getline(g, s, l, rl);\n"
"    dst[o + s * 2u] = ok ? rl.x : R; dst[o + s * 2u + 1u] = ok ? rl.y : R;\n"
"  }\n"
"}\n";

static GLuint g_prog;
static int g_state;                 /* 0 sin probar, 1 listo, -1 no disponible */
static GLint u_op, u_prim, u_type, u_count, u_first, u_srcoff, u_restart, u_hasr, u_threads, u_k, u_gx;
static GLuint g_scratch, g_pad;
static size_t g_cap, g_padcap;      /* bytes reservados en g_scratch / g_pad */

static int ensure_prog(void)
{
    GLuint sh;
    GLint ok = 0;
    const char* src = k_src;
    if (g_state) return g_state == 1;
    g_state = -1;
    if (!gl31_caps.es31) return 0;
    sh = BE(glCreateShader)(GL_COMPUTE_SHADER);
    BE(glShaderSource)(sh, 1, &src, NULL);
    BE(glCompileShader)(sh);
    BE(glGetShaderiv)(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = "";
        BE(glGetShaderInfoLog)(sh, sizeof log, NULL, log);
        fprintf(stderr, "LORICA: kernel de indices no compila: %s\n", log);
        BE(glDeleteShader)(sh);
        return 0;
    }
    g_prog = BE(glCreateProgram)();
    BE(glAttachShader)(g_prog, sh);
    BE(glLinkProgram)(g_prog);
    BE(glDeleteShader)(sh);
    BE(glGetProgramiv)(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) { BE(glDeleteProgram)(g_prog); g_prog = 0; return 0; }
#define UL(n) u_##n = BE(glGetUniformLocation)(g_prog, "u_" #n)
    UL(op); UL(prim); UL(type); UL(count); UL(first); UL(srcoff); UL(restart); UL(hasr); UL(threads); UL(k); UL(gx);
#undef UL
    g_state = 1;
    return 1;
}

void gl31_idx_shutdown(void)
{
    /* los objetos GL se destruyen con el contexto; solo se olvida el estado */
    g_prog = 0; g_scratch = 0; g_pad = 0; g_cap = 0; g_padcap = 0; g_state = 0;
}

static int is_tri(GLenum m) { return m == GL_TRIANGLES || m == GL_TRIANGLE_STRIP || m == GL_TRIANGLE_FAN; }
static int is_line(GLenum m) { return m == GL_LINES || m == GL_LINE_STRIP || m == GL_LINE_LOOP; }
static unsigned prim_code(GLenum m)
{
    switch (m) {
        case GL_POINTS: return 0; case GL_LINES: return 1; case GL_LINE_STRIP: return 2; case GL_LINE_LOOP: return 3;
        case GL_TRIANGLES: return 4; case GL_TRIANGLE_STRIP: return 5; default: return 6;
    }
}

int gl31_idx_needed(GLenum mode, int indexed, GLenum type, GLenum* out_mode, int* unavailable)
{
    gl31_state_t* s = gl31_state();
    GLuint fixed = (type == GL_UNSIGNED_BYTE) ? 0xFFu : (type == GL_UNSIGNED_SHORT) ? 0xFFFFu : 0xFFFFFFFFu;
    int op = IDX_NONE;
    *out_mode = mode; *unavailable = 0;
    if (s->polygon_mode != GL_FILL && is_tri(mode)) {
        op = (s->polygon_mode == GL_LINE) ? IDX_POLY_LINES : IDX_POLY_POINTS;
        *out_mode = (s->polygon_mode == GL_LINE) ? GL_LINES : GL_POINTS;
        if (s->xfb_active) *unavailable = 1;          /* TF exige primitivas del tipo con que se inicio */
    } else if (s->provoking_vertex == GL_FIRST_VERTEX_CONVENTION && !gl31_caps.provoking_vertex &&
               (is_tri(mode) || is_line(mode))) {
        op = IDX_PROVOKE;
        *out_mode = is_tri(mode) ? GL_TRIANGLES : GL_LINES;
    } else if (indexed && s->prim_restart && s->prim_restart_index != fixed) {
        op = IDX_RESTART;
    }
    if (op != IDX_NONE && !*unavailable && !ensure_prog()) *unavailable = 1;
    return op;
}

/* Salva el estado que toca el kernel y lo restaura */
typedef struct { GLint prog, gen, id[2]; GLint64 start[2], size[2]; } saved_t;

static void save(saved_t* sv)
{
    int i;
    BE(glGetIntegerv)(GL_SHADER_STORAGE_BUFFER_BINDING, &sv->gen);
    for (i = 0; i < 2; i++) {
        BE(glGetIntegeri_v)(GL_SHADER_STORAGE_BUFFER_BINDING, (GLuint)i, &sv->id[i]);
        BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_START, (GLuint)i, &sv->start[i]);
        BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_SIZE, (GLuint)i, &sv->size[i]);
    }
}

static void restore(const saved_t* sv)
{
    gl31_state_t* s = gl31_state();
    int i;
    for (i = 0; i < 2; i++) {
        if (sv->id[i] && sv->size[i] > 0) BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, (GLuint)i, (GLuint)sv->id[i], (GLintptr)sv->start[i], (GLsizeiptr)sv->size[i]);
        else BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, (GLuint)i, (GLuint)sv->id[i]);
    }
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, (GLuint)sv->gen);
    BE(glUseProgram)(s->program);
}

GLuint gl31_idx_generate(int op, GLenum mode, int indexed, GLenum type, GLint first, GLsizei count,
                         uintptr_t srcoff, GLsizei* out_count)
{
    gl31_state_t* s = gl31_state();
    uint64_t threads = 0, k = 1, nout;
    int hasr = indexed && s->prim_restart;
    GLint srcbuf = 0;
    GLuint groups, gx, gy;
    saved_t sv;
    *out_count = 0;
    if (count <= 0) return g_scratch ? g_scratch : 0;
    if (indexed) { BE(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &srcbuf); if (!srcbuf) return 0; }
    {
        uint64_t n = (uint64_t)count;
        if (op == IDX_RESTART) { threads = n; k = 1; }
        else if (is_tri(mode)) {
            threads = hasr ? n : (mode == GL_TRIANGLES ? n / 3 : (n >= 3 ? n - 2 : 0));
            k = (op == IDX_POLY_LINES) ? 6 : 3;
        } else {      /* lineas (provoking) */
            if (hasr) { threads = n; k = (mode == GL_LINE_LOOP) ? 4 : 2; }
            else {
                threads = (mode == GL_LINES) ? n / 2 : (mode == GL_LINE_STRIP ? (n >= 2 ? n - 1 : 0) : (n >= 2 ? n : 0));
                k = 2;
            }
        }
    }
    if (!threads) return g_scratch ? g_scratch : 0;
    nout = threads * k;
    if (nout * 4 > (uint64_t)256 * 1024 * 1024) return 0;      /* mas de 256 MB de indices: no se intenta */

    save(&sv);
    /* El SSBO se lee por palabras de 32 bit: si la ultima palabra del buffer de indices queda parcialmente
     * fuera (tamano no multiplo de 4), leerla seria un acceso fuera de rango. En ese caso la region
     * necesaria se copia, en la GPU, a un buffer con relleno. */
    if (indexed) {
        GLint64 bs = 0;
        uint64_t esz = (type == GL_UNSIGNED_BYTE) ? 1 : (type == GL_UNSIGNED_SHORT) ? 2 : 4;
        uint64_t endb = (uint64_t)srcoff + (uint64_t)count * esz;
        if (BE(glGetBufferParameteri64v)) BE(glGetBufferParameteri64v)(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &bs);
        else { GLint b32 = 0; BE(glGetBufferParameteriv)(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &b32); bs = b32; }
        if ((bs & 3) && endb > (uint64_t)(bs & ~(GLint64)3)) {
            uint64_t start = (uint64_t)srcoff & ~(uint64_t)3;
            uint64_t clen = (endb < (uint64_t)bs ? endb : (uint64_t)bs) - start;
            uint64_t plen = (clen + 3) & ~(uint64_t)3;
            if (!g_pad) BE(glGenBuffers)(1, &g_pad);
            BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, g_pad);
            if (g_padcap < plen) {
                size_t nc = 1024;
                while (nc < plen) nc <<= 1;
                BE(glBufferData)(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)nc, NULL, GL_DYNAMIC_COPY);
                g_padcap = nc;
            }
            BE(glCopyBufferSubData)(GL_ELEMENT_ARRAY_BUFFER, GL_SHADER_STORAGE_BUFFER, (GLintptr)start, 0, (GLsizeiptr)clen);
            srcbuf = (GLint)g_pad;
            srcoff = (uintptr_t)(srcoff - start);
        }
    }
    if (!g_scratch) BE(glGenBuffers)(1, &g_scratch);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, g_scratch);
    if (g_cap < nout * 4) {
        size_t nc = 1024;
        while (nc < nout * 4) nc <<= 1;
        BE(glBufferData)(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)nc, NULL, GL_DYNAMIC_COPY);
        g_cap = nc;
    }
    BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 1, g_scratch, 0, (GLsizeiptr)(g_cap));
    if (indexed) BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)srcbuf);
    BE(glUseProgram)(g_prog);
    BE(glUniform1ui)(u_op, (op == IDX_RESTART) ? 0u : (op == IDX_POLY_LINES) ? 1u : (op == IDX_POLY_POINTS) ? 2u : 3u);
    BE(glUniform1ui)(u_prim, prim_code(mode));
    BE(glUniform1ui)(u_type, !indexed ? 0u : (type == GL_UNSIGNED_BYTE) ? 1u : (type == GL_UNSIGNED_SHORT) ? 2u : 3u);
    BE(glUniform1ui)(u_count, (GLuint)count);
    BE(glUniform1ui)(u_first, (GLuint)first);
    BE(glUniform1ui)(u_srcoff, (GLuint)srcoff);
    BE(glUniform1ui)(u_restart, s->prim_restart_index);
    BE(glUniform1ui)(u_hasr, hasr ? 1u : 0u);
    BE(glUniform1ui)(u_threads, (GLuint)threads);
    BE(glUniform1ui)(u_k, (GLuint)k);
    groups = (GLuint)((threads + 63) / 64);
    gx = groups > 65535u ? 65535u : groups;
    gy = (groups + gx - 1) / gx;
    BE(glUniform1ui)(u_gx, gx * 64u);
    if (gy > 65535u) { restore(&sv); return 0; }
    BE(glDispatchCompute)(gx, gy, 1);
    BE(glMemoryBarrier)(GL_ELEMENT_ARRAY_BARRIER_BIT);
    restore(&sv);
    *out_count = (GLsizei)nout;
    return g_scratch;
}
