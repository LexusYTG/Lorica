/* gl31_render.c - clear, viewport y estado de rasterizacion/fragmento.
 *
 * Ademas de reenviar al backend validando argumentos, aqui viven los
 * `glEnable` de capacidades de OpenGL de escritorio que GLES no tiene
 * (GL_MULTISAMPLE, GL_LINE_SMOOTH, GL_PROGRAM_POINT_SIZE, ...): se recuerdan
 * en el estado propio para que Enable/IsEnabled/Get* sean coherentes en vez
 * de acabar en GL_INVALID_ENUM del backend. */
#include "gl31.h"
#include "gl31_vk.h"
#include <string.h>

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_LINE_SMOOTH
#define GL_LINE_SMOOTH 0x0B20
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_LINE_SMOOTH_HINT 0x0C52
#define GL_POLYGON_SMOOTH_HINT 0x0C53
#endif
#ifndef GL_SAMPLE_ALPHA_TO_ONE
#define GL_SAMPLE_ALPHA_TO_ONE 0x809F
#endif
#ifndef GL_COLOR_LOGIC_OP
#define GL_COLOR_LOGIC_OP 0x0BF2
#endif
#ifndef GL_POLYGON_OFFSET_POINT
#define GL_POLYGON_OFFSET_POINT 0x2A01
#define GL_POLYGON_OFFSET_LINE  0x2A02
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif
#ifndef GL_CLIP_DISTANCE0
#define GL_CLIP_DISTANCE0 0x3000
#endif
#ifndef GL_TEXTURE_COMPRESSION_HINT
#define GL_TEXTURE_COMPRESSION_HINT 0x84EF
#endif
#ifndef GL_UPPER_LEFT
#define GL_UPPER_LEFT 0x8CA2
#define GL_LOWER_LEFT 0x8CA1
#endif
#ifndef GL_POINT_FADE_THRESHOLD_SIZE
#define GL_POINT_FADE_THRESHOLD_SIZE 0x8128
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
#endif

/* ---------- caps de desktop sin equivalente en GLES ---------- */
typedef struct { GLenum cap; GLboolean def; const char* warn; } soft_cap_t;

/* warn != NULL: activarlo no tiene efecto real y se avisa una vez */
static const soft_cap_t k_soft[] = {
    { GL_MULTISAMPLE,           GL_TRUE,  NULL },
    { GL_SAMPLE_SHADING,        GL_FALSE, NULL },     /* se reenvia ademas si el backend lo tiene */
    { GL_LINE_SMOOTH,           GL_FALSE, NULL },
    { GL_POLYGON_SMOOTH,        GL_FALSE, NULL },
    { GL_SAMPLE_ALPHA_TO_ONE,   GL_FALSE, NULL },
    { GL_COLOR_LOGIC_OP,        GL_FALSE, "glEnable(GL_COLOR_LOGIC_OP)" },
    { GL_POLYGON_OFFSET_POINT,  GL_FALSE, NULL },
    { GL_POLYGON_OFFSET_LINE,   GL_FALSE, NULL },
    { GL_PROGRAM_POINT_SIZE,    GL_FALSE, NULL },
    { GL_FRAMEBUFFER_SRGB,      GL_FALSE, NULL },
    { GL_DEBUG_OUTPUT,          GL_FALSE, NULL },
    { GL_DEBUG_OUTPUT_SYNCHRONOUS, GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 0,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 1,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 2,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 3,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 4,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 5,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 6,    GL_FALSE, NULL },
    { GL_CLIP_DISTANCE0 + 7,    GL_FALSE, NULL },
};
#define N_SOFT ((int)(sizeof k_soft / sizeof k_soft[0]))

static int soft_index(GLenum cap)
{
    int i;
    for (i = 0; i < N_SOFT; i++) if (k_soft[i].cap == cap) return i;
    return -1;
}

/* caps que alimentan los uniforms propios: al cambiar hay que resubirlos */
static int lu_cap(GLenum cap)
{
    return cap == GL_PROGRAM_POINT_SIZE || (cap >= GL_CLIP_DISTANCE0 && cap < GL_CLIP_DISTANCE0 + 8) ||
           (cap == GL_DEPTH_CLAMP && !gl31_caps.depth_clamp);
}

