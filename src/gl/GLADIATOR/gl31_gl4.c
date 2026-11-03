/* gl31_gl4.c - resto de funciones de GL 4.0-4.3: tessellation, blend indexado, sample shading,
 * KHR_debug, viewport arrays (solo el viewport 0), invalidate, clear buffer, objetos de
 * transform feedback, compatibilidad ES2 y consultas. */
#include "gl31.h"
#include <stdint.h>
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
    /* es una pista de calidad ("al menos este porcentaje de muestras"): sin OES_sample_shading el
     * backend sombrea por pixel y el valor solo queda registrado para las consultas */
    value = value < 0.f ? 0.f : value > 1.f ? 1.f : value;
    gl31_state()->min_sample_shading = value;
    if (gl31_caps.sample_shading) BE(glMinSampleShading)(value);
}

/* ---------- blend por draw buffer (GL 4.0) ---------- */
static int blend_index_ok(GLuint buf)
{
    GLint max = 1;
    BE(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &max);
    if ((GLint)buf >= max) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

/* glBlend*i: nativo con draw_buffers_indexed; sin el, se guarda por buffer y se aplica el del primer buffer
 * activo (ver gl31_render_sync_blendfn) */
void gl31_glBlendEquationi(GLuint buf, GLenum mode) { gl31_glBlendEquationSeparatei(buf, mode, mode); }

void gl31_glBlendEquationSeparatei(GLuint buf, GLenum rgb, GLenum alpha)
{
    gl31_state_t* s = gl31_state();
    if (!blend_index_ok(buf)) return;
    if (gl31_caps.draw_buf_indexed) BE(glBlendEquationSeparatei)(buf, rgb, alpha);
    if (buf < GL31_MAX_DRAW_BUFFERS) { s->blend_f[buf].erg = rgb; s->blend_f[buf].ea = alpha; }
    if (!gl31_caps.draw_buf_indexed) gl31_render_sync_blendfn();
}

void gl31_glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor) { gl31_glBlendFuncSeparatei(buf, sfactor, dfactor, sfactor, dfactor); }

void gl31_glBlendFuncSeparatei(GLuint buf, GLenum srgb, GLenum drgb, GLenum salpha, GLenum dalpha)
{
    gl31_state_t* s = gl31_state();
    if (!blend_index_ok(buf)) return;
    if (gl31_caps.draw_buf_indexed) BE(glBlendFuncSeparatei)(buf, srgb, drgb, salpha, dalpha);
    if (buf < GL31_MAX_DRAW_BUFFERS) { s->blend_f[buf].srgb = srgb; s->blend_f[buf].drgb = drgb; s->blend_f[buf].sa = salpha; s->blend_f[buf].da = dalpha; }
    if (!gl31_caps.draw_buf_indexed) gl31_render_sync_blendfn();
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

/* ---------- viewport / scissor / depth range por indice ----------
 * ES solo tiene el viewport 0: ese se reenvia al backend. Los indices 1..15 se guardan (para que las
 * consultas devuelvan lo que la app fijo); solo se usarian al escribir gl_ViewportIndex desde un
 * geometry shader, que se rechaza si el backend no tiene geometry shaders. */
#define VP_MAX 16
static int vp_index_ok(GLuint i)
{
    if (i < VP_MAX) return 1;
    gl31_set_error(GL_INVALID_VALUE);
    return 0;
}

void gl31_glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
    gl31_state_t* s = gl31_state();
    if (!vp_index_ok(index)) return;
    if (w < 0.f || h < 0.f) { gl31_set_error(GL_INVALID_VALUE); return; }
    s->vpi[index][0] = x; s->vpi[index][1] = y; s->vpi[index][2] = w; s->vpi[index][3] = h;
    s->vpi_set[index] = GL_TRUE;
    if (index == 0) gl31_glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
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
    if ((uint64_t)first + (uint64_t)count > VP_MAX) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) gl31_glViewportIndexedfv(first + (GLuint)i, v + 4 * i);
}

