#!/bin/bash
# Compila Lorica con gcc sin cmake (replica src/CMakeLists.txt para Linux con X11).
# Uso: tools/build_linux.sh [directorio_salida] [CFLAGS extra...]
set -u
export ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT=${1:-$ROOT/build_linux}; shift || true
EXTRA="$*"
mkdir -p "$OUT/obj"
SRCS=$(python3 - <<'PY'
import os, re
root = os.environ["ROOT"]
t = open(os.path.join(root, "src/CMakeLists.txt")).read()
seen = []
for s in re.findall(r'\$\{CMAKE_CURRENT_SOURCE_DIR\}/([\w/\.]+\.c)', t):
    if s.startswith(("egl/", "agl/")): continue
    if s not in seen: seen.append(s)
print(" ".join(seen))
PY
)
DEFS="-DDEFAULT_ES=2 -DNO_GBM -DHAS_BACKTRACE"
CF="-std=gnu11 -O1 -g -fPIC -funwind-tables -fvisibility=hidden -Wall -Wno-unused-function -Wno-unused-variable -Wno-pointer-sign -Wno-dangling-else $DEFS -I$ROOT/include -I$ROOT/src/glx -I$ROOT/src/gl $EXTRA"
fail=0; objs=""
: > "$OUT/warnings.log"
for s in $SRCS; do
  o="$OUT/obj/$(echo $s | tr '/' '_').o"
  inc=""
  case $s in gl/GLADIATOR/gl31*) inc="-I$ROOT/src/gl/GLADIATOR/third_party/khronos";; esac
  gcc $CF $inc -c "$ROOT/src/$s" -o "$o" 2>>"$OUT/warnings.log" || { echo "FALLO: $s"; fail=1; }
  objs="$objs $o"
done
echo "fuentes: $(echo $SRCS | wc -w), fallos: $fail, warnings: $(grep -c 'warning:' "$OUT/warnings.log")"
[ $fail = 0 ] && gcc -shared -o "$OUT/libGL.so.1" $objs -lX11 -lm -ldl -lpthread 2>"$OUT/link.log" \
  && echo "OK -> $OUT/libGL.so.1" || { echo "ENLACE/COMPILACION FALLO"; head -20 "$OUT/link.log" 2>/dev/null; }
