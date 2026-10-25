#!/data/data/com.termux/files/usr/bin/bash
# Instala Lorica como libGL del sistema en Termux: ocupa el lugar del libGL de Mesa.
# Uso: bash scripts/termux-install.sh [ruta/a/libGL.so.1]
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-$REPO/lib/libGL.so.1}"
DST="${PREFIX:-/data/data/com.termux/files/usr}/lib"
[ -f "$SRC" ] || { echo "FALLO: no existe $SRC (compilá primero)"; exit 1; }
N=$(nm -D --defined-only "$SRC" | grep -c " glX")
[ "$N" -ge 40 ] || { echo "FALLO: $SRC tiene solo $N símbolos glX"; exit 1; }
rm -f "$DST"/libGL.so "$DST"/libGL.so.1 "$DST"/libGL.so.1.[0-9]*
ln -s "$SRC" "$DST/libGL.so.1" && ln -s libGL.so.1 "$DST/libGL.so" || { echo "FALLO: no pude crear los enlaces"; exit 1; }
R=$(DISPLAY=:0 env -u LD_LIBRARY_PATH -u LD_PRELOAD -u LIBGL_EGL -u LIBGL_GLES -u LIBGL_FB -u LIBGL_GL glxinfo 2>/dev/null | grep -i "renderer string" | head -1)
echo "renderer: $R"
echo "$R" | grep -qi "Mali" && echo "OK: libGL del sistema = Lorica (Mali)" || { echo "FALLO: glxinfo no usó Lorica/Mali (¿termux-x11 abierto?)"; exit 1; }
