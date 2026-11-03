/* gl31_fbo.c - framebuffers, renderbuffers, lectura y clear por buffer.
 *
 * Diferencias de desktop que se resuelven aqui:
 *   - framebuffer por defecto: GL_BACK_LEFT/GL_BACK ... se traducen a GL_BACK;
 *     GL_FRONT* no existe en ES (se avisa).
 *   - glDrawBuffer / glReadBuffer (desktop) sobre glDrawBuffers / glReadBuffer.
 *   - internalformats sin tamano de renderbuffer (GL_DEPTH_COMPONENT, GL_RGBA...).
 *   - glReadPixels con formatos que ES no ofrece (BGRA, RGB, RED, tipos no
 *     nativos...): se lee RGBA8/RGBA32F y se convierte respetando GL_PACK_*.
 *   - glFramebufferTexture1D/3D y texturas RECTANGLE/1D (alias de 2D en el backend). */
#include "gl31.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef GL_IMPLEMENTATION_COLOR_READ_FORMAT
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#define GL_IMPLEMENTATION_COLOR_READ_TYPE   0x8B9A
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8     0x8035
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_FRONT_LEFT
#define GL_FRONT_LEFT 0x0400
#endif
#ifndef GL_BACK_LEFT
#define GL_BACK_LEFT 0x0402
#endif

/* ---------- utilidades ---------- */
static int is_fbo_target(GLenum t)
{
    return t == GL_FRAMEBUFFER || t == GL_READ_FRAMEBUFFER || t == GL_DRAW_FRAMEBUFFER;
}

static GLuint bound_fbo(GLenum target)
{
    gl31_state_t* s = gl31_state();
    return target == GL_READ_FRAMEBUFFER ? s->read_fbo : s->draw_fbo;
}

static GLint max_color_attachments(void)
{
    GLint m = 4;
    BE(glGetIntegerv)(GL_MAX_COLOR_ATTACHMENTS, &m);
    return m < 1 ? 1 : m;
}

/* 1 = valido */
static int attachment_ok(GLenum a)
{
    if (a == GL_DEPTH_ATTACHMENT || a == GL_STENCIL_ATTACHMENT || a == GL_DEPTH_STENCIL_ATTACHMENT)
        return 1;
    if (a >= GL_COLOR_ATTACHMENT0 && a < GL_COLOR_ATTACHMENT0 + 32u &&
        (GLint)(a - GL_COLOR_ATTACHMENT0) < max_color_attachments())
        return 1;
    gl31_set_error(GL_INVALID_ENUM);
    return 0;
}

/* validacion comun de los glFramebuffer{Texture,Renderbuffer}* */
static int attach_prologue(GLenum target, GLenum attachment)
{
    if (!is_fbo_target(target)) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    if (!attachment_ok(attachment)) return 0;
    if (!bound_fbo(target)) { gl31_set_error(GL_INVALID_OPERATION); return 0; }  /* el por defecto no admite attach */
    return 1;
}

/* ---------- framebuffers ---------- */
void gl31_glGenFramebuffers(GLsizei n, GLuint* ids)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenFramebuffers)(n, ids);
}

static void rm_drop(GLuint fbo);

void gl31_glDeleteFramebuffers(GLsizei n, const GLuint* ids)
{
    gl31_state_t* s = gl31_state();
    GLsizei i;
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        if (!ids[i]) continue;
        if (s->read_fbo == ids[i]) s->read_fbo = 0;
        if (s->draw_fbo == ids[i]) { s->draw_fbo = 0; BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0); }
        rm_drop(ids[i]);
    }
    BE(glDeleteFramebuffers)(n, ids);
}

void gl31_glBindFramebuffer(GLenum target, GLuint fbo)
{
    gl31_state_t* s = gl31_state();
    if (!is_fbo_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (target != GL_READ_FRAMEBUFFER) s->draw_fbo = fbo;
    if (target != GL_DRAW_FRAMEBUFFER) s->read_fbo = fbo;
    if (target == GL_FRAMEBUFFER) {
        BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, fbo);
        BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, gl31_fbo_be_draw(fbo));
    } else if (target == GL_DRAW_FRAMEBUFFER) BE(glBindFramebuffer)(target, gl31_fbo_be_draw(fbo));
    else BE(glBindFramebuffer)(target, fbo);
    gl31_render_resync_indexed();
}

GLboolean gl31_glIsFramebuffer(GLuint fbo) { return fbo ? BE(glIsFramebuffer)(fbo) : GL_FALSE; }

GLenum gl31_glCheckFramebufferStatus(GLenum target)
{
    if (!is_fbo_target(target)) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    {   int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_), st_ = BE(glCheckFramebufferStatus)(rt_);
        gl31_fbo_redir_end(rd_, 0);
        return st_;
    }
}

void gl31_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    GLenum be;
    if (!attach_prologue(target, attachment)) return;
    if (textarget != GL_TEXTURE_2D && textarget != GL_TEXTURE_RECTANGLE &&
        !(textarget >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && textarget <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (textarget == GL_TEXTURE_RECTANGLE && level != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    be = gl31_tex_be_target(textarget);
    if (!be) { gl31_set_error(GL_INVALID_ENUM); return; }
    { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferTexture2D)(rt_, attachment, be, texture, level); gl31_fbo_redir_end(rd_, 1); }
}

/* 1D = 2D de alto 1 en el backend */
void gl31_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    if (!attach_prologue(target, attachment)) return;
    if (textarget != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferTexture2D)(rt_, attachment, GL_TEXTURE_2D, texture, level); gl31_fbo_redir_end(rd_, 1); }
}

void gl31_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                 GLint level, GLint zoffset)
{
    if (!attach_prologue(target, attachment)) return;
    if (textarget != GL_TEXTURE_3D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (zoffset < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferTextureLayer)(rt_, attachment, texture, level, zoffset); gl31_fbo_redir_end(rd_, 1); }
}

void gl31_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    if (!attach_prologue(target, attachment)) return;
    if (layer < 0 || level < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferTextureLayer)(rt_, attachment, texture, level, layer); gl31_fbo_redir_end(rd_, 1); }
}

void gl31_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb)
{
    if (!attach_prologue(target, attachment)) return;
    if (rbtarget != GL_RENDERBUFFER) { gl31_set_error(GL_INVALID_ENUM); return; }
    { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferRenderbuffer)(rt_, attachment, rbtarget, rb); gl31_fbo_redir_end(rd_, 1); }
}

void gl31_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
    if (!params) return;
    if (!is_fbo_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!bound_fbo(target)) {                       /* framebuffer por defecto */
        switch (attachment) {
            case GL_BACK_LEFT: case GL_BACK: attachment = GL_BACK; break;
            case GL_DEPTH: case GL_STENCIL: break;
            case GL_FRONT_LEFT: case GL_FRONT:
                /* ES solo tiene el buffer trasero: el delantero comparte formato y tamanos */
                attachment = GL_BACK;
                break;
            default: gl31_set_error(GL_INVALID_ENUM); return;
        }
    }
    {   int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_);
        BE(glGetFramebufferAttachmentParameteriv)(rt_, attachment, pname, params);
        gl31_fbo_redir_end(rd_, 0);
    }
}

/* ---------- renderbuffers ---------- */
void gl31_glGenRenderbuffers(GLsizei n, GLuint* ids)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenRenderbuffers)(n, ids);
}

void gl31_glDeleteRenderbuffers(GLsizei n, const GLuint* ids)
{
    gl31_state_t* s = gl31_state();
    GLsizei i;
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) if (ids[i] && s->renderbuffer == ids[i]) s->renderbuffer = 0;
    BE(glDeleteRenderbuffers)(n, ids);
}

void gl31_glBindRenderbuffer(GLenum target, GLuint rb)
{
    if (target != GL_RENDERBUFFER) { gl31_set_error(GL_INVALID_ENUM); return; }
    gl31_state()->renderbuffer = rb;
    BE(glBindRenderbuffer)(target, rb);
}

GLboolean gl31_glIsRenderbuffer(GLuint rb) { return rb ? BE(glIsRenderbuffer)(rb) : GL_FALSE; }

/* desktop admite formatos sin tamano / de otros anchos; ES exige formatos con tamano */
static GLenum rb_format(GLenum f)
{
    switch (f) {
        case GL_DEPTH_COMPONENT:   return GL_DEPTH_COMPONENT24;
        case 0x81A7:               return GL_DEPTH_COMPONENT32F;   /* GL_DEPTH_COMPONENT32 */
        case 0x84F9:               return GL_DEPTH24_STENCIL8;     /* GL_DEPTH_STENCIL */
        case 0x1901:               /* GL_STENCIL_INDEX */
        case 0x8D46: case 0x8D47: case 0x8D49:  /* STENCIL_INDEX1/4/16 */
                                   return GL_STENCIL_INDEX8;
        case GL_RGBA:              return GL_RGBA8;
        case GL_RGB:               return GL_RGB8;
        case GL_RG:                return GL_RG8;
        case GL_RED:               return GL_R8;
    }
    return f;
}

static int rb_check(GLenum target, GLsizei w, GLsizei h)
{
    GLint m = 4096;
    if (target != GL_RENDERBUFFER) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    if (!gl31_state()->renderbuffer) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    BE(glGetIntegerv)(GL_MAX_RENDERBUFFER_SIZE, &m);
    if (w < 0 || h < 0 || w > m || h > m) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

void gl31_glRenderbufferStorage(GLenum target, GLenum ifmt, GLsizei w, GLsizei h)
{
    if (!rb_check(target, w, h)) return;
    BE(glRenderbufferStorage)(target, rb_format(ifmt), w, h);
}

void gl31_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h)
{
    if (!rb_check(target, w, h)) return;
    if (samples < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glRenderbufferStorageMultisample)(target, samples, rb_format(ifmt), w, h);
}

void gl31_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    if (!params) return;
    if (target != GL_RENDERBUFFER) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!gl31_state()->renderbuffer) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glGetRenderbufferParameteriv)(target, pname, params);
}