void gl31_render_state_defaults(gl31_state_t* s)
{
    int i, k;
    for (i = 0; i < N_SOFT; i++) s->softcap[i] = k_soft[i].def;
    for (i = 0; i < 8; i++) for (k = 0; k < 4; k++) s->colormask[i][k] = GL_TRUE;
    for (i = 0; i < GL31_MAX_DRAW_BUFFERS; i++) {
        s->blend_f[i].srgb = s->blend_f[i].sa = GL_ONE;
        s->blend_f[i].drgb = s->blend_f[i].da = GL_ZERO;
        s->blend_f[i].erg = s->blend_f[i].ea = GL_FUNC_ADD;
    }
    s->pack_alignment = 4;
    s->clamp_read_color = GL_FIXED_ONLY;
    s->clamp_vertex_color = GL_TRUE;
    s->clamp_fragment_color = GL_FIXED_ONLY;
    s->unpack_alignment = 4;
    s->provoking_vertex = GL_LAST_VERTEX_CONVENTION;
    s->point_size = 1.0f;
    s->point_fade_threshold = 1.0f;
    s->point_sprite_origin = GL_UPPER_LEFT;
    s->clip_origin = GL_LOWER_LEFT;
    s->clip_depth = GL_NEGATIVE_ONE_TO_ONE;
    s->front_face_app = GL_CCW;
    s->polygon_mode = GL_FILL;
}

/* 1 = es una cap "blanda" y se sirvio aqui */
int gl31_soft_cap_get(GLenum cap, GLboolean* out)
{
    int i = soft_index(cap);
    if (i < 0) return 0;
    *out = gl31_state()->softcap[i];
    return 1;
}

void gl31_glEnable(GLenum cap)
{
    gl31_state_t* s = gl31_state();
    int i;
    if (cap == GL_PRIMITIVE_RESTART) { s->prim_restart = GL_TRUE; return; }
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (cap == GL_DEPTH_CLAMP && !gl31_caps.depth_clamp) { s->depth_clamp_soft = GL_TRUE; gl31_lu_bump(); return; }
    if ((i = soft_index(cap)) >= 0) {
        s->softcap[i] = GL_TRUE;
        if (lu_cap(cap)) gl31_lu_bump();
        if (k_soft[i].warn) gl31_stub_warn(k_soft[i].warn);
        if (gl31_caps.debug && (cap == GL_DEBUG_OUTPUT || cap == GL_DEBUG_OUTPUT_SYNCHRONOUS)) BE(glEnable)(cap);
        if (cap == GL_SAMPLE_SHADING && gl31_caps.sample_shading) BE(glEnable)(cap);
        return;
    }
    if (cap == GL_BLEND) { int k; for (k = 0; k < GL31_MAX_DRAW_BUFFERS; k++) s->blend_en[k] = GL_TRUE; }
    BE(glEnable)(cap);
}

void gl31_glDisable(GLenum cap)
{
    gl31_state_t* s = gl31_state();
    int i;
    if (cap == GL_PRIMITIVE_RESTART) { s->prim_restart = GL_FALSE; return; }
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (cap == GL_DEPTH_CLAMP && !gl31_caps.depth_clamp) { s->depth_clamp_soft = GL_FALSE; gl31_lu_bump(); return; }
    if ((i = soft_index(cap)) >= 0) {
        s->softcap[i] = GL_FALSE;
        if (lu_cap(cap)) gl31_lu_bump();
        if (gl31_caps.debug && (cap == GL_DEBUG_OUTPUT || cap == GL_DEBUG_OUTPUT_SYNCHRONOUS)) BE(glDisable)(cap);
        if (cap == GL_SAMPLE_SHADING && gl31_caps.sample_shading) BE(glDisable)(cap);
        return;
    }
    if (cap == GL_BLEND) { int k; for (k = 0; k < GL31_MAX_DRAW_BUFFERS; k++) s->blend_en[k] = GL_FALSE; }
    BE(glDisable)(cap);
}

GLboolean gl31_glIsEnabled(GLenum cap)
{
    gl31_state_t* s = gl31_state();
    int i;
    if (cap == GL_PRIMITIVE_RESTART) return s->prim_restart;
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX) { gl31_set_error(GL_INVALID_ENUM); return GL_FALSE; }
    if (cap == GL_DEPTH_CLAMP && !gl31_caps.depth_clamp) return s->depth_clamp_soft;
    if ((i = soft_index(cap)) >= 0) return s->softcap[i];
    return BE(glIsEnabled)(cap);
}

