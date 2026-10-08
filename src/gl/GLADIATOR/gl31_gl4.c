/* gl31_gl4.c - resto de funciones de GL 4.0-4.3: tessellation, blend indexado, sample shading,
 * KHR_debug, viewport arrays (solo el viewport 0), invalidate, clear buffer, objetos de
 * transform feedback, compatibilidad ES2 y consultas. */
#include "gl31.h"
#include <stdlib.h>
#include <string.h>

#ifndef GL_PATCH_DEFAULT_INNER_LEVEL
#define GL_PATCH_DEFAULT_INNER_LEVEL 0x8E73
#define GL_PATCH_DEFAULT_OUTER_LEVEL 0x8E74
#endif
#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif
#ifndef GL_INTERNALFORMAT_SUPPORTED
#define GL_INTERNALFORMAT_SUPPORTED 0x826F
#endif
#ifndef GL_NUM_SAMPLE_COUNTS
#define GL_NUM_SAMPLE_COUNTS 0x9380
#endif
#ifndef GL_DEBUG_TYPE_ERROR
#define GL_DEBUG_TYPE_ERROR 0x824C
#endif

/* ---------- tessellation (GL 4.0) ---------- */
void gl31_glPatchParameteri(GLenum pname, GLint value)
{
    if (pname != GL_PATCH_VERTICES) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (value < 1) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!gl31_caps.tess) {
        gl31_stub_warn("glPatchParameteri (backend sin tessellation)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    BE(glPatchParameteri)(pname, value);
}

/* los niveles por defecto (sin TCS) no existen en ES: hace falta un TCS que los escriba */
void gl31_glPatchParameterfv(GLenum pname, const GLfloat* values)
{
    if (pname != GL_PATCH_DEFAULT_OUTER_LEVEL && pname != GL_PATCH_DEFAULT_INNER_LEVEL) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (!values) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_stub_warn("glPatchParameterfv (niveles por defecto: usar un tessellation control shader)");
}

/* ---------- sample shading ---------- */
void gl31_glMinSampleShading(GLfloat value)
{
    if (!gl31_caps.sample_shading) {
        gl31_stub_warn("glMinSampleShading (backend sin OES_sample_shading / ES 3.2)");
        return;                                  /* es una pista de calidad: se ignora */
    }
    BE(glMinSampleShading)(value < 0.f ? 0.f : value > 1.f ? 1.f : value);
}

/* ---------- blend por draw buffer (GL 4.0) ---------- */
static int blend_index_ok(GLuint buf)
{
    GLint max = 1;
    BE(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &max);
    if ((GLint)buf >= max) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

/* sin draw_buffers_indexed: el buffer 0 usa el estado global; los demas no se pueden fijar aparte */
static int blend_native_or_global(GLuint buf, const char* fn)
{
    if (!blend_index_ok(buf)) return -1;
    if (gl31_caps.draw_buf_indexed) return 1;
    if (buf == 0) return 0;
    gl31_stub_warn(fn);
    return -1;
}

void gl31_glBlendEquationi(GLuint buf, GLenum mode)
{
    int r = blend_native_or_global(buf, "glBlendEquationi (backend sin draw_buffers_indexed)");
    if (r > 0) BE(glBlendEquationi)(buf, mode);
    else if (r == 0) gl31_glBlendEquation(mode);
}

void gl31_glBlendEquationSeparatei(GLuint buf, GLenum rgb, GLenum alpha)
{
    int r = blend_native_or_global(buf, "glBlendEquationSeparatei (backend sin draw_buffers_indexed)");
    if (r > 0) BE(glBlendEquationSeparatei)(buf, rgb, alpha);
    else if (r == 0) gl31_glBlendEquationSeparate(rgb, alpha);
}

void gl31_glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor)
{
    int r = blend_native_or_global(buf, "glBlendFunci (backend sin draw_buffers_indexed)");
    if (r > 0) BE(glBlendFunci)(buf, sfactor, dfactor);
    else if (r == 0) gl31_glBlendFunc(sfactor, dfactor);
}

void gl31_glBlendFuncSeparatei(GLuint buf, GLenum srgb, GLenum drgb, GLenum salpha, GLenum dalpha)
{
    int r = blend_native_or_global(buf, "glBlendFuncSeparatei (backend sin draw_buffers_indexed)");
    if (r > 0) BE(glBlendFuncSeparatei)(buf, srgb, drgb, salpha, dalpha);
    else if (r == 0) gl31_glBlendFuncSeparate(srgb, drgb, salpha, dalpha);
}

/* ---------- KHR_debug ---------- */
static GLDEBUGPROC g_dbg_cb;
static const void* g_dbg_user;

void gl31_glDebugMessageCallback(GLDEBUGPROC callback, const void* user)
{
    g_dbg_cb = callback; g_dbg_user = user;
    if (gl31_caps.debug) BE(glDebugMessageCallback)(callback, user);
}

void gl31_glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count,
                                const GLuint* ids, GLboolean enabled)
{
    if (count < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_caps.debug) BE(glDebugMessageControl)(source, type, severity, count, ids, enabled);
}

