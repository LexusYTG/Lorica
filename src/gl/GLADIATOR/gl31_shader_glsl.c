/* gl31_shader_glsl.c - conversor GLSL 1.10-1.50 / 3.30  ->  GLSL ES 3.00 / 3.10 / 3.20
 * (la version de salida es la del backend: gl31_caps.glsl_es)
 *
 * Transformaciones (todas textuales, sin AST):
 *   - #version NNN [core]  ->  #version 300 es  + precisiones por defecto
 *   - #extension GL_ARB_*  -> se elimina; el resto se sube a la cabecera
 *   - layout(): se quitan los calificadores que ES 3.00 no admite
 *     (binding, index, offset, ...) y location en varyings
 *   - GLSL < 1.30: attribute/varying -> in/out ; gl_FragColor/gl_FragData
 *   - texture2D/textureCube/... -> texture/textureLod/...
 *   - noperspective se elimina
 *   - sampler1D / 1DArray / 2DRect (y variantes i/u) -> sampler2D / 2DArray; las llamadas
 *     texture/textureLod/texelFetch/textureSize sobre ellos se redirigen a funciones
 *     auxiliares lorica_* inyectadas en la cabecera (coordenada extra 0.5, o p/size en Rect)
 *   - samplerBuffer, sampler2DMS, gl_ClipDistance y shaders de geometria: se pasan tal cual
 *     si el backend los tiene (con el #extension EXT/OES necesario bajo ES 3.20); si no, error
 *   - glBindFragDataLocation: se inyecta layout(location=N) en las salidas del fragment shader
 * Se rechaza (error claro) lo que no tiene equivalente: sombras 1D/Rect y built-ins del
 * perfil de compatibilidad (FPE).
 * Limitacion conocida: no se insertan conversiones implicitas int->float
 * (GLSL >= 1.20 las permite, ES no).
 * Los numeros de linea se conservan (#line 1) para que los logs sean utiles. */
#include "gl31.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { char* p; size_t n, cap; int oom; } sb_t;

static void sb_cat(sb_t* b, const char* s, size_t len)
{
    if (b->oom) return;
    if (b->n + len + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 256;
        char* p;
        while (c < b->n + len + 1) c *= 2;
        p = (char*)realloc(b->p, c);
        if (!p) { b->oom = 1; return; }
        b->p = p; b->cap = c;
    }
    if (len) memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = 0;
}
static void sb_str(sb_t* b, const char* s) { sb_cat(b, s, strlen(s)); }

static int is_id(unsigned char c) { return isalnum(c) || c == '_'; }

static void set_err(char* err, size_t len, const char* msg, const char* arg)
{
    if (err && len) snprintf(err, len, "ERROR: 0:1: %s%s%s", msg, arg ? ": " : "", arg ? arg : "");
}

/* comentarios -> espacios (se conservan los \n) */
static void strip_comments(char* s)
{
    while (*s) {
        if (s[0] == '/' && s[1] == '/') {
            while (*s && *s != '\n') *s++ = ' ';
        } else if (s[0] == '/' && s[1] == '*') {
            s[0] = s[1] = ' '; s += 2;
            while (*s && !(s[0] == '*' && s[1] == '/')) { if (*s != '\n') *s = ' '; s++; }
            if (*s) { s[0] = s[1] = ' '; s += 2; }
        } else s++;
    }
}

static const char* find_word(const char* s, const char* w)
{
    size_t n = strlen(w);
    const char* p = s;
    while ((p = strstr(p, w)) != NULL) {
        int pre  = (p == s) || !is_id((unsigned char)p[-1]);
        int post = !is_id((unsigned char)p[n]);
        if (pre && post) return p;
        p += n;
    }
    return NULL;
}

