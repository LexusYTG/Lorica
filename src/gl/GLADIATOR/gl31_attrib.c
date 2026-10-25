/* gl31_attrib.c - valores genericos de atributos (glVertexAttrib*) y consultas.
 * GLES solo tiene glVertexAttrib{1..4}f y glVertexAttribI4{i,ui}[v]; el resto de
 * variantes de GL desktop (s, d, N*, b, ub, us, ui...) se convierten aqui. */
#include "gl31.h"

static GL31_TLS GLint g_max_attribs;

static int idx_ok(GLuint idx)
{
    if (!g_max_attribs) {
        g_max_attribs = 16;
        BE(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &g_max_attribs);
    }
    if ((GLint)idx >= g_max_attribs) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

static void a4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    if (idx_ok(i)) BE(glVertexAttrib4f)(i, x, y, z, w);
}
static void a4i(GLuint i, GLint x, GLint y, GLint z, GLint w)
{
    if (idx_ok(i)) BE(glVertexAttribI4i)(i, x, y, z, w);
}
static void a4u(GLuint i, GLuint x, GLuint y, GLuint z, GLuint w)
{
    if (idx_ok(i)) BE(glVertexAttribI4ui)(i, x, y, z, w);
}

/* ---------- flotantes: f, s, d ---------- */
#define ATTR_FLOAT(sfx, T) \
    void gl31_glVertexAttrib1##sfx(GLuint i, T a) { a4f(i, (GLfloat)a, 0.f, 0.f, 1.f); } \
    void gl31_glVertexAttrib2##sfx(GLuint i, T a, T b) { a4f(i, (GLfloat)a, (GLfloat)b, 0.f, 1.f); } \
    void gl31_glVertexAttrib3##sfx(GLuint i, T a, T b, T c) { a4f(i, (GLfloat)a, (GLfloat)b, (GLfloat)c, 1.f); } \
    void gl31_glVertexAttrib4##sfx(GLuint i, T a, T b, T c, T d) { a4f(i, (GLfloat)a, (GLfloat)b, (GLfloat)c, (GLfloat)d); } \
    void gl31_glVertexAttrib1##sfx##v(GLuint i, const T* v) { a4f(i, (GLfloat)v[0], 0.f, 0.f, 1.f); } \
    void gl31_glVertexAttrib2##sfx##v(GLuint i, const T* v) { a4f(i, (GLfloat)v[0], (GLfloat)v[1], 0.f, 1.f); } \
    void gl31_glVertexAttrib3##sfx##v(GLuint i, const T* v) { a4f(i, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.f); } \
    void gl31_glVertexAttrib4##sfx##v(GLuint i, const T* v) { a4f(i, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }

ATTR_FLOAT(f, GLfloat)
ATTR_FLOAT(s, GLshort)
ATTR_FLOAT(d, GLdouble)