/* ---------- blit ---------- */
void gl31_glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0,
                            GLint dx1, GLint dy1, GLbitfield mask, GLenum filter)
{
    if (mask & ~(GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (filter != GL_NEAREST && filter != GL_LINEAR) { gl31_set_error(GL_INVALID_ENUM); return; }
    if ((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) && filter != GL_NEAREST) {
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    BE(glBlitFramebuffer)(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
}

/* ---------- mapeo arbitrario de draw buffers (FBO proxy) ----------
 * Desktop permite glDrawBuffers(n, {ATTk, ...}): la salida j del shader va al attachment k. ES solo admite
 * salida j -> attachment j (o NONE). Cuando el mapeo no es la identidad se crea en el backend un FBO proxy
 * cuyo attachment j es la misma imagen que el attachment k del FBO de la app, mas su depth/stencil. El
 * backend dibuja en el proxy; todo lo que modifica o consulta los attachments del FBO de la app se
 * redirige al FBO real (gl31_fbo_redir), y el proxy se reconstruye despues de cada cambio. */
typedef struct { GLuint fbo, proxy; GLenum map[GL31_MAX_DRAW_BUFFERS]; int n; } remap_t;
static remap_t* g_rm; static size_t g_nrm, g_crm;

static remap_t* rm_find(GLuint fbo)
{
    size_t i;
    if (!fbo) return NULL;
    for (i = 0; i < g_nrm; i++) if (g_rm[i].fbo == fbo) return &g_rm[i];
    return NULL;
}

static void rm_drop(GLuint fbo)
{
    size_t i;
    for (i = 0; i < g_nrm; i++) if (g_rm[i].fbo == fbo) {
        if (g_rm[i].proxy) BE(glDeleteFramebuffers)(1, &g_rm[i].proxy);
        g_rm[i] = g_rm[--g_nrm];
        return;
    }
}

GLuint gl31_fbo_be_draw(GLuint app_fbo)
{
    remap_t* r = rm_find(app_fbo);
    return (r && r->proxy) ? r->proxy : app_fbo;
}

/* copia al attachment `dst` del FBO enlazado en `dt` la imagen que `src` tiene en el FBO enlazado en READ */
static GLint att_query(GLenum target, GLenum att, GLenum pname);

static void rm_copy_attachment(GLenum dt, GLenum dst, GLenum src)
{
    GLint ty = 0, name = 0, level = 0, layer = 0, face = 0;
    ty = att_query(GL_READ_FRAMEBUFFER, src, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
    if (ty == 0) { BE(glFramebufferRenderbuffer)(dt, dst, GL_RENDERBUFFER, 0); return; }
    name = att_query(GL_READ_FRAMEBUFFER, src, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
    if (ty == (GLint)GL_RENDERBUFFER) { BE(glFramebufferRenderbuffer)(dt, dst, GL_RENDERBUFFER, (GLuint)name); return; }
    level = att_query(GL_READ_FRAMEBUFFER, src, 0x8CD2);       /* TEXTURE_LEVEL */
    layer = att_query(GL_READ_FRAMEBUFFER, src, 0x8CD4);       /* TEXTURE_LAYER */
    face  = att_query(GL_READ_FRAMEBUFFER, src, 0x8CD3);       /* CUBE_MAP_FACE */
    {
        GLenum be = gl31_tex_be_target_of((GLuint)name);
        if (be == GL_TEXTURE_CUBE_MAP) BE(glFramebufferTexture2D)(dt, dst, face ? (GLenum)face : GL_TEXTURE_CUBE_MAP_POSITIVE_X, (GLuint)name, level);
        else if (be == GL_TEXTURE_3D || be == GL_TEXTURE_2D_ARRAY) BE(glFramebufferTextureLayer)(dt, dst, (GLuint)name, level, layer);
        else BE(glFramebufferTexture2D)(dt, dst, GL_TEXTURE_2D, (GLuint)name, level);
    }
}

static void rm_build(remap_t* r)
{
    gl31_state_t* s = gl31_state();
    GLenum db[GL31_MAX_DRAW_BUFFERS];
    int j;
    if (!r->proxy) BE(glGenFramebuffers)(1, &r->proxy);
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, r->fbo);
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, r->proxy);
    for (j = 0; j < r->n; j++) {
        if (r->map[j] == GL_NONE) { BE(glFramebufferRenderbuffer)(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + j, GL_RENDERBUFFER, 0); db[j] = GL_NONE; }
        else { rm_copy_attachment(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + j, r->map[j]); db[j] = GL_COLOR_ATTACHMENT0 + j; }
    }
    rm_copy_attachment(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_DEPTH_ATTACHMENT);
    rm_copy_attachment(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_STENCIL_ATTACHMENT);
    BE(glDrawBuffers)(r->n, db);
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, s->read_fbo);
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, gl31_fbo_be_draw(s->draw_fbo));
}

/* redirige una operacion sobre target DRAW/FRAMEBUFFER al FBO real de la app cuando hay proxy */
GLenum gl31_fbo_redir(GLenum target, int* redirected)
{
    gl31_state_t* s = gl31_state();
    remap_t* r = (target == GL_READ_FRAMEBUFFER) ? NULL : rm_find(s->draw_fbo);
    *redirected = 0;
    if (!r) return target;
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, r->fbo);
    *redirected = 1;
    return GL_READ_FRAMEBUFFER;
}

void gl31_fbo_redir_end(int redirected, int edited)
{
    gl31_state_t* s = gl31_state();
    remap_t* r;
    if (!redirected) return;
    r = rm_find(s->draw_fbo);
    BE(glBindFramebuffer)(GL_READ_FRAMEBUFFER, s->read_fbo);
    if (edited && r) rm_build(r);
}

/* GL_DRAW_BUFFERi segun la app (el backend ve el proxy) */
int gl31_fbo_draw_buffer_query(GLenum pname, GLint* out)
{
    gl31_state_t* s = gl31_state();
    remap_t* r;
    if (pname < GL_DRAW_BUFFER0 || pname >= GL_DRAW_BUFFER0 + GL31_MAX_DRAW_BUFFERS) return 0;
    r = rm_find(s->draw_fbo);
    if (!r) return 0;
    *out = (GLint)((int)(pname - GL_DRAW_BUFFER0) < r->n ? r->map[pname - GL_DRAW_BUFFER0] : GL_NONE);
    return 1;
}

/* aplica el mapeo n/bufs al FBO de dibujo de la app (no por defecto) */
static void rm_apply(GLsizei n, const GLenum* bufs)
{
    gl31_state_t* s = gl31_state();
    GLsizei j;
    int identity = 1;
    for (j = 0; j < n; j++)
        if (bufs[j] != GL_NONE && bufs[j] != GL_COLOR_ATTACHMENT0 + (GLenum)j) identity = 0;
    if (identity) {
        rm_drop(s->draw_fbo);
        BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, s->draw_fbo);
        BE(glDrawBuffers)(n, bufs);
        return;
    }
    {
        remap_t* r = rm_find(s->draw_fbo);
        if (!r) {
            if (g_nrm == g_crm) {
                size_t c = g_crm ? g_crm * 2 : 8;
                remap_t* p = (remap_t*)realloc(g_rm, c * sizeof *p);
                if (!p) { gl31_set_error(GL_OUT_OF_MEMORY); return; }
                g_rm = p; g_crm = c;
            }
            r = &g_rm[g_nrm++];
            memset(r, 0, sizeof *r);
            r->fbo = s->draw_fbo;
        }
        r->n = (int)n;
        for (j = 0; j < n; j++) r->map[j] = bufs[j];
        rm_build(r);
    }
}

void gl31_fbo_shutdown_remap(void) { free(g_rm); g_rm = NULL; g_nrm = g_crm = 0; }

/* ---------- draw/read buffers ---------- */
/* buffer del framebuffer por defecto: devuelve el equivalente ES, o 0 si no existe */
static GLenum default_fb_buffer(GLenum m, const char* who)
{
    switch (m) {
        case GL_NONE:
        case GL_BACK:
            return m;
        case GL_BACK_LEFT:
            return GL_BACK;
        case GL_FRONT: case GL_FRONT_LEFT: case GL_FRONT_AND_BACK:
            gl31_stub_warn(who);
            return 0;
    }
    return 0;
}

void gl31_glDrawBuffer(GLenum mode)
{
    gl31_state_t* s = gl31_state();
    if (!s->draw_fbo) {
        GLenum m = default_fb_buffer(mode, "glDrawBuffer(GL_FRONT*) sobre el framebuffer por defecto");
        if (!m) {
            /* FRONT/FRONT_AND_BACK: valido en desktop pero ES solo tiene BACK -> error visible */
            gl31_set_error((mode == GL_FRONT || mode == GL_FRONT_LEFT || mode == GL_FRONT_AND_BACK)
                           ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
            return;
        }
        BE(glDrawBuffers)(1, &m);
        return;
    }
    if (mode == GL_NONE) { rm_apply(1, &mode); gl31_render_resync_indexed(); return; }
    if (mode >= GL_COLOR_ATTACHMENT0 && (GLint)(mode - GL_COLOR_ATTACHMENT0) < max_color_attachments()) {
        /* desktop: la salida 0 del shader va al attachment i; ES solo admite salida i -> attachment i,
         * asi que para i>0 se dibuja en un FBO proxy (ver rm_apply) */
        rm_apply(1, &mode);
        gl31_render_resync_indexed();
        return;
    }
    gl31_set_error(mode == GL_BACK || mode == GL_FRONT || mode == GL_FRONT_AND_BACK ||
                   mode == GL_BACK_LEFT || mode == GL_FRONT_LEFT
                   ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
}

void gl31_glDrawBuffers(GLsizei n, const GLenum* bufs)
{
    gl31_state_t* s = gl31_state();
    GLint maxdb = 4;
    if (n < 0 || (n > 0 && !bufs)) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &maxdb);
    if (n > maxdb) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!s->draw_fbo) {
        GLenum m;
        if (n != 1) { gl31_set_error(GL_INVALID_OPERATION); return; }
        m = default_fb_buffer(bufs[0], "glDrawBuffers(GL_FRONT*) sobre el framebuffer por defecto");
        if (!m) { gl31_set_error(GL_INVALID_OPERATION); return; }
        BE(glDrawBuffers)(1, &m);
        gl31_render_resync_indexed();
        return;
    }
    /* mapeo identidad (o NONE) -> directo; cualquier otro -> FBO proxy */
    {
        GLsizei k;
        for (k = 0; k < n; k++)
            if (bufs[k] != GL_NONE && !(bufs[k] >= GL_COLOR_ATTACHMENT0 && (GLint)(bufs[k] - GL_COLOR_ATTACHMENT0) < max_color_attachments())) {
                gl31_set_error(bufs[k] == GL_BACK || bufs[k] == GL_FRONT || bufs[k] == GL_FRONT_AND_BACK ||
                               bufs[k] == GL_BACK_LEFT || bufs[k] == GL_FRONT_LEFT ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
                return;
            }
    }
    rm_apply(n, bufs);
    gl31_render_resync_indexed();
}

void gl31_glReadBuffer(GLenum mode)
{
    gl31_state_t* s = gl31_state();
    if (!s->read_fbo) {
        GLenum m = default_fb_buffer(mode, "glReadBuffer(GL_FRONT*) sobre el framebuffer por defecto");
        if (!m) {
            gl31_set_error((mode == GL_FRONT || mode == GL_FRONT_LEFT || mode == GL_FRONT_AND_BACK)
                           ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
            return;
        }
        BE(glReadBuffer)(m);
        return;
    }
    BE(glReadBuffer)(mode);
}

/* ---------- clear por buffer ---------- */
void gl31_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    if (buffer != GL_COLOR && buffer != GL_STENCIL) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!value || drawbuffer < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_cond_skip()) return;
    BE(glClearBufferiv)(buffer, drawbuffer, value);
}

