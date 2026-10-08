/* Prueba de uniforms / consultas de programa con backend simulado.
 * Se compila y corre con `make test` (enlaza contra liblorica_gl31.a). */
#include "../gl31.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* ---- backend simulado ---- */
static struct { char fn[32]; GLint loc; GLsizei count; GLfloat f[4]; GLint i[4]; GLboolean tr; int calls; } last;
static GLenum be_err;
static GLint be_active_uniforms = 3, be_active_attribs = 2;

static void m_Uniform1f(GLint l, GLfloat a) { strcpy(last.fn, "1f"); last.loc = l; last.f[0] = a; last.calls++; }
static void m_Uniform4i(GLint l, GLint a, GLint b, GLint c, GLint d) { strcpy(last.fn, "4i"); last.loc = l; last.i[0]=a; last.i[1]=b; last.i[2]=c; last.i[3]=d; last.calls++; }
static void m_Uniform3uiv(GLint l, GLsizei c, const GLuint* v) { strcpy(last.fn, "3uiv"); last.loc = l; last.count = c; last.i[0]=(GLint)v[0]; last.calls++; }
static void m_UniformMatrix3x2fv(GLint l, GLsizei c, GLboolean t, const GLfloat* v) { strcpy(last.fn, "M3x2"); last.loc = l; last.count = c; last.tr = t; last.f[0]=v[0]; last.calls++; }
static void m_GetUniformfv(GLuint p, GLint l, GLfloat* o) { (void)p; strcpy(last.fn, "getfv"); last.loc = l; o[0] = 42.f; last.calls++; }
static void m_GetActiveUniform(GLuint p, GLuint idx, GLsizei bs, GLsizei* len, GLint* sz, GLenum* ty, GLchar* name)
{ (void)p; strcpy(last.fn, "gau"); last.calls++; *sz = 1; *ty = GL_FLOAT_VEC3; snprintf(name, bs, "u%u", idx); *len = (GLsizei)strlen(name); }
static void m_GetActiveAttrib(GLuint p, GLuint idx, GLsizei bs, GLsizei* len, GLint* sz, GLenum* ty, GLchar* name)
{ (void)p; strcpy(last.fn, "gaa"); last.calls++; *sz = 1; *ty = GL_FLOAT_VEC4; snprintf(name, bs, "a%u", idx); *len = (GLsizei)strlen(name); }
static GLint m_GetFragDataLocation(GLuint p, const GLchar* n) { (void)p; (void)n; return 0; }
static GLuint m_CreateProgram(void) { return 7; }
static void m_LinkProgram(GLuint p) { (void)p; }
static void m_GetProgramiv(GLuint p, GLenum pn, GLint* o)
{
    (void)p;
    if (pn == GL_LINK_STATUS) *o = 1;
    else if (pn == GL_ACTIVE_UNIFORMS) *o = be_active_uniforms;
    else if (pn == GL_ACTIVE_ATTRIBUTES) *o = be_active_attribs;
    else *o = 0;
}
static void m_GetProgramInfoLog(GLuint p, GLsizei b, GLsizei* l, GLchar* log) { (void)p; if (b > 0) log[0] = 0; if (l) *l = 0; }
static GLuint m_CreateShader(GLenum t) { (void)t; return 9; }
static void m_ShaderSource(GLuint s, GLsizei c, const GLchar* const* str, const GLint* l) { (void)s;(void)c;(void)str;(void)l; }
static void m_CompileShader(GLuint s) { (void)s; }
static void m_GetShaderiv(GLuint s, GLenum pn, GLint* o) { (void)s; *o = (pn == GL_COMPILE_STATUS) ? 1 : 0; }
static GLenum m_GetError(void) { GLenum e = be_err; be_err = 0; return e; }

