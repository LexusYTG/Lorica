#!/data/data/com.termux/files/usr/bin/bash
# Lorica: installer for Termux.
#   1) checks the environment   2) builds if needed   3) verifies integrity
#   4) installs Lorica as the system libGL (starts termux-x11 if it is installed)
#   5) asks whether to test it and, if so, builds the tests, runs them in a
#      chain and shows the results. The only test used is TEST/LoriTest.c.
#
# Usage: bash scripts/termux-install.sh [options] [path/to/libGL.so.1]
#   --rebuild       rebuild from scratch
#   -y, --tests     run the tests without asking
#   -n, --no-tests  neither run the tests nor ask
#   --no-install    do not replace the system libGL
#   --no-x11        do not try to launch termux-x11
#   -h, --help      this help
# If you pass the path to an already built libGL.so.1, the build is skipped.

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PFX="${PREFIX:-/data/data/com.termux/files/usr}"
DST="$PFX/lib"
BUILD="$REPO/build"
LIB="$REPO/lib/libGL.so.1"
REBUILD=0; ASK=ask; DO_INSTALL=1; START_X=1; LIB_ARG=""
FAILED=0; TD=""

while [ $# -gt 0 ]; do
  case "$1" in
    --rebuild) REBUILD=1 ;;
    -y|--tests) ASK=yes ;;
    -n|--no-tests) ASK=no ;;
    --no-install) DO_INSTALL=0 ;;
    --no-x11) START_X=0 ;;
    -h|--help) sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "Unknown option: $1 (try --help)"; exit 2 ;;
    *) LIB_ARG="$1" ;;
  esac
  shift
done
[ -n "$LIB_ARG" ] && LIB="$LIB_ARG"

# ───────────────────────── Presentation ─────────────────────────
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
  TTY=1
  C_RST=$'\e[0m'; C_B=$'\e[1m'; C_DIM=$'\e[2m'
  C_RED=$'\e[31m'; C_GRN=$'\e[32m'; C_YEL=$'\e[33m'; C_CYN=$'\e[36m'; C_MAG=$'\e[35m'
else
  TTY=0; C_RST=; C_B=; C_DIM=; C_RED=; C_GRN=; C_YEL=; C_CYN=; C_MAG=
fi
COLS="${COLUMNS:-$(tput cols 2>/dev/null || echo 60)}"

ok()   { printf '  %s✔%s %s\n' "$C_GRN" "$C_RST" "$*"; }
bad()  { printf '  %s✘%s %s\n' "$C_RED" "$C_RST" "$*"; FAILED=1; }
warn() { printf '  %s⚠%s %s\n' "$C_YEL" "$C_RST" "$*"; }
info() { printf '  %s•%s %s%s%s\n' "$C_DIM" "$C_RST" "$C_DIM" "$*" "$C_RST"; }
step() { printf '\n%s%s[%s/5]%s %s%s%s\n' "$C_B" "$C_MAG" "$1" "$C_RST" "$C_B" "$2" "$C_RST"; }

banner() {
  printf '\n%s  ┌─ LORICA ─────────────────────────────%s\n' "$C_CYN" "$C_RST"
  printf '%s  │%s  OpenGL 4.3 over GLES · Termux\n' "$C_CYN" "$C_RST"
  printf '%s  └──────────────────────────────────────%s\n' "$C_CYN" "$C_RST"
}

bar() {   # bar <pct> <etiqueta>
  local w=24 f=$(( $1 * 24 / 100 )) i s="" max=$(( COLS - 38 ))
  [ "$max" -lt 8 ] && max=8
  for ((i = 0; i < w; i++)); do if ((i < f)); then s+="█"; else s+="░"; fi; done
  printf '\r  %s%s%s %3d%%  %s%s%s\e[K' "$C_CYN" "$s" "$C_RST" "$1" "$C_DIM" "${2:0:$max}" "$C_RST"
}

