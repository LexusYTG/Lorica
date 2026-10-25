/* gl31_dsa.c - ARB_direct_state_access (GL 4.5 core) sobre GLES.
 *
 * ES no tiene acceso directo por nombre: cada funcion enlaza el objeto, llama a la version
 * clasica (ya traducida por Lorica) y restaura el enlace anterior. El estado enlazado que ve
 * la app no cambia. Limitaciones: ver GL45.md (cube maps en TextureSubImage3D/GetTextureImage
 * solo sin compresion, sin GetTextureSubImage, sin glClipControl). */
#include "gl31.h"
#include <string.h>

/* =====================================================================
 * Texturas
 * ===================================================================== */
typedef struct { GLenum target; GLuint prev; } tbind_t;

static int tb_push(tbind_t* b, GLuint tex)
{
    GLenum t = gl31_tex_target_of(tex);
    if (!tex || !t) { gl31_set_error(GL_INVALID_OPERATION); return 0; }   /* sin crear con glCreateTextures / glBindTexture */
    if (!gl31_tex_bound_for(t, &b->prev)) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    b->target = t;
    gl31_glBindTexture(t, tex);
    return 1;
}

static void tb_pop(const tbind_t* b) { gl31_glBindTexture(b->target, b->prev); }

#define WITH_TEX(tex) tbind_t tb_; if (!tb_push(&tb_, (tex))) return
#define END_TEX()     tb_pop(&tb_)

void gl31_glCreateTextures(GLenum target, GLsizei n, GLuint* textures)
{
    GLuint prev;
    GLsizei i;
    if (n < 0 || (n && !textures)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!gl31_tex_bound_for(target, &prev)) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    gl31_glGenTextures(n, textures);
    for (i = 0; i < n; i++) gl31_glBindTexture(target, textures[i]);     /* fija el tipo del objeto */
    gl31_glBindTexture(target, prev);
}

void gl31_glBindTextureUnit(GLuint unit, GLuint texture)
{
    GLuint prev_unit = gl31_tex_active_unit();
    if (texture) {
        GLenum t = gl31_tex_target_of(texture);
        if (!t) { gl31_set_error(GL_INVALID_OPERATION); return; }
        gl31_glActiveTexture(GL_TEXTURE0 + unit);
        if (gl31_glGetError() != GL_NO_ERROR) { gl31_set_error(GL_INVALID_OPERATION); return; }
        gl31_glBindTexture(t, texture);
    } else {
        static const GLenum all[] = { GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D_ARRAY,
                                      GL_TEXTURE_1D, GL_TEXTURE_1D_ARRAY, GL_TEXTURE_RECTANGLE };
        size_t i;
        gl31_glActiveTexture(GL_TEXTURE0 + unit);
        if (gl31_glGetError() != GL_NO_ERROR) { gl31_set_error(GL_INVALID_OPERATION); return; }
        for (i = 0; i < sizeof all / sizeof all[0]; i++) gl31_glBindTexture(all[i], 0);
    }
    gl31_glActiveTexture(GL_TEXTURE0 + prev_unit);
}