int main(void)
{
    GLfloat m[6] = { 5, 0, 0, 0, 0, 0 };
    GLuint u3[3] = { 11, 12, 13 };
    GLfloat out = 0;
    GLsizei len = -1; GLint sz = 0; GLenum ty = 0; char name[16];
    GLuint prog, shader;

    memset(&gl31_be, 0, sizeof gl31_be);
    gl31_be.glUniform1f = m_Uniform1f; gl31_be.glUniform4i = m_Uniform4i;
    gl31_be.glUniform3uiv = m_Uniform3uiv; gl31_be.glUniformMatrix3x2fv = m_UniformMatrix3x2fv;
    gl31_be.glGetUniformfv = m_GetUniformfv; gl31_be.glGetActiveUniform = m_GetActiveUniform;
    gl31_be.glGetActiveAttrib = m_GetActiveAttrib; gl31_be.glGetFragDataLocation = m_GetFragDataLocation;
    gl31_be.glCreateProgram = m_CreateProgram; gl31_be.glLinkProgram = m_LinkProgram;
    gl31_be.glGetProgramiv = m_GetProgramiv; gl31_be.glGetProgramInfoLog = m_GetProgramInfoLog;
    gl31_be.glCreateShader = m_CreateShader; gl31_be.glShaderSource = m_ShaderSource;
    gl31_be.glCompileShader = m_CompileShader; gl31_be.glGetShaderiv = m_GetShaderiv;
    gl31_be.glGetError = m_GetError;
    gl31_state_init();

    /* sin programa en uso -> INVALID_OPERATION y no llega al backend */
    gl31_glUniform1f(3, 1.5f);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    CHECK(last.calls == 0);

    gl31_state()->program = 7;   /* programa en uso */

    gl31_glUniform1f(3, 1.5f);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    CHECK(!strcmp(last.fn, "1f") && last.loc == 3 && last.f[0] == 1.5f);

    gl31_glUniform4i(4, 1, 2, 3, 4);
    CHECK(!strcmp(last.fn, "4i") && last.i[0] == 1 && last.i[3] == 4);

    /* location -1: se ignora sin error ni llamada */
    last.calls = 0;
    gl31_glUniform1f(-1, 9.f);
    gl31_glUniform3uiv(-1, 1, u3);
    CHECK(last.calls == 0 && gl31_glGetError() == GL_NO_ERROR);

    gl31_glUniform3uiv(5, 1, u3);
    CHECK(!strcmp(last.fn, "3uiv") && last.count == 1 && last.i[0] == 11);

    /* count negativo y puntero nulo */
    last.calls = 0;
    gl31_glUniform3uiv(5, -1, u3);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE && last.calls == 0);
    gl31_glUniform3uiv(5, 2, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE && last.calls == 0);

    gl31_glUniformMatrix3x2fv(6, 2, GL_TRUE, m);
    CHECK(!strcmp(last.fn, "M3x2") && last.count == 2 && last.tr == GL_TRUE && last.f[0] == 5.f);

    /* programa linkeado de verdad a traves de la capa: id 7 */
    prog = gl31_glCreateProgram();
    CHECK(prog == 7);
    CHECK(gl31_program_status(7) == 0);
    shader = gl31_glCreateShader(GL_VERTEX_SHADER);
    CHECK(shader == 9 && gl31_program_status(9) == -2 && gl31_program_status(1234) == -1);

    /* sin linkear: consultas -> INVALID_OPERATION; activos -> INVALID_VALUE */
    gl31_glGetUniformfv(7, 0, &out);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glGetActiveUniform(7, 0, 16, &len, &sz, &ty, name);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    CHECK(gl31_glGetFragDataLocation(7, "c") == -1 && gl31_glGetError() == GL_INVALID_OPERATION);

    gl31_glLinkProgram(7);
    CHECK(gl31_program_status(7) == 1);

    last.calls = 0;
    gl31_glGetUniformfv(7, 2, &out);
    CHECK(gl31_glGetError() == GL_NO_ERROR && out == 42.f && last.loc == 2);
    gl31_glGetUniformfv(7, -1, &out);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glGetUniformfv(9, 0, &out);                 /* es un shader */
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glGetUniformfv(555, 0, &out);               /* no existe */
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    gl31_glGetActiveUniform(7, 1, 16, &len, &sz, &ty, name);
    CHECK(gl31_glGetError() == GL_NO_ERROR && !strcmp(name, "u1") && len == 2 && ty == GL_FLOAT_VEC3);
    gl31_glGetActiveUniform(7, 3, 16, &len, &sz, &ty, name);          /* index == activos */
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGetActiveUniform(7, 0, -1, &len, &sz, &ty, name);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGetActiveUniform(7, 2, 16, NULL, NULL, NULL, name);        /* punteros opcionales nulos */
    CHECK(gl31_glGetError() == GL_NO_ERROR && !strcmp(name, "u2"));

    len = -1; name[0] = 0;
    gl31_glGetActiveUniformName(7, 2, 16, &len, name);
    CHECK(gl31_glGetError() == GL_NO_ERROR && !strcmp(name, "u2") && len == 2);
    gl31_glGetActiveUniformName(7, 9, 16, &len, name);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    gl31_glGetActiveAttrib(7, 1, 16, &len, &sz, &ty, name);
    CHECK(gl31_glGetError() == GL_NO_ERROR && !strcmp(name, "a1") && ty == GL_FLOAT_VEC4);
    gl31_glGetActiveAttrib(7, 2, 16, &len, &sz, &ty, name);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    CHECK(gl31_glGetFragDataLocation(7, "c") == 0 && gl31_glGetError() == GL_NO_ERROR);
    CHECK(gl31_glGetFragDataLocation(7, NULL) == -1);
    CHECK(gl31_glGetFragDataLocation(9, "c") == -1 && gl31_glGetError() == GL_INVALID_OPERATION);

    /* error del backend se propaga por glGetError */
    be_err = GL_INVALID_OPERATION;
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);

    printf(fails ? "%d FALLOS\n" : "TODO OK\n", fails);
    return fails != 0;
}
