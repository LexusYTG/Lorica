/* gl31_xfb.c - transform feedback (GL 3.0 core): objeto implícito, sin pausa/reanudación.
 * El estado "activo" se recuerda aquí porque GL lo usa para rechazar UseProgram,
 * Begin anidado y cambios de programa mientras se captura. */
#include "gl31.h"

#ifndef GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS 0x8C8B
#endif

void gl31_glBeginTransformFeedback(GLenum mode)
{
    gl31_state_t* s = gl31_state();
    if (mode != GL_POINTS && mode != GL_LINES && mode != GL_TRIANGLES) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (s->xfb_active || !s->program) { 
        gl31_set_error(GL_INVALID_OPERATION); 
        return; 
    }
    gl31_err_flush();
    BE(glBeginTransformFeedback)(mode);
    s->xfb_active = 1;
}

void gl31_glEndTransformFeedback(void)
{
    gl31_state_t* s = gl31_state();
    if (!s->xfb_active) {
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    gl31_err_flush();
    BE(glEndTransformFeedback)();
    s->xfb_active = 0;
}

void gl31_glTransformFeedbackVaryings(GLuint program, GLsizei count, 
                                      const GLchar* const* varyings, GLenum bufferMode)
{
    if (!program || count < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (count > 0 && !varyings) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glTransformFeedbackVaryings)(program, count, varyings, bufferMode);
}

void gl31_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                        GLsizei* length, GLsizei* size, GLenum* type,
                                        GLchar* name)
{
    if (bufSize < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glGetTransformFeedbackVarying)(program, index, bufSize, length, size, type, name);
}
