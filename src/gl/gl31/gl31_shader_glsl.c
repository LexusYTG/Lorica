/* gl31_shader_glsl.c - conversor GLSL 1.10-1.50 / 3.30  ->  GLSL ES 3.00
 *
 * Transformaciones (todas textuales, sin AST):
 *   - #version NNN [core]  ->  #version 300 es  + precisiones por defecto
 *   - #extension GL_ARB_*  -> se elimina; el resto se sube a la cabecera
 *   - layout(): se quitan los calificadores que ES 3.00 no admite
 *     (binding, index, offset, ...) y location en varyings
 *   - GLSL < 1.30: attribute/varying -> in/out ; gl_FragColor/gl_FragData
 *   - texture2D/textureCube/... -> texture/textureLod/...
 *   - noperspective se elimina
 * Se rechaza (error claro) lo que no tiene equivalente: samplers 1D/Rect/Buffer,
 * gl_ClipDistance y built-ins del perfil de compatibilidad (FPE).
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
static const char* const k_layout_strip[] = {
    "binding", "index", "offset", "component", "origin_upper_left", "pixel_center_integer",
    "early_fragment_tests", "xfb_buffer", "xfb_offset", "xfb_stride",
    "depth_any", "depth_greater", "depth_less", "depth_unchanged", NULL
};

static int layout_strip_name(const char* n, size_t len)
{
    int i;
    for (i = 0; k_layout_strip[i]; i++)
        if (strlen(k_layout_strip[i]) == len && strncmp(k_layout_strip[i], n, len) == 0) return 1;
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

static void emit_layout(sb_t* b, const char* a, const char* z, int drop_location)
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
            if (!layout_strip_name(nm, nlen) &&
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

static char* filter_layouts(const char* s, GLenum type)
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
        drop = (type == GL_VERTEX_SHADER && dir == 2) || (type == GL_FRAGMENT_SHADER && dir == 1);
        emit_layout(&b, o + 1, close, drop);
        p = close + 1;
    }
    sb_str(&b, p);
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

/* ---------- tablas ---------- */
static const char* const k_unsupported[] = {
    "sampler1D", "isampler1D", "usampler1D", "sampler1DShadow", "sampler1DArray",
    "sampler2DRect", "isampler2DRect", "usampler2DRect", "sampler2DRectShadow",
    "samplerBuffer", "isamplerBuffer", "usamplerBuffer", "sampler2DMS", "gl_ClipDistance",
    /* perfil de compatibilidad / FPE */
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
    { NULL, NULL }
};

#define DEFAULT_PRECISIONS \
    "precision highp float; precision highp int; precision highp sampler2D; " \
    "precision highp samplerCube; precision highp sampler3D; precision highp sampler2DArray; " \
    "precision highp sampler2DShadow; precision highp isampler2D; precision highp usampler2D; "

/* Devuelve memoria malloc'd (free) o NULL y rellena err. */
char* gl31_glsl_convert(const char* src, GLenum shader_type, char* err, size_t errlen)
{
    char *s = NULL, *p;
    sb_t ext = {0, 0, 0, 0}, o = {0, 0, 0, 0};
    long ver = 110;
    int i, uses_color = 0, uses_data = 0;
    char num[32];

    if (err && errlen) err[0] = 0;
    if (!src || (shader_type != GL_VERTEX_SHADER && shader_type != GL_FRAGMENT_SHADER)) {
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
    if (!(ver == 110 || ver == 120 || ver == 130 || ver == 140 || ver == 150 || ver == 330)) {
        snprintf(num, sizeof num, "%ld", ver);
        set_err(err, errlen, "version GLSL no soportada (admitido 110-150, 330)", num);
        goto fail;
    }

    /* --- construcciones sin equivalente --- */
    for (i = 0; k_unsupported[i]; i++)
        if (find_word(s, k_unsupported[i])) {
            set_err(err, errlen, "no disponible en GL 3.1 core sobre GLES", k_unsupported[i]);
            goto fail;
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

    p = filter_layouts(s, shader_type);
    free(s);
    s = p;
    if (!s) goto oom;

    /* --- salida: cabecera ES + #line 1 para conservar numeracion --- */
    sb_str(&o, "#version 300 es\n");
    if (ext.n) sb_cat(&o, ext.p, ext.n);
    sb_str(&o, DEFAULT_PRECISIONS);
    if (uses_color) sb_str(&o, "layout(location = 0) out highp vec4 lorica_FragColor; ");
    if (uses_data)  sb_str(&o, "layout(location = 0) out highp vec4 lorica_FragData[4]; ");
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
