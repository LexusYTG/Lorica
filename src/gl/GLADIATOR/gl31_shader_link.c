/* gl31_shader_link.c - shaders y programas: conversion GLSL en CompileShader,
 * comprobaciones de estado de GL 3.1 y linkeo en el backend GLES.
 * Los nombres (ids) son los del backend. Las tablas son globales: sin locking,
 * como el resto del estado compartido de esta capa. */
#include "gl31.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ATTACH 16

typedef struct {
    GLuint id; GLenum type;
    char *source, *log;
    GLint compiled;
    int deleted;
    int has_binds;                 /* el fuente del backend lleva layout(location) de BindFragDataLocation */
    gl31_sampler_info_t* smp;      /* samplers 1D/Rect reescritos (NULL si no hay) */
} shader_t;

typedef struct {
    GLuint id;
    GLuint att[MAX_ATTACH]; int natt;
    GLint linked;
    char* log;
    int deleted;
    struct { char name[64]; GLuint loc; GLuint idx; } fb[16];   /* glBindFragDataLocation */
    int nfb;
    gl31_sampler_info_t smp;       /* tipos de desktop de los samplers del programa */
} prog_t;

static shader_t* g_sh; static size_t g_nsh, g_csh;
static prog_t*   g_pr; static size_t g_npr, g_cpr;

static char* dupstr(const char* s)
{
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void set_str(char** dst, const char* s)
{
    free(*dst);
    *dst = s ? dupstr(s) : NULL;
}

/* ---------- tablas ---------- */
static shader_t* sh_find(GLuint id)
{
    size_t i;
    for (i = 0; i < g_nsh; i++) if (g_sh[i].id == id) return &g_sh[i];
    return NULL;
}

static shader_t* sh_add(GLuint id, GLenum type)
{
    shader_t* s = sh_find(id);
    if (!s) {
        if (g_nsh == g_csh) {
            size_t c = g_csh ? g_csh * 2 : 32;
            shader_t* p = (shader_t*)realloc(g_sh, c * sizeof *p);
            if (!p) return NULL;
            g_sh = p; g_csh = c;
        }
        s = &g_sh[g_nsh++];
    } else {
        free(s->source); free(s->log); free(s->smp);
    }
    memset(s, 0, sizeof *s);
    s->id = id; s->type = type;
    return s;
}

static void sh_free_at(size_t i)
{
    free(g_sh[i].source); free(g_sh[i].log); free(g_sh[i].smp);
    g_sh[i] = g_sh[--g_nsh];
}

static prog_t* pr_find(GLuint id)
{
    size_t i;
    for (i = 0; i < g_npr; i++) if (g_pr[i].id == id) return &g_pr[i];
    return NULL;
}

static prog_t* pr_add(GLuint id)
{
    prog_t* p = pr_find(id);
    if (!p) {
        if (g_npr == g_cpr) {
            size_t c = g_cpr ? g_cpr * 2 : 16;
            prog_t* n = (prog_t*)realloc(g_pr, c * sizeof *n);
            if (!n) return NULL;
            g_pr = n; g_cpr = c;
        }
        p = &g_pr[g_npr++];
    } else free(p->log);
    memset(p, 0, sizeof *p);
    p->id = id;
    return p;
}

static void pr_free_at(size_t i)
{
    free(g_pr[i].log);
    g_pr[i] = g_pr[--g_npr];
}

static int shader_referenced(GLuint id)
{
    size_t i; int k;
    for (i = 0; i < g_npr; i++)
        for (k = 0; k < g_pr[i].natt; k++) if (g_pr[i].att[k] == id) return 1;
    return 0;
}

/* libera shaders marcados como borrados que ya no estan adjuntos a ningun programa */
static void shader_gc(void)
{
    size_t i = 0;
    while (i < g_nsh) {
        if (g_sh[i].deleted && !shader_referenced(g_sh[i].id)) sh_free_at(i);
        else i++;
    }
}

static void copy_log(const char* log, GLsizei bufSize, GLsizei* length, GLchar* out)
{
    size_t n = 0;
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (bufSize > 0 && out) {
        n = log ? strlen(log) : 0;
        if (n > (size_t)bufSize - 1) n = (size_t)bufSize - 1;
        if (n) memcpy(out, log, n);
        out[n] = 0;
    }
    if (length) *length = (GLsizei)n;
}

static char* backend_log(int is_shader, GLuint id)
{
    GLint len = 0;
    char* buf;
    if (is_shader) BE(glGetShaderiv)(id, GL_INFO_LOG_LENGTH, &len);
    else           BE(glGetProgramiv)(id, GL_INFO_LOG_LENGTH, &len);
    if (len <= 1) return NULL;
    buf = (char*)malloc((size_t)len);
    if (!buf) return NULL;
    buf[0] = 0;
    if (is_shader) BE(glGetShaderInfoLog)(id, len, NULL, buf);
    else           BE(glGetProgramInfoLog)(id, len, NULL, buf);
    return buf;
}

void gl31_link_shutdown(void)
{
    size_t i;
    for (i = 0; i < g_nsh; i++) { free(g_sh[i].source); free(g_sh[i].log); free(g_sh[i].smp); }
    for (i = 0; i < g_npr; i++) free(g_pr[i].log);
    free(g_sh); free(g_pr);
    g_sh = NULL; g_nsh = g_csh = 0;
    g_pr = NULL; g_npr = g_cpr = 0;
}

/* ---------- shaders ---------- */
GLuint gl31_glCreateShader(GLenum type)
{
    GLuint id;
    /* geometry shaders: GL 3.2, solo si el backend los tiene (ES 3.2 o EXT_geometry_shader) */
    if (type == GL_GEOMETRY_SHADER && !gl31_caps.geometry) {
        gl31_stub_warn("GL_GEOMETRY_SHADER (backend sin geometry shaders)");
        gl31_set_error(GL_INVALID_ENUM);
        return 0;
    }
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER && type != GL_GEOMETRY_SHADER) {
        gl31_set_error(GL_INVALID_ENUM);
        return 0;
    }
    id = BE(glCreateShader)(type);
    if (id && !sh_add(id, type)) {
        BE(glDeleteShader)(id);
        gl31_set_error(GL_OUT_OF_MEMORY);
        return 0;
    }
    return id;
}