void gl31_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value)
{
    if (buffer != GL_COLOR) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!value || drawbuffer < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_cond_skip()) return;
    BE(glClearBufferuiv)(buffer, drawbuffer, value);
}

void gl31_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    if (buffer != GL_COLOR && buffer != GL_DEPTH) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!value || drawbuffer < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_cond_skip()) return;
    BE(glClearBufferfv)(buffer, drawbuffer, value);
}

void gl31_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    if (buffer != GL_DEPTH_STENCIL) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (drawbuffer != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_cond_skip()) return;
    BE(glClearBufferfi)(buffer, drawbuffer, depth, stencil);
}

/* ==================== lectura de pixeles ==================== */
static int format_ncomp(GLenum f)
{
    switch (f) {
        case GL_RED: return 1;
        case GL_RG:  return 2;
        case GL_RGB: case GL_BGR: return 3;
        case GL_RGBA: case GL_BGRA: return 4;
    }
    return 0;
}

/* bytes por pixel, o 0 si el par formato/tipo no se convierte aqui */
static int dst_pixel_size(GLenum format, GLenum type)
{
    int n = format_ncomp(format);
    switch (type) {
        case GL_UNSIGNED_BYTE: case GL_BYTE:           return n;
        case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: return 2 * n;
        case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT:          return 4 * n;
        case GL_UNSIGNED_SHORT_5_6_5:
            return (format == GL_RGB || format == GL_BGR) ? 2 : 0;
        case GL_UNSIGNED_INT_8_8_8_8: case GL_UNSIGNED_INT_8_8_8_8_REV:
            return (format == GL_RGBA || format == GL_BGRA) ? 4 : 0;
    }
    return 0;
}

static uint16_t f2h(float f)
{
    union { float f; uint32_t u; } v;
    uint32_t sign, e, m;
    int32_t exp;
    v.f = f;
    sign = (v.u >> 16) & 0x8000u;
    e = (v.u >> 23) & 0xFFu;
    m = v.u & 0x7FFFFFu;
    if (e == 0xFFu) return (uint16_t)(sign | 0x7C00u | (m ? 0x200u : 0));   /* inf / nan */
    exp = (int32_t)e - 127 + 15;
    if (exp >= 31) return (uint16_t)(sign | 0x7C00u);
    if (exp <= 0) {
        if (exp < -10) return (uint16_t)sign;
        m = (m | 0x800000u) >> (1 - exp);
        return (uint16_t)(sign | ((m + 0x1000u) >> 13));
    }
    return (uint16_t)(sign | ((uint32_t)exp << 10) | ((m + 0x1000u) >> 13));
}

/* canal normalizado 0..1 -> entero de `max` */
static uint32_t nrm(float v, uint32_t max)
{
    if (v <= 0.f) return 0;
    if (v >= 1.f) return max;
    return (uint32_t)(v * (float)max + 0.5f);
}

/* tamano de cada elemento que se intercambia con GL_PACK_SWAP_BYTES (1 = no aplica) */
static int dst_elem_size(GLenum type)
{
    switch (type) {
        case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT:
        case GL_UNSIGNED_SHORT_5_6_5: case GL_UNSIGNED_SHORT_4_4_4_4: case GL_UNSIGNED_SHORT_5_5_5_1:
            return 2;
        case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT:
        case GL_UNSIGNED_INT_8_8_8_8: case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_24_8: case GL_UNSIGNED_INT_10F_11F_11F_REV: case GL_UNSIGNED_INT_5_9_9_9_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return 4;
    }
    return 1;
}

static void swap_elems(unsigned char* p, size_t n, int es)
{
    size_t i; int k;
    for (i = 0; i < n; i++, p += es)
        for (k = 0; k < es / 2; k++) { unsigned char t = p[k]; p[k] = p[es - 1 - k]; p[es - 1 - k] = t; }
}

/* escribe un pixel (c[0..3] = r,g,b,a normalizados) en el formato/tipo de destino */
static void put_pixel(unsigned char* d, GLenum format, GLenum type, const float c[4])
{
    static const int idx_rgba[4] = {0, 1, 2, 3};
    static const int idx_bgra[4] = {2, 1, 0, 3};
    const int* ord = (format == GL_BGR || format == GL_BGRA) ? idx_bgra : idx_rgba;
    int n = format_ncomp(format), k;

    switch (type) {
        case GL_UNSIGNED_SHORT_5_6_5: {
            uint16_t v = (uint16_t)((nrm(c[ord[0]], 31) << 11) | (nrm(c[ord[1]], 63) << 5) | nrm(c[ord[2]], 31));
            memcpy(d, &v, 2);
            return;
        }
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV: {
            /* los componentes se ordenan segun format; _REV invierte el orden dentro de la palabra */
            uint32_t a = nrm(c[ord[0]], 255), b = nrm(c[ord[1]], 255);
            uint32_t g = nrm(c[ord[2]], 255), r = nrm(c[ord[3]], 255);
            uint32_t v = (type == GL_UNSIGNED_INT_8_8_8_8)
                       ? ((a << 24) | (b << 16) | (g << 8) | r)
                       : ((r << 24) | (g << 16) | (b << 8) | a);
            memcpy(d, &v, 4);
            return;
        }
    }
    for (k = 0; k < n; k++) {
        float v = c[ord[k]];
        switch (type) {
            case GL_UNSIGNED_BYTE:  d[k] = (unsigned char)nrm(v, 255); break;
            case GL_BYTE:           ((signed char*)d)[k] = (signed char)nrm(v, 127); break;
            case GL_UNSIGNED_SHORT: { uint16_t x = (uint16_t)nrm(v, 65535); memcpy(d + 2 * k, &x, 2); break; }
            case GL_SHORT:          { int16_t x = (int16_t)nrm(v, 32767);   memcpy(d + 2 * k, &x, 2); break; }
            case GL_HALF_FLOAT:     { uint16_t x = f2h(v);                  memcpy(d + 2 * k, &x, 2); break; }
            case GL_UNSIGNED_INT:   { uint32_t x = (uint32_t)((double)(v < 0 ? 0 : v > 1 ? 1 : v) * 4294967295.0 + 0.5);
                                      memcpy(d + 4 * k, &x, 4); break; }
            case GL_INT:            { int32_t x = (int32_t)((double)(v < 0 ? 0 : v > 1 ? 1 : v) * 2147483647.0 + 0.5);
                                      memcpy(d + 4 * k, &x, 4); break; }
            case GL_FLOAT:          memcpy(d + 4 * k, &v, 4); break;
        }
    }
}

/* 1 = (format,type) se puede pedir tal cual al backend */
static int native_pair(GLenum format, GLenum type)
{
    GLint f = 0, t = 0;
    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) return 1;
    if (format == GL_RGBA_INTEGER && (type == GL_INT || type == GL_UNSIGNED_INT)) return 1;
    BE(glGetIntegerv)(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &f);
    BE(glGetIntegerv)(GL_IMPLEMENTATION_COLOR_READ_TYPE, &t);
    return (GLenum)f == format && (GLenum)t == type;
}

static void set_pack_state(GLint align, GLint row, GLint skr, GLint skp)
{
    BE(glPixelStorei)(GL_PACK_ALIGNMENT, align);
    BE(glPixelStorei)(GL_PACK_ROW_LENGTH, row);
    BE(glPixelStorei)(GL_PACK_SKIP_ROWS, skr);
    BE(glPixelStorei)(GL_PACK_SKIP_PIXELS, skp);
}


/* ==================================================================
 * glReadPixels(DEPTH_COMPONENT / STENCIL_INDEX / DEPTH_STENCIL)
 * ES no puede leer depth/stencil. Se copia (glBlitFramebuffer, GPU) la region a texturas temporales
 * de profundidad/stencil, y un kernel de compute las muestrea y empaqueta bytes en el formato/tipo
 * de desktop (con GL_PACK_SWAP_BYTES). El resultado va a un buffer de GPU y de ahi al PBO (copia GPU)
 * o a la memoria de la app (una copia por fila segun GL_PACK_*). Sin conversion pixel a pixel en CPU.
 * ================================================================== */
#ifndef GL_DEPTH_STENCIL_TEXTURE_MODE
#define GL_DEPTH_STENCIL_TEXTURE_MODE 0x90EA
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE 0x8217
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE 0x8211
#endif

