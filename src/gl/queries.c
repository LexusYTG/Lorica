#ifndef _WIN32
#ifdef USE_CLOCK
#include <time.h>
#else
#include <sys/time.h>
#endif
#endif

#include "queries.h"

#include "khash.h"
#include "gl4es.h"
#include "glstate.h"
#include "loader.h"

/* ---- Lorica: tabla id_local -> id_backend para occlusion queries reales ---- */
#include <dlfcn.h>
#include <stdlib.h>

#define SC_MAX_OCCL 512

typedef void (*PFN_gq_Delete)(GLsizei, const GLuint*);
typedef void (*PFN_gq_Gen)   (GLsizei, GLuint*);
typedef void (*PFN_gq_Begin) (GLenum, GLuint);
typedef void (*PFN_gq_End)   (GLenum);
typedef void (*PFN_gq_Getuiv)(GLuint, GLenum, GLuint*);

static PFN_gq_Delete s_beDeleteQueries = NULL;
static PFN_gq_Gen    s_beGenQueries    = NULL;
static PFN_gq_Begin  s_beBeginQuery    = NULL;
static PFN_gq_End    s_beEndQuery      = NULL;
static PFN_gq_Getuiv s_beGetuiv        = NULL;
static int           s_be_loaded       = 0;

static void *s_gles_handle = NULL;

static void *lorica_lookup(const char *name) {
    if (!s_gles_handle) {
        s_gles_handle = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
        if (!s_gles_handle)
            s_gles_handle = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
    }
    if (!s_gles_handle) return NULL;
    return dlsym(s_gles_handle, name);
}

static void load_query_funcs(void) {
    if (s_be_loaded) return;
    s_be_loaded = 1;
    s_beDeleteQueries = (PFN_gq_Delete)lorica_lookup("glDeleteQueries");
    s_beGenQueries    = (PFN_gq_Gen)   lorica_lookup("glGenQueries");
    s_beBeginQuery    = (PFN_gq_Begin) lorica_lookup("glBeginQuery");
    s_beEndQuery      = (PFN_gq_End)   lorica_lookup("glEndQuery");
    s_beGetuiv        = (PFN_gq_Getuiv)lorica_lookup("glGetQueryObjectuiv");
}

static glquery_t* find_query_target_fwd(GLenum target);
glquery_t* gl4es_occl_active(void);
void gl4es_occl_add_samples(GLuint n);
float gl4es_occl_tri_area(const float*, const float*, const float*, const float*, const float*, GLenum, int);
typedef struct { glquery_t* q; GLuint be_id; int has; GLuint cached; } occl_pair_t;
static occl_pair_t s_occl[SC_MAX_OCCL];

/* LIBGL_OCCL=0: no se usa el backend (siempre "disponible", resultado = fallback/estimado).
   LIBGL_OCCL=1 (defecto): ANY_SAMPLES_PASSED real, con AVAILABLE asincrono. */
static int occl_backend_enabled(void) {
    static int m = -1;
    if (m < 0) { const char* e = getenv("LIBGL_OCCL"); m = (e && e[0] == '0') ? 0 : 1; }
    return m;
}

static int is_occlusion_target(GLenum t) {
    return t == GL_SAMPLES_PASSED || t == GL_ANY_SAMPLES_PASSED
        || t == GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
}

static int occl_by_q(glquery_t* q) {
    int i;
    if (!q) return -1;
    for (i = 0; i < SC_MAX_OCCL; i++) if (s_occl[i].q == q) return i;
    return -1;
}

static int occl_new(glquery_t* q, GLuint be_id) {
    int i;
    for (i = 0; i < SC_MAX_OCCL; i++) if (!s_occl[i].q) {
        s_occl[i].q = q; s_occl[i].be_id = be_id; s_occl[i].has = 0; s_occl[i].cached = 0; return i;
    }
    return -1;
}

static void occl_del(glquery_t* q) {
    int i = occl_by_q(q);
    if (i < 0) return;
    load_query_funcs();
    if (s_beDeleteQueries && s_occl[i].be_id)
        s_beDeleteQueries(1, &s_occl[i].be_id);
    s_occl[i].q = NULL;
    s_occl[i].be_id = 0;
    s_occl[i].has = 0;
}