/* ---- parametros ---- */
void gl31_glTextureParameteri(GLuint t, GLenum p, GLint v)        { WITH_TEX(t); gl31_glTexParameteri(tb_.target, p, v); END_TEX(); }
void gl31_glTextureParameterf(GLuint t, GLenum p, GLfloat v)      { WITH_TEX(t); gl31_glTexParameterf(tb_.target, p, v); END_TEX(); }
void gl31_glTextureParameteriv(GLuint t, GLenum p, const GLint* v)   { WITH_TEX(t); gl31_glTexParameteriv(tb_.target, p, v); END_TEX(); }
void gl31_glTextureParameterfv(GLuint t, GLenum p, const GLfloat* v) { WITH_TEX(t); gl31_glTexParameterfv(tb_.target, p, v); END_TEX(); }
void gl31_glTextureParameterIiv(GLuint t, GLenum p, const GLint* v)   { WITH_TEX(t); gl31_glTexParameterIiv(tb_.target, p, v); END_TEX(); }
void gl31_glTextureParameterIuiv(GLuint t, GLenum p, const GLuint* v) { WITH_TEX(t); gl31_glTexParameterIuiv(tb_.target, p, v); END_TEX(); }
void gl31_glGetTextureParameteriv(GLuint t, GLenum p, GLint* v)    { WITH_TEX(t); gl31_glGetTexParameteriv(tb_.target, p, v); END_TEX(); }
void gl31_glGetTextureParameterfv(GLuint t, GLenum p, GLfloat* v)  { WITH_TEX(t); gl31_glGetTexParameterfv(tb_.target, p, v); END_TEX(); }
void gl31_glGetTextureParameterIiv(GLuint t, GLenum p, GLint* v)   { WITH_TEX(t); gl31_glGetTexParameterIiv(tb_.target, p, v); END_TEX(); }
void gl31_glGetTextureParameterIuiv(GLuint t, GLenum p, GLuint* v) { WITH_TEX(t); gl31_glGetTexParameterIuiv(tb_.target, p, v); END_TEX(); }
void gl31_glGetTextureLevelParameteriv(GLuint t, GLint l, GLenum p, GLint* v)   { WITH_TEX(t); gl31_glGetTexLevelParameteriv(tb_.target, l, p, v); END_TEX(); }
void gl31_glGetTextureLevelParameterfv(GLuint t, GLint l, GLenum p, GLfloat* v) { WITH_TEX(t); gl31_glGetTexLevelParameterfv(tb_.target, l, p, v); END_TEX(); }
void gl31_glGenerateTextureMipmap(GLuint t) { WITH_TEX(t); gl31_glGenerateMipmap(tb_.target); END_TEX(); }

/* ---- almacenamiento ---- */
void gl31_glTextureStorage1D(GLuint t, GLsizei lv, GLenum f, GLsizei w)
{ WITH_TEX(t); gl31_glTexStorage1D(tb_.target, lv, f, w); END_TEX(); }
void gl31_glTextureStorage2D(GLuint t, GLsizei lv, GLenum f, GLsizei w, GLsizei h)
{ WITH_TEX(t); gl31_glTexStorage2D(tb_.target, lv, f, w, h); END_TEX(); }
void gl31_glTextureStorage3D(GLuint t, GLsizei lv, GLenum f, GLsizei w, GLsizei h, GLsizei d)
{ WITH_TEX(t); gl31_glTexStorage3D(tb_.target, lv, f, w, h, d); END_TEX(); }
void gl31_glTextureStorage2DMultisample(GLuint t, GLsizei s, GLenum f, GLsizei w, GLsizei h, GLboolean fixed)
{ WITH_TEX(t); gl31_glTexStorage2DMultisample(tb_.target, s, f, w, h, fixed); END_TEX(); }
void gl31_glTextureStorage3DMultisample(GLuint t, GLsizei s, GLenum f, GLsizei w, GLsizei h, GLsizei d, GLboolean fixed)
{ WITH_TEX(t); gl31_glTexStorage3DMultisample(tb_.target, s, f, w, h, d, fixed); END_TEX(); }
void gl31_glTextureBuffer(GLuint t, GLenum f, GLuint buf)
{ WITH_TEX(t); gl31_glTexBuffer(tb_.target, f, buf); END_TEX(); }
void gl31_glTextureBufferRange(GLuint t, GLenum f, GLuint buf, GLintptr off, GLsizeiptr size)
{ WITH_TEX(t); gl31_glTexBufferRange(tb_.target, f, buf, off, size); END_TEX(); }

/* ---- tamano de una imagen sin comprimir (para recorrer las 6 caras de un cube map) ---- */
static size_t type_bytes(GLenum type, int* packed)
{
    *packed = 0;
    switch (type) {
        case GL_UNSIGNED_BYTE: case GL_BYTE: return 1;
        case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: return 2;
        case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT: return 4;
        case GL_UNSIGNED_SHORT_5_6_5: case GL_UNSIGNED_SHORT_4_4_4_4: case GL_UNSIGNED_SHORT_5_5_5_1: *packed = 1; return 2;
        case GL_UNSIGNED_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV: case GL_UNSIGNED_INT_24_8: *packed = 1; return 4;
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV: *packed = 1; return 8;
    }
    return 0;
}