static const char k_ds_src[] =
"#version 310 es\n"
"precision highp float; precision highp int; precision highp sampler2D; precision highp usampler2D;\n"
"layout(local_size_x = 64) in;\n"
"layout(std430, binding = 0) writeonly buffer DstB { uint dst[]; };\n"
"uniform sampler2D u_depth;\n"
"uniform usampler2D u_stencil;\n"
"uniform uint u_mode;    /* 0 depth, 1 stencil, 2 depth+stencil 24_8, 3 depth+stencil float_32_24_8_rev */\n"
"uniform uint u_type;    /* 0 float 1 uint 2 ushort 3 ubyte 4 int 5 short 6 byte */\n"
"uniform uint u_es, u_ne, u_w, u_total, u_swap, u_dbits, u_gx;\n"
"uniform ivec2 u_org;\n"
"uint dnorm(float d, uint bits) {\n"
"  d = clamp(d, 0.0, 1.0);\n"
"  if (bits == 16u) return uint(d * 65535.0 + 0.5);\n"
"  if (bits == 24u) return uint(d * 16777215.0 + 0.5);\n"
"  return uint(d * 4294967040.0);\n"
"}\n"
"uint depthval(float d) {\n"
"  if (u_type == 0u) return floatBitsToUint(d);\n"
"  if (u_type == 3u || u_type == 6u) return uint(clamp(d, 0.0, 1.0) * (u_type == 3u ? 255.0 : 127.0) + 0.5);\n"
"  if (u_type == 2u || u_type == 5u) return uint(clamp(d, 0.0, 1.0) * (u_type == 2u ? 65535.0 : 32767.0) + 0.5);\n"
"  if (u_type == 4u) return uint(clamp(d, 0.0, 1.0) * 2147483520.0);\n"
"  uint k = dnorm(d, u_dbits);                       /* uint: UNORM de 16/24/32 bits -> 32 bits */\n"
"  if (u_dbits == 16u) return k * 65537u;\n"
"  if (u_dbits == 24u) return (k << 8) | (k >> 16);\n"
"  return k;\n"
"}\n"
"uint stenval(uint s) { return u_type == 0u ? floatBitsToUint(float(s)) : s; }\n"
"uint elem(uint pix, uint c) {\n"
"  ivec2 p = u_org + ivec2(int(pix % u_w), int(pix / u_w));\n"
"  if (u_mode == 0u) return depthval(texelFetch(u_depth, p, 0).r);\n"
"  if (u_mode == 1u) return stenval(texelFetch(u_stencil, p, 0).r);\n"
"  if (u_mode == 2u) return (dnorm(texelFetch(u_depth, p, 0).r, 24u) << 8) | (texelFetch(u_stencil, p, 0).r & 0xFFu);\n"
"  if (c == 0u) return floatBitsToUint(texelFetch(u_depth, p, 0).r);\n"
"  return texelFetch(u_stencil, p, 0).r & 0xFFu;\n"
"}\n"
"void main() {\n"
"  uint i = gl_GlobalInvocationID.y * u_gx + gl_GlobalInvocationID.x;\n"
"  if (i * 4u >= u_total) return;\n"
"  uint word = 0u;\n"
"  for (uint b = 0u; b < 4u; b++) {\n"
"    uint byte = i * 4u + b;\n"
"    if (byte >= u_total) break;\n"
"    uint e = byte / u_es, j = byte % u_es;\n"
"    if (u_swap != 0u) j = u_es - 1u - j;\n"
"    uint v = elem(e / u_ne, e % u_ne);\n"
"    word |= ((v >> (8u * j)) & 0xFFu) << (8u * b);\n"
"  }\n"
"  dst[i] = word;\n"
"}\n";

static GLuint g_ds_prog, g_ds_buf;
static size_t g_ds_cap;
static int g_ds_state;
static GLint ds_u_mode, ds_u_type, ds_u_es, ds_u_ne, ds_u_w, ds_u_total, ds_u_swap, ds_u_dbits, ds_u_gx, ds_u_org,
             ds_u_depth, ds_u_stencil;

static int ds_ensure_prog(void)
{
    GLuint sh;
    GLint ok = 0;
    const char* src = k_ds_src;
    if (g_ds_state) return g_ds_state == 1;
    g_ds_state = -1;
    if (!gl31_caps.es31) return 0;
    sh = BE(glCreateShader)(GL_COMPUTE_SHADER);
    BE(glShaderSource)(sh, 1, &src, NULL);
    BE(glCompileShader)(sh);
    BE(glGetShaderiv)(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = "";
        BE(glGetShaderInfoLog)(sh, sizeof log, NULL, log);
        fprintf(stderr, "LORICA: kernel de lectura depth/stencil no compila: %s\n", log);
        BE(glDeleteShader)(sh);
        return 0;
    }
    g_ds_prog = BE(glCreateProgram)();
    BE(glAttachShader)(g_ds_prog, sh);
    BE(glLinkProgram)(g_ds_prog);
    BE(glDeleteShader)(sh);
    BE(glGetProgramiv)(g_ds_prog, GL_LINK_STATUS, &ok);
    if (!ok) { BE(glDeleteProgram)(g_ds_prog); g_ds_prog = 0; return 0; }
#define UL(n) ds_u_##n = BE(glGetUniformLocation)(g_ds_prog, "u_" #n)
    UL(mode); UL(type); UL(es); UL(ne); UL(w); UL(total); UL(swap); UL(dbits); UL(gx); UL(org); UL(depth); UL(stencil);
#undef UL
    g_ds_state = 1;
    return 1;
}

void gl31_pxread_shutdown(void) { g_ds_prog = 0; g_ds_buf = 0; g_ds_cap = 0; g_ds_state = 0; }