/* Resultado de una query. SAMPLES_PASSED -> conteo estimado por software,
   anulado a 0 si el backend (ANY_SAMPLES_PASSED) dice que todo está ocluido.
   ANY_SAMPLES_PASSED[_CONSERVATIVE] -> 0/1 (nunca 0xFFFFFFFF). */
/* ¿Esta listo el resultado del backend? NO bloquea. Si esta listo lo cachea. */
static int occl_poll(int i) {
    GLuint av = 0;
    if (s_occl[i].has) return 1;
    load_query_funcs();
    if (!s_beGetuiv) return 1;
    s_beGetuiv(s_occl[i].be_id, GL_QUERY_RESULT_AVAILABLE, &av);
    if (!av) return 0;
    s_beGetuiv(s_occl[i].be_id, GL_QUERY_RESULT, &s_occl[i].cached);
    s_occl[i].has = 1;
    return 1;
}

/* GL_QUERY_RESULT_AVAILABLE real (Cube2 y otros lo usan para NO bloquear la GPU). */
static int query_available(glquery_t* q) {
    int i;
    if (!is_occlusion_target(q->target)) return 1;
    i = occl_by_q(q);
    if (i < 0) return 1;                  /* sin backend (LIBGL_OCCL=0) */
    return occl_poll(i);
}

static GLuint64 occl_result(glquery_t* q) {
    int i = occl_by_q(q);
    GLuint any = 1;                       /* sin backend: asumir visible */
    if (i >= 0) {
        if (!s_occl[i].has) {             /* GL_QUERY_RESULT bloquea, como manda la spec */
            load_query_funcs();
            if (s_beGetuiv) {
                s_beGetuiv(s_occl[i].be_id, GL_QUERY_RESULT, &s_occl[i].cached);
                s_occl[i].has = 1;
            }
        }
        if (s_occl[i].has) any = s_occl[i].cached;
    }
    if (q->target != GL_SAMPLES_PASSED)
        return any ? 1 : 0;
    if (!any) return 0;
    if (q->num) return q->num;
    /* Backend dice "visible" pero no hay estimación por software: devolver un valor
       alto para que umbrales tipo Cube2 (oqfrags=8, máx 64) no oculten geometría.
       Ajustable con LIBGL_OCCL_FALLBACK. */
    {
        static GLuint fb = 0;
        if (!fb) { const char* e = getenv("LIBGL_OCCL_FALLBACK"); fb = e ? (GLuint)atoi(e) : 0; if (!fb) fb = 128; }
        return fb;
    }
}

static GLuint64 query_result(glquery_t* q) {
    if (is_occlusion_target(q->target)) return occl_result(q);
    return (q->target==GL_TIME_ELAPSED) ? q->start : q->num;
}

/* ---- API para el camino de dibujo ---- */
glquery_t* gl4es_occl_active(void) {
    return find_query_target_fwd(GL_SAMPLES_PASSED);
}

void gl4es_occl_add_samples(GLuint n) {
    glquery_t* q = find_query_target_fwd(GL_SAMPLES_PASSED);
    if (!q) return;
    GLuint64 t = (GLuint64)q->num + n;
    q->num = (t > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (GLuint)t;
}

/* Área en píxeles de un triángulo en espacio objeto, tras MVP (column-major),
   recorte near, recorte al rect [rx,ry,rw,rh] (viewport o scissor) y culling.
   cull: 0 / GL_FRONT / GL_BACK / GL_FRONT_AND_BACK ; ccw_front: glFrontFace==GL_CCW */
typedef struct { float x, y, z, w; } ov4_t;
typedef struct { float x, y; } ov2_t;

static ov4_t ov_xform(const float* m, const float* v) {
    ov4_t r;
    r.x = m[0]*v[0] + m[4]*v[1] + m[8]*v[2]  + m[12];
    r.y = m[1]*v[0] + m[5]*v[1] + m[9]*v[2]  + m[13];
    r.z = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14];
    r.w = m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15];
    return r;
}

static float ov_area(const ov2_t* p, int n) {
    float a = 0; int i;
    for (i = 0; i < n; i++) { const ov2_t *u=&p[i], *v=&p[(i+1)%n]; a += u->x*v->y - v->x*u->y; }
    return 0.5f*a;
}