LOG=""; ELAPSED=0
run_progress() {   # run_progress <command...>: shows a progress bar and stores the output in $LOG
  local line rc=1 pct=0 cur="" t0=$SECONDS
  local re_pct='^\[ *([0-9]+)%\]' re_ninja='^\[([0-9]+)/([0-9]+)\]'
  LOG=""
  while IFS= read -r line; do
    case "$line" in @@RC:*) rc="${line#@@RC:}"; continue ;; esac
    LOG+="$line"$'\n'
    if [[ $line =~ $re_pct ]]; then pct="${BASH_REMATCH[1]}"
    elif [[ $line =~ $re_ninja ]]; then pct=$(( BASH_REMATCH[1] * 100 / BASH_REMATCH[2] )); fi
    case "$line" in *Building*|*Linking*) cur="${line##*/}"; cur="${cur%.o}" ;; esac
    [ "$TTY" = 1 ] && bar "$pct" "$cur"
  done < <("$@" 2>&1; echo "@@RC:$?")
  [ "$TTY" = 1 ] && printf '\r\e[K'
  ELAPSED=$(( SECONDS - t0 ))
  return "$rc"
}

show_log() {   # last lines of $LOG, to understand a failure
  printf '%s' "$LOG" | tail -n 25 | sed "s/^/    ${C_DIM}│${C_RST} /"
}

ask_yn() {   # ask_yn <question>: default answer = no
  local a
  [ "$ASK" = yes ] && return 0
  [ "$ASK" = no ] && return 1
  [ -t 0 ] || return 1
  printf '\n  %s?%s %s%s%s %s[y/N]%s ' "$C_YEL" "$C_RST" "$C_B" "$1" "$C_RST" "$C_DIM" "$C_RST"
  read -r a
  case "${a,,}" in y|yes) return 0 ;; esac
  return 1
}

cleanup() { [ -n "$TD" ] && [ -d "$TD" ] && rm -rf "$TD"; [ "$TTY" = 1 ] && printf '\e[?25h'; }
trap cleanup EXIT
trap 'printf "\n"; warn "interrupted"; exit 130' INT TERM

x_up() {   # is an X server (termux-x11) answering? Prefers a real connection (the socket may be stale)
  local dsp="${DISPLAY:-:0}" d
  d="${dsp#:}"; d="${d%%.*}"
  if command -v xdpyinfo >/dev/null 2>&1; then DISPLAY="$dsp" timeout 5 xdpyinfo >/dev/null 2>&1; return; fi
  if command -v xset >/dev/null 2>&1; then DISPLAY="$dsp" timeout 5 xset q >/dev/null 2>&1; return; fi
  if command -v glxinfo >/dev/null 2>&1; then DISPLAY="$dsp" timeout 5 glxinfo -B >/dev/null 2>&1; return; fi
  [ -S "$PFX/tmp/.X11-unix/X$d" ]
}

X_TRIED=0
ensure_x() {   # checks for an X server; if none is running and termux-x11 is installed, launches it
  export DISPLAY="${DISPLAY:-:0}"
  x_up && { ok "X server already running ($DISPLAY)"; return 0; }
  [ "$X_TRIED" = 1 ] && return 1
  X_TRIED=1
  if [ "$START_X" = 0 ]; then info "X11 launch skipped (--no-x11)"; return 1; fi
  if ! command -v termux-x11 >/dev/null 2>&1; then
    info "termux-x11 is not installed (pkg install -y x11-repo termux-x11-nightly)"
    return 1
  fi
  info "launching termux-x11 on $DISPLAY..."
  nohup termux-x11 "$DISPLAY" >/dev/null 2>&1 &
  disown 2>/dev/null
  # bring the Termux:X11 app to the foreground, if it is available
  command -v am >/dev/null 2>&1 && am start --user 0 -n com.termux.x11/com.termux.x11.MainActivity >/dev/null 2>&1
  local i
  for i in $(seq 1 20); do
    x_up && { ok "termux-x11 started on $DISPLAY"; return 0; }
    sleep 1
  done
  warn "termux-x11 did not respond after 20 s"
  return 1
}

# ───────────────────────── 1/5 Environment ─────────────────────────
banner
step 1 "Checking the environment"
MISS=""
for c in cmake make nm sed grep find; do command -v "$c" >/dev/null 2>&1 || MISS="$MISS $c"; done
CC_BIN="${CC:-$(command -v clang || command -v gcc || command -v cc)}"
[ -n "$CC_BIN" ] || MISS="$MISS clang"
if [ -n "$MISS" ] && [ -z "$LIB_ARG" ]; then
  bad "missing tools:$MISS"
  info "install them with: pkg install -y cmake make clang binutils"
  exit 1