void gl31_glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)
{
    shader_t* sh = sh_find(shader);
    size_t total = 0, off = 0;
    GLsizei i;
    char* buf;
    if (!sh || count < 0 || (count > 0 && !string)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) {
        if (!string[i]) { gl31_set_error(GL_INVALID_VALUE); return; }
        total += (length && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
    }
    buf = (char*)malloc(total + 1);
    if (!buf) { gl31_set_error(GL_OUT_OF_MEMORY); return; }
    for (i = 0; i < count; i++) {
        size_t n = (length && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
        memcpy(buf + off, string[i], n);
        off += n;
    }
    buf[total] = 0;
    free(sh->source);
    sh->source = buf;
}

void gl31_glCompileShader(GLuint shader)
{
    shader_t* sh = sh_find(shader);
    char err[512];
    char* conv;
    GLint ok = 0;
    if (!sh) { gl31_set_error(GL_INVALID_VALUE); return; }
    sh->compiled = 0;
    set_str(&sh->log, NULL);
    if (!sh->source || !sh->source[0]) {
        set_str(&sh->log, "ERROR: 0:0: sin codigo fuente");
        return;
    }
    {
        gl31_sampler_info_t info;
        memset(&info, 0, sizeof info);
        conv = gl31_glsl_convert_ex(sh->source, sh->type, NULL, 0, &info, err, sizeof err);
        if (!conv) { set_str(&sh->log, err); return; }
        free(sh->smp);
        sh->smp = NULL;
        sh->has_binds = 0;
        if (info.n > 0 && (sh->smp = (gl31_sampler_info_t*)malloc(sizeof *sh->smp)) != NULL)
            *sh->smp = info;
    }
    BE(glShaderSource)(shader, 1, (const GLchar* const*)&conv, NULL);
    BE(glCompileShader)(shader);
    BE(glGetShaderiv)(shader, GL_COMPILE_STATUS, &ok);
    sh->compiled = ok;
    free(sh->log);
    sh->log = backend_log(1, shader);
    free(conv);
}

void gl31_glGetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    shader_t* sh = sh_find(shader);
    if (!params) return;
    if (!sh) { gl31_set_error(GL_INVALID_VALUE); return; }
    switch (pname) {
        case GL_SHADER_TYPE:           *params = (GLint)sh->type; break;
        case GL_DELETE_STATUS:         *params = sh->deleted ? GL_TRUE : GL_FALSE; break;
        case GL_COMPILE_STATUS:        *params = sh->compiled ? GL_TRUE : GL_FALSE; break;
        case GL_INFO_LOG_LENGTH:       *params = (sh->log && sh->log[0]) ? (GLint)strlen(sh->log) + 1 : 0; break;
        case GL_SHADER_SOURCE_LENGTH:  *params = sh->source ? (GLint)strlen(sh->source) + 1 : 0; break;
        default: gl31_set_error(GL_INVALID_ENUM);
    }
}

void gl31_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* log)
{
    shader_t* sh = sh_find(shader);
    if (!sh) { gl31_set_error(GL_INVALID_VALUE); return; }
    copy_log(sh->log, bufSize, length, log);
}