static size_t format_comps(GLenum f)
{
    switch (f) {
        case GL_RED: case GL_RED_INTEGER: case GL_DEPTH_COMPONENT: case GL_STENCIL_INDEX:
        case GL_DEPTH_STENCIL: case GL_ALPHA: case GL_LUMINANCE: return 1;
        case GL_RG: case GL_RG_INTEGER: case GL_LUMINANCE_ALPHA: return 2;
        case GL_RGB: case GL_RGB_INTEGER: case GL_BGR: return 3;
        case GL_RGBA: case GL_RGBA_INTEGER: case GL_BGRA: return 4;
    }
    return 0;
}

/* bytes de una imagen w x h (sin GL_UNPACK_IMAGE_HEIGHT ni skips: la app que use cube maps con DSA
 * entrega las caras contiguas); 0 si formato/tipo desconocido */
static size_t image_bytes(GLsizei w, GLsizei h, GLenum fmt, GLenum type)
{
    int packed;
    size_t tb = type_bytes(type, &packed), px, row, align;
    if (!tb) return 0;
    if (packed) px = tb;
    else {
        size_t c = format_comps(fmt);
        if (!c) return 0;
        px = tb * c;
    }
    align = (size_t)gl31_state()->unpack_alignment;
    if (!align) align = 4;
    row = ((size_t)w * px + align - 1) / align * align;
    return row * (size_t)h;
}

/* ---- subida de datos ---- */
void gl31_glTextureSubImage1D(GLuint t, GLint l, GLint x, GLsizei w, GLenum f, GLenum ty, const void* p)
{ WITH_TEX(t); gl31_glTexSubImage1D(tb_.target, l, x, w, f, ty, p); END_TEX(); }

void gl31_glTextureSubImage2D(GLuint t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void* p)
{ WITH_TEX(t); gl31_glTexSubImage2D(tb_.target, l, x, y, w, h, f, ty, p); END_TEX(); }

void gl31_glTextureSubImage3D(GLuint t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d,
                              GLenum f, GLenum ty, const void* p)
{
    WITH_TEX(t);
    if (tb_.target == GL_TEXTURE_CUBE_MAP) {       /* z = cara inicial, d = numero de caras */
        GLint i;
        size_t step = image_bytes(w, h, f, ty);
        if (z < 0 || d < 0 || z + d > 6) gl31_set_error(GL_INVALID_VALUE);
        else if (!step && p) gl31_set_error(GL_INVALID_ENUM);
        else
            for (i = 0; i < d; i++)
                gl31_glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)(z + i), l, x, y, w, h, f, ty,
                                     p ? (const void*)((const unsigned char*)p + step * (size_t)i) : NULL);
    } else {
        gl31_glTexSubImage3D(tb_.target, l, x, y, z, w, h, d, f, ty, p);
    }
    END_TEX();
}

void gl31_glCompressedTextureSubImage1D(GLuint t, GLint l, GLint x, GLsizei w, GLenum f, GLsizei sz, const void* p)
{ WITH_TEX(t); gl31_glCompressedTexSubImage1D(tb_.target, l, x, w, f, sz, p); END_TEX(); }

void gl31_glCompressedTextureSubImage2D(GLuint t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLsizei sz, const void* p)
{ WITH_TEX(t); gl31_glCompressedTexSubImage2D(tb_.target, l, x, y, w, h, f, sz, p); END_TEX(); }

void gl31_glCompressedTextureSubImage3D(GLuint t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d,
                                        GLenum f, GLsizei sz, const void* p)
{
    WITH_TEX(t);
    if (tb_.target == GL_TEXTURE_CUBE_MAP) {
        gl31_stub_warn("glCompressedTextureSubImage3D sobre cube map");
        gl31_set_error(GL_INVALID_OPERATION);
    } else {
        gl31_glCompressedTexSubImage3D(tb_.target, l, x, y, z, w, h, d, f, sz, p);
    }
    END_TEX();
}

void gl31_glCopyTextureSubImage1D(GLuint t, GLint l, GLint xo, GLint x, GLint y, GLsizei w)
{ WITH_TEX(t); gl31_glCopyTexSubImage1D(tb_.target, l, xo, x, y, w); END_TEX(); }