void gl31_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                               const GLchar* buf)
{
    if (!buf) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_caps.debug) { BE(glDebugMessageInsert)(source, type, id, severity, length, buf); return; }
    {   /* sin backend: se entrega directamente a la callback de la app */
        GLboolean on = GL_FALSE;
        gl31_soft_cap_get(GL_DEBUG_OUTPUT, &on);
        if (on && g_dbg_cb) g_dbg_cb(source, type, id, severity, length < 0 ? (GLsizei)strlen(buf) : length, buf, g_dbg_user);
    }
}

void gl31_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message)
{
    if (gl31_caps.debug) BE(glPushDebugGroup)(source, id, length, message);
}

void gl31_glPopDebugGroup(void)
{
    if (gl31_caps.debug) BE(glPopDebugGroup)();
}

void gl31_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label)
{
    if (gl31_caps.debug) BE(glObjectLabel)(identifier, name, length, label);
}

void gl31_glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label)
{
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_caps.debug) { BE(glGetObjectLabel)(identifier, name, bufSize, length, label); return; }
    if (length) *length = 0;
    if (bufSize > 0 && label) label[0] = 0;
}

GLuint gl31_glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids,
                                 GLenum* severities, GLsizei* lengths, GLchar* messageLog)
{
    (void)count; (void)bufSize; (void)sources; (void)types; (void)ids; (void)severities; (void)lengths; (void)messageLog;
    return 0;
}

/* ---------- viewport / scissor / depth range por indice: ES solo tiene el 0 ---------- */
static int vp_index_ok(GLuint i)
{
    if (i == 0) return 1;
    gl31_stub_warn("viewports / scissors con indice > 0 (ES solo tiene uno)");
    gl31_set_error(GL_INVALID_VALUE);
    return 0;
}

void gl31_glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
    if (!vp_index_ok(index)) return;
    if (w < 0.f || h < 0.f) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
}

void gl31_glViewportIndexedfv(GLuint index, const GLfloat* v)
{
    if (!v) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glViewportIndexedf(index, v[0], v[1], v[2], v[3]);
}

