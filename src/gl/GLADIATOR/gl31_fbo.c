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

void gl31_glDeleteFramebuffers(GLsizei n, const GLuint* ids)
{
    gl31_state_t* s = gl31_state();
    GLsizei i;
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        if (!ids[i]) continue;
        if (s->read_fbo == ids[i]) s->read_fbo = 0;
        if (s->draw_fbo == ids[i]) s->draw_fbo = 0;
    }
    BE(glDeleteFramebuffers)(n, ids);
}

void gl31_glBindFramebuffer(GLenum target, GLuint fbo)
{
    gl31_state_t* s = gl31_state();
    if (!is_fbo_target(target)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (target != GL_READ_FRAMEBUFFER) s->draw_fbo = fbo;
    if (target != GL_DRAW_FRAMEBUFFER) s->read_fbo = fbo;
    BE(glBindFramebuffer)(target, fbo);
}

GLboolean gl31_glIsFramebuffer(GLuint fbo) { return fbo ? BE(glIsFramebuffer)(fbo) : GL_FALSE; }

GLenum gl31_glCheckFramebufferStatus(GLenum target)
{
    if (!is_fbo_target(target)) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    return BE(glCheckFramebufferStatus)(target);
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
    BE(glFramebufferTexture2D)(target, attachment, be, texture, level);
}

/* 1D = 2D de alto 1 en el backend */
void gl31_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    if (!attach_prologue(target, attachment)) return;
    if (textarget != GL_TEXTURE_1D) { gl31_set_error(GL_INVALID_ENUM); return; }
    BE(glFramebufferTexture2D)(target, attachment, GL_TEXTURE_2D, texture, level);
}

void gl31_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                 GLint level, GLint zoffset)
{
    if (!attach_prologue(target, attachment)) return;
    if (textarget != GL_TEXTURE_3D) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (zoffset < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glFramebufferTextureLayer)(target, attachment, texture, level, zoffset);
}

void gl31_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    if (!attach_prologue(target, attachment)) return;
    if (layer < 0 || level < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glFramebufferTextureLayer)(target, attachment, texture, level, layer);
}

void gl31_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb)
{
    if (!attach_prologue(target, attachment)) return;
    if (rbtarget != GL_RENDERBUFFER) { gl31_set_error(GL_INVALID_ENUM); return; }
    BE(glFramebufferRenderbuffer)(target, attachment, rbtarget, rb);
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
                gl31_stub_warn("glGetFramebufferAttachmentParameteriv(GL_FRONT)");
                gl31_set_error(GL_INVALID_OPERATION);
                return;
            default: gl31_set_error(GL_INVALID_ENUM); return;
        }
    }
    BE(glGetFramebufferAttachmentParameteriv)(target, attachment, pname, params);
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
    if (mode == GL_NONE) { BE(glDrawBuffers)(1, &mode); return; }
    if (mode >= GL_COLOR_ATTACHMENT0 && (GLint)(mode - GL_COLOR_ATTACHMENT0) < max_color_attachments()) {
        GLuint i = mode - GL_COLOR_ATTACHMENT0;
        if (i == 0) { BE(glDrawBuffers)(1, &mode); return; }
        /* desktop: la salida 0 del shader va al attachment i. ES solo permite salida i -> attachment i */
        gl31_stub_warn("glDrawBuffer(GL_COLOR_ATTACHMENTi, i>0)");
        gl31_set_error(GL_INVALID_OPERATION);
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
        return;
    }
    /* ES exige bufs[i] == NONE o COLOR_ATTACHMENTi; el backend informa el error */
    BE(glDrawBuffers)(n, bufs);
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

    if (native_pair(format, type)) {
        BE(glReadPixels)(x, y, w, h, format, type, pixels);
        return 1;
    }
    psize = dst_pixel_size(format, type);
    if (!psize) {
        if (format == GL_DEPTH_COMPONENT || format == GL_STENCIL_INDEX || format == GL_DEPTH_STENCIL)
            gl31_stub_warn("glReadPixels(DEPTH/STENCIL)");
        else
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
            } else {
                const unsigned char* b = tmp + px * 4;
                c[0] = b[0] / 255.f; c[1] = b[1] / 255.f; c[2] = b[2] / 255.f; c[3] = b[3] / 255.f;
            }
            put_pixel(row + (size_t)i * (size_t)psize, format, type, c);
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
        BE(glFramebufferTexture)(target, attachment, texture, level);
        return;
    }
    if (texture) gl31_stub_warn("glFramebufferTexture (sin geometry_shader: se adjunta la capa 0)");
    {
        GLint ty = 0;
        (void)ty;
        BE(glFramebufferTextureLayer)(target, attachment, texture, level, 0);
    }
}