fi
JOBS="$(nproc 2>/dev/null || echo 4)"; [ "$JOBS" -gt 4 ] && JOBS=4
ok "tools OK (compiler: $(basename "$CC_BIN"), -j$JOBS)"
[ -d "$DST" ] || { bad "$DST does not exist (are you in Termux?)"; exit 1; }
ok "library destination: $DST"

# ───────────────────────── 2/5 Build ─────────────────────────
step 2 "Build"
needs_build() {
  [ -n "$LIB_ARG" ] && return 1
  [ "$REBUILD" = 1 ] && { WHY="you asked for --rebuild"; return 0; }
  [ -f "$LIB" ] || { WHY="it has not been built yet"; return 0; }
  local newer
  newer="$(find "$REPO/src" "$REPO/include" "$REPO/CMakeLists.txt" -newer "$LIB" -type f -print -quit 2>/dev/null)"
  [ -n "$newer" ] && { WHY="${newer#"$REPO"/} changed"; return 0; }
  return 1
}

if [ -n "$LIB_ARG" ]; then
  ok "using the given library: $LIB"
elif needs_build; then
  info "build needed: $WHY"
  CLEAN=(); [ "$REBUILD" = 1 ] && CLEAN=(--clean-first)
  mkdir -p "$BUILD"
  if ! run_progress cmake -S "$REPO" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
        -DNOX11=OFF -DNOEGL=OFF -DSTATICLIB=OFF -DGBM=OFF; then
    bad "cmake failed"; show_log; exit 1
  fi
  ok "configured in ${ELAPSED}s"
  if ! run_progress cmake --build "$BUILD" -j"$JOBS" "${CLEAN[@]}"; then
    bad "build failed"; show_log; exit 1
  fi
  ok "built in ${ELAPSED}s"
else
  ok "already built and up to date: ${LIB#"$REPO"/}"
fi

# ───────────────────────── 3/5 Integrity ─────────────────────────
step 3 "Verifying integrity"
check_lib() {
  local syms n s missing=""
  [ -s "$LIB" ] || { bad "missing or empty: $LIB"; return 1; }
  [ "$(head -c4 "$LIB" | od -An -c | tr -d ' \n')" = '177ELF' ] || { bad "not an ELF binary"; return 1; }
  ok "ELF format"
  if [ "$(uname -m)" = aarch64 ] && command -v readelf >/dev/null 2>&1; then
    readelf -h "$LIB" 2>/dev/null | grep -q AArch64 || { bad "not AArch64"; return 1; }
    ok "AArch64 architecture"
  fi
  syms="$(nm -D --defined-only "$LIB" 2>/dev/null)"
  n="$(printf '%s\n' "$syms" | grep -c ' glX')"
  [ "$n" -ge 40 ] || { bad "only $n glX symbol(s) exported; 40 or more expected"; return 1; }
  ok "$n glX symbols exported"
  for s in glXChooseVisual glXCreateContext glXDestroyContext glXMakeCurrent glXSwapBuffers \
           glXGetProcAddress glXChooseFBConfig glXCreateContextAttribsARB \
           glGetString glClear glDrawArrays glReadPixels; do
    printf '%s\n' "$syms" | grep -q " $s\$" || missing="$missing $s"
  done
  [ -z "$missing" ] || { bad "missing key symbols:$missing"; return 1; }
  ok "key API present (contexts, FBConfig, swap, drawing)"
  local sum=""
  command -v sha256sum >/dev/null 2>&1 && sum="$(sha256sum "$LIB" | cut -c1-12)"
  info "size $(du -hL "$LIB" | cut -f1)${sum:+ · sha256 ${sum}…}"
}
check_lib || { printf '\n  %sStopping: the library failed verification.%s\n' "$C_RED" "$C_RST"; exit 1; }

# ───────────────────────── 4/5 Installation ─────────────────────────
step 4 "Installing as the system libGL"
LIBDIR="$(cd "$(dirname "$LIB")" && pwd)"; LIB="$LIBDIR/$(basename "$LIB")"
if [ "$DO_INSTALL" = 0 ]; then
  warn "skipped (--no-install): the tests will use LD_LIBRARY_PATH"