void gl31_glScissorIndexed(GLuint index, GLint l, GLint b, GLsizei w, GLsizei h)
{
    gl31_state_t* s = gl31_state();
    if (!vp_index_ok(index)) return;
    if (w < 0 || h < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    s->sci[index][0] = l; s->sci[index][1] = b; s->sci[index][2] = w; s->sci[index][3] = h;
    s->sci_set[index] = GL_TRUE;
    if (index == 0) gl31_glScissor(l, b, w, h);
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
    if ((uint64_t)first + (uint64_t)count > VP_MAX) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) gl31_glScissorIndexedv(first + (GLuint)i, v + 4 * i);
}

void gl31_glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f)
{
    gl31_state_t* s = gl31_state();
    if (!vp_index_ok(index)) return;
    s->dri[index][0] = n; s->dri[index][1] = f;
    s->dri_set[index] = GL_TRUE;
    if (index == 0) gl31_glDepthRange(n, f);
}

void gl31_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble* v)
{
    GLsizei i;
    if (count < 0 || (count > 0 && !v)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if ((uint64_t)first + (uint64_t)count > VP_MAX) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) gl31_glDepthRangeIndexed(first + (GLuint)i, v[2 * i], v[2 * i + 1]);
}

/* consulta indexada: GL_VIEWPORT / GL_SCISSOR_BOX / GL_DEPTH_RANGE del indice; sin fijar = como el 0 */
void gl31_glGetFloati_v(GLenum target, GLuint index, GLfloat* data)
{
    gl31_state_t* s = gl31_state();
    if (!data) return;
    if (!vp_index_ok(index)) return;
    if (index != 0 && target == GL_VIEWPORT && s->vpi_set[index]) { memcpy(data, s->vpi[index], sizeof(GLfloat) * 4); return; }
    if (index != 0 && target == GL_SCISSOR_BOX && s->sci_set[index]) {
        int k; for (k = 0; k < 4; k++) data[k] = (GLfloat)s->sci[index][k];
        return;
    }
    if (index != 0 && target == GL_DEPTH_RANGE && s->dri_set[index]) {
        data[0] = (GLfloat)s->dri[index][0]; data[1] = (GLfloat)s->dri[index][1];
        return;
    }
    gl31_glGetFloatv(target, data);
}

void gl31_glGetDoublei_v(GLenum target, GLuint index, GLdouble* data)
{
    gl31_state_t* s = gl31_state();
    if (!data) return;
    if (!vp_index_ok(index)) return;
    if (index != 0 && target == GL_DEPTH_RANGE && s->dri_set[index]) { data[0] = s->dri[index][0]; data[1] = s->dri[index][1]; return; }
    if (index != 0 && (target == GL_VIEWPORT || target == GL_SCISSOR_BOX)) {
        GLfloat f[4]; int k;
        gl31_glGetFloati_v(target, index, f);
        for (k = 0; k < 4; k++) data[k] = f[k];
        return;
    }
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
    GLsizei i;
    gl31_state_t* s = gl31_state();
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        if (ids[i] && ids[i] == s->xfb_bound) s->xfb_bound = 0;     /* GL: pasa a enlazarse el objeto 0 */
        gl31_xfb_forget(ids[i]);
    }
    BE(glDeleteTransformFeedbacks)(n, ids);
}

void gl31_glBindTransformFeedback(GLenum target, GLuint id)
{
    gl31_state_t* s = gl31_state();
    if (target != GL_TRANSFORM_FEEDBACK) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (s->xfb_active && !s->xfb_paused) { gl31_set_error(GL_INVALID_OPERATION); return; }
    gl31_err_flush();
    BE(glBindTransformFeedback)(target, id);
    if (gl31_err_peek() == GL_NO_ERROR) s->xfb_bound = id;
}

GLboolean gl31_glIsTransformFeedback(GLuint id) { return id ? BE(glIsTransformFeedback)(id) : GL_FALSE; }

void gl31_glPauseTransformFeedback(void)
{
    gl31_state_t* s = gl31_state();
    if (!s->xfb_active || s->xfb_paused) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glPauseTransformFeedback)();
    s->xfb_paused = 1;
}