void gl31_glViewportArrayv(GLuint first, GLsizei count, const GLfloat* v)
{
    GLsizei i;
    if (count < 0 || (count > 0 && !v)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) {
        gl31_glViewportIndexedfv(first + (GLuint)i, v + 4 * i);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}

void gl31_glScissorIndexed(GLuint index, GLint l, GLint b, GLsizei w, GLsizei h)
{
    if (!vp_index_ok(index)) return;
    gl31_glScissor(l, b, w, h);
}

void gl31_glScissorIndexedv(GLuint index, const GLint* v)
{
    if (!v) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glScissorIndexed(index, v[0], v[1], v[2], v[3]);
}

void gl31_glScissorArrayv(GLuint first, GLsizei count, const GLint* v)
{
    GLsizei i;
    if (count < 0 || (count > 0 && !v)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) {
        gl31_glScissorIndexedv(first + (GLuint)i, v + 4 * i);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}

void gl31_glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f)
{
    if (!vp_index_ok(index)) return;
    gl31_glDepthRange(n, f);
}

void gl31_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble* v)
{
    GLsizei i;
    if (count < 0 || (count > 0 && !v)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) {
        gl31_glDepthRangeIndexed(first + (GLuint)i, v[2 * i], v[2 * i + 1]);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}

void gl31_glGetFloati_v(GLenum target, GLuint index, GLfloat* data)
{
    if (!data) return;
    if (index != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGetFloatv(target, data);
}

void gl31_glGetDoublei_v(GLenum target, GLuint index, GLdouble* data)
{
    if (!data) return;
    if (index != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glGetDoublev(target, data);
}

/* ---------- compatibilidad ES2 (ARB_ES2_compatibility, GL 4.1) ---------- */
void gl31_glDepthRangef(GLfloat n, GLfloat f) { gl31_glDepthRange(n, f); }
void gl31_glClearDepthf(GLfloat d) { gl31_glClearDepth(d); }

void gl31_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
    if (!range || !precision) return;
    if (shadertype != GL_VERTEX_SHADER && shadertype != GL_FRAGMENT_SHADER) { gl31_set_error(GL_INVALID_ENUM); return; }
    BE(glGetShaderPrecisionFormat)(shadertype, precisiontype, range, precision);
}

void gl31_glReleaseShaderCompiler(void) { BE(glReleaseShaderCompiler)(); }

/* ---------- objetos de transform feedback (GL 4.0; nativos en ES 3.0) ---------- */
void gl31_glGenTransformFeedbacks(GLsizei n, GLuint* ids)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenTransformFeedbacks)(n, ids);
}

void gl31_glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glDeleteTransformFeedbacks)(n, ids);
}

void gl31_glBindTransformFeedback(GLenum target, GLuint id)
{
    if (target != GL_TRANSFORM_FEEDBACK) { gl31_set_error(GL_INVALID_ENUM); return; }
    BE(glBindTransformFeedback)(target, id);
}

GLboolean gl31_glIsTransformFeedback(GLuint id) { return id ? BE(glIsTransformFeedback)(id) : GL_FALSE; }
void gl31_glPauseTransformFeedback(void) { BE(glPauseTransformFeedback)(); }
void gl31_glResumeTransformFeedback(void) { BE(glResumeTransformFeedback)(); }

/* ---------- invalidate (GL 4.3) ---------- */
static int fb_target_ok(GLenum t)
{
    if (t == GL_FRAMEBUFFER || t == GL_DRAW_FRAMEBUFFER || t == GL_READ_FRAMEBUFFER) return 1;
    gl31_set_error(GL_INVALID_ENUM);
    return 0;
}

void gl31_glInvalidateFramebuffer(GLenum target, GLsizei n, const GLenum* attachments)
{
    if (!fb_target_ok(target)) return;
    if (n < 0 || (n > 0 && !attachments)) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glInvalidateFramebuffer)(target, n, attachments);
}

void gl31_glInvalidateSubFramebuffer(GLenum target, GLsizei n, const GLenum* attachments,
                                     GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!fb_target_ok(target)) return;
    if (n < 0 || w < 0 || h < 0 || (n > 0 && !attachments)) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glInvalidateSubFramebuffer)(target, n, attachments, x, y, w, h);
}

/* pistas de rendimiento: sin equivalente en ES y sin efecto observable */
void gl31_glInvalidateTexImage(GLuint texture, GLint level) { (void)texture; (void)level; }
void gl31_glInvalidateTexSubImage(GLuint texture, GLint level, GLint xo, GLint yo, GLint zo,
                                  GLsizei w, GLsizei h, GLsizei d)
{ (void)texture; (void)level; (void)xo; (void)yo; (void)zo; (void)w; (void)h; (void)d; }
void gl31_glInvalidateBufferData(GLuint buffer) { (void)buffer; }
void gl31_glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length)
{ (void)buffer; (void)offset; (void)length; }

