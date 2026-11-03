/* gl31_stubs.c - funciones de GL 3.1 que GLES 3.0 no puede ofrecer (o aun no implementamos).
 * Politica: avisar UNA vez por funcion por stderr y, si el resultado seria
 * incorrecto, generar un error GL para que la app no crea que funciono. */
#include "gl31.h"
#include <stdio.h>
#include <string.h>

#ifndef GL_POINT
#define GL_POINT 0x1B00
#define GL_LINE  0x1B01
#define GL_FILL  0x1B02
#endif
#ifndef GL_CLAMP_READ_COLOR
#define GL_CLAMP_READ_COLOR 0x891C
#endif

/* `fn` debe ser un literal (se guarda el puntero para deduplicar). */
void gl31_stub_warn(const char* fn)
{
    enum { MAX_WARNED = 64 };
    static const char* seen[MAX_WARNED];
    static int n;
    int i;
    if (!fn) return;
    for (i = 0; i < n; i++)
        if (strcmp(seen[i], fn) == 0) return;
    if (n < MAX_WARNED) seen[n++] = fn;
    fprintf(stderr, "LORICA GL31: %s no soportado sobre GLES (ignorado)\n", fn);
}

/* ES no tiene glGetBufferSubData. Con almacenamiento inmutable (glBufferStorage) el buffer puede no
 * tener MAP_READ o estar respaldado por memoria de Vulkan: se copia en GPU a un buffer temporal
 * (ordenado con el trabajo pendiente) y se lee de ese. Si no, MapBufferRange(READ) directo. */
void gl31_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data)
{
    GLint total = 0;
    GLbitfield sf = 0; int vk = 0, imm;
    void* p;
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (size == 0) return;
    if (!data) { gl31_set_error(GL_INVALID_VALUE); return; }
    imm = gl31_buffer_immutable_info(target, &sf, &vk);
    gl31_glGetBufferParameteriv(target, GL_BUFFER_SIZE, &total);
    if (offset + size > (GLsizeiptr)total) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (imm) {            /* inmutable: puede estar mapeado de forma persistente o no tener MAP_READ */
        gl31_state_t* st = gl31_state();
        GLenum rt = (target == GL_COPY_WRITE_BUFFER) ? GL_COPY_READ_BUFFER : GL_COPY_WRITE_BUFFER;
        GLuint prev = (rt == GL_COPY_READ_BUFFER) ? st->copy_read_buffer : st->copy_write_buffer, tmp = 0;
        BE(glGenBuffers)(1, &tmp);
        BE(glBindBuffer)(rt, tmp);
        BE(glBufferData)(rt, size, NULL, GL_STREAM_READ);
        BE(glCopyBufferSubData)(target, rt, offset, 0, size);
        p = BE(glMapBufferRange)(rt, 0, size, GL_MAP_READ_BIT);
        if (p) { memcpy(data, p, (size_t)size); BE(glUnmapBuffer)(rt); }
        else gl31_set_error(GL_OUT_OF_MEMORY);
        BE(glBindBuffer)(rt, prev);
        BE(glDeleteBuffers)(1, &tmp);
        return;
    }
    p = gl31_glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);
    if (!p) { gl31_set_error(GL_INVALID_OPERATION); return; }
    memcpy(data, p, (size_t)size);
    gl31_glUnmapBuffer(target);
}

/* GL_LINE / GL_POINT: ES solo rasteriza triangulos rellenos. Los triangulos se reconvierten en la GPU a
 * lineas o puntos (gl31_idxgen.c) al dibujar. Con cull face activo se dibujan los bordes de todos los
 * triangulos (GL descartaria antes los de la cara culleada). */
void gl31_glPolygonMode(GLenum face, GLenum mode)
{
    if (face != GL_FRONT_AND_BACK) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (mode != GL_FILL && mode != GL_LINE && mode != GL_POINT) { gl31_set_error(GL_INVALID_ENUM); return; }
    gl31_state()->polygon_mode = mode;
}

/* GL_CLAMP_READ_COLOR se aplica de verdad en glReadPixels (gl31_fbo.c). En ES el color del fragmento
 * solo se recorta en buffers de punto fijo (como FIXED_ONLY) y los atributos de vertice nunca se
 * recortan: CLAMP_VERTEX/FRAGMENT_COLOR se registran para poder consultarlos, y se aceptan los tres
 * valores validos porque el comportamiento de ES coincide con el modo por defecto de GL. */
void gl31_glClampColor(GLenum target, GLenum clamp)
{
    gl31_state_t* s = gl31_state();
    if (clamp != GL_TRUE && clamp != GL_FALSE && clamp != GL_FIXED_ONLY) { gl31_set_error(GL_INVALID_ENUM); return; }
    switch (target) {
        case GL_CLAMP_READ_COLOR:     s->clamp_read_color = clamp; break;
        case GL_CLAMP_VERTEX_COLOR:   s->clamp_vertex_color = clamp; break;
        case GL_CLAMP_FRAGMENT_COLOR: s->clamp_fragment_color = clamp; break;
        default: gl31_set_error(GL_INVALID_ENUM); return;
    }
}