void gl31_glResumeTransformFeedback(void)
{
    gl31_state_t* s = gl31_state();
    if (!s->xfb_active || !s->xfb_paused) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glResumeTransformFeedback)();
    s->xfb_paused = 0;
}

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
    {   int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_);
        BE(glInvalidateFramebuffer)(rt_, n, attachments);
        gl31_fbo_redir_end(rd_, 0);
    }
}

void gl31_glInvalidateSubFramebuffer(GLenum target, GLsizei n, const GLenum* attachments,
                                     GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!fb_target_ok(target)) return;
    if (n < 0 || w < 0 || h < 0 || (n > 0 && !attachments)) { gl31_set_error(GL_INVALID_VALUE); return; }
    {   int rd_; GLenum rt_ = gl31_fbo_redir(target, &rd_);
        BE(glInvalidateSubFramebuffer)(rt_, n, attachments, x, y, w, h);
        gl31_fbo_redir_end(rd_, 0);
    }
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
/* El patron (format/type) se convierte al formato interno `ifmt` con que el buffer se interpreta.
 * Soporta los formatos de buffer de GL 4.3 (R/RG/RGB/RGBA de 8/16/32 bits, normalizados, enteros y
 * flotantes). El patron resultante (1 a 16 bytes) se repite sobre la region en la GPU. */
typedef struct { GLenum ifmt; int n; int bits; char kind; } cb_fmt_t;   /* kind: u unorm, s snorm, U uint, I int, f float */
static int cb_lookup(GLenum f, cb_fmt_t* o)
{
    static const cb_fmt_t t[] = {
        {0x8229,1,8,'u'},{0x822B,2,8,'u'},{0x8051,3,8,'u'},{0x8058,4,8,'u'},                       /* R8 RG8 RGB8 RGBA8 */
        {0x8F94,1,8,'s'},{0x8F95,2,8,'s'},{0x8F96,3,8,'s'},{0x8F97,4,8,'s'},                       /* *8_SNORM */
        {0x8232,1,8,'U'},{0x8238,2,8,'U'},{0x8D7D,3,8,'U'},{0x8D7C,4,8,'U'},                       /* R8UI .. */
        {0x8231,1,8,'I'},{0x8237,2,8,'I'},{0x8D8F,3,8,'I'},{0x8D8E,4,8,'I'},                       /* R8I .. */
        {0x822A,1,16,'u'},{0x822C,2,16,'u'},{0x8054,3,16,'u'},{0x805B,4,16,'u'},                   /* R16 .. RGBA16 */
        {0x8F98,1,16,'s'},{0x8F99,2,16,'s'},{0x8F9A,3,16,'s'},{0x8F9B,4,16,'s'},                   /* *16_SNORM */
        {0x8234,1,16,'U'},{0x823A,2,16,'U'},{0x8D77,3,16,'U'},{0x8D76,4,16,'U'},
        {0x8233,1,16,'I'},{0x8239,2,16,'I'},{0x8D89,3,16,'I'},{0x8D88,4,16,'I'},
        {0x822D,1,16,'f'},{0x822F,2,16,'f'},{0x881B,3,16,'f'},{0x881A,4,16,'f'},                   /* R16F .. */
        {0x8236,1,32,'U'},{0x823C,2,32,'U'},{0x8D71,3,32,'U'},{0x8D70,4,32,'U'},                   /* R32UI .. */
        {0x8235,1,32,'I'},{0x823B,2,32,'I'},{0x8D83,3,32,'I'},{0x8D82,4,32,'I'},
        {0x822E,1,32,'f'},{0x8230,2,32,'f'},{0x8815,3,32,'f'},{0x8814,4,32,'f'},                   /* R32F .. */
    };
    size_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) if (t[i].ifmt == f) { *o = t[i]; return 1; }
    return 0;
}