else
  cur="$(readlink "$DST/libGL.so.1" 2>/dev/null)"
  if [ "$cur" = "$LIB" ] && [ "$(readlink "$DST/libGL.so" 2>/dev/null)" = libGL.so.1 ]; then
    ok "already installed: libGL.so.1 → Lorica"
  else
    if ln -sfn "$LIB" "$DST/libGL.so.1" && ln -sfn libGL.so.1 "$DST/libGL.so"; then
      rm -f "$DST"/libGL.so.1.[0-9]*
      ok "libGL.so.1 y libGL.so → ${LIB#"$HOME"/}"
    else
      bad "could not create the links in $DST"; exit 1
    fi
  fi
  if ensure_x && command -v glxinfo >/dev/null 2>&1; then
    R="$(DISPLAY="${DISPLAY:-:0}" env -u LD_LIBRARY_PATH -u LD_PRELOAD -u LIBGL_EGL -u LIBGL_GLES -u LIBGL_FB -u LIBGL_GL \
         timeout 10 glxinfo 2>/dev/null | grep -i 'renderer string' | head -1 | sed 's/^[^:]*: *//')"
    case "$R" in
      "") bad "glxinfo returned no renderer" ;;
      *llvmpipe*|*softpipe*|*swrast*) bad "glxinfo is still using the software Mesa libGL: $R" ;;
      *) ok "the system now uses Lorica · renderer: ${C_B}$R${C_RST}" ;;
    esac
    [ "$FAILED" = 1 ] && exit 1
  else
    warn "no X server (termux-x11) or no glxinfo: could not check the renderer"
  fi
fi

# ───────────────────────── 5/5 Tests ─────────────────────────
step 5 "Tests"
RES_N=(); RES_S=(); RES_T=(); RES_D=()
add_res() { RES_N+=("$1"); RES_S+=("$2"); RES_T+=("$3"); RES_D+=("$4"); }
now_ms() { echo $(( $(date +%s%N) / 1000000 )); }
fmt_ms() { printf '%d.%d s' $(( $1 / 1000 )) $(( ($1 % 1000) / 100 )); }

run_tests() {
  local spec name src args envs bin t0 out rc st det fps i
  local specs=(
    "LoriTest 4.3 (with fallback)|LoriTest.c|--test|LIBGL_GL=43"
    "LoriTest 3.3|LoriTest.c|--test 3 3|LIBGL_GL=43"
  )
  TD="$(mktemp -d "${TMPDIR:-$PFX/tmp}/lorica-tests.XXXXXX")" || { bad "could not create a temporary directory"; return 1; }
  export DISPLAY="${DISPLAY:-:0}"
  local TMO=""; command -v timeout >/dev/null 2>&1 && TMO="timeout 60"

  printf '\n  %sBuilding tests%s\n' "$C_B" "$C_RST"
  local -A tried=()
  for spec in "${specs[@]}"; do
    IFS='|' read -r name src args envs <<<"$spec"
    bin="${src%.c}"
    [ -n "${tried[$src]:-}" ] && continue   # several tests can share the same .c file
    tried[$src]=1
    if out="$("$CC_BIN" -O1 -w "$REPO/TEST/$src" -o "$TD/$bin" -I"$REPO/include" \
                 -L"$LIBDIR" -l:libGL.so.1 -lX11 -lm 2>&1)"; then
      ok "$src"
    else
      bad "$src failed to compile"; printf '%s\n' "$out" | tail -n 6 | sed "s/^/    ${C_DIM}│${C_RST} /"
      rm -f "$TD/$bin"
    fi
  done

  printf '\n  %sRunning in a chain%s\n' "$C_B" "$C_RST"
  for spec in "${specs[@]}"; do
    IFS='|' read -r name src args envs <<<"$spec"
    bin="${src%.c}"
    if [ ! -x "$TD/$bin" ]; then add_res "$name" fail 0 "did not compile"; continue; fi
    t0="$(now_ms)"
    # shellcheck disable=SC2086
    out="$(env LD_LIBRARY_PATH="$LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ${envs:+$envs} $TMO "$TD/$bin" $args 2>&1)"; rc=$?
    t0=$(( $(now_ms) - t0 ))
    st=fail; det=""
    # LoriTest in --test mode: exit 0 and "RESULTADO: TODO OK" means pass
    if [ "$rc" = 0 ] && printf '%s' "$out" | grep -Eq 'RESULTADO: TODO OK|RESULT: ALL OK'; then
      st=ok
      det="$(printf '%s\n' "$out" | grep -m1 '^USANDO:' | sed 's/^USANDO: *//')"
      [ -n "$det" ] || det="$(printf '%s\n' "$out" | grep -m1 '^GL_VERSION=' | sed 's/^GL_VERSION=//')"
      fps="$(printf '%s\n' "$out" | grep -m1 '^FPS_PROMEDIO=' | sed 's/^FPS_PROMEDIO=//')"
      [ -n "$fps" ] && det="${det:+$det · }$fps FPS"
    fi
    printf '%s' "$out" | grep -Eq 'AHB cero-copia activo|AHB zero-copy active' && det="${det:+$det · }AHB zero-copy"
    if [ "$st" = fail ]; then
      det="$(printf '%s\n' "$out" | grep -E 'FALLO|FAIL|sin X|no X|no display|no visual|no context|Error|error' | head -2 | tr '\n' ' ')"
      [ -n "$det" ] || det="exited with code $rc"
    fi
    add_res "$name" "$st" "$t0" "$det"
    if [ "$st" = ok ]; then ok "$name"; else bad "$name"; fi
  done
}