void gl31_glCopyTextureSubImage2D(GLuint t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{ WITH_TEX(t); gl31_glCopyTexSubImage2D(tb_.target, l, xo, yo, x, y, w, h); END_TEX(); }

void gl31_glCopyTextureSubImage3D(GLuint t, GLint l, GLint xo, GLint yo, GLint zo, GLint x, GLint y, GLsizei w, GLsizei h)
{
    WITH_TEX(t);
    if (tb_.target == GL_TEXTURE_CUBE_MAP) {
        if (zo < 0 || zo > 5) gl31_set_error(GL_INVALID_VALUE);
        else gl31_glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)zo, l, xo, yo, x, y, w, h);
    } else {
        gl31_glCopyTexSubImage3D(tb_.target, l, xo, yo, zo, x, y, w, h);
    }
    END_TEX();
}

/* ---- lectura ---- */
void gl31_glGetTextureImage(GLuint t, GLint l, GLenum f, GLenum ty, GLsizei bufSize, void* pixels)
{
    WITH_TEX(t);
    (void)bufSize;
    if (tb_.target == GL_TEXTURE_CUBE_MAP) {       /* las 6 caras seguidas */
        GLint w = 0, h = 0, i;
        size_t step;
        gl31_glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, l, GL_TEXTURE_WIDTH, &w);
        gl31_glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, l, GL_TEXTURE_HEIGHT, &h);
        step = image_bytes(w, h, f, ty);
        if (!step) gl31_set_error(GL_INVALID_ENUM);
        else
            for (i = 0; i < 6; i++)
                gl31_glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, l, f, ty,
                                   (unsigned char*)pixels + step * (size_t)i);
    } else {
        gl31_glGetTexImage(tb_.target, l, f, ty, pixels);
    }
    END_TEX();
}

void gl31_glGetCompressedTextureImage(GLuint t, GLint l, GLsizei bufSize, void* pixels)
{
    WITH_TEX(t);
    (void)bufSize;
    if (tb_.target == GL_TEXTURE_CUBE_MAP) {
        gl31_stub_warn("glGetCompressedTextureImage sobre cube map");
        gl31_set_error(GL_INVALID_OPERATION);
    } else {
        gl31_glGetCompressedTexImage(tb_.target, l, pixels);
    }
    END_TEX();
}

/* =====================================================================
 * Buffers: se usa GL_COPY_WRITE_BUFFER como punto de enlace temporal
 * ===================================================================== */
typedef struct { GLuint prev; } bbind_t;

static int bb_push(bbind_t* b, GLuint buf)
{
    if (!buf) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    b->prev = gl31_state()->copy_write_buffer;
    gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
    return 1;
}
static void bb_pop(const bbind_t* b) { gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, b->prev); }

#define WITH_BUF(buf) bbind_t bb_; if (!bb_push(&bb_, (buf))) return
#define END_BUF()     bb_pop(&bb_)

void gl31_glCreateBuffers(GLsizei n, GLuint* buffers)
{
    GLsizei i;
    GLuint prev;
    if (n < 0 || (n && !buffers)) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGenBuffers(n, buffers);
    prev = gl31_state()->copy_write_buffer;
    for (i = 0; i < n; i++) gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, buffers[i]);   /* crea el objeto */
    gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, prev);
}