void gl31_glDeleteShader(GLuint shader)
{
    shader_t* sh;
    if (!shader) return;
    sh = sh_find(shader);
    if (!sh) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glDeleteShader)(shader);
    sh->deleted = 1;
    shader_gc();
}

/* ---------- programas ---------- */
GLuint gl31_glCreateProgram(void)
{
    GLuint id = BE(glCreateProgram)();
    if (id && !pr_add(id)) {
        BE(glDeleteProgram)(id);
        gl31_set_error(GL_OUT_OF_MEMORY);
        return 0;
    }
    return id;
}

void gl31_glAttachShader(GLuint program, GLuint shader)
{
    prog_t* p = pr_find(program);
    shader_t* sh = sh_find(shader);
    int k;
    if (!p || !sh) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (k = 0; k < p->natt; k++)
        if (p->att[k] == shader) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (p->natt >= MAX_ATTACH) { gl31_set_error(GL_INVALID_OPERATION); return; }
    p->att[p->natt++] = shader;
    BE(glAttachShader)(program, shader);
}

void gl31_glDetachShader(GLuint program, GLuint shader)
{
    prog_t* p = pr_find(program);
    int k;
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (k = 0; k < p->natt; k++) {
        if (p->att[k] == shader) {
            p->att[k] = p->att[--p->natt];
            BE(glDetachShader)(program, shader);
            shader_gc();
            return;
        }
    }
    gl31_set_error(sh_find(shader) ? GL_INVALID_OPERATION : GL_INVALID_VALUE);
}

void gl31_glLinkProgram(GLuint program)
{
    prog_t* p = pr_find(program);
    GLint ok = 0;
    int k;
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    p->linked = 0;
    set_str(&p->log, NULL);
    for (k = 0; k < p->natt; k++) {
        shader_t* sh = sh_find(p->att[k]);
        if (!sh || !sh->compiled) {
            char msg[96];
            snprintf(msg, sizeof msg, "ERROR: shader %u adjunto no compilado correctamente", p->att[k]);
            set_str(&p->log, msg);
            return;
        }
    }
    /* glBindFragDataLocation: GLES solo admite layout(location) en el shader, asi que los
     * fragment shaders se reconvierten con las ubicaciones pedidas antes de linkear */
    for (k = 0; k < p->natt; k++) {
        shader_t* sh = sh_find(p->att[k]);
        gl31_fragbind_t fb[16];
        char err[512];
        char* conv;
        int n;
        if (!sh || sh->type != GL_FRAGMENT_SHADER || !sh->source) continue;
        if (!p->nfb && !sh->has_binds) continue;
        for (n = 0; n < p->nfb; n++) { fb[n].name = p->fb[n].name; fb[n].location = p->fb[n].loc; fb[n].index = p->fb[n].idx; }
        conv = gl31_glsl_convert_ex(sh->source, sh->type, fb, p->nfb, NULL, err, sizeof err);
        if (!conv) { set_str(&p->log, err); return; }
        BE(glShaderSource)(sh->id, 1, (const GLchar* const*)&conv, NULL);
        BE(glCompileShader)(sh->id);
        BE(glGetShaderiv)(sh->id, GL_COMPILE_STATUS, &ok);
        free(conv);
        sh->has_binds = p->nfb > 0;
        if (!ok) {
            free(p->log);
            p->log = backend_log(1, sh->id);
            sh->compiled = 0;
            return;
        }
    }
    p->smp.n = 0;
    for (k = 0; k < p->natt; k++) {
        shader_t* sh = sh_find(p->att[k]);
        int j;
        if (!sh || !sh->smp) continue;
        for (j = 0; j < sh->smp->n && p->smp.n < GL31_MAX_SAMPLER_INFO; j++)
            p->smp.s[p->smp.n++] = sh->smp->s[j];
    }
    BE(glLinkProgram)(program);
    BE(glGetProgramiv)(program, GL_LINK_STATUS, &ok);
    p->linked = ok;
    free(p->log);
    p->log = backend_log(0, program);
}

