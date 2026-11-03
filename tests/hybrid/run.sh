#!/bin/bash
# Pruebas de la capa GLES+Vulkan. Necesita Mesa (EGL surfaceless, llvmpipe) y un Vulkan 1.1+
# (en contenedores sin GPU: LORICA_VK_LIB=/ruta/libvk_swiftshader.so).
set -e
cd "$(dirname "$0")"; R=../..; G=$R/src/gl/GLADIATOR
L=${LIBDIR:-/usr/lib/x86_64-linux-gnu}
build() { gcc -std=gnu11 -g -w -ftrivial-auto-var-init=pattern -I$G -I$G/third_party/khronos -I$R/include "$1" $G/gl31*.c $R/src/vk/lvk.c -o "${1%.c}" \
          $L/libEGL.so.1 $L/libGLESv2.so.2 -ldl -lpthread -lm; }
export EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1
gcc -std=gnu11 -g -w test_lvk.c $R/src/vk/lvk.c -o test_lvk -ldl -lpthread && ./test_lvk
build test_timer.c && LORICA_VK_TIMER=1 ./test_timer
build test_bufstore.c && LORICA_BUFSTORE=native ./test_bufstore && ./test_bufstore
build test_lu.c && ./test_lu
build test_idx.c
for t in restart poly prims tf prov2; do ./test_idx $t; done
build test_pixels.c && ./test_pixels
build test_gl4misc.c && ./test_gl4misc
build test_fallback.c && ./test_fallback
build test_compressed.c && ./test_compressed
build test_decode.c && ./test_decode