static GLuint ds_make_tex(GLenum ifmt, GLsizei w, GLsizei h)
{
    GLuint t = 0;
    BE(glGenTextures)(1, &t);
    BE(glBindTexture)(GL_TEXTURE_2D, t);
    BE(glTexStorage2D)(GL_TEXTURE_2D, 1, ifmt, w, h);
    BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    BE(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return t;
}

/* 1 = hecho (o error ya registrado) */
/* Entrega el resultado empaquetado (g_ds_buf, w*h*psize bytes sin relleno) con la geometria de GL_PACK_*:
 * a un PBO con copias GPU->GPU (una por fila si hay relleno) o a la memoria de la app con una copia por fila. */
static int px_deliver(GLsizei w, GLsizei h, size_t psize, size_t total, void* pixels)
{
    gl31_state_t* s = gl31_state();
    GLint rl = s->pack_row_length > 0 ? s->pack_row_length : w;
    GLint al = s->pack_alignment > 0 ? s->pack_alignment : 4;
    size_t rowb = (size_t)rl * psize;
    size_t stride = ((rowb + (size_t)al - 1) / (size_t)al) * (size_t)al;
    size_t tight = (size_t)w * psize;
    size_t base = (size_t)s->pack_skip_rows * stride + (size_t)s->pack_skip_pixels * psize;
    GLint j;
    GLuint prev_cr = s->copy_read_buffer;
    if (s->pixel_pack_buffer) {
        size_t off = (size_t)(intptr_t)pixels;
        BE(glBindBuffer)(GL_COPY_READ_BUFFER, g_ds_buf);
        if (stride == tight && base == 0)
            BE(glCopyBufferSubData)(GL_COPY_READ_BUFFER, GL_PIXEL_PACK_BUFFER, 0, (GLintptr)off, (GLsizeiptr)total);
        else
            for (j = 0; j < h; j++)
                BE(glCopyBufferSubData)(GL_COPY_READ_BUFFER, GL_PIXEL_PACK_BUFFER, (GLintptr)((size_t)j * tight),
                                        (GLintptr)(off + base + (size_t)j * stride), (GLsizeiptr)tight);
        BE(glBindBuffer)(GL_COPY_READ_BUFFER, prev_cr);
    } else {
        const unsigned char* m;
        unsigned char* dst = (unsigned char*)pixels;
        if (!dst) { gl31_set_error(GL_INVALID_VALUE); return 0; }
        BE(glBindBuffer)(GL_COPY_READ_BUFFER, g_ds_buf);
        m = (const unsigned char*)BE(glMapBufferRange)(GL_COPY_READ_BUFFER, 0, (GLsizeiptr)total, GL_MAP_READ_BIT);
        if (!m) { BE(glBindBuffer)(GL_COPY_READ_BUFFER, prev_cr); gl31_set_error(GL_OUT_OF_MEMORY); return 0; }
        for (j = 0; j < h; j++) memcpy(dst + base + (size_t)j * stride, m + (size_t)j * tight, tight);
        BE(glUnmapBuffer)(GL_COPY_READ_BUFFER);
        BE(glBindBuffer)(GL_COPY_READ_BUFFER, prev_cr);
    }
    return 1;
}

/* consulta de un attachment que no deja errores pendientes en el backend (0 si no hay objeto) */
static GLint att_query(GLenum target, GLenum att, GLenum pname)
{
    GLint ty = 0, v = 0;
    BE(glGetFramebufferAttachmentParameteriv)(target, att, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &ty);
    if (ty == 0 /* GL_NONE */) return 0;
    BE(glGetFramebufferAttachmentParameteriv)(target, att, pname, &v);
    return v;
}

static int read_depth_stencil(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels)
{
    gl31_state_t* s = gl31_state();
    GLenum rd = GL_READ_FRAMEBUFFER;
    GLint rfb_ = 0;
    GLenum a_d, a_s;
    BE(glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING, &rfb_);          /* el FBO real del backend (puede ser uno interno) */
    a_d = rfb_ ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
    a_s = rfb_ ? GL_STENCIL_ATTACHMENT : GL_STENCIL;
    GLint dbits = 0, sbits = 0, dtype = 0, tmp_ok = 1;
    int want_d = format != GL_STENCIL_INDEX, want_s = format != GL_DEPTH_COMPONENT;
    unsigned mode, tcode, es, ne;
    GLenum ifmt, att;
    GLuint tex_d = 0, tex_s = 0, fbo = 0;
    size_t psize, total, words;
    GLint sv_u0 = 0, sv_u1 = 0, sv_act = 0, sv_smp0 = 0, sv_smp1 = 0, sv_ssbo = 0, sv_id = 0;
    GLint64 sv_start = 0, sv_size = 0;
    GLboolean scissor;
    GLuint groups, gx, gy;
    int ok = 1;

    /* tipo valido para el formato */
    if (format == GL_DEPTH_STENCIL) {
        if (type != GL_UNSIGNED_INT_24_8 && type != GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
            gl31_set_error(GL_INVALID_OPERATION); return 0;
        }
    } else {
        switch (type) {
            case GL_FLOAT: case GL_UNSIGNED_INT: case GL_INT: case GL_UNSIGNED_SHORT: case GL_SHORT:
            case GL_UNSIGNED_BYTE: case GL_BYTE: break;
            default: gl31_set_error(GL_INVALID_ENUM); return 0;
        }
    }
    if (!ds_ensure_prog()) { gl31_set_error(GL_INVALID_OPERATION); return 0; }

    dbits = att_query(rd, a_d, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE);
    sbits = att_query(rd, a_s, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE);
    if ((want_d && dbits <= 0) || (want_s && sbits <= 0)) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    if (dbits > 0) dtype = att_query(rd, a_d, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE);

    /* formato de la copia: el mismo que el origen (glBlitFramebuffer exige coincidencia) */
    if (dbits > 0 && sbits > 0) ifmt = dtype == (GLint)GL_FLOAT ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8;
    else if (dbits > 0)         ifmt = dtype == (GLint)GL_FLOAT ? GL_DEPTH_COMPONENT32F
                                     : dbits <= 16 ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24;
    else                        ifmt = GL_DEPTH24_STENCIL8;
    att = (ifmt == GL_DEPTH_COMPONENT16 || ifmt == GL_DEPTH_COMPONENT24 || ifmt == GL_DEPTH_COMPONENT32F)
        ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT;

    /* geometria del paquete */
    if (format == GL_DEPTH_STENCIL) {
        mode = type == GL_UNSIGNED_INT_24_8 ? 2u : 3u;
        tcode = 0; es = 4; ne = type == GL_UNSIGNED_INT_24_8 ? 1u : 2u;
    } else {
        mode = format == GL_DEPTH_COMPONENT ? 0u : 1u;
        switch (type) {
            case GL_FLOAT: tcode = 0; es = 4; break;   case GL_UNSIGNED_INT: tcode = 1; es = 4; break;
            case GL_UNSIGNED_SHORT: tcode = 2; es = 2; break; case GL_UNSIGNED_BYTE: tcode = 3; es = 1; break;
            case GL_INT: tcode = 4; es = 4; break;     case GL_SHORT: tcode = 5; es = 2; break;
            default: tcode = 6; es = 1; break;
        }
        ne = 1;
    }
    psize = (size_t)es * ne;
    total = (size_t)w * (size_t)h * psize;
    words = (total + 3) / 4;
    if (total > (size_t)256 * 1024 * 1024) { gl31_set_error(GL_OUT_OF_MEMORY); return 0; }

    /* ---- estado a restaurar ---- */
    BE(glGetIntegerv)(GL_ACTIVE_TEXTURE, &sv_act);
    BE(glActiveTexture)(GL_TEXTURE0);
    BE(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &sv_u0);
    BE(glGetIntegerv)(GL_SAMPLER_BINDING, &sv_smp0);
    BE(glActiveTexture)(GL_TEXTURE1);
    BE(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &sv_u1);
    BE(glGetIntegerv)(GL_SAMPLER_BINDING, &sv_smp1);
    BE(glGetIntegerv)(GL_SHADER_STORAGE_BUFFER_BINDING, &sv_ssbo);
    BE(glGetIntegeri_v)(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &sv_id);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_START, 0, &sv_start);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &sv_size);
    scissor = BE(glIsEnabled)(GL_SCISSOR_TEST);
    if (scissor) BE(glDisable)(GL_SCISSOR_TEST);
    BE(glBindSampler)(0, 0); BE(glBindSampler)(1, 0);

    /* ---- copia GPU de la region a texturas temporales ---- */
    BE(glGenFramebuffers)(1, &fbo);
    BE(glActiveTexture)(GL_TEXTURE0);
    tex_d = ds_make_tex(ifmt, w, h);
    if (want_d && want_s) { BE(glActiveTexture)(GL_TEXTURE1); tex_s = ds_make_tex(ifmt, w, h); }
    else if (want_s)      { tex_s = tex_d; tex_d = 0; BE(glActiveTexture)(GL_TEXTURE1); BE(glBindTexture)(GL_TEXTURE_2D, tex_s); }
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fbo);
    {
        GLuint t[2]; int k, nt = 0;
        GLbitfield mk[2];
        if (want_d && want_s) { t[0] = tex_d; mk[0] = GL_DEPTH_BUFFER_BIT; t[1] = tex_s; mk[1] = GL_STENCIL_BUFFER_BIT; nt = 2; }
        else if (want_d)      { t[0] = tex_d; mk[0] = GL_DEPTH_BUFFER_BIT; nt = 1; }
        else                  { t[0] = tex_s; mk[0] = GL_STENCIL_BUFFER_BIT; nt = 1; }
        for (k = 0; k < nt; k++) {
            BE(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, att, GL_TEXTURE_2D, t[k], 0);
            BE(glBlitFramebuffer)(x, y, x + w, y + h, 0, 0, w, h, mk[k], GL_NEAREST);
        }
    }
    if (BE(glGetError)() != GL_NO_ERROR) tmp_ok = 0;
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, gl31_fbo_be_draw(s->draw_fbo));
    if (scissor) BE(glEnable)(GL_SCISSOR_TEST);
    if (!tmp_ok) { ok = 0; gl31_set_error(GL_INVALID_OPERATION); goto done; }

    if (want_s) {
        BE(glActiveTexture)(GL_TEXTURE1);
        BE(glBindTexture)(GL_TEXTURE_2D, tex_s);
        BE(glTexParameteri)(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
    }
    if (want_d) { BE(glActiveTexture)(GL_TEXTURE0); BE(glBindTexture)(GL_TEXTURE_2D, tex_d); }

    /* ---- kernel ---- */
    if (!g_ds_buf) BE(glGenBuffers)(1, &g_ds_buf);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, g_ds_buf);
    if (g_ds_cap < words * 4) {
        size_t nc = 1024;
        while (nc < words * 4) nc <<= 1;
        BE(glBufferData)(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)nc, NULL, GL_DYNAMIC_COPY);
        g_ds_cap = nc;
    }
    BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 0, g_ds_buf, 0, (GLsizeiptr)g_ds_cap);
    BE(glUseProgram)(g_ds_prog);
    BE(glUniform1i)(ds_u_depth, 0);
    BE(glUniform1i)(ds_u_stencil, 1);
    BE(glUniform1ui)(ds_u_mode, mode);
    BE(glUniform1ui)(ds_u_type, tcode);
    BE(glUniform1ui)(ds_u_es, es);
    BE(glUniform1ui)(ds_u_ne, ne);
    BE(glUniform1ui)(ds_u_w, (GLuint)w);
    BE(glUniform1ui)(ds_u_total, (GLuint)total);
    BE(glUniform1ui)(ds_u_swap, (s->pack_swap_bytes && es > 1) ? 1u : 0u);
    BE(glUniform1ui)(ds_u_dbits, (GLuint)(dtype == (GLint)GL_FLOAT ? 32 : dbits));
    BE(glUniform2i)(ds_u_org, 0, 0);
    groups = (GLuint)((words + 63) / 64);
    gx = groups > 65535u ? 65535u : groups;
    gy = (groups + gx - 1) / gx;
    BE(glUniform1ui)(ds_u_gx, gx * 64u);
    if (gy > 65535u) { ok = 0; gl31_set_error(GL_OUT_OF_MEMORY); goto done; }
    BE(glDispatchCompute)(gx, gy, 1);
    BE(glMemoryBarrier)(0xFFFFFFFFu);

    ok = px_deliver(w, h, psize, total, pixels);

done:
    if (sv_id && sv_size > 0) BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id, (GLintptr)sv_start, (GLsizeiptr)sv_size);
    else BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, (GLuint)sv_ssbo);
    BE(glUseProgram)(s->program);
    BE(glActiveTexture)(GL_TEXTURE0); BE(glBindTexture)(GL_TEXTURE_2D, (GLuint)sv_u0); BE(glBindSampler)(0, (GLuint)sv_smp0);
    BE(glActiveTexture)(GL_TEXTURE1); BE(glBindTexture)(GL_TEXTURE_2D, (GLuint)sv_u1); BE(glBindSampler)(1, (GLuint)sv_smp1);
    BE(glActiveTexture)((GLenum)sv_act);
    if (tex_d) BE(glDeleteTextures)(1, &tex_d);
    if (tex_s) BE(glDeleteTextures)(1, &tex_s);
    if (fbo) BE(glDeleteFramebuffers)(1, &fbo);
    return ok;
}


/* ==================================================================
 * glReadPixels de color en GPU (cualquier formato/tipo de GL 3.x/4.x)
 * Se copia la region (glBlitFramebuffer, resuelve multimuestra y convierte entre formatos de color) a una
 * textura temporal, y un kernel de compute la empaqueta bytes en el formato/tipo pedido: normalizados,
 * enteros con/sin signo, flotantes, half, 5_6_5, 4_4_4_4, 5_5_5_1, 3_3_2, 8_8_8_8, 2_10_10_10_REV,
 * 10F_11F_11F_REV y 5_9_9_9_REV, con GL_PACK_SWAP_BYTES y GL_CLAMP_READ_COLOR. Sin bucle por pixel en CPU.
 * ================================================================== */
