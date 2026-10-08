#ifndef _GL4ES_GL31_BRIDGE_H_
#define _GL4ES_GL31_BRIDGE_H_

/* Puente GL4ES <-> GLADIATOR (gl31_*), pasos 1 y 2 de la integracion:
 *   1. contextos EGL de ES 3.x cuando la app pide GL >= 3.1
 *   2. arranque / parada de GLADIATOR (gl31_init / gl31_shutdown) con un cargador del backend GLES
 * El despacho por contexto cubre solo glGetProcAddress (paso 3); el framebuffer por defecto (paso 4)
 * se resuelve dejando LIBGL_FB=2 en ES2 (ver gl4es_gl31_create_backend_context). */

#ifndef NOEGL
#include <EGL/egl.h>

/* GL version (major*10+minor) a partir de la cual el contexto de backend pasa a ES 3.x */
#define GL31_BRIDGE_MIN_GL_VERSION 31

/* Crea el EGLContext de backend para un contexto GLX.
 *   gl_version : version GL negociada (0 = legacy)
 *   es2only    : contexto pedido con perfil ES2
 *   configs / nconfigs / *cfgidx : configs devueltas por eglChooseConfig; *cfgidx se cambia
 *                si hace falta otra con el bit ES3
 *   *es_major / *es_minor : version ES del contexto creado (2.0 si cae a ES2)
 * Con gl_version < 31 (o es2only, o backend ES1, o LIBGL_FB=2 / globals4es.usefbo) se comporta exactamente
 * como antes: ES2. */
EGLContext gl4es_gl31_create_backend_context(EGLDisplay dpy, EGLConfig* configs, int nconfigs, int* cfgidx,
                                             EGLContext shared, int gl_version, int es2only,
                                             int* es_major, int* es_minor);

/* Cuenta de contextos ES3 vivos (decide cuando se apaga GLADIATOR) */
void gl4es_gl31_bridge_retain(void);
void gl4es_gl31_bridge_release(void);   /* llamar con el contexto que se destruye aun activo */

/* Arranca GLADIATOR si el contexto EGL actual es ES3+. Devuelve 1 si GLADIATOR esta listo.
 * Barato si ya esta iniciado o si el contexto actual es ES2. */
int gl4es_gl31_bridge_ensure(void);

int gl4es_gl31_bridge_ready(void);

/* Paso 3: glGetProcAddress. Funcion gl31_* para `name` si el contexto GLX actual es core GL>=3.1
 * con backend ES 3.x y GLADIATOR iniciado; NULL si no (hay que seguir con la tabla de GL4ES). */
void* gl4es_gl31_proc_lookup(const char* name);

/* Paso 3b: repunta los simbolos GL exportados (GLADIATOR con contexto core GL>=3.1, GL4ES si no).
 * Llamar tras cada cambio del contexto GLX actual. Barato si el modo no cambia. */
void gl4es_gl31_dispatch_update(void);
#endif /* NOEGL */

#endif