/* ---------- estado indexado por draw buffer (GL 3.0) ---------- */
static GLint max_draw_buffers(void)
{
    GLint m = 1;
    BE(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &m);
    if (m < 1) m = 1;
    if (m > 8) m = 8;
    return m;
}

/* 1 = valido. Indexables: GL_BLEND (por draw buffer) y GL_SCISSOR_TEST (por viewport, GL 4.1). */
static int indexed_ok(GLenum cap, GLuint index)
{
    if (cap == GL_SCISSOR_TEST) {
        if (index >= 16) { gl31_set_error(GL_INVALID_VALUE); return 0; }
        return 1;
    }
    if (cap != GL_BLEND) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    if ((GLint)index >= max_draw_buffers()) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

/* buffers de dibujo activos (los que no son GL_NONE) en este momento */
static unsigned active_draw_mask(int n)
{
    unsigned m = 0; int i;
    for (i = 0; i < n && i < 32; i++) {
        GLint v = 0;
        BE(glGetIntegerv)(GL_DRAW_BUFFER0 + i, &v);
        if (v != (GLint)GL_NONE) m |= 1u << i;
    }
    return m;
}

/* El backend sin draw_buffers_indexed tiene un unico GL_BLEND. Manda el del buffer 0; si algun otro buffer
 * activo pide otro valor, ES no puede expresarlo y se avisa una vez. */
void gl31_render_sync_blend(void)
{
    gl31_state_t* s = gl31_state();
    int i, ref = 0, n = max_draw_buffers();
    unsigned act;
    if (n > GL31_MAX_DRAW_BUFFERS) n = GL31_MAX_DRAW_BUFFERS;
    act = active_draw_mask(n);
    s->idx_div &= ~1u;
    for (i = 1; i < n; i++) if (s->blend_en[i] != s->blend_en[0]) { s->idx_div |= 1u; break; }
    for (i = 0; i < n; i++) if (act & (1u << i)) { ref = i; break; }     /* manda el primer buffer activo */
    if (s->blend_en[ref]) BE(glEnable)(GL_BLEND); else BE(glDisable)(GL_BLEND);
    for (i = ref + 1; i < n; i++)
        if ((act & (1u << i)) && s->blend_en[i] != s->blend_en[ref]) {
            gl31_stub_warn("glEnablei(GL_BLEND) distinto por buffer (backend sin draw_buffers_indexed)");
            break;
        }
}

/* tras cambiar los buffers de dibujo (glDrawBuffers / glBindFramebuffer) hay que reaplicar el estado por
 * buffer que el backend sin draw_buffers_indexed solo puede tener en una copia global */
void gl31_render_resync_indexed(void)
{
    gl31_state_t* s = gl31_state();
    if (gl31_caps.draw_buf_indexed || !s->idx_div) return;
    if (s->idx_div & 1u) gl31_render_sync_blend();
    if (s->idx_div & 2u) gl31_render_sync_blendfn();
    if (s->idx_div & 4u) {
        int i, ref = 0, n = max_draw_buffers();
        unsigned act;
        if (n > GL31_MAX_DRAW_BUFFERS) n = GL31_MAX_DRAW_BUFFERS;
        act = active_draw_mask(n);
        for (i = 0; i < n; i++) if (act & (1u << i)) { ref = i; break; }
        BE(glColorMask)(s->colormask[ref][0], s->colormask[ref][1], s->colormask[ref][2], s->colormask[ref][3]);
    }
}

/* GL_BLEND sin draw_buffers_indexed: si los demas draw buffers tienen el mismo estado se usa el global;
 * si difieren, el estado se guarda por indice y glDraw* lo reconcilia con el que haya activo
 * (ver gl31_render_apply_blend). Aqui solo se registra. */
void gl31_glEnablei(GLenum cap, GLuint index)
{
    gl31_state_t* s = gl31_state();
    if (!indexed_ok(cap, index)) return;
    if (cap == GL_SCISSOR_TEST) {
        s->sci_enable[index] = GL_TRUE;
        if (index == 0) BE(glEnable)(GL_SCISSOR_TEST);
        return;
    }
    if (gl31_caps.draw_buf_indexed) { BE(glEnablei)(cap, index); return; }
    s->blend_en[index] = GL_TRUE;
    gl31_render_sync_blend();
}

void gl31_glDisablei(GLenum cap, GLuint index)
{
    gl31_state_t* s = gl31_state();
    if (!indexed_ok(cap, index)) return;
    if (cap == GL_SCISSOR_TEST) {
        s->sci_enable[index] = GL_FALSE;
        if (index == 0) BE(glDisable)(GL_SCISSOR_TEST);
        return;
    }
    if (gl31_caps.draw_buf_indexed) { BE(glDisablei)(cap, index); return; }
    s->blend_en[index] = GL_FALSE;
    gl31_render_sync_blend();
}

GLboolean gl31_glIsEnabledi(GLenum cap, GLuint index)
{
    gl31_state_t* s = gl31_state();
    if (!indexed_ok(cap, index)) return GL_FALSE;
    if (cap == GL_SCISSOR_TEST) return index == 0 ? BE(glIsEnabled)(GL_SCISSOR_TEST) : s->sci_enable[index];
    if (gl31_caps.draw_buf_indexed) return BE(glIsEnabledi)(cap, index);
    return s->blend_en[index];
}

void gl31_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    gl31_state_t* s = gl31_state();
    int i;
    for (i = 0; i < 8; i++) {
        s->colormask[i][0] = r; s->colormask[i][1] = g;
        s->colormask[i][2] = b; s->colormask[i][3] = a;
    }
    BE(glColorMask)(r, g, b, a);
}