void gl31_glNamedBufferData(GLuint b, GLsizeiptr size, const void* data, GLenum usage)
{ WITH_BUF(b); gl31_glBufferData(GL_COPY_WRITE_BUFFER, size, data, usage); END_BUF(); }
void gl31_glNamedBufferStorage(GLuint b, GLsizeiptr size, const void* data, GLbitfield flags)
{ WITH_BUF(b); gl31_glBufferStorage(GL_COPY_WRITE_BUFFER, size, data, flags); END_BUF(); }
void gl31_glNamedBufferSubData(GLuint b, GLintptr off, GLsizeiptr size, const void* data)
{ WITH_BUF(b); gl31_glBufferSubData(GL_COPY_WRITE_BUFFER, off, size, data); END_BUF(); }
void gl31_glGetNamedBufferSubData(GLuint b, GLintptr off, GLsizeiptr size, void* data)
{ WITH_BUF(b); gl31_glGetBufferSubData(GL_COPY_WRITE_BUFFER, off, size, data); END_BUF(); }
void gl31_glGetNamedBufferParameteriv(GLuint b, GLenum pn, GLint* v)
{ WITH_BUF(b); gl31_glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, pn, v); END_BUF(); }
void gl31_glGetNamedBufferParameteri64v(GLuint b, GLenum pn, GLint64* v)
{
    GLint t = 0;
    if (!v) return;
    gl31_glGetNamedBufferParameteriv(b, pn, &t);
    *v = (GLint64)t;
}
void gl31_glGetNamedBufferPointerv(GLuint b, GLenum pn, void** v)
{ WITH_BUF(b); gl31_glGetBufferPointerv(GL_COPY_WRITE_BUFFER, pn, v); END_BUF(); }
void gl31_glClearNamedBufferData(GLuint b, GLenum ifmt, GLenum f, GLenum ty, const void* data)
{ WITH_BUF(b); gl31_glClearBufferData(GL_COPY_WRITE_BUFFER, ifmt, f, ty, data); END_BUF(); }
void gl31_glClearNamedBufferSubData(GLuint b, GLenum ifmt, GLintptr off, GLsizeiptr size, GLenum f, GLenum ty, const void* data)
{ WITH_BUF(b); gl31_glClearBufferSubData(GL_COPY_WRITE_BUFFER, ifmt, off, size, f, ty, data); END_BUF(); }
void* gl31_glMapNamedBuffer(GLuint b, GLenum access)
{
    void* p;
    bbind_t bb_;
    if (!bb_push(&bb_, b)) return NULL;
    p = gl31_glMapBuffer(GL_COPY_WRITE_BUFFER, access);
    bb_pop(&bb_);
    return p;
}
void* gl31_glMapNamedBufferRange(GLuint b, GLintptr off, GLsizeiptr len, GLbitfield access)
{
    void* p;
    bbind_t bb_;
    if (!bb_push(&bb_, b)) return NULL;
    p = gl31_glMapBufferRange(GL_COPY_WRITE_BUFFER, off, len, access);
    bb_pop(&bb_);
    return p;
}
GLboolean gl31_glUnmapNamedBuffer(GLuint b)
{
    GLboolean r;
    bbind_t bb_;
    if (!bb_push(&bb_, b)) return GL_FALSE;
    r = gl31_glUnmapBuffer(GL_COPY_WRITE_BUFFER);
    bb_pop(&bb_);
    return r;
}
void gl31_glFlushMappedNamedBufferRange(GLuint b, GLintptr off, GLsizeiptr len)
{ WITH_BUF(b); gl31_glFlushMappedBufferRange(GL_COPY_WRITE_BUFFER, off, len); END_BUF(); }

void gl31_glCopyNamedBufferSubData(GLuint rb, GLuint wb, GLintptr ro, GLintptr wo, GLsizeiptr size)
{
    gl31_state_t* s = gl31_state();
    GLuint prev_r = s->copy_read_buffer, prev_w = s->copy_write_buffer;
    if (!rb || !wb) { gl31_set_error(GL_INVALID_OPERATION); return; }
    gl31_glBindBuffer(GL_COPY_READ_BUFFER, rb);
    gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, wb);
    gl31_glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, ro, wo, size);
    gl31_glBindBuffer(GL_COPY_READ_BUFFER, prev_r);
    gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, prev_w);
}

/* =====================================================================
 * Framebuffers y renderbuffers
 * ===================================================================== */
typedef struct { GLuint draw, read; } fbind_t;

static void fb_push(fbind_t* b, GLuint fbo)
{
    gl31_state_t* s = gl31_state();
    b->draw = s->draw_fbo; b->read = s->read_fbo;
    gl31_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}
static void fb_pop(const fbind_t* b)
{
    gl31_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, b->draw);
    gl31_glBindFramebuffer(GL_READ_FRAMEBUFFER, b->read);
}

#define WITH_FB(fbo) fbind_t fb_; fb_push(&fb_, (fbo))
#define END_FB()     fb_pop(&fb_)