/* Reemplaza palabras completas. Libera s y devuelve la nueva cadena (NULL si OOM). */
static char* repl_word(char* s, const char* from, const char* to)
{
    sb_t b = {0, 0, 0, 0};
    const char *p, *q;
    size_t fl = strlen(from);
    if (!s || !find_word(s, from)) return s;
    p = s;
    while ((q = find_word(p, from)) != NULL) {
        sb_cat(&b, p, (size_t)(q - p));
        sb_str(&b, to);
        p = q + fl;
    }
    sb_str(&b, p);
    free(s);
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

/* ---------- #extension ---------- */
static void process_extensions(char* s, sb_t* ext)
{
    char* line = s;
    while (*line) {
        char* eol = strchr(line, '\n');
        char* t = line;
        if (!eol) eol = line + strlen(line);
        while (t < eol && (*t == ' ' || *t == '\t' || *t == '\r')) t++;
        if (eol - t >= 10 && strncmp(t, "#extension", 10) == 0) {
            const char* name = t + 10;
            while (name < eol && isspace((unsigned char)*name)) name++;
            if (strncmp(name, "GL_ARB_", 7) != 0) {   /* ARB: ya es core en ES */
                sb_cat(ext, t, (size_t)(eol - t));
                sb_str(ext, "\n");
            }
            while (t < eol) *t++ = ' ';
        }
        line = *eol ? eol + 1 : eol;
    }
}

/* ---------- layout() ---------- */
/* calificadores que ES (cualquier version) no admite */
static const char* const k_layout_strip[] = {
    "index", "component", "origin_upper_left", "pixel_center_integer",
    "xfb_buffer", "xfb_offset", "xfb_stride",
    "depth_any", "depth_greater", "depth_less", "depth_unchanged", NULL
};
/* calificadores que ES 3.00 no admite pero ES 3.10+ si (binding de UBO/SSBO/samplers/imagenes/atomics) */
static const char* const k_layout_strip_300[] = { "binding", "offset", "early_fragment_tests", NULL };

static GL31_TLS int g_saw_index;      /* un layout(index = N) quedo en el shader */

static int layout_strip_name(const char* n, size_t len, int es)
{
    int i;
    if (len == 5 && strncmp(n, "index", 5) == 0 && gl31_caps.dual_src) { g_saw_index = 1; return 0; }
    for (i = 0; k_layout_strip[i]; i++)
        if (strlen(k_layout_strip[i]) == len && strncmp(k_layout_strip[i], n, len) == 0) return 1;
    if (es < 310)
        for (i = 0; k_layout_strip_300[i]; i++)
            if (strlen(k_layout_strip_300[i]) == len && strncmp(k_layout_strip_300[i], n, len) == 0) return 1;
    return 0;
}

/* 1 = in, 2 = out, 0 = otra cosa; salta calificadores intermedios */
static int next_direction(const char* p)
{
    static const char* const skip[] = { "flat", "smooth", "centroid", "noperspective", "invariant",
                                        "highp", "mediump", "lowp", NULL };
    int iter;
    for (iter = 0; iter < 8; iter++) {
        const char* w; size_t n; int i, found = 0;
        while (*p && isspace((unsigned char)*p)) p++;
        w = p;
        while (is_id((unsigned char)*p)) p++;
        n = (size_t)(p - w);
        if (!n) return 0;
        if (n == 2 && strncmp(w, "in", 2) == 0)  return 1;
        if (n == 3 && strncmp(w, "out", 3) == 0) return 2;
        for (i = 0; skip[i]; i++)
            if (strlen(skip[i]) == n && strncmp(skip[i], w, n) == 0) { found = 1; break; }
        if (!found) return 0;
    }
    return 0;
}

static void emit_layout(sb_t* b, const char* a, const char* z, int drop_location, int es)
{
    sb_t t = {0, 0, 0, 0};
    const char* it = a;
    const char* c;
    int kept = 0;
    while (it <= z) {
        const char *end = it, *s0, *e0, *nm;
        while (end < z && *end != ',') end++;
        s0 = it; e0 = end;
        while (s0 < e0 && isspace((unsigned char)*s0)) s0++;
        while (e0 > s0 && isspace((unsigned char)e0[-1])) e0--;
        if (e0 > s0) {
            size_t nlen = 0;
            nm = s0;
            while (nm + nlen < e0 && is_id((unsigned char)nm[nlen])) nlen++;
            if (!layout_strip_name(nm, nlen, es) &&
                !(drop_location && nlen == 8 && strncmp(nm, "location", 8) == 0)) {
                if (kept++) sb_str(&t, ", ");
                sb_cat(&t, s0, (size_t)(e0 - s0));
            }
        }
        if (end >= z) break;
        it = end + 1;
    }
    if (kept) { sb_str(b, "layout("); sb_cat(b, t.p, t.n); sb_str(b, ")"); }
    for (c = a; c < z; c++) if (*c == '\n') sb_str(b, "\n");   /* conservar lineas */
    free(t.p);
}

static char* filter_layouts(const char* s, GLenum type, int es)
{
    sb_t b = {0, 0, 0, 0};
    const char* p = s;
    for (;;) {
        const char* q = find_word(p, "layout");
        const char *o, *close;
        int dir, drop;
        if (!q) break;
        o = q + 6;
        while (*o && isspace((unsigned char)*o)) o++;
        close = (*o == '(') ? strchr(o, ')') : NULL;
        if (!close) { sb_cat(&b, p, (size_t)(q + 6 - p)); p = q + 6; continue; }
        sb_cat(&b, p, (size_t)(q - p));
        dir = next_direction(close + 1);
        /* ES 3.00: location solo en entradas de vertice y salidas de fragmento; ES 3.10+ lo admite entre etapas */
        drop = es < 310 && ((type == GL_VERTEX_SHADER && dir == 2) || (type == GL_FRAGMENT_SHADER && dir == 1));
        emit_layout(&b, o + 1, close, drop, es);
        p = close + 1;
    }
    sb_str(&b, p);
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

/* ---------- tablas ---------- */
/* siempre sin equivalente: perfil de compatibilidad / FPE y sombras 1D/Rect */
static const char* const k_unsupported[] = {
    "sampler1DShadow", "sampler1DArrayShadow", "sampler2DRectShadow",
    "image1D", "iimage1D", "uimage1D", "image1DArray", "iimage1DArray", "uimage1DArray",
    "image2DRect", "iimage2DRect", "uimage2DRect", "subroutine", "textureQueryLod", "textureQueryLevels",
    "gl_Vertex", "gl_Normal", "gl_Color", "gl_SecondaryColor", "gl_MultiTexCoord0",
    "gl_ModelViewMatrix", "gl_ProjectionMatrix", "gl_ModelViewProjectionMatrix",
    "gl_NormalMatrix", "gl_TexCoord", "gl_FogFragCoord", "gl_FrontColor", "gl_BackColor",
    "gl_ClipVertex", NULL
};

static const char* const k_rename[][2] = {
    { "texture2D", "texture" },         { "texture2DProj", "textureProj" },
    { "texture2DLod", "textureLod" },   { "texture2DProjLod", "textureProjLod" },
    { "texture3D", "texture" },         { "texture3DLod", "textureLod" },
    { "textureCube", "texture" },       { "textureCubeLod", "textureLod" },
    { "texture1D", "texture" },         { "texture1DLod", "textureLod" },
    { "texture1DArray", "texture" },    { "texture1DArrayLod", "textureLod" },
    { "texture2DRect", "texture" },
    { NULL, NULL }
};

/* ---------- samplers que ES no tiene ---------- */
enum { K_1D, K_1DA, K_RECT, K_COUNT };
enum { V_F, V_I, V_U };

typedef struct { const char* from; const char* to; int kind, v; GLenum gl; } smp_t;
static const smp_t k_smp[] = {
    { "sampler1D",        "sampler2D",       K_1D,   V_F, GL_SAMPLER_1D },
    { "isampler1D",       "isampler2D",      K_1D,   V_I, GL_INT_SAMPLER_1D },
    { "usampler1D",       "usampler2D",      K_1D,   V_U, GL_UNSIGNED_INT_SAMPLER_1D },
    { "sampler1DArray",   "sampler2DArray",  K_1DA,  V_F, GL_SAMPLER_1D_ARRAY },
    { "isampler1DArray",  "isampler2DArray", K_1DA,  V_I, GL_INT_SAMPLER_1D_ARRAY },
    { "usampler1DArray",  "usampler2DArray", K_1DA,  V_U, GL_UNSIGNED_INT_SAMPLER_1D_ARRAY },
    { "sampler2DRect",    "sampler2D",       K_RECT, V_F, GL_SAMPLER_2D_RECT },
    { "isampler2DRect",   "isampler2D",      K_RECT, V_I, GL_INT_SAMPLER_2D_RECT },
    { "usampler2DRect",   "usampler2D",      K_RECT, V_U, GL_UNSIGNED_INT_SAMPLER_2D_RECT },
    { NULL, NULL, 0, 0, 0 }
};

#define MAX_SMP_NAMES 64
typedef struct { char name[64]; int kind, v; GLenum gl; } smpname_t;
typedef struct { smpname_t n[MAX_SMP_NAMES]; int cnt; } smpnames_t;

static const smpname_t* smp_lookup(const smpnames_t* t, const char* nm, size_t len)
{
    int i;
    for (i = 0; i < t->cnt; i++)
        if (strlen(t->n[i].name) == len && strncmp(t->n[i].name, nm, len) == 0) return &t->n[i];
    return NULL;
}

static void skip_ws(const char** p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

/* lee un identificador; devuelve su longitud (0 = no hay) */
static size_t read_id(const char** p, const char** start)
{
    const char* q = *p;
    skip_ws(&q);
    *start = q;
    while (is_id((unsigned char)*q)) q++;
    *p = q;
    return (size_t)(q - *start);
}

/* Recoge los nombres declarados con un tipo de sampler de desktop. 0 = demasiados */
static int collect_smp_names(const char* s, smpnames_t* t)
{
    int k;
    for (k = 0; k_smp[k].from; k++) {
        const char* p = s;
        const char* q;
        while ((q = find_word(p, k_smp[k].from)) != NULL) {
            const char* c = q + strlen(k_smp[k].from);
            const char* nm;
            size_t nl;
            p = c;
            for (;;) {
                nl = read_id(&c, &nm);
                if (!nl || nl >= sizeof t->n[0].name) break;
                if (!smp_lookup(t, nm, nl)) {
                    if (t->cnt >= MAX_SMP_NAMES) return 0;
                    memcpy(t->n[t->cnt].name, nm, nl);
                    t->n[t->cnt].name[nl] = 0;
                    t->n[t->cnt].kind = k_smp[k].kind;
                    t->n[t->cnt].v = k_smp[k].v;
                    t->n[t->cnt].gl = k_smp[k].gl;
                    t->cnt++;
                }
                skip_ws(&c);
                while (*c == '[') { while (*c && *c != ']') c++; if (*c) c++; skip_ws(&c); }
                if (*c != ',') break;
                {   /* otro nombre solo si lo que sigue a la coma es `ident` + , ; ) [ */
                    const char* d = c + 1;
                    const char* n2;
                    size_t l2 = read_id(&d, &n2);
                    skip_ws(&d);
                    if (!l2 || !(*d == ',' || *d == ';' || *d == ')' || *d == '[')) break;
                    c = c + 1;
                }
            }
        }
    }
    return 1;
}

static const char* const k_vsfx[] = { "f", "i", "u" };
static const char* const k_ksfx[] = { "1D", "1DA", "RECT" };

/* Reemplaza fn(NAME, ...) por lorica_fn_KIND_V(NAME, ...) cuando NAME es un sampler reescrito.
 * `used` marca los (fn, kind, v) usados para generar los auxiliares. Devuelve NULL si
 * hay un error (msg en *bad) o sin memoria (bad == NULL). */
enum { F_TEX, F_LOD, F_FETCH, F_SIZE, F_COUNT };
static const char* const k_fn[F_COUNT] = { "texture", "textureLod", "texelFetch", "textureSize" };
static const char* const k_fn_unsup[] = { "textureProj", "textureProjLod", "textureGrad", "textureOffset",
                                          "texelFetchOffset", "textureLodOffset", "textureGather", NULL };

static char* rewrite_calls(char* s, const smpnames_t* t, unsigned char used[F_COUNT][K_COUNT][3],
                           const char** bad)
{
    sb_t b = {0, 0, 0, 0};
    const char* p = s;
    int f, any = 0;
    *bad = NULL;
    if (!t->cnt) return s;
    /* funciones sin soporte sobre samplers reescritos */
    for (f = 0; k_fn_unsup[f]; f++) {
        const char* q = s;
        while ((q = find_word(q, k_fn_unsup[f])) != NULL) {
            const char* c = q + strlen(k_fn_unsup[f]);
            const char* nm;
            size_t nl;
            q = c;
            skip_ws(&c);
            if (*c != '(') continue;
            c++;
            nl = read_id(&c, &nm);
            if (nl && smp_lookup(t, nm, nl)) { *bad = k_fn_unsup[f]; free(s); return NULL; }
        }
    }
    while (*p) {
        /* siguiente aparicion de cualquiera de las 4 funciones */
        const char* best = NULL;
        int bf = -1;
        for (f = 0; f < F_COUNT; f++) {
            const char* q = find_word(p, k_fn[f]);
            if (q && (!best || q < best)) { best = q; bf = f; }
        }
        if (!best) break;
        {
            const char* c = best + strlen(k_fn[bf]);
            const char* nm;
            size_t nl;
            const smpname_t* e = NULL;
            skip_ws(&c);
            if (*c == '(') {
                c++;
                nl = read_id(&c, &nm);
                if (nl) e = smp_lookup(t, nm, nl);
            }
            sb_cat(&b, p, (size_t)(best - p));
            if (e) {
                char nmb[64];
                snprintf(nmb, sizeof nmb, "lorica_%s_%s_%s", k_fn[bf], k_ksfx[e->kind], k_vsfx[e->v]);
                sb_str(&b, nmb);
                used[bf][e->kind][e->v] = 1;
                any = 1;
            } else {
                sb_str(&b, k_fn[bf]);
            }
            p = best + strlen(k_fn[bf]);
        }
    }
    sb_str(&b, p);
    (void)any;
    free(s);
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

/* texto de las funciones auxiliares usadas */
static void emit_helpers(sb_t* o, unsigned char used[F_COUNT][K_COUNT][3])
{
    static const char* const pre[3]  = { "", "i", "u" };
    static const char* const rty[3]  = { "vec4", "ivec4", "uvec4" };
    char buf[512];
    int f, k, v;
    for (v = 0; v < 3; v++)
        for (k = 0; k < K_COUNT; k++)
            for (f = 0; f < F_COUNT; f++) {
                const char* sty = (k == K_1DA) ? "sampler2DArray" : "sampler2D";
                const char* nm = k_fn[f];
                if (!used[f][k][v]) continue;
#define EMIT(...) do { snprintf(buf, sizeof buf, __VA_ARGS__); sb_str(o, buf); } while (0)
                if (k == K_1D) {
                    if (f == F_TEX) {
                        EMIT("%s lorica_%s_1D_%s(highp %s%s s, float x){return texture(s,vec2(x,0.5));} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                        if (v == V_F)
                            sb_str(o, "vec4 lorica_texture_1D_f(highp sampler2D s, float x, float b){return texture(s,vec2(x,0.5),b);} ");
                    } else if (f == F_LOD)
                        EMIT("%s lorica_%s_1D_%s(highp %s%s s, float x, float l){return textureLod(s,vec2(x,0.5),l);} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                    else if (f == F_FETCH)
                        EMIT("%s lorica_%s_1D_%s(highp %s%s s, int p, int l){return texelFetch(s,ivec2(p,0),l);} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                    else
                        EMIT("int lorica_%s_1D_%s(highp %s%s s, int l){return textureSize(s,l).x;} ",
                             nm, k_vsfx[v], pre[v], sty);
                } else if (k == K_1DA) {
                    if (f == F_TEX) {
                        EMIT("%s lorica_%s_1DA_%s(highp %s%s s, vec2 c){return texture(s,vec3(c.x,0.5,c.y));} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                        if (v == V_F)
                            sb_str(o, "vec4 lorica_texture_1DA_f(highp sampler2DArray s, vec2 c, float b){return texture(s,vec3(c.x,0.5,c.y),b);} ");
                    } else if (f == F_LOD)
                        EMIT("%s lorica_%s_1DA_%s(highp %s%s s, vec2 c, float l){return textureLod(s,vec3(c.x,0.5,c.y),l);} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                    else if (f == F_FETCH)
                        EMIT("%s lorica_%s_1DA_%s(highp %s%s s, ivec2 p, int l){return texelFetch(s,ivec3(p.x,0,p.y),l);} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                    else
                        EMIT("ivec2 lorica_%s_1DA_%s(highp %s%s s, int l){ivec3 z=textureSize(s,l);return ivec2(z.x,z.z);} ",
                             nm, k_vsfx[v], pre[v], sty);
                } else {   /* K_RECT: coordenadas sin normalizar, sin mipmaps */
                    if (f == F_TEX)
                        EMIT("%s lorica_%s_RECT_%s(highp %s%s s, vec2 p){return texture(s,p/vec2(textureSize(s,0)));} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                    else if (f == F_FETCH)
                        EMIT("%s lorica_%s_RECT_%s(highp %s%s s, ivec2 p){return texelFetch(s,p,0);} ",
                             rty[v], nm, k_vsfx[v], pre[v], sty);
                    else if (f == F_SIZE)
                        EMIT("ivec2 lorica_%s_RECT_%s(highp %s%s s){return textureSize(s,0);} ",
                             nm, k_vsfx[v], pre[v], sty);
                    /* textureLod sobre Rect no existe en GLSL de escritorio */
                }
#undef EMIT
            }
}

/* ---------- salidas del fragment shader: layout(location=N) segun glBindFragDataLocation ---------- */
static const gl31_fragbind_t* find_bind(const gl31_fragbind_t* b, int n, const char* nm, size_t len)
{
    int i;
    for (i = 0; i < n; i++)
        if (b[i].name && strlen(b[i].name) == len && strncmp(b[i].name, nm, len) == 0) return &b[i];
    return NULL;
}

static char* inject_frag_locations(char* s, const gl31_fragbind_t* binds, int nbinds)
{
    sb_t b = {0, 0, 0, 0};
    const char* p = s;
    const char* q;
    if (!binds || nbinds <= 0) return s;
    while ((q = find_word(p, "out")) != NULL) {
        /* inicio de la sentencia: tras el ultimo ; { } anterior */
        const char* st = q;
        const gl31_fragbind_t* bd = NULL;
        int has_layout = 0;
        const char* c;
        while (st > p && st[-1] != ';' && st[-1] != '{' && st[-1] != '}') st--;
        {
            const char* w;
            for (w = st; w < q; w++)
                if (strncmp(w, "layout", 6) == 0 && (w == st || !is_id((unsigned char)w[-1]))) has_layout = 1;
        }
        /* despues de `out`: [calificadores] tipo nombre[...] */
        c = q + 3;
        {
            const char* w; size_t n; int tok = 0; const char* last = NULL; size_t lastn = 0;
            for (;;) {
                n = read_id(&c, &w);
                if (!n) break;
                tok++; last = w; lastn = n;
                skip_ws(&c);
                if (*c == '[') { while (*c && *c != ']') c++; if (*c) c++; skip_ws(&c); }
                if (*c == ';' || *c == ',' || *c == '{') break;
                if (tok > 6) { last = NULL; break; }
            }
            if (last && tok >= 2 && *c != '{') bd = find_bind(binds, nbinds, last, lastn);
        }
        sb_cat(&b, p, (size_t)(q - p));
        if (bd && !has_layout) {
            char tmp[48];
            if (bd->index) snprintf(tmp, sizeof tmp, "layout(location = %u, index = %u) ", bd->location, bd->index);
            else           snprintf(tmp, sizeof tmp, "layout(location = %u) ", bd->location);
            sb_str(&b, tmp);
        }
        sb_str(&b, "out");
        p = q + 3;
    }
    sb_str(&b, p);
    free(s);
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

/* precision por defecto: solo los tipos que el backend tiene */
static void emit_precisions(sb_t* o)
{
    sb_str(o, "precision highp float; precision highp int; ");
    sb_str(o, "precision highp sampler2D; precision highp samplerCube; precision highp sampler3D; "
              "precision highp sampler2DArray; precision highp sampler2DShadow; "
              "precision highp samplerCubeShadow; precision highp sampler2DArrayShadow; "
              "precision highp isampler2D; precision highp isampler3D; precision highp isamplerCube; "
              "precision highp isampler2DArray; precision highp usampler2D; precision highp usampler3D; "
              "precision highp usamplerCube; precision highp usampler2DArray; ");
    if (gl31_caps.multisample_tex)
        sb_str(o, "precision highp sampler2DMS; precision highp isampler2DMS; precision highp usampler2DMS; ");
    if (gl31_caps.ms_array)
        sb_str(o, "precision highp sampler2DMSArray; precision highp isampler2DMSArray; "
                  "precision highp usampler2DMSArray; ");
    if (gl31_caps.cube_array)
        sb_str(o, "precision highp samplerCubeArray; precision highp isamplerCubeArray; "
                  "precision highp usamplerCubeArray; precision highp samplerCubeArrayShadow; ");
    if (gl31_caps.es31)
        sb_str(o, "precision highp image2D; precision highp iimage2D; precision highp uimage2D; "
                  "precision highp image3D; precision highp iimage3D; precision highp uimage3D; "
                  "precision highp imageCube; precision highp iimageCube; precision highp uimageCube; "
                  "precision highp image2DArray; precision highp iimage2DArray; precision highp uimage2DArray; ");
    if (gl31_caps.tex_buffer)
        sb_str(o, "precision highp samplerBuffer; precision highp isamplerBuffer; "
                  "precision highp usamplerBuffer; ");
}

static int is_stage(GLenum t)
{
    return t == GL_VERTEX_SHADER || t == GL_FRAGMENT_SHADER ||
           (t == GL_GEOMETRY_SHADER && gl31_caps.geometry) ||
           (t == GL_COMPUTE_SHADER && gl31_caps.es31) ||
           ((t == GL_TESS_CONTROL_SHADER || t == GL_TESS_EVALUATION_SHADER) && gl31_caps.tess);
}

/* gl_PerVertex redeclarado (GLSL de escritorio, programas separables / tessellation): ES lo tiene implicito */
static void strip_pervertex(char* s)
{
    const char* q = s;
    while ((q = find_word(q, "gl_PerVertex")) != NULL) {
        char* a = s + (q - s);
        char* st = a;
        char* e;
        int depth = 0;
        {   /* retroceder sobre espacios y un calificador in / out */
            char* t = a;
            while (t > s && isspace((unsigned char)t[-1])) t--;
            if (t - s >= 3 && strncmp(t - 3, "out", 3) == 0 && (t - 3 == s || !is_id((unsigned char)t[-4]))) st = t - 3;
            else if (t - s >= 2 && strncmp(t - 2, "in", 2) == 0 && (t - 2 == s || !is_id((unsigned char)t[-3]))) st = t - 2;
        }
        e = a;
        while (*e && *e != '{' && *e != ';') e++;
        if (*e == '{') {
            for (; *e; e++) {
                if (*e == '{') depth++;
                else if (*e == '}' && --depth == 0) { e++; break; }
            }
            while (*e && *e != ';') e++;
        }
        if (*e == ';') e++;
        for (; st < e; st++) if (*st != '\n') *st = ' ';
        q = e;
    }
}

/* double / dvecN / dmatN -> float / vecN / matN (ES no tiene fp64: se pierde precision) */
static char* demote_doubles(char* s, int* did)
{
    static const char* const from[] = { "double", "dvec2", "dvec3", "dvec4", "dmat2", "dmat3", "dmat4",
                                        "dmat2x2", "dmat2x3", "dmat2x4", "dmat3x2", "dmat3x3", "dmat3x4",
                                        "dmat4x2", "dmat4x3", "dmat4x4", NULL };
    static const char* const to[]   = { "float", "vec2", "vec3", "vec4", "mat2", "mat3", "mat4",
                                        "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4",
                                        "mat4x2", "mat4x3", "mat4x4" };
    int i;
    size_t k, n;
    *did = 0;
    for (i = 0; from[i]; i++)
        if (find_word(s, from[i])) {
            *did = 1;
            if (!(s = repl_word(s, from[i], to[i]))) return NULL;
        }
    if (!*did) return s;
    /* sufijos de literal lf / LF */
    n = strlen(s);
    for (k = 1; k + 1 < n; k++) {
        if ((s[k] == 'l' || s[k] == 'L') && (s[k + 1] == 'f' || s[k + 1] == 'F') &&
            (isdigit((unsigned char)s[k - 1]) || s[k - 1] == '.') && !is_id((unsigned char)s[k + 2])) {
            size_t j = k;
            while (j > 0 && (isalnum((unsigned char)s[j - 1]) || s[j - 1] == '.' || s[j - 1] == '_')) j--;
            if (!isdigit((unsigned char)s[j]) && s[j] != '.') continue;
            s[k] = ' '; s[k + 1] = ' ';
        }
    }
    return s;
}

/* Devuelve memoria malloc'd (free) o NULL y rellena err. */
char* gl31_glsl_convert_ex(const char* src, GLenum shader_type,
                           const gl31_fragbind_t* binds, int nbinds,
                           gl31_sampler_info_t* samplers, char* err, size_t errlen)
{
    char *s = NULL, *p;
    sb_t ext = {0, 0, 0, 0}, o = {0, 0, 0, 0};
    long ver = 110;
    int i, uses_color = 0, uses_data = 0, es = gl31_caps.glsl_es ? gl31_caps.glsl_es : 300;
    char num[32];
    smpnames_t names;
    unsigned char used[F_COUNT][K_COUNT][3];
    const char* bad = NULL;

    memset(&names, 0, sizeof names);
    memset(used, 0, sizeof used);
    if (samplers) samplers->n = 0;
    if (err && errlen) err[0] = 0;
    if (!src || !is_stage(shader_type)) {
        set_err(err, errlen, "tipo de shader o fuente invalido", NULL);
        return NULL;
    }
    s = (char*)malloc(strlen(src) + 1);
    if (!s) goto oom;
    strcpy(s, src);
    strip_comments(s);

    /* --- #version --- */
    p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "#version", 8) == 0) {
        char* q = p + 8;
        char* endp;
        ver = strtol(q, &endp, 10);
        while (*endp == ' ' || *endp == '\t') endp++;
        if (endp[0] == 'e' && endp[1] == 's' && !is_id((unsigned char)endp[2])) {
            char* same = (char*)malloc(strlen(src) + 1);     /* ya es GLSL ES: sin tocar */
            if (!same) goto oom;
            strcpy(same, src);
            free(s);
            return same;
        }
        while (*p && *p != '\n') *p++ = ' ';
    }
    if (!(ver == 110 || ver == 120 || ver == 130 || ver == 140 || ver == 150 || ver == 330 ||
          ver == 400 || ver == 410 || ver == 420 || ver == 430 || ver == 440 || ver == 450 || ver == 460)) {
        snprintf(num, sizeof num, "%ld", ver);
        set_err(err, errlen, "version GLSL no soportada (admitido 110-150, 330, 400-460)", num);
        goto fail;
    }
    if (shader_type == GL_GEOMETRY_SHADER && ver < 150) {
        set_err(err, errlen, "los geometry shaders requieren #version 150", NULL);
        goto fail;
    }
    if ((shader_type == GL_TESS_CONTROL_SHADER || shader_type == GL_TESS_EVALUATION_SHADER) && ver < 400) {
        set_err(err, errlen, "los shaders de tessellation requieren #version 400", NULL);
        goto fail;
    }
    if (shader_type == GL_COMPUTE_SHADER && ver < 430) {
        set_err(err, errlen, "los compute shaders requieren #version 430", NULL);
        goto fail;
    }

    /* --- construcciones sin equivalente o que dependen del backend --- */
    for (i = 0; k_unsupported[i]; i++)
        if (find_word(s, k_unsupported[i])) {
            set_err(err, errlen, "no disponible en GL 3.x sobre GLES", k_unsupported[i]);
            goto fail;
        }
    {
        static const char* const ca[] = { "samplerCubeArray", "isamplerCubeArray", "usamplerCubeArray",
                                          "samplerCubeArrayShadow", NULL };
        static const char* const im[] = { "image2D", "iimage2D", "uimage2D", "image3D", "iimage3D", "uimage3D",
                                          "imageCube", "iimageCube", "uimageCube", "image2DArray",
                                          "iimage2DArray", "uimage2DArray", "atomic_uint", NULL };
        for (i = 0; ca[i]; i++)
            if (find_word(s, ca[i]) && !gl31_caps.cube_array) {
                set_err(err, errlen, "requiere cube map arrays (ES 3.2 o EXT_texture_cube_map_array)", ca[i]);
                goto fail;
            }
        for (i = 0; im[i]; i++)
            if (find_word(s, im[i]) && !gl31_caps.es31) {
                set_err(err, errlen, "requiere ES 3.1 en el backend", im[i]);
                goto fail;
            }
        if ((find_word(s, "buffer") && find_word(s, "std430")) && !gl31_caps.es31) {
            set_err(err, errlen, "los SSBO requieren ES 3.1 en el backend", "buffer");
            goto fail;
        }
    }
    if (find_word(s, "gl_ClipDistance") && !gl31_caps.clip_distance) {
        set_err(err, errlen, "gl_ClipDistance requiere EXT_clip_cull_distance en el backend", "gl_ClipDistance");
        goto fail;
    }
    {
        static const char* const ms[] = { "sampler2DMS", "isampler2DMS", "usampler2DMS", NULL };
        static const char* const msa[] = { "sampler2DMSArray", "isampler2DMSArray", "usampler2DMSArray", NULL };
        static const char* const bf[] = { "samplerBuffer", "isamplerBuffer", "usamplerBuffer", NULL };
        for (i = 0; ms[i]; i++)
            if (find_word(s, ms[i]) && !gl31_caps.multisample_tex) { set_err(err, errlen, "requiere ES 3.1 en el backend", ms[i]); goto fail; }
        for (i = 0; msa[i]; i++)
            if (find_word(s, msa[i]) && !gl31_caps.ms_array) { set_err(err, errlen, "requiere OES_texture_storage_multisample_2d_array", msa[i]); goto fail; }
        for (i = 0; bf[i]; i++)
            if (find_word(s, bf[i]) && !gl31_caps.tex_buffer) { set_err(err, errlen, "requiere texture buffers (ES 3.2 o EXT_texture_buffer)", bf[i]); goto fail; }
    }
    if (shader_type == GL_FRAGMENT_SHADER) {
        uses_color = find_word(s, "gl_FragColor") != NULL;
        uses_data  = find_word(s, "gl_FragData")  != NULL;
        if (uses_color && uses_data) {
            set_err(err, errlen, "gl_FragColor y gl_FragData a la vez", NULL);
            goto fail;
        }
    }

    process_extensions(s, &ext);
    if (ext.oom) goto oom;

    /* extensiones que el backend exige bajo ES < 3.20 */
    if (shader_type == GL_GEOMETRY_SHADER && es < 320) sb_str(&ext, "#extension GL_EXT_geometry_shader : require\n");
    if ((shader_type == GL_TESS_CONTROL_SHADER || shader_type == GL_TESS_EVALUATION_SHADER) && es < 320)
        sb_str(&ext, "#extension GL_EXT_tessellation_shader : require\n");
    if (es < 320 && (find_word(s, "samplerCubeArray") || find_word(s, "isamplerCubeArray") ||
                     find_word(s, "usamplerCubeArray") || find_word(s, "samplerCubeArrayShadow")))
        sb_str(&ext, "#extension GL_EXT_texture_cube_map_array : require\n");
    if (es < 320 && gl31_caps.tex_buffer &&
        (find_word(s, "samplerBuffer") || find_word(s, "isamplerBuffer") || find_word(s, "usamplerBuffer")))
        sb_str(&ext, "#extension GL_EXT_texture_buffer : require\n");
    if (find_word(s, "sampler2DMSArray") || find_word(s, "isampler2DMSArray") || find_word(s, "usampler2DMSArray"))
        sb_str(&ext, "#extension GL_OES_texture_storage_multisample_2d_array : require\n");
    if (find_word(s, "gl_ClipDistance"))
        sb_str(&ext, gl31_caps.clip_distance == 2 ? "#extension GL_ANGLE_clip_cull_distance : require\n"
                                                  : "#extension GL_EXT_clip_cull_distance : require\n");
    if (es < 320 && (gl31_caps.geometry || gl31_caps.tess) && gl31_caps.io_blocks_ext)
        sb_str(&ext, "#extension GL_EXT_shader_io_blocks : enable\n");

    strip_pervertex(s);
    {
        int did = 0;
        s = demote_doubles(s, &did);
        if (!s) goto oom;
    }
    if (es < 320 && !(s = repl_word(s, "precise", ""))) goto oom;

    /* --- palabras clave --- */
    if (ver < 130) {
        if (shader_type == GL_VERTEX_SHADER) {
            if (!(s = repl_word(s, "attribute", "in"))) goto oom;
            if (!(s = repl_word(s, "varying", "out"))) goto oom;
        } else {
            if (!(s = repl_word(s, "varying", "in"))) goto oom;
        }
    }
    for (i = 0; k_rename[i][0]; i++)
        if (!(s = repl_word(s, k_rename[i][0], k_rename[i][1]))) goto oom;
    if (!(s = repl_word(s, "noperspective", ""))) goto oom;
    if (uses_color && !(s = repl_word(s, "gl_FragColor", "lorica_FragColor"))) goto oom;
    if (uses_data  && !(s = repl_word(s, "gl_FragData",  "lorica_FragData")))  goto oom;

    /* --- samplers 1D / 1DArray / Rect --- */
    if (!collect_smp_names(s, &names)) {
        set_err(err, errlen, "demasiados samplers 1D/Rect", NULL);
        goto fail;
    }
    if (names.cnt) {
        if (samplers)
            for (i = 0; i < names.cnt && samplers->n < GL31_MAX_SAMPLER_INFO; i++) {
                memcpy(samplers->s[samplers->n].name, names.n[i].name, sizeof samplers->s[0].name);
                samplers->s[samplers->n].type = names.n[i].gl;
                samplers->n++;
            }
        s = rewrite_calls(s, &names, used, &bad);
        if (!s) {
            if (bad) { set_err(err, errlen, "funcion sin soporte sobre sampler 1D/Rect", bad); goto fail; }
            goto oom;
        }
        for (i = 0; k_smp[i].from; i++)
            if (!(s = repl_word(s, k_smp[i].from, k_smp[i].to))) goto oom;
    }

    g_saw_index = 0;
    p = filter_layouts(s, shader_type, es);
    free(s);
    s = p;
    if (!s) goto oom;
    if (shader_type == GL_FRAGMENT_SHADER && nbinds > 0) {
        int k2;
        s = inject_frag_locations(s, binds, nbinds);
        if (!s) goto oom;
        for (k2 = 0; k2 < nbinds; k2++) if (binds[k2].index) g_saw_index = 1;
    }
    if (g_saw_index && shader_type == GL_FRAGMENT_SHADER)
        sb_str(&ext, "#extension GL_EXT_blend_func_extended : require\n");

    /* --- salida: cabecera ES + #line 1 para conservar numeracion --- */
    snprintf(num, sizeof num, "#version %d es\n", es);
    sb_str(&o, num);
    if (ext.n) sb_cat(&o, ext.p, ext.n);
    emit_precisions(&o);
    if (uses_color) sb_str(&o, "layout(location = 0) out highp vec4 lorica_FragColor; ");
    if (uses_data)  sb_str(&o, "layout(location = 0) out highp vec4 lorica_FragData[4]; ");
    emit_helpers(&o, used);
    sb_str(&o, "\n#line 1\n");
    sb_str(&o, s);
    if (o.oom) goto oom;

    free(s); free(ext.p);
    return o.p;

oom:
    set_err(err, errlen, "sin memoria", NULL);
fail:
    free(s); free(ext.p); free(o.p);
    return NULL;
}

char* gl31_glsl_convert(const char* src, GLenum shader_type, char* err, size_t errlen)
{
    return gl31_glsl_convert_ex(src, shader_type, NULL, 0, NULL, err, errlen);
}