void gl31_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    gl31_state_t* s = gl31_state();
    if ((GLint)index >= max_draw_buffers()) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (gl31_caps.draw_buf_indexed) {
        BE(glColorMaski)(index, r, g, b, a);
    } else {
        /* un unico ColorMask global: manda el del primer buffer activo; si otro buffer activo difiere,
         * ES no lo puede expresar */
        int i, ref = 0, n = max_draw_buffers();
        unsigned act;
        s->colormask[index][0] = r; s->colormask[index][1] = g;
        s->colormask[index][2] = b; s->colormask[index][3] = a;
        if (n > GL31_MAX_DRAW_BUFFERS) n = GL31_MAX_DRAW_BUFFERS;
        act = active_draw_mask(n);
        s->idx_div &= ~4u;
        for (i = 1; i < n; i++) if (memcmp(s->colormask[i], s->colormask[0], 4) != 0) { s->idx_div |= 4u; break; }
        for (i = 0; i < n; i++) if (act & (1u << i)) { ref = i; break; }
        BE(glColorMask)(s->colormask[ref][0], s->colormask[ref][1], s->colormask[ref][2], s->colormask[ref][3]);
        for (i = ref + 1; i < n; i++)
            if ((act & (1u << i)) && memcmp(s->colormask[i], s->colormask[ref], 4) != 0) {
                gl31_stub_warn("glColorMaski distinto por buffer (backend sin draw_buffers_indexed)");
                break;
            }
        return;
    }
    s->colormask[index][0] = r; s->colormask[index][1] = g;
    s->colormask[index][2] = b; s->colormask[index][3] = a;
}

void gl31_glGetBooleani_v(GLenum target, GLuint index, GLboolean* data)
{
    gl31_state_t* s = gl31_state();
    int k;
    if (!data) return;
    if (target != GL_COLOR_WRITEMASK) { gl31_set_error(GL_INVALID_ENUM); return; }
    if ((GLint)index >= max_draw_buffers()) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (k = 0; k < 4; k++) data[k] = s->colormask[index][k];
}

/* ---------- clear / viewport ---------- */
void gl31_glClear(GLbitfield mask)
{
    if (mask & ~(GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (gl31_cond_skip()) return;
    BE(glClear)(mask);
}

void gl31_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { BE(glClearColor)(r, g, b, a); }
void gl31_glClearStencil(GLint s) { BE(glClearStencil)(s); }

void gl31_glClearDepth(GLdouble depth)
{
    BE(glClearDepthf)((GLfloat)(depth < 0.0 ? 0.0 : depth > 1.0 ? 1.0 : depth));
}

void gl31_glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (w < 0 || h < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glViewport)(x, y, w, h);
}

void gl31_glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (w < 0 || h < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glScissor)(x, y, w, h);
}

void gl31_glDepthRange(GLdouble n, GLdouble f)
{
    n = n < 0.0 ? 0.0 : n > 1.0 ? 1.0 : n;
    f = f < 0.0 ? 0.0 : f > 1.0 ? 1.0 : f;
    BE(glDepthRangef)((GLfloat)n, (GLfloat)f);
}