static uint16_t cb_f2h(float f)
{
    union { float f; uint32_t u; } v;
    uint32_t sign, e, m;
    int32_t exp;
    v.f = f;
    sign = (v.u >> 16) & 0x8000u; e = (v.u >> 23) & 0xFFu; m = v.u & 0x7FFFFFu;
    if (e == 0xFFu) return (uint16_t)(sign | 0x7C00u | (m ? 0x200u : 0));
    exp = (int32_t)e - 127 + 15;
    if (exp >= 31) return (uint16_t)(sign | 0x7C00u);
    if (exp <= 0) { if (exp < -10) return (uint16_t)sign; m = (m | 0x800000u) >> (1 - exp); return (uint16_t)(sign | ((m + 0x1000u) >> 13)); }
    return (uint16_t)(sign | ((uint32_t)exp << 10) | ((m + 0x1000u) >> 13));
}

static int cb_ncomp(GLenum format)
{
    switch (format) {
        case GL_RED: case 0x8D94: return 1;                 /* RED, RED_INTEGER */
        case GL_RG:  case 0x8228: return 2;                 /* RG, RG_INTEGER */
        case GL_RGB: case 0x8D98: return 3;                 /* RGB, RGB_INTEGER */
        case GL_RGBA: case 0x8D99: return 4;                /* RGBA, RGBA_INTEGER */
    }
    return 0;
}

static int clear_pattern(GLenum ifmt, GLenum format, GLenum type, const void* data, unsigned char pat[16], size_t* psize)
{
    cb_fmt_t f;
    double v[4] = {0, 0, 0, 1};
    int nc, k, integer_src;
    if (!cb_lookup(ifmt, &f)) return 0;
    *psize = (size_t)(f.n * f.bits / 8);
    if (!data) { memset(pat, 0, 16); return 1; }
    nc = cb_ncomp(format);
    if (!nc) return 0;
    integer_src = (format == 0x8D94 || format == 0x8228 || format == 0x8D98 || format == 0x8D99);
    for (k = 0; k < nc; k++) {
        const unsigned char* d = (const unsigned char*)data;
        switch (type) {
            case GL_UNSIGNED_BYTE:  v[k] = d[k]; if (!integer_src) v[k] /= 255.0; break;
            case GL_BYTE:           v[k] = (signed char)d[k]; if (!integer_src) { v[k] /= 127.0; if (v[k] < -1) v[k] = -1; } break;
            case GL_UNSIGNED_SHORT: { uint16_t x; memcpy(&x, d + 2 * k, 2); v[k] = x; if (!integer_src) v[k] /= 65535.0; break; }
            case GL_SHORT:          { int16_t x; memcpy(&x, d + 2 * k, 2); v[k] = x; if (!integer_src) { v[k] /= 32767.0; if (v[k] < -1) v[k] = -1; } break; }
            case GL_UNSIGNED_INT:   { uint32_t x; memcpy(&x, d + 4 * k, 4); v[k] = x; if (!integer_src) v[k] /= 4294967295.0; break; }
            case GL_INT:            { int32_t x; memcpy(&x, d + 4 * k, 4); v[k] = x; if (!integer_src) { v[k] /= 2147483647.0; if (v[k] < -1) v[k] = -1; } break; }
            case GL_FLOAT:          { float x; memcpy(&x, d + 4 * k, 4); v[k] = x; break; }
            default: return 0;
        }
    }
    if (nc < 4) v[3] = (f.kind == 'U' || f.kind == 'I') ? 1.0 : 1.0;
    memset(pat, 0, 16);
    for (k = 0; k < f.n; k++) {
        double x = v[k];
        unsigned char* o = pat + (size_t)k * (size_t)(f.bits / 8);
        if (f.kind == 'f') {
            if (f.bits == 16) { uint16_t h = cb_f2h((float)x); memcpy(o, &h, 2); }
            else { float fl = (float)x; memcpy(o, &fl, 4); }
        } else if (f.kind == 'u') {
            double m = f.bits == 8 ? 255.0 : 65535.0;
            double c = x < 0 ? 0 : x > 1 ? 1 : x;
            uint32_t q = (uint32_t)(c * m + 0.5);
            if (f.bits == 8) o[0] = (unsigned char)q; else { uint16_t h = (uint16_t)q; memcpy(o, &h, 2); }
        } else if (f.kind == 's') {
            double m = f.bits == 8 ? 127.0 : 32767.0;
            double c = x < -1 ? -1 : x > 1 ? 1 : x;
            int32_t q = (int32_t)(c * m + (c < 0 ? -0.5 : 0.5));
            if (f.bits == 8) { signed char sc = (signed char)q; memcpy(o, &sc, 1); } else { int16_t h = (int16_t)q; memcpy(o, &h, 2); }
        } else if (f.kind == 'U') {
            double mx = f.bits == 8 ? 255.0 : f.bits == 16 ? 65535.0 : 4294967295.0;
            double c = x < 0 ? 0 : x > mx ? mx : x;
            uint32_t q = (uint32_t)c;
            memcpy(o, &q, (size_t)(f.bits / 8));          /* little endian */
        } else {                                           /* 'I' */
            double lo = f.bits == 8 ? -128.0 : f.bits == 16 ? -32768.0 : -2147483648.0;
            double hi = f.bits == 8 ? 127.0 : f.bits == 16 ? 32767.0 : 2147483647.0;
            double c = x < lo ? lo : x > hi ? hi : x;
            int32_t q = (int32_t)c;
            memcpy(o, &q, (size_t)(f.bits / 8));
        }
    }
    return 1;
}

