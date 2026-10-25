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

/* Emulado con MapBufferRange(READ) + memcpy. */
void gl31_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data)
{
    GLint total = 0;
    void* p;
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (size == 0) return;
    if (!data) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetBufferParameteriv)(target, GL_BUFFER_SIZE, &total);
    if (offset + size > (GLsizeiptr)total) { gl31_set_error(GL_INVALID_VALUE); return; }
    p = BE(glMapBufferRange)(target, offset, size, GL_MAP_READ_BIT);
    if (!p) { gl31_set_error(GL_INVALID_OPERATION); return; }
    memcpy(data, p, (size_t)size);
    BE(glUnmapBuffer)(target);
}

/* Solo GL_FILL esta disponible en GLES. */
void gl31_glPolygonMode(GLenum face, GLenum mode)
{
    if (face != GL_FRONT_AND_BACK) { gl31_set_error(GL_INVALID_ENUM); return; }
    switch (mode) {
        case GL_FILL: return;
        case GL_LINE:
        case GL_POINT: gl31_stub_warn("glPolygonMode(LINE/POINT)"); return;
        default: gl31_set_error(GL_INVALID_ENUM);
    }
}

void gl31_glClampColor(GLenum target, GLenum clamp)
{
    (void)target; (void)clamp;
    gl31_stub_warn("glClampColor");
}