void gl31_glGetProgramiv(GLuint program, GLenum pname, GLint* params)
{
    prog_t* p = pr_find(program);
    if (!params) return;
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    switch (pname) {
        case GL_LINK_STATUS:      *params = p->linked ? GL_TRUE : GL_FALSE; break;
        case GL_DELETE_STATUS:    *params = p->deleted ? GL_TRUE : GL_FALSE; break;
        case GL_ATTACHED_SHADERS: *params = p->natt; break;
        case GL_INFO_LOG_LENGTH:  *params = (p->log && p->log[0]) ? (GLint)strlen(p->log) + 1 : 0; break;
        default:
            if (!p->linked && pname != GL_VALIDATE_STATUS) { *params = 0; break; }
            BE(glGetProgramiv)(program, pname, params);
    }
}

void gl31_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* log)
{
    prog_t* p = pr_find(program);
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    copy_log(p->log, bufSize, length, log);
}

void gl31_glUseProgram(GLuint program)
{
    gl31_state_t* s = gl31_state();
    GLuint old = s->program;
    if (program) {
        prog_t* p = pr_find(program);
        if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (!p->linked) { gl31_set_error(GL_INVALID_OPERATION); return; }
    }
    s->program = program;
    BE(glUseProgram)(program);
    if (old && old != program) {              /* programa borrado que seguia en uso */
        size_t i;
        for (i = 0; i < g_npr; i++)
            if (g_pr[i].id == old && g_pr[i].deleted) { pr_free_at(i); shader_gc(); break; }
    }
}

void gl31_glDeleteProgram(GLuint program)
{
    prog_t* p;
    size_t i;
    if (!program) return;
    p = pr_find(program);
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glDeleteProgram)(program);
    if (gl31_state()->program == program) { p->deleted = 1; return; }   /* sigue en uso */
    for (i = 0; i < g_npr; i++)
        if (g_pr[i].id == program) { pr_free_at(i); break; }
    shader_gc();
}

void gl31_glBindAttribLocation(GLuint program, GLuint index, const GLchar* name)
{
    GLint max = 16;
    if (!pr_find(program) || !name) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &max);
    if ((GLint)index >= max) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (strncmp(name, "gl_", 3) == 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glBindAttribLocation)(program, index, name);
}

GLint gl31_glGetAttribLocation(GLuint program, const GLchar* name)
{
    prog_t* p = pr_find(program);
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return -1; }
    if (!p->linked) { gl31_set_error(GL_INVALID_OPERATION); return -1; }
    if (!name || strncmp(name, "gl_", 3) == 0) return -1;
    return BE(glGetAttribLocation)(program, name);
}

GLint gl31_glGetUniformLocation(GLuint program, const GLchar* name)
{
    prog_t* p = pr_find(program);
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return -1; }
    if (!p->linked) { gl31_set_error(GL_INVALID_OPERATION); return -1; }
    if (!name || strncmp(name, "gl_", 3) == 0) return -1;
    return BE(glGetUniformLocation)(program, name);
}