static void clear_buffer(GLenum target, GLenum ifmt, GLintptr offset, GLsizeiptr size, GLenum format,
                         GLenum type, const void* data, int whole)
{
    unsigned char pat[16];
    size_t ps = 4, i;
    unsigned char* tmp;
    GLint total = 0;
    if (!clear_pattern(ifmt, format, type, data, pat, &ps)) {
        gl31_set_error(GL_INVALID_ENUM);        /* ifmt/format/type que GL 4.3 no admite para buffers */
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

/* GL 4.2 -> glGetProgramResourceiv sobre la interfaz GL_ATOMIC_COUNTER_BUFFER de ES 3.1 */
void gl31_glGetActiveAtomicCounterBufferiv(GLuint program, GLuint index, GLenum pname, GLint* params)
{
    GLint nres = 0;
    GLenum prop;
    GLsizei len = 0;
    if (!params) return;
    if (!gl31_caps.es31) { gl31_set_error(GL_INVALID_OPERATION); return; }
    gl31_glGetProgramInterfaceiv(program, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &nres);
    if (gl31_err_peek() != GL_NO_ERROR) return;
    if ((GLint)index >= nres) { gl31_set_error(GL_INVALID_VALUE); return; }
    switch (pname) {
        case 0x92C1: prop = GL_BUFFER_BINDING; break;               /* ATOMIC_COUNTER_BUFFER_BINDING */
        case 0x92C4: prop = GL_BUFFER_DATA_SIZE; break;             /* ..._DATA_SIZE */
        case 0x92C5: prop = GL_NUM_ACTIVE_VARIABLES; break;         /* ..._ACTIVE_ATOMIC_COUNTERS */
        case 0x92C6: {                                              /* ..._ACTIVE_ATOMIC_COUNTER_INDICES */
            GLint n = 0;
            prop = GL_NUM_ACTIVE_VARIABLES;
            gl31_glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, index, 1, &prop, 1, &len, &n);
            if (n > 0) {
                prop = GL_ACTIVE_VARIABLES;
                gl31_glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, index, 1, &prop, n, &len, params);
            }
            return;
        }
        case 0x92C7: prop = GL_REFERENCED_BY_VERTEX_SHADER; break;
        case 0x92C8: prop = GL_REFERENCED_BY_TESS_CONTROL_SHADER; break;
        case 0x92C9: prop = GL_REFERENCED_BY_TESS_EVALUATION_SHADER; break;
        case 0x92CA: prop = GL_REFERENCED_BY_GEOMETRY_SHADER; break;
        case 0x92CB: prop = GL_REFERENCED_BY_FRAGMENT_SHADER; break;
        case 0x90ED: prop = GL_REFERENCED_BY_COMPUTE_SHADER; break;
        default: gl31_set_error(GL_INVALID_ENUM); return;
    }
    gl31_glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, index, 1, &prop, 1, &len, params);
}