void gl31_glCreateFramebuffers(GLsizei n, GLuint* ids)
{
    GLsizei i;
    fbind_t b;
    if (n < 0 || (n && !ids)) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGenFramebuffers(n, ids);
    fb_push(&b, 0);
    for (i = 0; i < n; i++) gl31_glBindFramebuffer(GL_FRAMEBUFFER, ids[i]);
    fb_pop(&b);
}

void gl31_glNamedFramebufferRenderbuffer(GLuint fbo, GLenum att, GLenum rbt, GLuint rb)
{ WITH_FB(fbo); gl31_glFramebufferRenderbuffer(GL_FRAMEBUFFER, att, rbt, rb); END_FB(); }

void gl31_glNamedFramebufferTexture(GLuint fbo, GLenum att, GLuint tex, GLint level)
{
    WITH_FB(fbo);
    if (!tex) {
        gl31_glFramebufferTexture2D(GL_FRAMEBUFFER, att, GL_TEXTURE_2D, 0, level);
    } else {
        GLenum t = gl31_tex_target_of(tex);
        switch (t) {
            case GL_TEXTURE_2D: case GL_TEXTURE_RECTANGLE: case GL_TEXTURE_2D_MULTISAMPLE:
                gl31_glFramebufferTexture2D(GL_FRAMEBUFFER, att, t, tex, level); break;
            case GL_TEXTURE_1D:
                gl31_glFramebufferTexture1D(GL_FRAMEBUFFER, att, t, tex, level); break;
            case 0:
                gl31_set_error(GL_INVALID_OPERATION); break;
            default:                                    /* 3D, arrays, cube: adjunto "en capas" */
                gl31_glFramebufferTexture(GL_FRAMEBUFFER, att, tex, level);
        }
    }
    END_FB();
}

void gl31_glNamedFramebufferTextureLayer(GLuint fbo, GLenum att, GLuint tex, GLint level, GLint layer)
{ WITH_FB(fbo); gl31_glFramebufferTextureLayer(GL_FRAMEBUFFER, att, tex, level, layer); END_FB(); }
void gl31_glNamedFramebufferDrawBuffer(GLuint fbo, GLenum mode)
{ WITH_FB(fbo); gl31_glDrawBuffer(mode); END_FB(); }
void gl31_glNamedFramebufferDrawBuffers(GLuint fbo, GLsizei n, const GLenum* bufs)
{ WITH_FB(fbo); gl31_glDrawBuffers(n, bufs); END_FB(); }
void gl31_glNamedFramebufferReadBuffer(GLuint fbo, GLenum mode)
{ WITH_FB(fbo); gl31_glReadBuffer(mode); END_FB(); }
void gl31_glNamedFramebufferParameteri(GLuint fbo, GLenum pn, GLint v)
{ WITH_FB(fbo); gl31_glFramebufferParameteri(GL_FRAMEBUFFER, pn, v); END_FB(); }
void gl31_glGetNamedFramebufferParameteriv(GLuint fbo, GLenum pn, GLint* v)
{ WITH_FB(fbo); gl31_glGetFramebufferParameteriv(GL_FRAMEBUFFER, pn, v); END_FB(); }
void gl31_glGetNamedFramebufferAttachmentParameteriv(GLuint fbo, GLenum att, GLenum pn, GLint* v)
{ WITH_FB(fbo); gl31_glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, att, pn, v); END_FB(); }
GLenum gl31_glCheckNamedFramebufferStatus(GLuint fbo, GLenum target)
{
    GLenum r;
    WITH_FB(fbo);
    r = gl31_glCheckFramebufferStatus(target ? target : GL_FRAMEBUFFER);
    END_FB();
    return r;
}
void gl31_glInvalidateNamedFramebufferData(GLuint fbo, GLsizei n, const GLenum* att)
{ WITH_FB(fbo); gl31_glInvalidateFramebuffer(GL_FRAMEBUFFER, n, att); END_FB(); }
void gl31_glInvalidateNamedFramebufferSubData(GLuint fbo, GLsizei n, const GLenum* att, GLint x, GLint y, GLsizei w, GLsizei h)
{ WITH_FB(fbo); gl31_glInvalidateSubFramebuffer(GL_FRAMEBUFFER, n, att, x, y, w, h); END_FB(); }
void gl31_glClearNamedFramebufferiv(GLuint fbo, GLenum buf, GLint db, const GLint* v)
{ WITH_FB(fbo); gl31_glClearBufferiv(buf, db, v); END_FB(); }
void gl31_glClearNamedFramebufferuiv(GLuint fbo, GLenum buf, GLint db, const GLuint* v)
{ WITH_FB(fbo); gl31_glClearBufferuiv(buf, db, v); END_FB(); }
void gl31_glClearNamedFramebufferfv(GLuint fbo, GLenum buf, GLint db, const GLfloat* v)
{ WITH_FB(fbo); gl31_glClearBufferfv(buf, db, v); END_FB(); }
void gl31_glClearNamedFramebufferfi(GLuint fbo, GLenum buf, GLint db, GLfloat d, GLint s)
{ WITH_FB(fbo); gl31_glClearBufferfi(buf, db, d, s); END_FB(); }

