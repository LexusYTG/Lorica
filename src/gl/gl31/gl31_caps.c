/* gl31_caps.c - inicializacion, deteccion de capacidades del backend y carga de
 * las funciones opcionales (las que GLES 3.0 base no trae). */
#include "gl31.h"
#include <stdio.h>
#include <string.h>

gl31_caps_t    gl31_caps;
gl31_backend_t gl31_be;

/* Busca `name`, `nameEXT`, `nameOES`, `nameKHR` */
static void* load_opt(gl31_loader_fn loader, const char* name)
{
    static const char* const sfx[] = { "", "EXT", "OES", "KHR", NULL };
    char buf[96];
    int i;

    for (i = 0; sfx[i]; i++) {
        void* f;
        snprintf(buf, sizeof buf, "%s%s", name, sfx[i]);
        f = loader(buf);
        if (f) return f;
    }
    return NULL;
}

/* Carga una funcion opcional en gl31_be. Solo se llama si la capacidad esta presente. */
#define LOAD_OPT(name) \
    gl31_be.name = (__typeof__(gl31_be.name))load_opt(loader, #name)

static int has_ext(const char* ext, const char* a, const char* b)
{
    return strcmp(ext, a) == 0 || (b && strcmp(ext, b) == 0);
}

/* Detecta capacidades consultando el backend (requiere gl31_be ya cargado) */
static void detect_capabilities(void)
{
    GLint major = 0, minor = 0, n = 0;
    GLint i;

    memset(&gl31_caps, 0, sizeof(gl31_caps));

    /* Version GLES */
    BE(glGetIntegerv)(GL_MAJOR_VERSION, &major);
    BE(glGetIntegerv)(GL_MINOR_VERSION, &minor);
    gl31_caps.es_major = major ? major : 3;
    gl31_caps.es_minor = minor;

    /* Extensiones */
    BE(glGetIntegerv)(GL_NUM_EXTENSIONS, &n);
    for (i = 0; i < n; i++) {
        const char* e = (const char*)BE(glGetStringi)(GL_EXTENSIONS, (GLuint)i);
        if (!e) continue;

        if (has_ext(e, "GL_EXT_texture_compression_s3tc", "GL_WEBGL_compressed_texture_s3tc"))
            gl31_caps.has_s3tc = 1;
        else if (has_ext(e, "GL_IMG_texture_compression_pvrtc", NULL))
            gl31_caps.has_pvrtc = 1;
        else if (has_ext(e, "GL_KHR_texture_compression_astc_ldr", NULL))
            gl31_caps.has_astc = 1;
        else if (has_ext(e, "GL_EXT_texture_compression_bptc", NULL))
            gl31_caps.has_bptc = 1;
        else if (has_ext(e, "GL_BASIS_EXT", NULL))
            gl31_caps.has_basis = 1;
        else if (has_ext(e, "GL_EXT_texture_buffer", "GL_OES_texture_buffer"))
            gl31_caps.tex_buffer = 1;
        else if (has_ext(e, "GL_EXT_draw_instanced", NULL))
            gl31_caps.draw_instanced = 1;
        else if (has_ext(e, "GL_EXT_draw_buffers_indexed", "GL_OES_draw_buffers_indexed"))
            gl31_caps.draw_buf_indexed = 1;
        else if (has_ext(e, "GL_EXT_color_buffer_half_float", NULL))
            gl31_caps.half_float = 1;
        else if (has_ext(e, "GL_EXT_color_buffer_float", NULL))
            gl31_caps.float_buffer = 1;
        else if (has_ext(e, "GL_EXT_texture_border_clamp", "GL_OES_texture_border_clamp"))
            gl31_caps.border_clamp = 1;
        else if (has_ext(e, "GL_EXT_texture_filter_anisotropic", NULL))
            gl31_caps.aniso = 1;
    }

    /* Lo que es core a partir de cierta version de ES */
    if (gl31_caps.es_major > 3 || (gl31_caps.es_major == 3 && gl31_caps.es_minor >= 1))
        gl31_caps.level_query = 1;
    if (gl31_caps.es_major > 3 || (gl31_caps.es_major == 3 && gl31_caps.es_minor >= 2)) {
        gl31_caps.draw_buf_indexed = 1;
        gl31_caps.border_clamp = 1;
        gl31_caps.tex_buffer = 1;
    }
}

int gl31_init(gl31_loader_fn loader)
{
    int missing = 0;
    if (!loader) return -1;

    memset(&gl31_be, 0, sizeof(gl31_be));

#define X(ret, name, args) \
    gl31_be.name = (ret (*) args)loader(#name); \
    if (!gl31_be.name) { \
        fprintf(stderr, "LORICA GL31: backend sin %s\n", #name); \
        missing++; \
    }
    GL31_BACKEND_FUNCS(X)
#undef X

    if (missing) return -1;

    detect_capabilities();

    /* Funciones opcionales, solo si la capacidad lo indica */
    if (gl31_caps.draw_buf_indexed) {
        LOAD_OPT(glEnablei);
        LOAD_OPT(glDisablei);
        LOAD_OPT(glIsEnabledi);
        LOAD_OPT(glColorMaski);
        /* sin las cuatro no sirve: se trata como ausente */
        if (!gl31_be.glEnablei || !gl31_be.glDisablei || !gl31_be.glIsEnabledi || !gl31_be.glColorMaski) {
            gl31_be.glEnablei = NULL; gl31_be.glDisablei = NULL;
            gl31_be.glIsEnabledi = NULL; gl31_be.glColorMaski = NULL;
            gl31_caps.draw_buf_indexed = 0;
        }
    }
    if (gl31_caps.tex_buffer) {
        LOAD_OPT(glTexBuffer);
        LOAD_OPT(glTexBufferRange);
    }
    if (gl31_caps.border_clamp) {
        LOAD_OPT(glTexParameterIiv);
        LOAD_OPT(glTexParameterIuiv);
        LOAD_OPT(glGetTexParameterIiv);
        LOAD_OPT(glGetTexParameterIuiv);
        LOAD_OPT(glSamplerParameterIiv);
        LOAD_OPT(glSamplerParameterIuiv);
        if (!gl31_be.glTexParameterIiv || !gl31_be.glTexParameterIuiv) gl31_caps.border_clamp = 0;
    }
    if (gl31_caps.level_query) {
        LOAD_OPT(glGetTexLevelParameteriv);
        if (!gl31_be.glGetTexLevelParameteriv) gl31_caps.level_query = 0;
    }

    gl31_state_init();
    return 0;
}

void gl31_shutdown(void)
{
    gl31_link_shutdown();
    memset(&gl31_be, 0, sizeof(gl31_be));
    memset(&gl31_caps, 0, sizeof(gl31_caps));
}

/* Acceso a funciones opcionales por nombre (NULL si el backend no las tiene) */
void* gl31_get_optional(const char* name)
{
    if (!name) return NULL;
#define X(ret, fn, args) if (strcmp(name, #fn) == 0) return (void*)gl31_be.fn;
    GL31_BACKEND_OPT_FUNCS(X)
#undef X
    return NULL;
}
