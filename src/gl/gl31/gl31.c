/*
 * gl31.c — implementacion GL 3.0/3.1 core sobre GLES 3.0/3.1.
 *
 * Skeleton: por ahora solo expone init y un flag de estado.
 * Los entry points reales (VAOs core, UBOs, texStorage, etc.) se agregan
 * en archivos hermanos dentro de este mismo directorio.
 */
#include "gl31.h"

static int g_gl31_active = 0;

void gl31_init(void) {
    g_gl31_active = 1;
}

int gl31_is_active(void) {
    return g_gl31_active;
}
