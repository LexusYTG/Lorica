# Uso: source lorica-env.sh   (con termux-x11 abierto)
export DISPLAY=:0
export LD_LIBRARY_PATH=$HOME/Lorica/lib:$LD_LIBRARY_PATH
export LIBGL_EGL=/system/lib64/libEGL.so LIBGL_GLES=/system/lib64/libGLESv2.so
export LIBGL_FB=3 LIBGL_GL=43