TESTS_RAN=0
if ask_yn "Do you want to build and run the tests?"; then
  if ! ensure_x; then
    warn "the tests need termux-x11 running (for example: termux-x11 :0 &)"
    add_res "Tests" skip 0 "no X server"
  elif [ ! -f "$PFX/include/X11/Xlib.h" ]; then
    warn "X11 headers are missing: pkg install -y libx11"
    add_res "Tests" skip 0 "missing X11 headers"
  else
    run_tests && TESTS_RAN=1
  fi
else
  info "tests skipped"
fi

# ───────────────────────── Summary ─────────────────────────
if [ "${#RES_N[@]}" -gt 0 ]; then
  nok=0; ntot=0; i=0
  printf '\n%s  ┌─ RESULTS ─────────────────────────%s\n' "$C_CYN" "$C_RST"
  for i in "${!RES_N[@]}"; do
    case "${RES_S[$i]}" in
      ok)   ic="${C_GRN}✔${C_RST}"; nok=$(( nok + 1 )); ntot=$(( ntot + 1 )) ;;
      fail) ic="${C_RED}✘${C_RST}"; ntot=$(( ntot + 1 )); FAILED=1 ;;
      *)    ic="${C_YEL}–${C_RST}" ;;
    esac
    printf '%s  │%s %s  %-24s %s%s%s\n' "$C_CYN" "$C_RST" "$ic" "${RES_N[$i]}" "$C_DIM" "$(fmt_ms "${RES_T[$i]}")" "$C_RST"
    [ -n "${RES_D[$i]}" ] && printf '%s  │%s     %s%s%s\n' "$C_CYN" "$C_RST" "$C_DIM" "${RES_D[$i]}" "$C_RST"
  done
  printf '%s  └──────────────────────────────────────%s\n' "$C_CYN" "$C_RST"
  if [ "$ntot" -gt 0 ]; then
    if [ "$nok" = "$ntot" ]; then printf '\n  %s%s✔ %s/%s tests OK%s\n' "$C_B" "$C_GRN" "$nok" "$ntot" "$C_RST"
    else printf '\n  %s%s✘ %s/%s tests OK%s\n' "$C_B" "$C_RED" "$nok" "$ntot" "$C_RST"; fi
  fi
fi

printf '\n'
if [ "$FAILED" = 0 ]; then
  if [ "$DO_INSTALL" = 1 ]; then WHAT="built, verified and installed"; else WHAT="built and verified (not installed)"; fi
  printf '  %s%sDone.%s Lorica is %s.\n' "$C_B" "$C_GRN" "$C_RST" "$WHAT"
  info "try it with: DISPLAY=:0 glxinfo | grep -i renderer   (or glmark2)"
  [ "$TESTS_RAN" = 0 ] && info "to run the tests later: bash scripts/termux-install.sh -y"
  exit 0
else
  printf '  %s%sFinished with failures.%s Check the ✘ marks above.\n' "$C_B" "$C_RED" "$C_RST"
  exit 1
fi