static const char k_cr_src[] =
"#version 310 es\n"
"precision highp float; precision highp int;\n"
"precision highp sampler2D; precision highp isampler2D; precision highp usampler2D;\n"
"layout(local_size_x = 64) in;\n"
"layout(std430, binding = 0) writeonly buffer DstB { uint dst[]; };\n"
"uniform sampler2D u_f; uniform isampler2D u_i; uniform usampler2D u_u;\n"
"uniform uint u_dom;      /* 0 float/unorm, 1 int, 2 uint */\n"
"uniform uint u_type;     /* ver codigos en el host */\n"
"uniform uint u_bgr, u_ncomp, u_es, u_ne, u_w, u_total, u_swap, u_gx, u_srgb, u_clamp;\n"
"uint nrm(float x, float m) { return uint(clamp(x, 0.0, 1.0) * m + 0.5); }\n"
"uint f11(float f, uint mb) {                    /* float sin signo a 11/10 bits (truncado) */\n"
"  if (!(f > 0.0)) return 0u;\n"
"  uint u = floatBitsToUint(f); uint e = (u >> 23) & 255u; uint m = u & 0x7FFFFFu;\n"
"  uint emax = (1u << 5) - 1u;\n"
"  if (e == 255u) return (emax << mb) | (m != 0u ? 1u : 0u);\n"
"  int ex = int(e) - 112;\n"
"  if (ex >= int(emax)) return emax << mb;       /* desborda: infinito */\n"
"  if (ex <= 0) { int sh = (mb == 6u ? 18 : 19) - ex; if (sh >= 32) return 0u; return (m | 0x800000u) >> uint(sh); }\n"
"  return (uint(ex) << mb) | (m >> (23u - mb));\n"
"}\n"
"uint rgb9e5(vec3 c) {\n"
"  c = clamp(c, vec3(0.0), vec3(65408.0));\n"
"  float mx = max(c.r, max(c.g, c.b));\n"
"  int ep = max(-16, int(floor(log2(max(mx, 1e-30))))) + 1 + 15;\n"
"  float sc = exp2(float(ep - 24));\n"
"  float ms = floor(mx / sc + 0.5);\n"
"  if (ms == 512.0) { ep++; sc = exp2(float(ep - 24)); }\n"
"  uvec3 s = uvec3(floor(c / sc + 0.5));\n"
"  return s.r | (s.g << 9) | (s.b << 18) | (uint(ep) << 27);\n"
"}\n"
"float enc(float c) { c = clamp(c, 0.0, 1.0); return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }\n"
"vec4 fetchf(ivec2 p) {\n"
"  vec4 v = texelFetch(u_f, p, 0);\n"
"  if (u_srgb != 0u) v.rgb = vec3(enc(v.r), enc(v.g), enc(v.b));\n"
"  if (u_clamp != 0u) v = clamp(v, 0.0, 1.0);\n"
"  return v;\n"
"}\n"
"uint elemf(vec4 v, uint k) {                     /* dominio float, componente k en el orden del formato */\n"
"  float c = v[k];\n"
"  if (u_type == 0u) return nrm(c, 255.0);\n"
"  if (u_type == 1u) return uint(int(round(clamp(c, -1.0, 1.0) * 127.0))) & 0xFFu;\n"
"  if (u_type == 2u) return nrm(c, 65535.0);\n"
"  if (u_type == 3u) return uint(int(round(clamp(c, -1.0, 1.0) * 32767.0))) & 0xFFFFu;\n"
"  if (u_type == 4u) { uint q = nrm(c, 16777215.0); return (q << 8) | (q >> 16); }\n"
"  if (u_type == 5u) return uint(int(clamp(c, 0.0, 1.0) * 2147483520.0));\n"
"  if (u_type == 6u) return floatBitsToUint(c);\n"
"  return packHalf2x16(vec2(c, 0.0)) & 0xFFFFu;   /* 7: half */\n"
"}\n"
"uint packedf(vec4 v) {                           /* v ya en el orden del formato (rgba o bgra) */\n"
"  uint t = u_type;\n"
"  if (t == 8u)  return (nrm(v.x,31.0) << 11) | (nrm(v.y,63.0) << 5) | nrm(v.z,31.0);\n"
"  if (t == 9u)  return nrm(v.x,31.0) | (nrm(v.y,63.0) << 5) | (nrm(v.z,31.0) << 11);\n"
"  if (t == 10u) return (nrm(v.x,15.0) << 12) | (nrm(v.y,15.0) << 8) | (nrm(v.z,15.0) << 4) | nrm(v.w,15.0);\n"
"  if (t == 11u) return nrm(v.x,15.0) | (nrm(v.y,15.0) << 4) | (nrm(v.z,15.0) << 8) | (nrm(v.w,15.0) << 12);\n"
"  if (t == 12u) return (nrm(v.x,31.0) << 11) | (nrm(v.y,31.0) << 6) | (nrm(v.z,31.0) << 1) | nrm(v.w,1.0);\n"
"  if (t == 13u) return nrm(v.x,31.0) | (nrm(v.y,31.0) << 5) | (nrm(v.z,31.0) << 10) | (nrm(v.w,1.0) << 15);\n"
"  if (t == 14u) return (nrm(v.x,255.0) << 24) | (nrm(v.y,255.0) << 16) | (nrm(v.z,255.0) << 8) | nrm(v.w,255.0);\n"
"  if (t == 15u) return nrm(v.x,255.0) | (nrm(v.y,255.0) << 8) | (nrm(v.z,255.0) << 16) | (nrm(v.w,255.0) << 24);\n"
"  if (t == 16u) return nrm(v.x,1023.0) | (nrm(v.y,1023.0) << 10) | (nrm(v.z,1023.0) << 20) | (nrm(v.w,3.0) << 30);\n"
"  if (t == 17u) return f11(v.x, 6u) | (f11(v.y, 6u) << 11) | (f11(v.z, 5u) << 22);\n"
"  if (t == 18u) return rgb9e5(v.xyz);\n"
"  if (t == 19u) return (nrm(v.x,7.0) << 5) | (nrm(v.y,7.0) << 2) | nrm(v.z,3.0);\n"
"  return nrm(v.x,7.0) | (nrm(v.y,7.0) << 3) | (nrm(v.z,3.0) << 6);   /* 20: 2_3_3_REV */\n"
"}\n"
"uint elem(uint pix, uint c) {\n"
"  ivec2 p = ivec2(int(pix % u_w), int(pix / u_w));\n"
"  if (u_dom == 0u) {\n"
"    vec4 v = fetchf(p);\n"
"    if (u_bgr != 0u) v = vec4(v.b, v.g, v.r, v.a);\n"
"    if (u_type >= 8u) return packedf(v);\n"
"    return elemf(v, c);\n"
"  }\n"
"  if (u_dom == 1u) {\n"
"    ivec4 v = texelFetch(u_i, p, 0);\n"
"    if (u_bgr != 0u) v = ivec4(v.b, v.g, v.r, v.a);\n"
"    int x = v[c];\n"
"    if (u_type == 0u) return uint(clamp(x, 0, 255));\n"
"    if (u_type == 1u) return uint(clamp(x, -128, 127)) & 0xFFu;\n"
"    if (u_type == 2u) return uint(clamp(x, 0, 65535));\n"
"    if (u_type == 3u) return uint(clamp(x, -32768, 32767)) & 0xFFFFu;\n"
"    if (u_type == 4u) return uint(max(x, 0));\n"
"    return uint(x);\n"
"  }\n"
"  uvec4 v = texelFetch(u_u, p, 0);\n"
"  if (u_bgr != 0u) v = uvec4(v.b, v.g, v.r, v.a);\n"
"  uint x = v[c];\n"
"  if (u_type == 0u) return min(x, 255u);\n"
"  if (u_type == 1u) return min(x, 127u);\n"
"  if (u_type == 2u) return min(x, 65535u);\n"
"  if (u_type == 3u) return min(x, 32767u);\n"
"  if (u_type == 5u) return min(x, 2147483647u);\n"
"  return x;\n"
"}\n"
"void main() {\n"
"  uint i = gl_GlobalInvocationID.y * u_gx + gl_GlobalInvocationID.x;\n"
"  if (i * 4u >= u_total) return;\n"
"  uint word = 0u;\n"
"  for (uint b = 0u; b < 4u; b++) {\n"
"    uint byte = i * 4u + b;\n"
"    if (byte >= u_total) break;\n"
"    uint e = byte / u_es, j = byte % u_es;\n"
"    if (u_swap != 0u) j = u_es - 1u - j;\n"
"    uint v = elem(e / u_ne, e % u_ne);\n"
"    word |= ((v >> (8u * j)) & 0xFFu) << (8u * b);\n"
"  }\n"
"  dst[i] = word;\n"
"}\n";

static GLuint g_cr_prog;
static int g_cr_state;
static GLint cr_u[16];
enum { CU_f, CU_i, CU_u, CU_dom, CU_type, CU_bgr, CU_ncomp, CU_es, CU_ne, CU_w, CU_total, CU_swap, CU_gx, CU_srgb, CU_clamp };

static int cr_ensure_prog(void)
{
    static const char* const nm[] = {"u_f","u_i","u_u","u_dom","u_type","u_bgr","u_ncomp","u_es","u_ne","u_w","u_total","u_swap","u_gx","u_srgb","u_clamp"};
    GLuint sh; GLint ok = 0; const char* src = k_cr_src; size_t k;
    if (g_cr_state) return g_cr_state == 1;
    g_cr_state = -1;
    if (!gl31_caps.es31) return 0;
    sh = BE(glCreateShader)(GL_COMPUTE_SHADER);
    BE(glShaderSource)(sh, 1, &src, NULL);
    BE(glCompileShader)(sh);
    BE(glGetShaderiv)(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = "";
        BE(glGetShaderInfoLog)(sh, sizeof log, NULL, log);
        fprintf(stderr, "LORICA: kernel de lectura de color no compila: %s\n", log);
        BE(glDeleteShader)(sh);
        return 0;
    }
    g_cr_prog = BE(glCreateProgram)();
    BE(glAttachShader)(g_cr_prog, sh);
    BE(glLinkProgram)(g_cr_prog);
    BE(glDeleteShader)(sh);
    BE(glGetProgramiv)(g_cr_prog, GL_LINK_STATUS, &ok);
    if (!ok) { BE(glDeleteProgram)(g_cr_prog); g_cr_prog = 0; return 0; }
    for (k = 0; k < sizeof nm / sizeof nm[0]; k++) cr_u[k] = BE(glGetUniformLocation)(g_cr_prog, nm[k]);
    g_cr_state = 1;
    return 1;
}