/* Sutherland-Hodgman contra un semiplano: axis 0=x 1=y, keep>=lim (sgn=+1) o <=lim (sgn=-1) */
static int ov_clip(const ov2_t* in, int n, ov2_t* out, int axis, float lim, int sgn) {
    int i, m = 0;
    for (i = 0; i < n; i++) {
        ov2_t a = in[i], b = in[(i+1)%n];
        float da = sgn*((axis?a.y:a.x)-lim), db = sgn*((axis?b.y:b.x)-lim);
        if (da >= 0) out[m++] = a;
        if ((da >= 0) != (db >= 0)) {
            float t = da/(da-db);
            out[m].x = a.x + t*(b.x-a.x); out[m].y = a.y + t*(b.y-a.y); m++;
        }
    }
    return m;
}

float gl4es_occl_tri_area(const float* a, const float* b, const float* c, const float* mvp,
                          const float* rect, GLenum cull, int ccw_front)
{
    ov4_t v[3] = { ov_xform(mvp,a), ov_xform(mvp,b), ov_xform(mvp,c) };
    ov4_t poly[8]; int n = 0, i;
    /* recorte contra z >= -w (near) */
    for (i = 0; i < 3; i++) {
        ov4_t p = v[i], q = v[(i+1)%3];
        float dp = p.z + p.w, dq = q.z + q.w;
        if (dp >= 0) poly[n++] = p;
        if ((dp >= 0) != (dq >= 0)) {
            float t = dp/(dp-dq); ov4_t r;
            r.x=p.x+t*(q.x-p.x); r.y=p.y+t*(q.y-p.y); r.z=p.z+t*(q.z-p.z); r.w=p.w+t*(q.w-p.w);
            poly[n++] = r;
        }
    }
    if (n < 3) return 0.f;
    ov2_t s[16], t1[16], t2[16];
    for (i = 0; i < n; i++) {
        float iw = 1.0f / (poly[i].w > 1e-6f ? poly[i].w : 1e-6f);
        s[i].x = rect[0] + (poly[i].x*iw*0.5f + 0.5f) * rect[2];
        s[i].y = rect[1] + (poly[i].y*iw*0.5f + 0.5f) * rect[3];
    }
    float sa = ov_area(s, n);
    if (sa == 0.f) return 0.f;
    if (cull) {
        int front = ccw_front ? (sa > 0) : (sa < 0);
        if (cull == GL_FRONT_AND_BACK || (cull == GL_BACK && !front) || (cull == GL_FRONT && front))
            return 0.f;
    }
    n = ov_clip(s,  n, t1, 0, rect[0],          +1); if (n < 3) return 0.f;
    n = ov_clip(t1, n, t2, 0, rect[0]+rect[2],  -1); if (n < 3) return 0.f;
    n = ov_clip(t2, n, t1, 1, rect[1],          +1); if (n < 3) return 0.f;
    n = ov_clip(t1, n, t2, 1, rect[1]+rect[3],  -1); if (n < 3) return 0.f;
    sa = ov_area(t2, n);
    return sa < 0 ? -sa : sa;
}

#ifdef _WIN32
#ifdef _WINBASE_
#define GSM_CAST(c) ((LPFILETIME)c)
#else
__declspec(dllimport)
void __stdcall GetSystemTimeAsFileTime(unsigned __int64*);
#define GSM_CAST(c) ((__int64*)c)
#endif
#endif

KHASH_MAP_IMPL_INT(queries, glquery_t *);

static GLuint new_query(GLuint base) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    while(1) {
        k = kh_get(queries, list, base);
        if (k == kh_end(list))
            return base;
        ++base;
    }
}

static glquery_t* find_query(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    
    if (k != kh_end(list)){
        return kh_value(list, k);
    }
    return NULL;
}

static glquery_t* find_query_target(GLenum target) {
    khash_t(queries) *list = glstate->queries.querylist;
	glquery_t *q;
    kh_foreach_value(list, q,
		if(q->active && q->target==target)
			return q;
	);
    return NULL;
}


static glquery_t* find_query_target_fwd(GLenum target) { return find_query_target(target); }

void del_querie(GLuint querie) {
    khint_t k;
    khash_t(queries) *list = glstate->queries.querylist;
    k = kh_get(queries, list, querie);
    glquery_t* s = NULL;
    if (k != kh_end(list)){
        s = kh_value(list, k);
        kh_del(queries, list, k);
    }
    if(s) {
        occl_del(s);
        free(s);
    }
}

