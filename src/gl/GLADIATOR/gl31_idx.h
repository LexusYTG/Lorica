/* gl31_idx.h - motor de reescritura de indices en GPU (ver gl31_idxgen.c) */
#ifndef GL31_IDX_H
#define GL31_IDX_H
#include "gl31.h"
#include <stdint.h>
enum { IDX_NONE = 0, IDX_RESTART, IDX_POLY_LINES, IDX_POLY_POINTS, IDX_PROVOKE };
/* Decide si este draw necesita reescritura; *out_mode = modo con el que se dibuja el resultado;
 * *unavailable = 1 si se necesita pero no se puede (sin compute / TF activo / kernel no compila). */
int    gl31_idx_needed(GLenum mode, int indexed, GLenum type, GLenum* out_mode, int* unavailable);
/* Genera los indices en un buffer de GPU (UNSIGNED_INT, 0xFFFFFFFF = ranura vacia) y devuelve su id
 * (0 si falla). *out_count = numero de indices. El llamador debe dibujar con restart de indice fijo. */
GLuint gl31_idx_generate(int op, GLenum mode, int indexed, GLenum type, GLint first, GLsizei count,
                         uintptr_t srcoff, GLsizei* out_count);
void   gl31_idx_shutdown(void);
#endif