/* ---------- profundidad / mezcla / stencil / culling ---------- */
void gl31_glDepthFunc(GLenum func) { BE(glDepthFunc)(func); }
void gl31_glDepthMask(GLboolean flag) { BE(glDepthMask)(flag); }

/* las funciones globales de mezcla fijan todos los buffers; se recuerdan por buffer para poder resolver
 * glBlendFunci/glBlendEquationi en backends sin draw_buffers_indexed (ver gl31_render_sync_blendfn) */
void gl31_glBlendFunc(GLenum sf, GLenum df) { gl31_glBlendFuncSeparate(sf, df, sf, df); }
void gl31_glBlendFuncSeparate(GLenum srgb, GLenum drgb, GLenum sa, GLenum da)
{
    gl31_state_t* s = gl31_state();
    int i;
    BE(glBlendFuncSeparate)(srgb, drgb, sa, da);
    for (i = 0; i < GL31_MAX_DRAW_BUFFERS; i++) { s->blend_f[i].srgb = srgb; s->blend_f[i].drgb = drgb; s->blend_f[i].sa = sa; s->blend_f[i].da = da; }
}
void gl31_glBlendEquation(GLenum mode) { gl31_glBlendEquationSeparate(mode, mode); }
void gl31_glBlendEquationSeparate(GLenum rgb, GLenum alpha)
{
    gl31_state_t* s = gl31_state();
    int i;
    BE(glBlendEquationSeparate)(rgb, alpha);
    for (i = 0; i < GL31_MAX_DRAW_BUFFERS; i++) { s->blend_f[i].erg = rgb; s->blend_f[i].ea = alpha; }
}

/* sin draw_buffers_indexed el backend tiene una unica funcion/ecuacion de mezcla: manda la del primer
 * buffer activo; si otro buffer activo la tiene distinta, ES no lo puede expresar y se avisa */
void gl31_render_sync_blendfn(void)
{
    gl31_state_t* s = gl31_state();
    int i, ref = 0, n = max_draw_buffers();
    unsigned act;
    if (n > GL31_MAX_DRAW_BUFFERS) n = GL31_MAX_DRAW_BUFFERS;
    act = active_draw_mask(n);
    s->idx_div &= ~2u;
    for (i = 1; i < n; i++) if (memcmp(&s->blend_f[i], &s->blend_f[0], sizeof s->blend_f[0]) != 0) { s->idx_div |= 2u; break; }
    for (i = 0; i < n; i++) if (act & (1u << i)) { ref = i; break; }
    BE(glBlendFuncSeparate)(s->blend_f[ref].srgb, s->blend_f[ref].drgb, s->blend_f[ref].sa, s->blend_f[ref].da);
    BE(glBlendEquationSeparate)(s->blend_f[ref].erg, s->blend_f[ref].ea);
    for (i = ref + 1; i < n; i++)
        if ((act & (1u << i)) && memcmp(&s->blend_f[i], &s->blend_f[ref], sizeof s->blend_f[0]) != 0) {
            gl31_stub_warn("glBlendFunci/glBlendEquationi distinto por buffer (backend sin draw_buffers_indexed)");
            break;
        }
}
void gl31_glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { BE(glBlendColor)(r, g, b, a); }

void gl31_glStencilFunc(GLenum func, GLint ref, GLuint mask) { BE(glStencilFunc)(func, ref, mask); }
void gl31_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    BE(glStencilFuncSeparate)(face, func, ref, mask);
}
void gl31_glStencilOp(GLenum sf, GLenum zf, GLenum zp) { BE(glStencilOp)(sf, zf, zp); }
void gl31_glStencilOpSeparate(GLenum face, GLenum sf, GLenum zf, GLenum zp)
{
    BE(glStencilOpSeparate)(face, sf, zf, zp);
}
void gl31_glStencilMask(GLuint mask) { BE(glStencilMask)(mask); }
void gl31_glStencilMaskSeparate(GLenum face, GLuint mask) { BE(glStencilMaskSeparate)(face, mask); }

void gl31_glCullFace(GLenum mode) { BE(glCullFace)(mode); }
/* El backend recibe el sentido efectivo: con origen UPPER_LEFT (glClipControl) la Y del clip space
 * se invierte en el VS, lo que invierte el sentido de giro de las caras. */