unsigned long long get_clock() {
	unsigned long long now;
	#ifdef _WIN32
        GetSystemTimeAsFileTime(GSM_CAST(&now));
	#elif defined(USE_CLOCK)
	struct timespec out;
	clock_gettime(CLOCK_MONOTONIC_RAW, &out);
	now = ((unsigned long long)out.tv_sec)*1000000000LL + out.tv_nsec;
	#else
	struct timeval out;
	gettimeofday(&out, NULL);
	now = ((unsigned long long)out.tv_sec)*1000000LL + out.tv_usec;
	#endif
	return now;
}

void APIENTRY_GL4ES gl4es_glGenQueries(GLsizei n, GLuint * ids) {
    FLUSH_BEGINEND;
	noerrorShim();
    if (n<1) {
		errorShim(GL_INVALID_VALUE);
        return;
    }
    for (int i=0; i<n; i++) {
        ids[i] = new_query(++glstate->queries.last_query);
    }
}

GLboolean APIENTRY_GL4ES gl4es_glIsQuery(GLuint id) {
	if(glstate->list.compiling) {errorShim(GL_INVALID_OPERATION); return GL_FALSE;}
	FLUSH_BEGINEND;
	glquery_t *querie = find_query(id);
	if(querie)
		return GL_TRUE;
	return GL_FALSE;
}

void APIENTRY_GL4ES gl4es_glDeleteQueries(GLsizei n, const GLuint* ids) {
    FLUSH_BEGINEND;
    if(n<0) {
        errorShim(GL_INVALID_VALUE);
        return;
    }
    noerrorShim();
    if(!n)
        return;
    for(int i=0; i<n; ++i)
        del_querie(ids[i]);
}

void APIENTRY_GL4ES gl4es_glBeginQuery(GLenum target, GLuint id) {
    FLUSH_BEGINEND;
    glquery_t *query = find_query(id);
	if(!query) {
		khint_t k;
		int ret;
		khash_t(queries) *list = glstate->queries.querylist;
        k = kh_put(queries, list, id, &ret);
        query = kh_value(list, k) = calloc(1, sizeof(glquery_t));
	}
	if(query->active || find_query_target(target)) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
	switch(target) {
		case GL_SAMPLES_PASSED:
		case GL_ANY_SAMPLES_PASSED:
		case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
		case GL_PRIMITIVES_GENERATED:
		case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
		case GL_TIME_ELAPSED:
			break;
		default:
			errorShim(GL_INVALID_ENUM);
			return;
	}
    query->target = target;
    query->num = 0;
	query->active = 1;
	query->start = get_clock() - glstate->queries.start;

    if (is_occlusion_target(target)) {
        load_query_funcs();
        if (occl_backend_enabled() && s_beGenQueries && s_beBeginQuery) {
            int i = occl_by_q(query);
            if (i >= 0) s_occl[i].has = 0;
            GLuint be_id = (i >= 0) ? s_occl[i].be_id : 0;
            if (!be_id) s_beGenQueries(1, &be_id);
            if (be_id) {
                if (i < 0 && occl_new(query, be_id) < 0) {      /* tabla llena */
                    if (s_beDeleteQueries) s_beDeleteQueries(1, &be_id);
                } else {
                    s_beBeginQuery(GL_ANY_SAMPLES_PASSED, be_id);
                }
            }
        }
    }

    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glEndQuery(GLenum target) {
    FLUSH_BEGINEND;
	glquery_t *query = find_query_target(target);
	if(!query) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
	switch(target) {
		case GL_SAMPLES_PASSED:
		case GL_ANY_SAMPLES_PASSED:
		case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
		case GL_PRIMITIVES_GENERATED:
		case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
		case GL_TIME_ELAPSED:
			break;
		default:
			errorShim(GL_INVALID_ENUM);
			return;
	}
    query->active = 0;
	query->start = (get_clock() - glstate->queries.start) - query->start;

    if (is_occlusion_target(target)) {
        if (occl_by_q(query) >= 0) {
            load_query_funcs();
            if (s_beEndQuery) s_beEndQuery(GL_ANY_SAMPLES_PASSED);
        }
    }

	noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;

	glquery_t *q = find_query_target(target);
	if(!q) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}

	noerrorShim();
	switch (pname) {
		case GL_CURRENT_QUERY:
			*params = (q->target==GL_TIME_ELAPSED)?q->start:q->num;
			break;
		case GL_QUERY_COUNTER_BITS:
			*params = (q->target==GL_TIME_ELAPSED)?32:0;	//no counter...
			break;
		default:
			errorShim(GL_INVALID_ENUM);
	}
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    FLUSH_BEGINEND;

	glquery_t *query = find_query(id);
	if(!query) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
    switch (pname) {
    	case GL_QUERY_RESULT_AVAILABLE:
    		*params = query_available(query) ? GL_TRUE : GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (GLint)query_result(query);
    		break;
    	default:
    		errorShim(GL_INVALID_ENUM);
			return;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    FLUSH_BEGINEND;
		
	glquery_t *query = find_query(id);
	if(!query) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}

    switch (pname) {
    	case GL_QUERY_RESULT_AVAILABLE:
    		*params = query_available(query) ? GL_TRUE : GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (GLuint)query_result(query);
    		break;
    	default:
    		errorShim(GL_INVALID_ENUM);
			return;
    }
    noerrorShim();
}