/* codigo del tipo para el kernel y tamano de elemento; 0 = no soportado */
static int cr_type(GLenum type, unsigned* code, unsigned* es, unsigned* packed)
{
    *packed = 0;
    switch (type) {
        case GL_UNSIGNED_BYTE: *code = 0; *es = 1; return 1;
        case GL_BYTE:          *code = 1; *es = 1; return 1;
        case GL_UNSIGNED_SHORT:*code = 2; *es = 2; return 1;
        case GL_SHORT:         *code = 3; *es = 2; return 1;
        case GL_UNSIGNED_INT:  *code = 4; *es = 4; return 1;
        case GL_INT:           *code = 5; *es = 4; return 1;
        case GL_FLOAT:         *code = 6; *es = 4; return 1;
        case GL_HALF_FLOAT:    *code = 7; *es = 2; return 1;
        case GL_UNSIGNED_SHORT_5_6_5:         *code = 8;  *es = 2; *packed = 3; return 1;
        case 0x8364:                          *code = 9;  *es = 2; *packed = 3; return 1;   /* 5_6_5_REV */
        case GL_UNSIGNED_SHORT_4_4_4_4:       *code = 10; *es = 2; *packed = 4; return 1;
        case 0x8365:                          *code = 11; *es = 2; *packed = 4; return 1;   /* 4_4_4_4_REV */
        case GL_UNSIGNED_SHORT_5_5_5_1:       *code = 12; *es = 2; *packed = 4; return 1;
        case 0x8366:                          *code = 13; *es = 2; *packed = 4; return 1;   /* 1_5_5_5_REV */
        case GL_UNSIGNED_INT_8_8_8_8:         *code = 14; *es = 4; *packed = 4; return 1;
        case GL_UNSIGNED_INT_8_8_8_8_REV:     *code = 15; *es = 4; *packed = 4; return 1;
        case 0x8368:                          *code = 16; *es = 4; *packed = 4; return 1;   /* 2_10_10_10_REV */
        case 0x8C3B:                          *code = 17; *es = 4; *packed = 3; return 1;   /* 10F_11F_11F_REV */
        case 0x8C3E:                          *code = 18; *es = 4; *packed = 3; return 1;   /* 5_9_9_9_REV */
        case 0x8032:                          *code = 19; *es = 1; *packed = 3; return 1;   /* 3_3_2 */
        case 0x8362:                          *code = 20; *es = 1; *packed = 3; return 1;   /* 2_3_3_REV */
    }
    return 0;
}

/* 1 = hecho (o error ya registrado); 0 con *fallback = 1 -> usar la ruta CPU de siempre */
static int read_color_gpu(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels, int* fallback)
{
    gl31_state_t* s = gl31_state();
    GLenum rd = GL_READ_FRAMEBUFFER;
    GLint rbuf = 0, ctype = 0, rs = 0, gs = 0, bs = 0, as = 0, enc = 0, samples = 0;
    GLenum att;
    unsigned code, es, packed, ncomp = 0, ne, bgr = 0, integer_fmt = 0, dom;
    GLenum ifmt, bf, bt;
    GLuint tex = 0, fbo = 0;
    size_t psize, total, words;
    GLint sv_u0 = 0, sv_act = 0, sv_smp0 = 0, sv_ssbo = 0, sv_id = 0;
    GLint64 sv_start = 0, sv_size = 0;
    GLboolean scissor;
    GLuint groups, gx, gy;
    int ok = 1, tmp_ok = 1;

    *fallback = 1;
    if (!cr_type(type, &code, &es, &packed)) return 0;
    switch (format) {
        case GL_RED: ncomp = 1; break;               case GL_RG: ncomp = 2; break;
        case GL_RGB: ncomp = 3; break;               case GL_RGBA: ncomp = 4; break;
        case GL_BGR: ncomp = 3; bgr = 1; break;      case GL_BGRA: ncomp = 4; bgr = 1; break;
        case 0x8D94: ncomp = 1; integer_fmt = 1; break;                   /* RED_INTEGER */
        case 0x8228: ncomp = 2; integer_fmt = 1; break;                   /* RG_INTEGER */
        case 0x8D98: ncomp = 3; integer_fmt = 1; break;                   /* RGB_INTEGER */
        case 0x8D99: ncomp = 4; integer_fmt = 1; break;                   /* RGBA_INTEGER */
        case 0x8D9A: ncomp = 3; integer_fmt = 1; bgr = 1; break;          /* BGR_INTEGER */
        case 0x8D9B: ncomp = 4; integer_fmt = 1; bgr = 1; break;          /* BGRA_INTEGER */
        default: return 0;
    }
    if (packed) {                                    /* los tipos empaquetados fijan los componentes */
        if (packed == 3 && ncomp != 3) { *fallback = 0; gl31_set_error(GL_INVALID_OPERATION); return 0; }
        if (packed == 4 && ncomp != 4) { *fallback = 0; gl31_set_error(GL_INVALID_OPERATION); return 0; }
        if (integer_fmt) { *fallback = 0; gl31_set_error(GL_INVALID_OPERATION); return 0; }
        ne = 1;
    } else ne = ncomp;
    if (!cr_ensure_prog()) return 0;

    {
        GLint rfb_ = 0;
        BE(glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING, &rfb_);      /* el FBO real del backend (puede ser uno interno) */
        att = rfb_ ? GL_COLOR_ATTACHMENT0 : GL_BACK;
        BE(glGetIntegerv)(GL_READ_BUFFER, &rbuf);
        if (rfb_) {
            if (rbuf == (GLint)GL_NONE) { *fallback = 0; gl31_set_error(GL_INVALID_OPERATION); return 0; }
            att = (GLenum)rbuf;
        }
        if (att_query(rd, att, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) == 0 && rfb_) return 0;
    }
    ctype = att_query(rd, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE);
    rs = att_query(rd, att, 0x8212); gs = att_query(rd, att, 0x8213);
    bs = att_query(rd, att, 0x8214); as = att_query(rd, att, 0x8215);
    enc = att_query(rd, att, 0x8210);
    if (!ctype) return 0;

    dom = (ctype == (GLint)GL_INT) ? 1u : (ctype == (GLint)GL_UNSIGNED_INT) ? 2u : 0u;
    if ((dom != 0) != (integer_fmt != 0)) { *fallback = 0; gl31_set_error(GL_INVALID_OPERATION); return 0; }
    if (dom != 0 && code >= 6) { *fallback = 0; gl31_set_error(GL_INVALID_OPERATION); return 0; }   /* enteros a float/packed */

    /* formato de la copia: conserva los datos del origen */
    if (dom == 1) { ifmt = GL_RGBA32I; bf = 0x8D99; bt = GL_INT; }
    else if (dom == 2) { ifmt = GL_RGBA32UI; bf = 0x8D99; bt = GL_UNSIGNED_INT; }
    else if (ctype == (GLint)GL_FLOAT || rs > 8 || gs > 8 || bs > 8 || ctype == (GLint)0x8F9C /*SIGNED_NORMALIZED*/) { ifmt = GL_RGBA32F; bf = GL_RGBA; bt = GL_FLOAT; }
    else if (enc == (GLint)GL_SRGB) { ifmt = GL_SRGB8_ALPHA8; bf = GL_RGBA; bt = GL_UNSIGNED_BYTE; }
    else { ifmt = GL_RGBA8; bf = GL_RGBA; bt = GL_UNSIGNED_BYTE; }
    (void)bf; (void)bt; (void)as;

    psize = (size_t)es * ne;
    total = (size_t)w * (size_t)h * psize;
    words = (total + 3) / 4;
    if (total > (size_t)256 * 1024 * 1024) { *fallback = 0; gl31_set_error(GL_OUT_OF_MEMORY); return 0; }

    BE(glGetIntegerv)(GL_ACTIVE_TEXTURE, &sv_act);
    BE(glActiveTexture)(GL_TEXTURE0);
    BE(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &sv_u0);
    BE(glGetIntegerv)(GL_SAMPLER_BINDING, &sv_smp0);
    BE(glGetIntegerv)(GL_SHADER_STORAGE_BUFFER_BINDING, &sv_ssbo);
    BE(glGetIntegeri_v)(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &sv_id);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_START, 0, &sv_start);
    BE(glGetInteger64i_v)(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &sv_size);
    scissor = BE(glIsEnabled)(GL_SCISSOR_TEST);
    if (scissor) BE(glDisable)(GL_SCISSOR_TEST);
    BE(glBindSampler)(0, 0);

    /* la lectura de glBlitFramebuffer usa el read buffer actual; el destino es COLOR_ATTACHMENT0 de un FBO temporal */
    BE(glGenFramebuffers)(1, &fbo);
    tex = ds_make_tex(ifmt, w, h);
    (void)samples;
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fbo);
    BE(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    {
        GLenum d0 = GL_COLOR_ATTACHMENT0;
        BE(glDrawBuffers)(1, &d0);
    }
    if (BE(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) tmp_ok = 0;
    if (tmp_ok) {
        BE(glBlitFramebuffer)(x, y, x + w, y + h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        if (BE(glGetError)() != GL_NO_ERROR) tmp_ok = 0;
    }
    BE(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, gl31_fbo_be_draw(s->draw_fbo));
    if (s->draw_fbo) { /* restaurar los draw buffers del FBO de la app no hace falta: son estado de cada FBO */ }
    if (scissor) BE(glEnable)(GL_SCISSOR_TEST);
    if (!tmp_ok) { ok = 0; *fallback = 1; goto done; }
    *fallback = 0;

    BE(glBindTexture)(GL_TEXTURE_2D, tex);
    if (!g_ds_buf) BE(glGenBuffers)(1, &g_ds_buf);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, g_ds_buf);
    if (g_ds_cap < words * 4) {
        size_t nc = 1024;
        while (nc < words * 4) nc <<= 1;
        BE(glBufferData)(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)nc, NULL, GL_DYNAMIC_COPY);
        g_ds_cap = nc;
    }
    BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 0, g_ds_buf, 0, (GLsizeiptr)g_ds_cap);
    BE(glUseProgram)(g_cr_prog);
    /* los tres samplers apuntan a la unidad 0; solo el del dominio usado se lee */
    BE(glUniform1i)(cr_u[CU_f], 0); BE(glUniform1i)(cr_u[CU_i], 0); BE(glUniform1i)(cr_u[CU_u], 0);
    if (dom == 1) { BE(glUniform1i)(cr_u[CU_f], 1); BE(glUniform1i)(cr_u[CU_u], 2); }
    else if (dom == 2) { BE(glUniform1i)(cr_u[CU_f], 1); BE(glUniform1i)(cr_u[CU_i], 2); }
    else { BE(glUniform1i)(cr_u[CU_i], 1); BE(glUniform1i)(cr_u[CU_u], 2); }
    BE(glUniform1ui)(cr_u[CU_dom], dom);
    BE(glUniform1ui)(cr_u[CU_type], code);
    BE(glUniform1ui)(cr_u[CU_bgr], bgr);
    BE(glUniform1ui)(cr_u[CU_ncomp], ncomp);
    BE(glUniform1ui)(cr_u[CU_es], es);
    BE(glUniform1ui)(cr_u[CU_ne], ne);
    BE(glUniform1ui)(cr_u[CU_w], (GLuint)w);
    BE(glUniform1ui)(cr_u[CU_total], (GLuint)total);
    BE(glUniform1ui)(cr_u[CU_swap], (s->pack_swap_bytes && es > 1) ? 1u : 0u);
    BE(glUniform1ui)(cr_u[CU_srgb], (enc == (GLint)GL_SRGB && ifmt == GL_SRGB8_ALPHA8) ? 1u : 0u);
    BE(glUniform1ui)(cr_u[CU_clamp], (s->clamp_read_color == GL_TRUE && dom == 0) ? 1u : 0u);
    groups = (GLuint)((words + 63) / 64);
    gx = groups > 65535u ? 65535u : groups;
    gy = (groups + gx - 1) / gx;
    BE(glUniform1ui)(cr_u[CU_gx], gx * 64u);
    if (gy > 65535u) { ok = 0; gl31_set_error(GL_OUT_OF_MEMORY); goto done; }
    BE(glDispatchCompute)(gx, gy, 1);
    BE(glMemoryBarrier)(0xFFFFFFFFu);
    ok = px_deliver(w, h, psize, total, pixels);