/* helper interno: -2 = es un shader, -1 = no existe, 0 = no linkeado, 1 = linkeado */
int gl31_program_status(GLuint program)
{
    prog_t* p = pr_find(program);
    if (p) return p->linked ? 1 : 0;
    return sh_find(program) ? -2 : -1;
}

/* Tipo de sampler tal como lo declaro la app. Los sampler1D / 1DArray / 2DRect se reescriben
 * a 2D / 2DArray en el backend; aqui se recupera su tipo de desktop por nombre. */
GLenum gl31_program_sampler_type(GLuint program, const char* uniform_name, GLenum backend_type)
{
    prog_t* p = pr_find(program);
    char base[64];
    size_t n;
    int i;
    if (!p || !uniform_name || !p->smp.n) return backend_type;
    n = strlen(uniform_name);
    if (n > 3 && strcmp(uniform_name + n - 3, "[0]") == 0) n -= 3;
    if (n >= sizeof base) return backend_type;
    memcpy(base, uniform_name, n);
    base[n] = 0;
    for (i = 0; i < p->smp.n; i++)
        if (strcmp(p->smp.s[i].name, base) == 0) return p->smp.s[i].type;
    return backend_type;
}

GLenum gl31_program_uniform_type(GLuint program, const char* uniform_name, GLenum backend_type)
{
    return gl31_program_sampler_type(program, uniform_name, backend_type);
}

/* GL 3.0 / 3.3: asocia una salida del fragment shader a un color attachment (y a un indice de blend dual) */
void gl31_glBindFragDataLocationIndexed(GLuint program, GLuint color, GLuint index, const GLchar* name)
{
    prog_t* p = pr_find(program);
    GLint max = 4;
    int i;
    if (!p || !name) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (index > 1) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (strncmp(name, "gl_", 3) == 0 || strlen(name) >= sizeof p->fb[0].name) {
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (index == 1) {
        if (!gl31_caps.dual_src) {
            gl31_stub_warn("dual-source blending (requiere EXT_blend_func_extended)");
            gl31_set_error(GL_INVALID_OPERATION);
            return;
        }
        if (color >= 1) { gl31_set_error(GL_INVALID_VALUE); return; }    /* MAX_DUAL_SOURCE_DRAW_BUFFERS = 1 */
    }
    BE(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &max);
    if ((GLint)color >= max) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < p->nfb; i++)
        if (strcmp(p->fb[i].name, name) == 0) { p->fb[i].loc = color; p->fb[i].idx = index; return; }
    if (p->nfb >= 16) { gl31_set_error(GL_OUT_OF_MEMORY); return; }
    strcpy(p->fb[p->nfb].name, name);
    p->fb[p->nfb].idx = index;
    p->fb[p->nfb++].loc = color;       /* surte efecto en el proximo glLinkProgram */
}

void gl31_glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name)
{
    gl31_glBindFragDataLocationIndexed(program, color, 0, name);
}

/* indice (0 o 1) de una salida del fragment shader; -1 si no existe */
GLint gl31_glGetFragDataIndex(GLuint program, const GLchar* name)
{
    prog_t* p = pr_find(program);
    int i;
    if (!p || !name) { gl31_set_error(GL_INVALID_OPERATION); return -1; }
    if (!p->linked) { gl31_set_error(GL_INVALID_OPERATION); return -1; }
    for (i = 0; i < p->nfb; i++)
        if (strcmp(p->fb[i].name, name) == 0) return (GLint)p->fb[i].idx;
    return BE(glGetFragDataLocation)(program, name) >= 0 ? 0 : -1;
}

/* ---------- consultas de objetos (se sirven de las tablas propias: el backend
 * veria el GLSL ya convertido y no el codigo original de la app) ---------- */
GLboolean gl31_glIsShader(GLuint shader)
{
    return (shader && sh_find(shader)) ? GL_TRUE : GL_FALSE;
}

GLboolean gl31_glIsProgram(GLuint program)
{
    return (program && pr_find(program)) ? GL_TRUE : GL_FALSE;
}

void gl31_glValidateProgram(GLuint program)
{
    switch (gl31_program_status(program)) {
        case -2: gl31_set_error(GL_INVALID_OPERATION); return;   /* es un shader */
        case -1: gl31_set_error(GL_INVALID_VALUE);     return;   /* no existe */
        default: BE(glValidateProgram)(program);
    }
}