void APIENTRY_GL4ES gl4es_glQueryCounter(GLuint id, GLenum target)
{
    FLUSH_BEGINEND;
		
	glquery_t *query = find_query(id);
	if(!query) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
	if(query->active) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}
	if(target!=GL_TIMESTAMP) {
		errorShim(GL_INVALID_ENUM);
		return;
	}
	query->target = target;
	// should finish first?
	query->start = get_clock() - glstate->queries.start;
}


void APIENTRY_GL4ES gl4es_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 * params)
{
	FLUSH_BEGINEND;
		
	glquery_t *query = find_query(id);
	if(!query) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}

    switch (pname) {
    	case GL_QUERY_RESULT_AVAILABLE:
    		*params = query_available(query) ? GL_TRUE : GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (GLint64)query_result(query);
    		break;
    	default:
    		errorShim(GL_INVALID_ENUM);
			return;
    }
    noerrorShim();
}
	
void APIENTRY_GL4ES gl4es_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 * params)
{
    FLUSH_BEGINEND;
		
	glquery_t *query = find_query(id);
	if(!query) {
		errorShim(GL_INVALID_OPERATION);
		return;
	}

    switch (pname) {
    	case GL_QUERY_RESULT_AVAILABLE:
    		*params = query_available(query) ? GL_TRUE : GL_FALSE;
    		break;
		case GL_QUERY_RESULT_NO_WAIT:
    	case GL_QUERY_RESULT:
    		*params = (GLuint64)query_result(query);
    		break;
    	default:
    		errorShim(GL_INVALID_ENUM);
			return;
    }
    noerrorShim();
}
 


//Direct wrapper
AliasExport(void,glGenQueries,,(GLsizei n, GLuint * ids));
AliasExport(GLboolean,glIsQuery,,(GLuint id));
AliasExport(void,glDeleteQueries,,(GLsizei n, const GLuint* ids));
AliasExport(void,glBeginQuery,,(GLenum target, GLuint id));
AliasExport(void,glEndQuery,,(GLenum target));
AliasExport(void,glGetQueryiv,,(GLenum target, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectiv,,(GLuint id, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectuiv,,(GLuint id, GLenum pname, GLuint* params));
AliasExport(void,glQueryCounter,,(GLuint id, GLenum target));
AliasExport(void,glGetQueryObjecti64v,,(GLuint id, GLenum pname, GLint64 * params));
AliasExport(void,glGetQueryObjectui64v,,(GLuint id, GLenum pname, GLuint64 * params));

// ARB wrapper
AliasExport(void,glGenQueries,ARB,(GLsizei n, GLuint * ids));
AliasExport(GLboolean,glIsQuery,ARB,(GLuint id));
AliasExport(void,glDeleteQueries,ARB,(GLsizei n, const GLuint* ids));
AliasExport(void,glBeginQuery,ARB,(GLenum target, GLuint id));
AliasExport(void,glEndQuery,ARB,(GLenum target));
AliasExport(void,glGetQueryiv,ARB,(GLenum target, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectiv,ARB,(GLuint id, GLenum pname, GLint* params));
AliasExport(void,glGetQueryObjectuiv,ARB,(GLuint id, GLenum pname, GLuint* params));
AliasExport(void,glQueryCounter,ARB,(GLuint id, GLenum target));