void gl31_glBlitNamedFramebuffer(GLuint rfbo, GLuint dfbo, GLint sx0, GLint sy0, GLint sx1, GLint sy1,
                                 GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, GLenum filter)
{
    fbind_t b;
    gl31_state_t* s = gl31_state();
    b.draw = s->draw_fbo; b.read = s->read_fbo;
    gl31_glBindFramebuffer(GL_READ_FRAMEBUFFER, rfbo);
    gl31_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dfbo);
    gl31_glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
    fb_pop(&b);
}

/* ---- renderbuffers ---- */
void gl31_glCreateRenderbuffers(GLsizei n, GLuint* ids)
{
    GLsizei i;
    GLuint prev;
    if (n < 0 || (n && !ids)) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGenRenderbuffers(n, ids);
    prev = gl31_state()->renderbuffer;
    for (i = 0; i < n; i++) gl31_glBindRenderbuffer(GL_RENDERBUFFER, ids[i]);
    gl31_glBindRenderbuffer(GL_RENDERBUFFER, prev);
}

#define WITH_RB(rb) GLuint rbprev_ = gl31_state()->renderbuffer; \
                    if (!(rb)) { gl31_set_error(GL_INVALID_OPERATION); return; } \
                    gl31_glBindRenderbuffer(GL_RENDERBUFFER, (rb))
#define END_RB()    gl31_glBindRenderbuffer(GL_RENDERBUFFER, rbprev_)

void gl31_glNamedRenderbufferStorage(GLuint rb, GLenum f, GLsizei w, GLsizei h)
{ WITH_RB(rb); gl31_glRenderbufferStorage(GL_RENDERBUFFER, f, w, h); END_RB(); }
void gl31_glNamedRenderbufferStorageMultisample(GLuint rb, GLsizei s, GLenum f, GLsizei w, GLsizei h)
{ WITH_RB(rb); gl31_glRenderbufferStorageMultisample(GL_RENDERBUFFER, s, f, w, h); END_RB(); }
void gl31_glGetNamedRenderbufferParameteriv(GLuint rb, GLenum pn, GLint* v)
{ WITH_RB(rb); gl31_glGetRenderbufferParameteriv(GL_RENDERBUFFER, pn, v); END_RB(); }

/* =====================================================================
 * Vertex arrays
 * ===================================================================== */
#define WITH_VAO(vao) GLuint vaprev_ = gl31_state()->vao; \
                      if (!(vao)) { gl31_set_error(GL_INVALID_OPERATION); return; } \
                      gl31_glBindVertexArray(vao)
#define END_VAO()     gl31_glBindVertexArray(vaprev_)

void gl31_glCreateVertexArrays(GLsizei n, GLuint* ids)
{
    GLsizei i;
    GLuint prev;
    if (n < 0 || (n && !ids)) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGenVertexArrays(n, ids);
    prev = gl31_state()->vao;
    for (i = 0; i < n; i++) gl31_glBindVertexArray(ids[i]);
    gl31_glBindVertexArray(prev);
}