/* devuelve el fuente original de la app (no el convertido a GLSL ES) */
void gl31_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source)
{
    shader_t* sh = sh_find(shader);
    if (!sh) { gl31_set_error(pr_find(shader) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    copy_log(sh->source, bufSize, length, source);
}

void gl31_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders)
{
    prog_t* p = pr_find(program);
    GLsizei n, i;
    if (!p) { gl31_set_error(sh_find(program) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (maxCount < 0 || (maxCount > 0 && !shaders)) { gl31_set_error(GL_INVALID_VALUE); return; }
    n = p->natt < maxCount ? p->natt : maxCount;
    for (i = 0; i < n; i++) shaders[i] = p->att[i];
    if (count) *count = n;
}

/* ==================================================================
 * GL 4.1: separate shader objects y program binary
 * ================================================================== */
#ifndef GL_PROGRAM_SEPARABLE
#define GL_PROGRAM_SEPARABLE 0x8258
#endif
#ifndef GL_PROGRAM_BINARY_RETRIEVABLE_HINT
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0x8257
#endif

/* glCreateShaderProgramv: shader + programa separable de una sola etapa (pasa por nuestro conversor) */
GLuint gl31_glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar* const* strings)
{
    GLuint sh, pr;
    prog_t* p;
    shader_t* s;
    GLint ok = 0;
    if (!gl31_caps.es31) {
        gl31_stub_warn("glCreateShaderProgramv (backend sin ES 3.1)");
        gl31_set_error(GL_INVALID_OPERATION);
        return 0;
    }
    if (count < 0 || (count > 0 && !strings)) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER && type != GL_GEOMETRY_SHADER &&
        type != GL_COMPUTE_SHADER && type != GL_TESS_CONTROL_SHADER && type != GL_TESS_EVALUATION_SHADER) {
        gl31_set_error(GL_INVALID_ENUM);
        return 0;
    }
    sh = gl31_glCreateShader(type);
    if (!sh) return 0;
    gl31_glShaderSource(sh, count, strings, NULL);
    gl31_glCompileShader(sh);
    pr = gl31_glCreateProgram();
    if (!pr) { gl31_glDeleteShader(sh); return 0; }
    s = sh_find(sh);
    p = pr_find(pr);
    if (s && p && s->compiled) {
        BE(glProgramParameteri)(pr, GL_PROGRAM_SEPARABLE, GL_TRUE);
        gl31_glAttachShader(pr, sh);
        gl31_glLinkProgram(pr);
        gl31_glDetachShader(pr, sh);
    } else if (s && p) {
        set_str(&p->log, s->log);             /* el log del programa es el de la compilacion */
    }
    (void)ok;
    gl31_glDeleteShader(sh);
    return pr;
}

void gl31_glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
    if (!pr_find(program)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (pname != GL_PROGRAM_SEPARABLE && pname != GL_PROGRAM_BINARY_RETRIEVABLE_HINT) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (value != GL_TRUE && value != GL_FALSE) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (pname == GL_PROGRAM_SEPARABLE && !gl31_caps.es31) {
        gl31_stub_warn("GL_PROGRAM_SEPARABLE (backend sin ES 3.1)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    BE(glProgramParameteri)(program, pname, value);
}

void gl31_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* format, void* binary)
{
    prog_t* p = pr_find(program);
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!p->linked || bufSize < 0) { gl31_set_error(p->linked ? GL_INVALID_VALUE : GL_INVALID_OPERATION); return; }
    BE(glGetProgramBinary)(program, bufSize, length, format, binary);
}

void gl31_glProgramBinary(GLuint program, GLenum format, const void* binary, GLsizei length)
{
    prog_t* p = pr_find(program);
    GLint ok = 0;
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!binary || length < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glProgramBinary)(program, format, binary, length);
    BE(glGetProgramiv)(program, GL_LINK_STATUS, &ok);
    p->linked = ok;
    free(p->log);
    p->log = backend_log(0, program);
    /* un binario no trae los tipos de sampler de desktop ni los frag-data de la app */
    p->smp.n = 0;
}