/* ---------- 4 componentes enteras convertidas a float ---------- */
#define ATTR_V4_CAST(sfx, T) \
    void gl31_glVertexAttrib4##sfx(GLuint i, const T* v) \
    { a4f(i, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }

ATTR_V4_CAST(bv, GLbyte)
ATTR_V4_CAST(iv, GLint)
ATTR_V4_CAST(ubv, GLubyte)
ATTR_V4_CAST(uiv, GLuint)
ATTR_V4_CAST(usv, GLushort)

/* ---------- normalizadas ---------- */
static GLfloat nrm_signed(double v, double max)
{
    double r = v / max;
    return (GLfloat)(r < -1.0 ? -1.0 : r);
}

void gl31_glVertexAttrib4Nub(GLuint i, GLubyte x, GLubyte y, GLubyte z, GLubyte w)
{
    a4f(i, x / 255.f, y / 255.f, z / 255.f, w / 255.f);
}
void gl31_glVertexAttrib4Nubv(GLuint i, const GLubyte* v)
{
    gl31_glVertexAttrib4Nub(i, v[0], v[1], v[2], v[3]);
}
void gl31_glVertexAttrib4Nbv(GLuint i, const GLbyte* v)
{
    a4f(i, nrm_signed(v[0], 127.0), nrm_signed(v[1], 127.0), nrm_signed(v[2], 127.0), nrm_signed(v[3], 127.0));
}
void gl31_glVertexAttrib4Nsv(GLuint i, const GLshort* v)
{
    a4f(i, nrm_signed(v[0], 32767.0), nrm_signed(v[1], 32767.0), nrm_signed(v[2], 32767.0), nrm_signed(v[3], 32767.0));
}
void gl31_glVertexAttrib4Nusv(GLuint i, const GLushort* v)
{
    a4f(i, v[0] / 65535.f, v[1] / 65535.f, v[2] / 65535.f, v[3] / 65535.f);
}
void gl31_glVertexAttrib4Nuiv(GLuint i, const GLuint* v)
{
    a4f(i, (GLfloat)(v[0] / 4294967295.0), (GLfloat)(v[1] / 4294967295.0),
           (GLfloat)(v[2] / 4294967295.0), (GLfloat)(v[3] / 4294967295.0));
}

/* ---------- enteras puras (I) ---------- */
void gl31_glVertexAttribI1i(GLuint i, GLint a) { a4i(i, a, 0, 0, 1); }
void gl31_glVertexAttribI2i(GLuint i, GLint a, GLint b) { a4i(i, a, b, 0, 1); }
void gl31_glVertexAttribI3i(GLuint i, GLint a, GLint b, GLint c) { a4i(i, a, b, c, 1); }
void gl31_glVertexAttribI4i(GLuint i, GLint a, GLint b, GLint c, GLint d) { a4i(i, a, b, c, d); }
void gl31_glVertexAttribI1iv(GLuint i, const GLint* v) { a4i(i, v[0], 0, 0, 1); }
void gl31_glVertexAttribI2iv(GLuint i, const GLint* v) { a4i(i, v[0], v[1], 0, 1); }
void gl31_glVertexAttribI3iv(GLuint i, const GLint* v) { a4i(i, v[0], v[1], v[2], 1); }
void gl31_glVertexAttribI4iv(GLuint i, const GLint* v) { a4i(i, v[0], v[1], v[2], v[3]); }

void gl31_glVertexAttribI1ui(GLuint i, GLuint a) { a4u(i, a, 0, 0, 1); }
void gl31_glVertexAttribI2ui(GLuint i, GLuint a, GLuint b) { a4u(i, a, b, 0, 1); }
void gl31_glVertexAttribI3ui(GLuint i, GLuint a, GLuint b, GLuint c) { a4u(i, a, b, c, 1); }
void gl31_glVertexAttribI4ui(GLuint i, GLuint a, GLuint b, GLuint c, GLuint d) { a4u(i, a, b, c, d); }
void gl31_glVertexAttribI1uiv(GLuint i, const GLuint* v) { a4u(i, v[0], 0, 0, 1); }
void gl31_glVertexAttribI2uiv(GLuint i, const GLuint* v) { a4u(i, v[0], v[1], 0, 1); }
void gl31_glVertexAttribI3uiv(GLuint i, const GLuint* v) { a4u(i, v[0], v[1], v[2], 1); }
void gl31_glVertexAttribI4uiv(GLuint i, const GLuint* v) { a4u(i, v[0], v[1], v[2], v[3]); }

void gl31_glVertexAttribI4bv(GLuint i, const GLbyte* v)   { a4i(i, v[0], v[1], v[2], v[3]); }
void gl31_glVertexAttribI4sv(GLuint i, const GLshort* v)  { a4i(i, v[0], v[1], v[2], v[3]); }
void gl31_glVertexAttribI4ubv(GLuint i, const GLubyte* v) { a4u(i, v[0], v[1], v[2], v[3]); }
void gl31_glVertexAttribI4usv(GLuint i, const GLushort* v){ a4u(i, v[0], v[1], v[2], v[3]); }

/* ---------- consultas ---------- */
void gl31_glGetVertexAttribiv(GLuint i, GLenum pname, GLint* p)
{
    if (!p || !idx_ok(i)) return;
    BE(glGetVertexAttribiv)(i, pname, p);
}

void gl31_glGetVertexAttribfv(GLuint i, GLenum pname, GLfloat* p)
{
    if (!p || !idx_ok(i)) return;
    BE(glGetVertexAttribfv)(i, pname, p);
}

void gl31_glGetVertexAttribdv(GLuint i, GLenum pname, GLdouble* p)
{
    GLfloat f[4] = {0, 0, 0, 0};
    int n, k;
    if (!p || !idx_ok(i)) return;
    BE(glGetVertexAttribfv)(i, pname, f);
    n = (pname == GL_CURRENT_VERTEX_ATTRIB) ? 4 : 1;
    for (k = 0; k < n; k++) p[k] = f[k];
}

void gl31_glGetVertexAttribIiv(GLuint i, GLenum pname, GLint* p)
{
    if (!p || !idx_ok(i)) return;
    BE(glGetVertexAttribIiv)(i, pname, p);
}

void gl31_glGetVertexAttribIuiv(GLuint i, GLenum pname, GLuint* p)
{
    if (!p || !idx_ok(i)) return;
    BE(glGetVertexAttribIuiv)(i, pname, p);
}

void gl31_glGetVertexAttribPointerv(GLuint i, GLenum pname, void** p)
{
    if (!p || !idx_ok(i)) return;
    BE(glGetVertexAttribPointerv)(i, pname, p);
}