void gl31_glVertexArrayElementBuffer(GLuint vao, GLuint buffer)
{ WITH_VAO(vao); gl31_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer); END_VAO(); }
void gl31_glVertexArrayVertexBuffer(GLuint vao, GLuint binding, GLuint buffer, GLintptr off, GLsizei stride)
{ WITH_VAO(vao); gl31_glBindVertexBuffer(binding, buffer, off, stride); END_VAO(); }
void gl31_glVertexArrayVertexBuffers(GLuint vao, GLuint first, GLsizei count, const GLuint* b, const GLintptr* o, const GLsizei* s)
{ WITH_VAO(vao); gl31_glBindVertexBuffers(first, count, b, o, s); END_VAO(); }
void gl31_glVertexArrayAttribFormat(GLuint vao, GLuint idx, GLint size, GLenum type, GLboolean norm, GLuint reloff)
{ WITH_VAO(vao); gl31_glVertexAttribFormat(idx, size, type, norm, reloff); END_VAO(); }
void gl31_glVertexArrayAttribIFormat(GLuint vao, GLuint idx, GLint size, GLenum type, GLuint reloff)
{ WITH_VAO(vao); gl31_glVertexAttribIFormat(idx, size, type, reloff); END_VAO(); }
void gl31_glVertexArrayAttribLFormat(GLuint vao, GLuint idx, GLint size, GLenum type, GLuint reloff)
{ WITH_VAO(vao); gl31_glVertexAttribLFormat(idx, size, type, reloff); END_VAO(); }
void gl31_glVertexArrayAttribBinding(GLuint vao, GLuint idx, GLuint binding)
{ WITH_VAO(vao); gl31_glVertexAttribBinding(idx, binding); END_VAO(); }
void gl31_glVertexArrayBindingDivisor(GLuint vao, GLuint binding, GLuint divisor)
{ WITH_VAO(vao); gl31_glVertexBindingDivisor(binding, divisor); END_VAO(); }
void gl31_glEnableVertexArrayAttrib(GLuint vao, GLuint idx)
{ WITH_VAO(vao); gl31_glEnableVertexAttribArray(idx); END_VAO(); }
void gl31_glDisableVertexArrayAttrib(GLuint vao, GLuint idx)
{ WITH_VAO(vao); gl31_glDisableVertexAttribArray(idx); END_VAO(); }

void gl31_glGetVertexArrayiv(GLuint vao, GLenum pname, GLint* param)
{
    if (!param) return;
    if (pname != GL_ELEMENT_ARRAY_BUFFER_BINDING) { gl31_set_error(GL_INVALID_ENUM); return; }
    {
        WITH_VAO(vao);
        gl31_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, param);
        END_VAO();
    }
}

void gl31_glGetVertexArrayIndexediv(GLuint vao, GLuint idx, GLenum pname, GLint* param)
{ WITH_VAO(vao); gl31_glGetVertexAttribiv(idx, pname, param); END_VAO(); }

/* =====================================================================
 * Samplers, queries, pipelines, transform feedback
 * ===================================================================== */
void gl31_glCreateSamplers(GLsizei n, GLuint* ids)   { gl31_glGenSamplers(n, ids); }
void gl31_glCreateProgramPipelines(GLsizei n, GLuint* ids) { gl31_glGenProgramPipelines(n, ids); }
void gl31_glCreateQueries(GLenum target, GLsizei n, GLuint* ids)
{
    (void)target;       /* en ES el tipo lo fija glBeginQuery */
    gl31_glGenQueries(n, ids);
}

void gl31_glCreateTransformFeedbacks(GLsizei n, GLuint* ids)
{
    GLsizei i;
    GLint prev = 0;
    if (n < 0 || (n && !ids)) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGenTransformFeedbacks(n, ids);
    gl31_glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &prev);
    for (i = 0; i < n; i++) gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[i]);
    gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, (GLuint)prev);
}

void gl31_glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
    GLint prev = 0;
    if (!xfb) { gl31_set_error(GL_INVALID_OPERATION); return; }
    gl31_glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &prev);
    gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    gl31_glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
    gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, (GLuint)prev);
}

void gl31_glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr off, GLsizeiptr size)
{
    GLint prev = 0;
    if (!xfb) { gl31_set_error(GL_INVALID_OPERATION); return; }
    gl31_glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &prev);
    gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    gl31_glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer, off, size);
    gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, (GLuint)prev);
}

/* GL 4.5 robustness: este contexto nunca se pierde */
GLenum gl31_glGetGraphicsResetStatus(void) { return GL_NO_ERROR; }