void gl31_front_face_apply(void)
{
    gl31_state_t* s = gl31_state();
    GLenum m = s->front_face_app;
    if (s->clip_origin == GL_UPPER_LEFT) m = (m == GL_CW) ? GL_CCW : GL_CW;
    BE(glFrontFace)(m);
}

void gl31_glFrontFace(GLenum mode)
{
    if (mode != GL_CW && mode != GL_CCW) { gl31_set_error(GL_INVALID_ENUM); return; }
    gl31_state()->front_face_app = mode;
    gl31_front_face_apply();
}

/* glClipControl (GL 4.5): GL_ZERO_TO_ONE se logra exactamente con z' = 2z - w en el VS;
 * GL_UPPER_LEFT invierte Y en el VS y el sentido de las caras. Limitacion: viewport, scissor y
 * gl_FragCoord siguen en coordenadas de ventana con origen abajo-izquierda. */
void gl31_glClipControl(GLenum origin, GLenum depth)
{
    gl31_state_t* s = gl31_state();
    if ((origin != GL_LOWER_LEFT && origin != GL_UPPER_LEFT) ||
        (depth != GL_NEGATIVE_ONE_TO_ONE && depth != GL_ZERO_TO_ONE)) { gl31_set_error(GL_INVALID_ENUM); return; }
    s->clip_origin = origin;
    s->clip_depth = depth;
    gl31_front_face_apply();
    gl31_lu_bump();
}
void gl31_glPolygonOffset(GLfloat factor, GLfloat units) { BE(glPolygonOffset)(factor, units); }

void gl31_glLineWidth(GLfloat width)
{
    if (!(width > 0.0f)) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glLineWidth)(width);
}

/* ---------- puntos ---------- */
/* GLES no tiene un tamano de punto global: el VS debe escribir gl_PointSize.
 * Se recuerda el valor para que las consultas sean coherentes. */
void gl31_glPointSize(GLfloat size)
{
    if (!(size > 0.0f)) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_state()->point_size = size;        /* lo aplica el wrapper del VS via lorica_vs */
    gl31_lu_bump();
}

void gl31_glPointParameterf(GLenum pname, GLfloat param)
{
    gl31_state_t* s = gl31_state();
    switch (pname) {
        case GL_POINT_FADE_THRESHOLD_SIZE:
            if (param < 0.f) { gl31_set_error(GL_INVALID_VALUE); return; }
            s->point_fade_threshold = param;
            return;
        case GL_POINT_SPRITE_COORD_ORIGIN:
            if (param != (GLfloat)GL_UPPER_LEFT && param != (GLfloat)GL_LOWER_LEFT) {
                gl31_set_error(GL_INVALID_VALUE);
                return;
            }
            s->point_sprite_origin = (GLenum)param;
            gl31_lu_bump();
            return;
        default:
            gl31_set_error(GL_INVALID_ENUM);
    }
}

void gl31_glPointParameteri(GLenum pname, GLint param) { gl31_glPointParameterf(pname, (GLfloat)param); }

void gl31_glPointParameterfv(GLenum pname, const GLfloat* p)
{
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glPointParameterf(pname, p[0]);
}

void gl31_glPointParameteriv(GLenum pname, const GLint* p)
{
    if (!p) { gl31_set_error(GL_INVALID_VALUE); return; }
    gl31_glPointParameterf(pname, (GLfloat)p[0]);
}

void gl31_glLogicOp(GLenum op)
{
    (void)op;
    gl31_stub_warn("glLogicOp");
}

/* ---------- hints / varios ---------- */
void gl31_glHint(GLenum target, GLenum mode)
{
    if (mode != GL_DONT_CARE && mode != GL_FASTEST && mode != GL_NICEST) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    switch (target) {
        case GL_LINE_SMOOTH_HINT:
        case GL_POLYGON_SMOOTH_HINT:
        case GL_TEXTURE_COMPRESSION_HINT:
            return;                       /* validos en desktop, sin efecto en ES */
        default:
            BE(glHint)(target, mode);     /* GENERATE_MIPMAP_HINT, FRAGMENT_SHADER_DERIVATIVE_HINT */
    }
}

void gl31_glSampleCoverage(GLfloat value, GLboolean invert) { BE(glSampleCoverage)(value, invert); }
void gl31_glFlush(void)  { BE(glFlush)(); }
void gl31_glFinish(void)
{
    BE(glFinish)();
    gl31_vkq_finish();      /* GL: tras glFinish los resultados de queries anteriores estan disponibles */
}
