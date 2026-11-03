/* gl31_vk.h - GLES (base) + Vulkan (apoyo) dentro de GLADIATOR.
 * Vulkan hace de coprocesador de GPU para lo que GLES no expresa; la memoria y la
 * sincronizacion se comparten con GLES (sync fd / memoria externa), no con la CPU. */
#ifndef LORICA_GL31_VK_H
#define LORICA_GL31_VK_H
#include "gl31.h"

/* El loader con el que se inicio GLADIATOR (para resolver EGL). */
void  gl31_vk_set_loader(gl31_loader_fn loader);

/* ---- sincronizacion GL -> Vulkan ---- */
/* Fence nativo de la GPU con todo lo enviado hasta ahora (EGL_ANDROID_native_fence_sync).
 * Devuelve un sync fd (>= 0) o -1 si la plataforma no lo ofrece; en ese caso el llamador
 * debe hacer glFinish antes de pedir trabajo a Vulkan. */
int   gl31_vk_gl_fence_fd(void);
/* Deja a GL "al dia" para Vulkan: devuelve el fd (>=0) o -1 tras haber hecho glFinish. */
int   gl31_vk_gl_sync_for_vk(void);

/* ---- queries de tiempo sobre timestamps de Vulkan ---- */
int   gl31_vkq_ready(void);                 /* 1 si Vulkan 1.1 con timestamps esta usable */
void  gl31_vkq_counter(GLuint id);          /* glQueryCounter(id, GL_TIMESTAMP) */
void  gl31_vkq_begin(GLuint id);            /* glBeginQuery(GL_TIME_ELAPSED) */
void  gl31_vkq_end(GLuint id);              /* glEndQuery(GL_TIME_ELAPSED) */
int   gl31_vkq_owns(GLuint id);
/* want_result: 0 = disponibilidad (*out = 0/1), 1 = resultado (bloquea hasta tenerlo) */
void  gl31_vkq_get(GLuint id, int want_result, GLuint64* out);
void  gl31_vkq_delete(GLuint id);
void  gl31_vkq_reset(void);
void  gl31_vkq_finish(void);                /* espera a que todos los marcadores pendientes terminen */
GLint64 gl31_vkq_now(void);                 /* glGetInteger64v(GL_TIMESTAMP) */
#endif