done:
    if (sv_id && sv_size > 0) BE(glBindBufferRange)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id, (GLintptr)sv_start, (GLsizeiptr)sv_size);
    else BE(glBindBufferBase)(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)sv_id);
    BE(glBindBuffer)(GL_SHADER_STORAGE_BUFFER, (GLuint)sv_ssbo);
    BE(glUseProgram)(s->program);
    BE(glActiveTexture)(GL_TEXTURE0); BE(glBindTexture)(GL_TEXTURE_2D, (GLuint)sv_u0); BE(glBindSampler)(0, (GLuint)sv_smp0);
    BE(glActiveTexture)((GLenum)sv_act);
    if (tex) BE(glDeleteTextures)(1, &tex);
    if (fbo) BE(glDeleteFramebuffers)(1, &fbo);
    return ok;
}

/* Lee un rectangulo del framebuffer de lectura actual y lo entrega en el
 * formato/tipo de desktop pedido. 1 = hecho (o error ya registrado). */
int gl31_read_pixels_ex(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels)
{
    gl31_state_t* s = gl31_state();
    int psize, float_src = 0, i, j;
    GLint implt = 0;
    size_t stride, total, rows_bytes;
    unsigned char* tmp = NULL;
    unsigned char* dst;
    unsigned char* mapped = NULL;
    GLsizeiptr off = 0;

    if (w < 0 || h < 0) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (w == 0 || h == 0) return 1;
    if (format == GL_DEPTH_COMPONENT || format == GL_STENCIL_INDEX || format == GL_DEPTH_STENCIL)
        return read_depth_stencil(x, y, w, h, format, type, pixels);

    {
        GLint implt0 = 0;
        int want_clamp = 0;
        BE(glGetIntegerv)(GL_IMPLEMENTATION_COLOR_READ_TYPE, &implt0);
        want_clamp = s->clamp_read_color == GL_TRUE &&
                     (implt0 == (GLint)GL_FLOAT || implt0 == (GLint)GL_HALF_FLOAT) &&
                     (type == GL_FLOAT || type == GL_HALF_FLOAT);
        if (!want_clamp && !(s->pack_swap_bytes && dst_elem_size(type) > 1) && native_pair(format, type)) {
            BE(glReadPixels)(x, y, w, h, format, type, pixels);
            return 1;
        }
    }
    if (gl31_caps.es31) {
        int fb = 1, r = read_color_gpu(x, y, w, h, format, type, pixels, &fb);
        if (!fb) return r;                    /* hecho en GPU (o error ya registrado) */
    }
    psize = dst_pixel_size(format, type);
    if (!psize) {
        gl31_stub_warn("glReadPixels(formato/tipo sin conversion)");
        gl31_set_error(GL_INVALID_OPERATION);
        return 0;
    }

    /* origen: float solo si el buffer lo es y el destino es float/half */
    BE(glGetIntegerv)(GL_IMPLEMENTATION_COLOR_READ_TYPE, &implt);
    if ((type == GL_FLOAT || type == GL_HALF_FLOAT) && (implt == (GLint)GL_FLOAT || implt == (GLint)GL_HALF_FLOAT))
        float_src = 1;

    {
        size_t src_px = float_src ? 16u : 4u;
        tmp = (unsigned char*)malloc((size_t)w * (size_t)h * src_px);
        if (!tmp) { gl31_set_error(GL_OUT_OF_MEMORY); return 0; }
    }

    /* lectura plana, sin PBO ni parametros de empaquetado */
    if (s->pixel_pack_buffer) BE(glBindBuffer)(GL_PIXEL_PACK_BUFFER, 0);
    set_pack_state(1, 0, 0, 0);
    BE(glReadPixels)(x, y, w, h, GL_RGBA, float_src ? GL_FLOAT : GL_UNSIGNED_BYTE, tmp);
    set_pack_state(s->pack_alignment, s->pack_row_length, s->pack_skip_rows, s->pack_skip_pixels);
    if (s->pixel_pack_buffer) BE(glBindBuffer)(GL_PIXEL_PACK_BUFFER, s->pixel_pack_buffer);

    /* geometria del destino con los GL_PACK_* de la app */
    {
        GLint rl = s->pack_row_length > 0 ? s->pack_row_length : w;
        GLint al = s->pack_alignment > 0 ? s->pack_alignment : 4;
        rows_bytes = (size_t)rl * (size_t)psize;
        stride = ((rows_bytes + (size_t)al - 1) / (size_t)al) * (size_t)al;
        total = (size_t)(s->pack_skip_rows + h - 1) * stride +
                (size_t)(s->pack_skip_pixels + w) * (size_t)psize;
    }

    if (s->pixel_pack_buffer) {          /* pixels es un offset dentro del PBO */
        off = (GLsizeiptr)(intptr_t)pixels;
        mapped = (unsigned char*)BE(glMapBufferRange)(GL_PIXEL_PACK_BUFFER, off, (GLsizeiptr)total,
                                                      GL_MAP_WRITE_BIT);
        if (!mapped) { free(tmp); gl31_set_error(GL_INVALID_OPERATION); return 0; }
        dst = mapped;
    } else {
        if (!pixels) { free(tmp); gl31_set_error(GL_INVALID_VALUE); return 0; }
        dst = (unsigned char*)pixels;
    }

    for (j = 0; j < h; j++) {
        unsigned char* row = dst + (size_t)(s->pack_skip_rows + j) * stride +
                             (size_t)s->pack_skip_pixels * (size_t)psize;
        for (i = 0; i < w; i++) {
            float c[4];
            size_t px = (size_t)j * (size_t)w + (size_t)i;
            if (float_src) {
                const float* f = (const float*)tmp + px * 4;
                c[0] = f[0]; c[1] = f[1]; c[2] = f[2]; c[3] = f[3];
                if (s->clamp_read_color == GL_TRUE) {
                    int q;
                    for (q = 0; q < 4; q++) c[q] = c[q] < 0.f ? 0.f : c[q] > 1.f ? 1.f : c[q];
                }
            } else {
                const unsigned char* b = tmp + px * 4;
                c[0] = b[0] / 255.f; c[1] = b[1] / 255.f; c[2] = b[2] / 255.f; c[3] = b[3] / 255.f;
            }
            put_pixel(row + (size_t)i * (size_t)psize, format, type, c);
        }
        if (s->pack_swap_bytes && dst_elem_size(type) > 1) {
            int es = dst_elem_size(type);
            swap_elems(row, (size_t)w * (size_t)psize / (size_t)es, es);
        }
    }
    if (mapped) BE(glUnmapBuffer)(GL_PIXEL_PACK_BUFFER);
    free(tmp);
    return 1;
}

void gl31_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels)
{
    gl31_read_pixels_ex(x, y, w, h, format, type, pixels);
}

/* GL 3.2: adjuntar una textura completa (en capas si es array/cubo/3D). Nativo con
 * OES/EXT_geometry_shader; sin el, se adjunta la capa 0 como aproximacion. */
void gl31_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
    if (!attach_prologue(target, attachment)) return;
    if (level < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_be.glFramebufferTexture) {
        { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferTexture)(rt_, attachment, texture, level); gl31_fbo_redir_end(rd_, 1); }
        return;
    }
    if (texture) gl31_stub_warn("glFramebufferTexture (sin geometry_shader: se adjunta la capa 0)");
    {
        GLint ty = 0;
        (void)ty;
        { int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_); BE(glFramebufferTextureLayer)(rt_, attachment, texture, level, 0); gl31_fbo_redir_end(rd_, 1); }
    }
}