/* ---------- glClearBufferData / SubData (GL 4.3) ---------- */
/* Soporta el relleno con cero (data == NULL) y patrones de un componente de 32 bits
 * (R32F / R32I / R32UI). El resto de formatos devuelve GL_INVALID_OPERATION. */
static int clear_pattern(GLenum ifmt, GLenum format, GLenum type, const void* data, unsigned char pat[4], size_t* psize)
{
    if (!data) { memset(pat, 0, 4); *psize = 4; return 1; }
    (void)format;
    if ((ifmt == GL_R32F || ifmt == GL_R32I || ifmt == GL_R32UI) &&
        (type == GL_FLOAT || type == GL_INT || type == GL_UNSIGNED_INT)) {
        memcpy(pat, data, 4);
        *psize = 4;
        return 1;
    }
    return 0;
}

static void clear_buffer(GLenum target, GLenum ifmt, GLintptr offset, GLsizeiptr size, GLenum format,
                         GLenum type, const void* data, int whole)
{
    unsigned char pat[4];
    size_t ps = 4, i;
    unsigned char* tmp;
    GLint total = 0;
    if (!clear_pattern(ifmt, format, type, data, pat, &ps)) {
        gl31_stub_warn("glClearBufferData/SubData (solo cero o R32F/R32I/R32UI)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    BE(glGetBufferParameteriv)(target, GL_BUFFER_SIZE, &total);
    if (whole) { offset = 0; size = total; }
    if (offset < 0 || size < 0 || offset + size > (GLsizeiptr)total ||
        (size_t)offset % ps || (size_t)size % ps) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (!size) return;
    tmp = (unsigned char*)malloc((size_t)size);
    if (!tmp) { gl31_set_error(GL_OUT_OF_MEMORY); return; }
    for (i = 0; i < (size_t)size; i++) tmp[i] = pat[i % ps];
    BE(glBufferSubData)(target, offset, size, tmp);
    free(tmp);
}

void gl31_glClearBufferData(GLenum target, GLenum ifmt, GLenum format, GLenum type, const void* data)
{
    clear_buffer(target, ifmt, 0, 0, format, type, data, 1);
}

void gl31_glClearBufferSubData(GLenum target, GLenum ifmt, GLintptr offset, GLsizeiptr size, GLenum format,
                               GLenum type, const void* data)
{
    clear_buffer(target, ifmt, offset, size, format, type, data, 0);
}

/* ---------- consultas ---------- */
void gl31_glGetInternalformativ(GLenum target, GLenum ifmt, GLenum pname, GLsizei bufSize, GLint* params)
{
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!bufSize || !params) return;
    switch (pname) {
        case GL_SAMPLES:
        case GL_NUM_SAMPLE_COUNTS:
            if (target == GL_RENDERBUFFER || target == GL_TEXTURE_2D_MULTISAMPLE ||
                target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
                BE(glGetInternalformativ)(target, ifmt, pname, bufSize, params);
                return;
            }
            params[0] = pname == GL_NUM_SAMPLE_COUNTS ? 0 : 0;
            return;
        case GL_INTERNALFORMAT_SUPPORTED:
            params[0] = GL_TRUE;               /* mejor esfuerzo: el backend decide al crear la textura */
            return;
        default:
            params[0] = 0;                     /* el resto de propiedades de GL 4.3 no existen en ES */
    }
}

void gl31_glGetActiveAtomicCounterBufferiv(GLuint program, GLuint index, GLenum pname, GLint* params)
{
    (void)program; (void)index; (void)pname; (void)params;
    gl31_stub_warn("glGetActiveAtomicCounterBufferiv (usar glGetProgramResourceiv)");
    gl31_set_error(GL_INVALID_OPERATION);
}
