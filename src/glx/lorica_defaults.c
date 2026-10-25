/* Defaults de Lorica para Termux/Android: lo que funciona ahi queda activo sin variables.
 * setenv(..., 0): si el usuario define la variable, su valor manda.
 * Lo llama initialize_gl4es() (src/gl/init.c) antes de leer cualquier LIBGL_*. */
typedef int lorica_defaults_unit;
#ifdef __ANDROID__
#include <stdlib.h>
#include <unistd.h>
void lorica_android_defaults(void) {
    const char *egl = "/system/lib64/libEGL.so", *gles = "/system/lib64/libGLESv2.so";
    if (access(egl, R_OK) != 0) { egl = "/system/lib/libEGL.so"; gles = "/system/lib/libGLESv2.so"; }
    if (access(egl, R_OK) == 0 && access(gles, R_OK) == 0) {
        setenv("LIBGL_EGL", egl, 0);
        setenv("LIBGL_GLES", gles, 0);
    }
    setenv("LIBGL_FB", "3", 0);
    setenv("LIBGL_GL", "43", 0);
}
#endif
