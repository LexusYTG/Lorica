/*
 * gl31.h — capa de traduccion GL 3.0/3.1 core -> GLES 3.0/3.1
 *
 * Modulo nuevo dentro de Lorica. Reemplaza el camino fixed-function (FPE)
 * de gl4es cuando la app pide version 3.0+ via LIBGL_GL=30/31.
 *
 * No reemplaza al FPE: coexisten. La decision de cual usar se toma en
 * init.c comparando globals4es.gl >= 30.
 *
 * Estado: skeleton.
 */
#ifndef _GL31_H_
#define _GL31_H_

void gl31_init(void);
int  gl31_is_active(void);

#endif
