/* cubo_rubik.c - prueba unica de Lorica: cubo de Rubik 3D que se arma mientras gira, con contador de FPS
 * arriba a la derecha. Usa un contexto core por GLX (glXCreateContextAttribsARB), shaders GLSL 3.30,
 * VAO/VBO, depth test y swap: si se ve el cubo, Lorica funciona de punta a punta.
 *
 * Escena: pared de ladrillos 3D al fondo + una linterna 3D que cada 10 s entra, da una vuelta
 * alrededor del cubo y se va (foco con sombras por shadow map, PCF 3x3, en GL >= 3.3).
 * Pide GL 4.3 y va bajando (4.3 ... 3.0, 2.1, 2.0, 1.5 ... 1.0) hasta que uno funcione.
 * GL >= 3.3: shaders GLSL 3.30. GL < 3.3: tubería fija (glBegin/glEnd), sin shaders.
 * Uso:   ./cubo_rubik [MAJOR MINOR]          interactivo (MAJOR MINOR = version maxima a probar) (Esc, q o cerrar la ventana para salir)
 *        ./cubo_rubik --test [MAJOR MINOR]   un ciclo de armado acelerado, verifica y sale con 0 (ok) o 1
 * Compilar: gcc cubo_rubik.c -o cubo_rubik -I../include -L../lib -l:libGL.so.1 -lX11 -lm
 * Ejecutar: LD_LIBRARY_PATH=../lib LIBGL_GL=43 ./cubo_rubik */
#define GL_GLEXT_PROTOTYPES 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

typedef GLXContext (*cca_t)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int fails = 0;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while(0)

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ---------- matematica 4x4 (column-major, como GL) ---------- */
static void m_ident(float *m) { memset(m, 0, 16 * sizeof(float)); m[0] = m[5] = m[10] = m[15] = 1.0f; }
static void m_mul(float *o, const float *a, const float *b) {
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a[k * 4 + rr] * b[c * 4 + k];
            r[c * 4 + rr] = s;
        }
    memcpy(o, r, sizeof r);
}
static void m_persp(float *m, float fovy, float asp, float n, float f) {
    float t = 1.0f / tanf(fovy * 0.5f);
    memset(m, 0, 16 * sizeof(float));
    m[0] = t / asp; m[5] = t; m[10] = (f + n) / (n - f); m[11] = -1.0f; m[14] = 2.0f * f * n / (n - f);
}
static void m_trans(float *m, float x, float y, float z) { m_ident(m); m[12] = x; m[13] = y; m[14] = z; }
static void m_rot(float *m, float x, float y, float z, float a) {
    float l = sqrtf(x * x + y * y + z * z);
    if (l < 1e-6f) { m_ident(m); return; }
    x /= l; y /= l; z /= l;
    float c = cosf(a), s = sinf(a), t = 1.0f - c;
    m_ident(m);
    m[0] = t * x * x + c;     m[1] = t * x * y + s * z; m[2]  = t * x * z - s * y;
    m[4] = t * x * y - s * z; m[5] = t * y * y + c;     m[6]  = t * y * z + s * x;
    m[8] = t * x * z + s * y; m[9] = t * y * z - s * x; m[10] = t * z * z + c;
}


/* ---------- shaders (GL >= 3.3) ---------- */
/* Iluminacion comun: luz fija suave (key) + foco de la linterna con sombra (shadow map) */
static const char *GLSL_COMMON =
    "uniform vec3 uLPos;\n"
    "uniform vec3 uLDir;\n"
    "uniform float uLInt;\n"
    "uniform float uKey;\n"
    "uniform vec3 uCam;\n"
    "uniform mat4 uLVP;\n"
    "uniform sampler2D uSM;\n"
    "uniform float uShOn;\n"
    "uniform float uSMInv;\n"
    "float shadowAt(vec3 wp, vec3 n, vec3 Ld) {\n"
    "    if (uShOn < 0.5) return 1.0;\n"
    "    vec4 lp = uLVP * vec4(wp + n * 0.03, 1.0);\n"
    "    vec3 pr = lp.xyz / lp.w * 0.5 + 0.5;\n"
    "    if (pr.z > 1.0 || pr.x < 0.0 || pr.x > 1.0 || pr.y < 0.0 || pr.y > 1.0) return 1.0;\n"
    "    float bias = 0.0008 + 0.002 * (1.0 - dot(n, Ld));\n"
    "    float s = 0.0;\n"
    "    for (int x = -1; x <= 1; x++)\n"
    "        for (int y = -1; y <= 1; y++) {\n"
    "            float d = textureLod(uSM, pr.xy + vec2(float(x), float(y)) * uSMInv, 0.0).r;\n"
    "            s += (pr.z - bias > d) ? 0.0 : 1.0;\n"
    "        }\n"
    "    return s / 9.0;\n"
    "}\n"
    "vec3 lighting(vec3 col, vec3 wp, vec3 n, float shin) {\n"
    "    vec3 K = normalize(vec3(0.5, 0.8, 0.9));\n"
    "    float dk = max(dot(n, K), 0.0);\n"
    "    vec3 c = col * ((0.08 + 0.27 * uKey) + 0.75 * uKey * dk);\n"
    "    if (uLInt > 0.001) {\n"
    "        vec3 Lv = uLPos - wp;\n"
    "        float dist = length(Lv);\n"
    "        vec3 Ld = Lv / dist;\n"
    "        float sp = smoothstep(0.8823, 0.9611, dot(-Ld, uLDir));\n"
    "        float att = 1.0 / (1.0 + 0.05 * dist + 0.012 * dist * dist);\n"
    "        float nl = max(dot(n, Ld), 0.0);\n"
    "        float sh = (sp > 0.0 && nl > 0.0) ? shadowAt(wp, n, Ld) : 1.0;\n"
    "        vec3 H = normalize(Ld + normalize(uCam - wp));\n"
    "        float spec = pow(max(dot(n, H), 0.0), shin) * 0.5;\n"
    "        vec3 lc = vec3(1.0, 0.92, 0.75) * uLInt * sp * att * sh;\n"
    "        c += col * lc * nl * 3.5 + lc * spec * nl;\n"
    "    }\n"
    "    return c;\n"
    "}\n";

static const char *VS3D =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNor;\n"
    "layout(location=2) in float aFace;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "out vec3 vLocal;\n"
    "out vec3 vNor;\n"
    "out vec3 vW;\n"
    "out float vFace;\n"
    "void main() {\n"
    "    vLocal = aPos;\n"
    "    vNor = mat3(uModel) * aNor;\n"
    "    vW = (uModel * vec4(aPos, 1.0)).xyz;\n"
    "    vFace = aFace;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char *FS3D_BODY =
    "in vec3 vLocal;\n"
    "in vec3 vNor;\n"
    "in vec3 vW;\n"
    "in float vFace;\n"
    "uniform vec3 uCol[6];\n"
    "out vec4 oColor;\n"
    "void main() {\n"
    "    int f = int(vFace + 0.5);\n"
    "    vec3 base = f == 0 ? uCol[0] : f == 1 ? uCol[1] : f == 2 ? uCol[2] : f == 3 ? uCol[3] : f == 4 ? uCol[4] : uCol[5];\n"
    "    vec3 a = abs(vLocal);\n"
    "    float m = f < 2 ? max(a.y, a.z) : (f < 4 ? max(a.x, a.z) : max(a.x, a.y));\n"
    "    bool sticker = (base.r + base.g + base.b) > 0.15;\n"
    "    vec3 col = (sticker && m < 0.40) ? base : vec3(0.03);\n"
    "    oColor = vec4(lighting(col, vW, normalize(vNor), 24.0), 1.0);\n"
    "}\n";

/* pared y linterna: pos(3) normal(3) color(3) por vertice */
static const char *VSW =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNor;\n"
    "layout(location=2) in vec3 aCol;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "out vec3 vW;\n"
    "out vec3 vNor;\n"
    "out vec3 vCol;\n"
    "void main() {\n"
    "    vW = (uModel * vec4(aPos, 1.0)).xyz;\n"
    "    vNor = mat3(uModel) * aNor;\n"
    "    vCol = aCol;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char *FSW_BODY =
    "in vec3 vW;\n"
    "in vec3 vNor;\n"
    "in vec3 vCol;\n"
    "uniform float uEmit;\n"
    "uniform float uBump;\n"
    "out vec4 oColor;\n"
    "float hash(vec3 p) {\n"
    "    p = fract(p * 0.3183099 + 0.1);\n"
    "    p *= 17.0;\n"
    "    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));\n"
    "}\n"
    "void main() {\n"
    "    if (uEmit > 0.5) { oColor = vec4(vCol, 1.0); return; }\n"
    "    vec3 q = floor(vW * 22.0);\n"
    "    float g1 = hash(q), g2 = hash(q + 17.0), g3 = hash(q + 43.0);\n"
    "    vec3 n = normalize(vNor);\n"
    "    n = normalize(n + uBump * 0.18 * vec3(g1 - 0.5, g2 - 0.5, g3 - 0.5));\n"
    "    vec3 col = vCol * (1.0 - uBump * 0.15 + uBump * 0.3 * g1);\n"
    "    oColor = vec4(lighting(col, vW, n, 16.0), 1.0);\n"
    "}\n";

static const char *VSD =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "uniform mat4 uMVP;\n"
    "void main() { gl_Position = uMVP * vec4(aPos, 1.0); }\n";
static const char *FSD =
    "#version 330 core\n"
    "void main() { }\n";

static char *glsl_join(const char *a, const char *b, const char *c) {
    char *s = (char *)malloc(strlen(a) + strlen(b) + strlen(c) + 1);
    strcpy(s, a); strcat(s, b); strcat(s, c);
    return s;
}

static const char *VSHUD =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";

static const char *FSHUD =
    "#version 330 core\n"
    "uniform vec4 uColor;\n"
    "out vec4 oColor;\n"
    "void main() { oColor = uColor; }\n";

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; log[0] = 0;
        glGetShaderInfoLog(s, (GLsizei)sizeof log, NULL, log);
        printf("FALLO: el shader no compila: %s\n", log);
        fails++;
        return 0;
    }
    return s;
}
static GLuint make_program(const char *vs, const char *fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; log[0] = 0;
        glGetProgramInfoLog(p, (GLsizei)sizeof log, NULL, log);
        printf("FALLO: el programa no enlaza: %s\n", log);
        fails++;
        return 0;
    }
    return p;
}

/* ---------- malla del cubito: 6 caras x 2 triangulos, vertice = pos(3) normal(3) cara(1) ---------- */
static void build_cube(float *v) {
    static const float N[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    static const float U[6][3] = { {0,0,1}, {0,0,1}, {1,0,0}, {1,0,0}, {1,0,0}, {1,0,0} };
    static const float V[6][3] = { {0,1,0}, {0,1,0}, {0,0,1}, {0,0,1}, {0,1,0}, {0,1,0} };
    static const int su[4] = { -1, 1, 1, -1 }, sv[4] = { -1, -1, 1, 1 };
    static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    int n = 0;
    for (int f = 0; f < 6; f++)
        for (int i = 0; i < 6; i++) {
            int k = idx[i];
            for (int j = 0; j < 3; j++) v[n++] = 0.5f * N[f][j] + 0.5f * su[k] * U[f][j] + 0.5f * sv[k] * V[f][j];
            for (int j = 0; j < 3; j++) v[n++] = N[f][j];
            v[n++] = (float)f;
        }
}

/* ---------- animacion: cada cubito vuela desde un punto al azar hasta su lugar ---------- */
#define NCUBIES 26
#define T_ASM  (NCUBIES * 0.10 + 1.10)
#define T_HOLD 4.0
#define T_SCT  (NCUBIES * 0.03 + 1.00)
#define T_CYC  (T_ASM + T_HOLD + T_SCT)

static float rnd(int i, int k) {
    unsigned x = (unsigned)i * 374761393u + (unsigned)k * 668265263u;
    x = (x ^ (x >> 13)) * 1274126177u;
    x ^= x >> 16;
    return (float)(x & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
}
static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smooth(float p) { return p * p * (3.0f - 2.0f * p); }

/* ---------- contador de FPS: 7 segmentos dibujados con rectangulos ---------- */
static int glyph(char c) {
    switch (c) {
        case '0': return 63;  case '1': return 6;   case '2': return 91;  case '3': return 79;
        case '4': return 102; case '5': return 109; case '6': return 125; case '7': return 7;
        case '8': return 127; case '9': return 111; case 'F': return 113; case 'P': return 115;
        case 'S': return 109;
        default: return 0;
    }
}
static int hud_rect(float *o, float W, float H, float x, float y, float w, float h) {
    float x0 = 2.0f * x / W - 1.0f, x1 = 2.0f * (x + w) / W - 1.0f;
    float y0 = 1.0f - 2.0f * y / H, y1 = 1.0f - 2.0f * (y + h) / H;
    float q[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
    memcpy(o, q, sizeof q);
    return 12;
}
#define HUD_FLOATS 1024
static int hud_build(float *buf, float W, float H, int fps) {
    char txt[16];
    if (fps > 9999) fps = 9999;
    snprintf(txt, sizeof txt, "FPS %d", fps);
    float h = H * 0.055f, w = h * 0.55f, t = h * 0.14f, gap = h * 0.28f, mar = h * 0.7f;
    int len = (int)strlen(txt);
    float tw = len * w + (len - 1) * gap, x0 = mar * 1.6f, y0 = mar;
    int n = hud_rect(buf, W, H, x0 - mar * 0.5f, y0 - mar * 0.5f, tw + mar, h + mar);   /* fondo */
    for (int i = 0; i < len; i++) {
        int g = glyph(txt[i]);
        float x = x0 + i * (w + gap), y = y0;
        if (g & 1)  n += hud_rect(buf + n, W, H, x, y, w, t);                       /* a */
        if (g & 2)  n += hud_rect(buf + n, W, H, x + w - t, y, t, h * 0.5f);        /* b */
        if (g & 4)  n += hud_rect(buf + n, W, H, x + w - t, y + h * 0.5f, t, h * 0.5f); /* c */
        if (g & 8)  n += hud_rect(buf + n, W, H, x, y + h - t, w, t);               /* d */
        if (g & 16) n += hud_rect(buf + n, W, H, x, y + h * 0.5f, t, h * 0.5f);     /* e */
        if (g & 32) n += hud_rect(buf + n, W, H, x, y, t, h * 0.5f);                /* f */
        if (g & 64) n += hud_rect(buf + n, W, H, x, y + h * 0.5f - t * 0.5f, w, t); /* g */
    }
    return n;
}

static int xerr = 0;
static int xerr_handler(Display *d, XErrorEvent *e) { (void)d; (void)e; xerr = 1; return 0; }

static GLXContext make_ctx(Display *d, GLXFBConfig fb, cca_t cca, int maj, int min) {
    int at[] = { GLX_CONTEXT_MAJOR_VERSION_ARB, maj, GLX_CONTEXT_MINOR_VERSION_ARB, min,
                 GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, 0 };
    if (maj * 10 + min < 33) at[4] = 0;              /* <3.3: sin perfil (compatibilidad / legado) */
    if (cca) return cca(d, fb, NULL, True, at);
    return glXCreateNewContext(d, fb, GLX_RGBA_TYPE, NULL, True);
}


/* ---------- vectores ---------- */
static void v_norm(float *v) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}
static void v_cross(float *o, const float *a, const float *b) {
    o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
static float v_dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void m_look(float *m, const float *e, const float *c, const float *up) {
    float f[3] = { c[0] - e[0], c[1] - e[1], c[2] - e[2] }, s[3], u[3];
    v_norm(f);
    v_cross(s, f, up); v_norm(s);
    v_cross(u, s, f);
    m_ident(m);
    m[0] = s[0]; m[4] = s[1]; m[8]  = s[2];
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2];
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2];
    m[12] = -v_dot(s, e); m[13] = -v_dot(u, e); m[14] = v_dot(f, e);
}

/* ---------- mallas: pared de ladrillos y linterna (pos3 nor3 col3) ---------- */
#define WALL_Z (-8.2f)
static float *g_mv; static int g_mn;
static float *wall_v, *fl_v;
static int wall_n, fl_n, fl_lens0;

static void pv(const float *p, const float *nr, const float *c) {
    float *o = g_mv + g_mn * 9;
    memcpy(o, p, 12); memcpy(o + 3, nr, 12); memcpy(o + 6, c, 12);
    g_mn++;
}
static void quad(const float *a, const float *b, const float *c, const float *d, const float *nr, const float *col) {
    pv(a, nr, col); pv(b, nr, col); pv(c, nr, col);
    pv(a, nr, col); pv(c, nr, col); pv(d, nr, col);
}
static void build_wall(void) {
    g_mv = (float *)malloc(30000 * 9 * sizeof(float));
    g_mn = 0;
    const float mort[3] = { 0.50f, 0.48f, 0.45f }, nz[3] = { 0, 0, 1 }, nu[3] = { 0, 1, 0 }, nd[3] = { 0, -1, 0 },
                nl[3] = { -1, 0, 0 }, nr_[3] = { 1, 0, 0 };
    for (int gy = 0; gy < 16; gy++)                       /* argamasa: plano de fondo teselado */
        for (int gx = -14; gx < 14; gx++) {
            float x0 = (float)gx, x1 = x0 + 1.0f, y0 = gy - 8.0f, y1 = y0 + 1.0f;
            float a[3] = { x0, y0, WALL_Z }, b[3] = { x1, y0, WALL_Z }, c[3] = { x1, y1, WALL_Z }, d[3] = { x0, y1, WALL_Z };
            quad(a, b, c, d, nz, mort);
        }
    for (int r = 0; r < 32; r++) {                        /* ladrillos con aparejo trabado */
        float off = (r & 1) ? 0.55f : 0.0f;
        for (int c = -1; c < 27; c++) {
            int i = r * 40 + (c + 1);
            float x0 = -14.0f + c * 1.1f - off + 0.035f, x1 = x0 + 1.03f;
            float y0 = -8.0f + r * 0.5f + 0.04f, y1 = y0 + 0.42f;
            float z0 = WALL_Z, z1 = WALL_Z + 0.16f + 0.05f * rnd(i, 1);
            float v = 1.0f + 0.25f * rnd(i, 2);
            if (rnd(i, 4) > 0.8f) v *= 0.6f;
            float col[3] = { 0.55f * v + 0.05f * rnd(i, 3), 0.20f * v, 0.13f * v };
            float p000[3] = { x0, y0, z0 }, p100[3] = { x1, y0, z0 }, p110[3] = { x1, y1, z0 }, p010[3] = { x0, y1, z0 };
            float p001[3] = { x0, y0, z1 }, p101[3] = { x1, y0, z1 }, p111[3] = { x1, y1, z1 }, p011[3] = { x0, y1, z1 };
            quad(p001, p101, p111, p011, nz, col);
            quad(p011, p111, p110, p010, nu, col);
            quad(p000, p100, p101, p001, nd, col);
            quad(p000, p001, p011, p010, nl, col);
            quad(p101, p100, p110, p111, nr_, col);
        }
    }
    wall_v = g_mv; wall_n = g_mn;
}
/* linterna: torneado alrededor de +Z (la lente mira hacia +Z), origen en la base */
#define FL_LENS_Z 0.425f
#define FL_SCALE 1.8f
static void build_flash(void) {
    enum { NS = 20 };
    static const float prof[5][2] = { {-0.55f, 0.0f}, {-0.55f, 0.10f}, {0.12f, 0.10f}, {0.16f, 0.12f}, {0.42f, 0.21f} };
    const float dark[3] = { 0.16f, 0.17f, 0.21f }, gold[3] = { 0.75f, 0.58f, 0.12f }, lens[3] = { 1.0f, 0.95f, 0.70f };
    g_mv = (float *)malloc(2000 * 9 * sizeof(float));
    g_mn = 0;
    for (int s = 0; s < 4; s++) {
        float z0 = prof[s][0], r0 = prof[s][1], z1 = prof[s + 1][0], r1 = prof[s + 1][1], dr = r1 - r0, dz = z1 - z0;
        const float *col = s < 2 ? dark : gold;
        for (int k = 0; k < NS; k++) {
            float a0 = 6.2831853f * k / NS, a1 = 6.2831853f * (k + 1) / NS;
            float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
            float n0[3] = { c0 * dz, s0 * dz, -dr }, n1[3] = { c1 * dz, s1 * dz, -dr };
            v_norm(n0); v_norm(n1);
            float p00[3] = { r0 * c0, r0 * s0, z0 }, p10[3] = { r0 * c1, r0 * s1, z0 };
            float p11[3] = { r1 * c1, r1 * s1, z1 }, p01[3] = { r1 * c0, r1 * s0, z1 };
            pv(p00, n0, col); pv(p10, n1, col); pv(p11, n1, col);
            pv(p00, n0, col); pv(p11, n1, col); pv(p01, n0, col);
        }
    }
    fl_lens0 = g_mn;
    const float nz[3] = { 0, 0, 1 }, ctr[3] = { 0, 0, FL_LENS_Z };
    for (int k = 0; k < NS; k++) {
        float a0 = 6.2831853f * k / NS, a1 = 6.2831853f * (k + 1) / NS;
        float p0[3] = { 0.19f * cosf(a0), 0.19f * sinf(a0), FL_LENS_Z }, p1[3] = { 0.19f * cosf(a1), 0.19f * sinf(a1), FL_LENS_Z };
        pv(ctr, nz, lens); pv(p0, nz, lens); pv(p1, nz, lens);
    }
    fl_v = g_mv; fl_n = g_mn;
}

/* ---------- uniformes de luz compartidos ---------- */
#define SM_SIZE 1024
typedef struct { GLint lpos, ldir, lint, key, cam, lvp, sm, shon, sminv, emit, bump; } LU;
static void lu_get(LU *u, GLuint p) {
    u->lpos = glGetUniformLocation(p, "uLPos");  u->ldir = glGetUniformLocation(p, "uLDir");
    u->lint = glGetUniformLocation(p, "uLInt");  u->key = glGetUniformLocation(p, "uKey");
    u->cam = glGetUniformLocation(p, "uCam");    u->lvp = glGetUniformLocation(p, "uLVP");
    u->sm = glGetUniformLocation(p, "uSM");      u->shon = glGetUniformLocation(p, "uShOn");
    u->sminv = glGetUniformLocation(p, "uSMInv"); u->emit = glGetUniformLocation(p, "uEmit");
    u->bump = glGetUniformLocation(p, "uBump");
}
static void lu_set(const LU *u, const float *lp, const float *ld, float li, float key, const float *lvp, int shon) {
    glUniform3fv(u->lpos, 1, lp); glUniform3fv(u->ldir, 1, ld);
    glUniform1f(u->lint, li); glUniform1f(u->key, key);
    glUniform3f(u->cam, 0.0f, 0.0f, 9.0f);
    glUniformMatrix4fv(u->lvp, 1, GL_FALSE, lvp);
    glUniform1i(u->sm, 0); glUniform1f(u->shon, shon ? 1.0f : 0.0f); glUniform1f(u->sminv, 1.0f / SM_SIZE);
}
static int make_shadow(GLuint *fbo, GLuint *tex) {
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SM_SIZE, SM_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, *tex, 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glReadBuffer(GL_NONE);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return st == GL_FRAMEBUFFER_COMPLETE;
}
static int build_programs(GLuint *prog, GLuint *wprog, GLuint *dprog, GLuint *hudp) {
    char *fs3 = glsl_join("#version 330 core\n", GLSL_COMMON, FS3D_BODY);
    char *fsw = glsl_join("#version 330 core\n", GLSL_COMMON, FSW_BODY);
    *prog = make_program(VS3D, fs3);
    *wprog = make_program(VSW, fsw);
    *dprog = make_program(VSD, FSD);
    *hudp = make_program(VSHUD, FSHUD);
    free(fs3); free(fsw);
    return *prog && *wprog && *dprog && *hudp;
}
static GLuint make_vao(GLuint *vbo, const float *data, int nfloats, int stride_f, int nattr3) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, nfloats * sizeof(float), data, GL_STATIC_DRAW);
    for (int a = 0; a < nattr3; a++) {
        glEnableVertexAttribArray(a);
        glVertexAttribPointer(a, 3, GL_FLOAT, GL_FALSE, stride_f * sizeof(float), (const void *)(a * 3 * sizeof(float)));
    }
    return vao;
}

/* ---------- ruta de tuberia fija (GL < 3.3): sin shaders ni sombras ---------- */
static float g_lpos[3], g_ldir[3], g_lint, g_key;
static void ff_cube(const float *cube, const float *M, const float *col6) {
    static const float L[3] = { 0.3835f, 0.6136f, 0.6903f };
    for (int f = 0; f < 6; f++) {
        const float *v = cube + f * 6 * 7, *n = v + 3;
        float nw[3], p[3];
        for (int i = 0; i < 3; i++) {
            nw[i] = M[i] * n[0] + M[4 + i] * n[1] + M[8 + i] * n[2];
            p[i] = 0.5f * nw[i] + M[12 + i];
        }
        float dd = v_dot(nw, L);
        float lit = (0.08f + 0.27f * g_key) + 0.75f * g_key * (dd > 0.0f ? dd : 0.0f);
        if (g_lint > 0.001f) {                            /* foco de la linterna, por cara */
            float Lv[3] = { g_lpos[0] - p[0], g_lpos[1] - p[1], g_lpos[2] - p[2] };
            float dist = sqrtf(v_dot(Lv, Lv)) + 1e-4f;
            for (int i = 0; i < 3; i++) Lv[i] /= dist;
            float ca = -v_dot(Lv, g_ldir), nl = v_dot(nw, Lv);
            float sp = smooth(clamp01((ca - 0.8823f) / (0.9611f - 0.8823f)));
            float att = 1.0f / (1.0f + 0.05f * dist + 0.012f * dist * dist);
            lit += 3.5f * g_lint * sp * att * (nl > 0.0f ? nl : 0.0f);
        }
        glColor3f(0.03f * lit, 0.03f * lit, 0.03f * lit);
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < 6; k++) glVertex3f(v[k * 7], v[k * 7 + 1], v[k * 7 + 2]);
        glEnd();
        const float *c = col6 + f * 3;
        if (c[0] + c[1] + c[2] > 0.15f) {
            glColor3f(c[0] * lit, c[1] * lit, c[2] * lit);
            glBegin(GL_TRIANGLES);
            for (int k = 0; k < 6; k++) {
                const float *q = v + k * 7;
                glVertex3f(0.505f * n[0] + 0.8f * (q[0] - 0.5f * n[0]),
                           0.505f * n[1] + 0.8f * (q[1] - 0.5f * n[1]),
                           0.505f * n[2] + 0.8f * (q[2] - 0.5f * n[2]));
            }
            glEnd();
        }
    }
}
static void ff_mesh(const float *v, int first, int count) {
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 36, v);
    glNormalPointer(GL_FLOAT, 36, v + 3);
    glColorPointer(3, GL_FLOAT, 36, v + 6);
    glDrawArrays(GL_TRIANGLES, first, count);
    glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_COLOR_ARRAY);
}
static void ff_hud(const float *hud, int hn) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glColor3f(0.02f, 0.02f, 0.04f);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) glVertex2f(hud[i * 2], hud[i * 2 + 1]);
    glEnd();
    glColor3f(0.20f, 1.00f, 0.35f);
    glBegin(GL_TRIANGLES);
    for (int i = 6; i < hn / 2; i++) glVertex2f(hud[i * 2], hud[i * 2 + 1]);
    glEnd();
}

int main(int argc, char **argv) {
    int test = 0, req = 43, na = 0, rmaj = 4, rmin = 3;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--test")) test = 1;
        else if (na == 0) { rmaj = atoi(argv[i]); na = 1; }
        else if (na == 1) { rmin = atoi(argv[i]); na = 2; }
    }
    req = rmaj * 10 + rmin;
    setvbuf(stdout, NULL, _IOLBF, 0);

    Display *d = XOpenDisplay(NULL);
    if (!d) { printf("sin X (¿DISPLAY=:0 y Termux:X11 abierto?)\n"); return 2; }
    XSetErrorHandler(xerr_handler);
    int scr = XDefaultScreen(d), n = 0;
    GLXFBConfig *fbc = NULL;
    static const int depths[2] = { 24, 16 };
    for (int k = 0; k < 2 && (!fbc || !n); k++) {
        int fa[] = { GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_DOUBLEBUFFER, True,
                     GLX_DEPTH_SIZE, depths[k], 0 };
        fbc = glXChooseFBConfig(d, scr, fa, &n);
    }
    if (!fbc || !n) { printf("sin fbconfig\n"); return 2; }
    XVisualInfo *vi = glXGetVisualFromFBConfig(d, fbc[0]);
    if (!vi) { printf("sin visual\n"); return 2; }
    int W = 900, H = 600;
    XSetWindowAttributes sa;
    memset(&sa, 0, sizeof sa);
    sa.colormap = XCreateColormap(d, XRootWindow(d, vi->screen), vi->visual, AllocNone);
    sa.event_mask = KeyPressMask | StructureNotifyMask;
    Window win = XCreateWindow(d, XRootWindow(d, vi->screen), 0, 0, W, H, 0, vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask, &sa);
    XStoreName(d, win, "Lorica - cubo de Rubik");
    Atom wmdel = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, win, &wmdel, 1);
    XMapWindow(d, win);
    XSync(d, False);

    /* ---- negociar contexto: de mas alto a mas bajo ---- */
    static const int cand[][2] = { {4,3},{4,2},{4,1},{4,0},{3,3},{3,2},{3,1},{3,0},{2,1},{2,0},
                                   {1,5},{1,4},{1,3},{1,2},{1,1},{1,0} };
    cca_t cca = (cca_t)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
    GLXContext ctx = NULL;
    int am = 0, an = 0, use_sh = 0;
    GLuint prog = 0, wprog = 0, dprog = 0, hudp = 0;
    const char *ver = NULL, *ren = NULL;
    printf("Probando versiones de OpenGL desde %d.%d hacia abajo...\n", rmaj, rmin);
    for (unsigned ci = 0; ci < sizeof cand / sizeof cand[0]; ci++) {
        int cm = cand[ci][0], cn = cand[ci][1];
        if (cm * 10 + cn > req) continue;
        xerr = 0;
        GLXContext c = make_ctx(d, fbc[0], cca, cm, cn);
        XSync(d, False);
        if (!c || xerr) { printf("  GL %d.%d: no disponible\n", cm, cn); if (c && !xerr) glXDestroyContext(d, c); continue; }
        if (!glXMakeCurrent(d, win, c)) { printf("  GL %d.%d: MakeCurrent fallo\n", cm, cn); glXDestroyContext(d, c); continue; }
        ver = (const char *)glGetString(GL_VERSION);
        ren = (const char *)glGetString(GL_RENDERER);
        if (!ver || sscanf(ver, "%d.%d", &am, &an) != 2) { am = cm; an = cn; }
        use_sh = am * 10 + an >= 33;
        if (use_sh) {
            int f0 = fails;
            if (!build_programs(&prog, &wprog, &dprog, &hudp)) {
                fails = f0;
                printf("  GL %d.%d: los shaders GLSL 3.30 no compilan, sigo bajando\n", cm, cn);
                glXMakeCurrent(d, None, NULL); glXDestroyContext(d, c);
                continue;
            }
        }
        printf("  GL %d.%d: OK (pedido %d.%d)\n", cm, cn, cm, cn);
        ctx = c;
        break;
    }
    if (!ctx) { printf("FALLO: ninguna version de OpenGL funciono\n"); return 1; }
    printf("GL_VERSION=%s\nGL_RENDERER=%s\n", ver ? ver : "(null)", ren ? ren : "(null)");
    printf("USANDO: OpenGL %d.%d, ruta %s\n", am, an, use_sh ? "shaders GLSL 3.30" : "tuberia fija (sin shaders)");

    build_wall();
    build_flash();
    printf("Pared: %d vertices (%d ladrillos) | Linterna: %d vertices\n", wall_n, 32 * 28, fl_n);

    float cube[36 * 7];
    build_cube(cube);
    GLuint vao = 0, vbo = 0, hvao = 0, hvbo = 0, wvao = 0, wvbo = 0, fvao = 0, fvbo = 0, sfbo = 0, stex = 0;
    int sh_ok = 0;
    GLint uMVP = -1, uModel = -1, uCol = -1, uColor = -1;
    GLint wMVP = -1, wModel = -1, dMVP = -1;
    LU luc, luw;
    memset(&luc, 0, sizeof luc); memset(&luw, 0, sizeof luw);
    if (use_sh) {
        uMVP = glGetUniformLocation(prog, "uMVP"); uModel = glGetUniformLocation(prog, "uModel");
        uCol = glGetUniformLocation(prog, "uCol[0]"); uColor = glGetUniformLocation(hudp, "uColor");
        wMVP = glGetUniformLocation(wprog, "uMVP"); wModel = glGetUniformLocation(wprog, "uModel");
        dMVP = glGetUniformLocation(dprog, "uMVP");
        CHECK(uMVP >= 0 && uModel >= 0 && uCol >= 0 && uColor >= 0 && wMVP >= 0 && wModel >= 0 && dMVP >= 0,
              "uniform no encontrado (MVP=%d Model=%d Col=%d Color=%d wMVP=%d wModel=%d dMVP=%d)",
              uMVP, uModel, uCol, uColor, wMVP, wModel, dMVP);
        lu_get(&luc, prog); lu_get(&luw, wprog);
        vao = make_vao(&vbo, cube, 36 * 7, 7, 0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (const void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (const void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (const void *)(6 * sizeof(float)));
        glGenVertexArrays(1, &hvao);
        glBindVertexArray(hvao);
        glGenBuffers(1, &hvbo);
        glBindBuffer(GL_ARRAY_BUFFER, hvbo);
        glBufferData(GL_ARRAY_BUFFER, HUD_FLOATS * sizeof(float), NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (const void *)0);
        wvao = make_vao(&wvbo, wall_v, wall_n * 9, 9, 3);
        fvao = make_vao(&fvbo, fl_v, fl_n * 9, 9, 3);
        sh_ok = make_shadow(&sfbo, &stex);
        printf("Sombras (shadow map %dx%d, PCF 3x3): %s\n", SM_SIZE, SM_SIZE, sh_ok ? "SI" : "NO (FBO de profundidad incompleto)");
    } else {
        printf("Sombras: NO (tuberia fija); la linterna ilumina por vertice\n");
    }

    int cx[NCUBIES], cy[NCUBIES], cz[NCUBIES], nc = 0;
    for (int y = -1; y <= 1; y++)
        for (int z = -1; z <= 1; z++)
            for (int x = -1; x <= 1; x++)
                if (x || y || z) { cx[nc] = x; cy[nc] = y; cz[nc] = z; nc++; }
    CHECK(nc == NCUBIES, "cantidad de cubitos %d (esperaba %d)", nc, NCUBIES);

    static const float PAL[6][3] = { {0.78f, 0.08f, 0.08f}, {1.00f, 0.45f, 0.00f}, {0.95f, 0.95f, 0.95f},
                                     {1.00f, 0.85f, 0.00f}, {0.00f, 0.60f, 0.20f}, {0.05f, 0.25f, 0.85f} };
    static float MM[NCUBIES][16], CC[NCUBIES][18];
    const float speed = test ? 2.5f : 1.0f;
    const float bgr = 0.10f, bgg = 0.12f, bgb = 0.18f;
    double t0 = now_s(), fps_t = t0;
    int fcount = 0, fps = 0, frames = 0, running = 1, sampled = 0, shadows_now = 0;
    float hud[HUD_FLOATS];

    while (running) {
        while (XPending(d)) {
            XEvent ev;
            XNextEvent(d, &ev);
            if (ev.type == ConfigureNotify) { W = ev.xconfigure.width; H = ev.xconfigure.height; }
            else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wmdel) running = 0;
            else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape || ks == XK_q) running = 0;
            }
        }
        if (W < 1) W = 1;
        if (H < 1) H = 1;
        double tn = now_s();
        float sim = (float)((tn - t0) * speed);
        float cyc = (float)fmod((double)sim, (double)T_CYC);
        int wall_on = !(test && cyc >= (float)T_ASM + 0.5f);   /* en --test la ultima muestra va sin pared */

        /* ---- linterna: cada 10 s entra, da una vuelta completa al cubo y se va ---- */
        float ft = test ? sim : (float)(tn - t0);
        float tf = fmodf(ft + 8.0f, 10.0f);                   /* la primera aparece a los 2 s */
        float env = 0.0f;
        if (tf < 7.5f) env = smooth(clamp01(tf / 1.0f)) * smooth(clamp01((7.5f - tf) / 1.0f));
        float u = clamp01((tf - 1.0f) / 5.5f);
        float ang = -1.5708f + 6.2831853f * u;
        float rad = 5.0f + 9.0f * (1.0f - env);
        float lens[3] = { rad * sinf(ang), 0.3f + 1.8f * sinf(ang + 0.6f), rad * cosf(ang) };
        float fdir[3] = { -lens[0], -lens[1], -lens[2] };
        v_norm(fdir);
        float upv[3] = { 0, 1, 0 }, rr[3], uu[3], Fm[16];
        v_cross(rr, upv, fdir); v_norm(rr);
        v_cross(uu, fdir, rr);
        m_ident(Fm);
        for (int i = 0; i < 3; i++) {
            Fm[i] = rr[i] * FL_SCALE; Fm[4 + i] = uu[i] * FL_SCALE; Fm[8 + i] = fdir[i] * FL_SCALE;
            Fm[12 + i] = lens[i] - fdir[i] * FL_LENS_Z * FL_SCALE;
        }
        float key = 1.0f - 0.8f * env;
        memcpy(g_lpos, lens, sizeof lens); memcpy(g_ldir, fdir, sizeof fdir);
        g_lint = env; g_key = key;
        int fl_on = env > 0.02f;

        float P[16], Vw[16], PV[16], Rx[16], Ry[16], G[16];
        m_persp(P, 0.80f, (float)W / (float)H, 2.0f, 30.0f);
        m_trans(Vw, 0.0f, 0.0f, -9.0f);
        m_mul(PV, P, Vw);
        m_rot(Rx, 1.0f, 0.0f, 0.0f, 0.50f + 0.15f * sinf(sim * 0.6f));
        m_rot(Ry, 0.0f, 1.0f, 0.0f, sim * 0.9f);
        m_mul(G, Rx, Ry);

        /* matrices y colores de los 26 cubitos */
        for (int i = 0; i < nc; i++) {
            float p;
            if (cyc < (float)T_ASM) p = clamp01((cyc - i * 0.10f) / 1.10f);
            else if (cyc < (float)(T_ASM + T_HOLD)) p = 1.0f;
            else p = 1.0f - clamp01((cyc - (float)(T_ASM + T_HOLD) - i * 0.03f) / 1.00f);
            float e = smooth(p);
            float dx = rnd(i, 0), dy = rnd(i, 1), dz = rnd(i, 2) + 0.2f;
            float dl = sqrtf(dx * dx + dy * dy + dz * dz) + 1e-3f;
            float R = 4.5f + 2.5f * fabsf(rnd(i, 3));
            float px = (dx / dl * R) * (1.0f - e) + cx[i] * 1.03f * e;
            float py = (dy / dl * R) * (1.0f - e) + cy[i] * 1.03f * e;
            float pz = (dz / dl * R) * (1.0f - e) + cz[i] * 1.03f * e;
            float spin = (2.0f + 3.0f * fabsf(rnd(i, 7))) * 3.14159265f * (rnd(i, 8) < 0.0f ? -1.0f : 1.0f);
            float T[16], Rc[16], M0[16];
            m_trans(T, px, py, pz);
            m_rot(Rc, rnd(i, 4), rnd(i, 5), rnd(i, 6), (1.0f - e) * spin);
            m_mul(M0, T, Rc);
            m_mul(MM[i], G, M0);
            for (int f = 0; f < 6; f++) {
                int outer = (f == 0 && cx[i] == 1) || (f == 1 && cx[i] == -1) || (f == 2 && cy[i] == 1) ||
                            (f == 3 && cy[i] == -1) || (f == 4 && cz[i] == 1) || (f == 5 && cz[i] == -1);
                for (int j = 0; j < 3; j++) CC[i][f * 3 + j] = outer ? PAL[f][j] : 0.0f;
            }
        }

        float LVP[16];
        m_ident(LVP);
        shadows_now = use_sh && sh_ok && fl_on;
        if (shadows_now) {
            float LP[16], LV[16], tgt[3] = { 0, 0, 0 }, tmp[16];
            m_persp(LP, 1.2f, 1.0f, 1.0f, 40.0f);
            m_look(LV, lens, tgt, upv);
            m_mul(LVP, LP, LV);
            glBindFramebuffer(GL_FRAMEBUFFER, sfbo);
            glViewport(0, 0, SM_SIZE, SM_SIZE);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);
            glUseProgram(dprog);
            if (wall_on) {
                glBindVertexArray(wvao);
                glUniformMatrix4fv(dMVP, 1, GL_FALSE, LVP);
                glDrawArrays(GL_TRIANGLES, 0, wall_n);
            }
            glBindVertexArray(vao);
            for (int i = 0; i < nc; i++) {
                m_mul(tmp, LVP, MM[i]);
                glUniformMatrix4fv(dMVP, 1, GL_FALSE, tmp);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glViewport(0, 0, W, H);
        glClearColor(bgr, bgg, bgb, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if (use_sh) {
            float I[16], MVP[16];
            m_ident(I);
            if (sh_ok) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, stex); }
            if (wall_on) {                                         /* pared */
                glUseProgram(wprog);
                lu_set(&luw, lens, fdir, env, key, LVP, shadows_now);
                glUniform1f(luw.emit, 0.0f); glUniform1f(luw.bump, 1.0f);
                glUniformMatrix4fv(wMVP, 1, GL_FALSE, PV);
                glUniformMatrix4fv(wModel, 1, GL_FALSE, I);
                glBindVertexArray(wvao);
                glDrawArrays(GL_TRIANGLES, 0, wall_n);
            }
            glUseProgram(prog);                                    /* cubitos */
            lu_set(&luc, lens, fdir, env, key, LVP, shadows_now);
            glBindVertexArray(vao);
            for (int i = 0; i < nc; i++) {
                m_mul(MVP, PV, MM[i]);
                glUniformMatrix4fv(uMVP, 1, GL_FALSE, MVP);
                glUniformMatrix4fv(uModel, 1, GL_FALSE, MM[i]);
                glUniform3fv(uCol, 6, CC[i]);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            if (fl_on) {                                           /* linterna */
                glUseProgram(wprog);
                lu_set(&luw, lens, fdir, env, key, LVP, shadows_now);
                glUniform1f(luw.bump, 0.0f);
                m_mul(MVP, PV, Fm);
                glUniformMatrix4fv(wMVP, 1, GL_FALSE, MVP);
                glUniformMatrix4fv(wModel, 1, GL_FALSE, Fm);
                glBindVertexArray(fvao);
                glUniform1f(luw.emit, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, fl_lens0);
                glUniform1f(luw.emit, 1.0f);
                glDrawArrays(GL_TRIANGLES, fl_lens0, fl_n - fl_lens0);
            }
        } else {
            glMatrixMode(GL_PROJECTION); glLoadMatrixf(P); glMatrixMode(GL_MODELVIEW);
            for (int i = 0; i < nc; i++) {
                float MV[16];
                m_mul(MV, Vw, MM[i]);
                glLoadMatrixf(MV);
                ff_cube(cube, MM[i], CC[i]);
            }
            glLoadMatrixf(Vw);                                     /* pared y linterna con iluminacion fija de GL */
            float lp4[4] = { lens[0], lens[1], lens[2], 1.0f }, kd4[4] = { 0.5f, 0.8f, 0.9f, 0.0f };
            float amb = 0.08f + 0.27f * key, am4[4] = { amb, amb, amb, 1.0f };
            float d0[4] = { 3.5f * env, 3.5f * env * 0.92f, 3.5f * env * 0.75f, 1.0f };
            float d1[4] = { 0.75f * key, 0.75f * key, 0.75f * key, 1.0f }, z4[4] = { 0, 0, 0, 1 };
            glLightfv(GL_LIGHT0, GL_POSITION, lp4);
            glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, fdir);
            glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 28.0f);
            glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 8.0f);
            glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
            glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05f);
            glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.012f);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, d0); glLightfv(GL_LIGHT0, GL_AMBIENT, z4); glLightfv(GL_LIGHT0, GL_SPECULAR, z4);
            glLightfv(GL_LIGHT1, GL_POSITION, kd4);
            glLightfv(GL_LIGHT1, GL_DIFFUSE, d1); glLightfv(GL_LIGHT1, GL_AMBIENT, z4); glLightfv(GL_LIGHT1, GL_SPECULAR, z4);
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, am4);
            glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1);
            glEnable(GL_NORMALIZE); glEnable(GL_COLOR_MATERIAL);
            glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
            if (wall_on) ff_mesh(wall_v, 0, wall_n);
            if (fl_on) {
                float MV[16];
                m_mul(MV, Vw, Fm);
                glLoadMatrixf(MV);
                ff_mesh(fl_v, 0, fl_lens0);
                glDisable(GL_LIGHTING);
                ff_mesh(fl_v, fl_lens0, fl_n - fl_lens0);
            }
            glDisable(GL_LIGHTING); glDisable(GL_LIGHT0); glDisable(GL_LIGHT1);
            glDisable(GL_NORMALIZE); glDisable(GL_COLOR_MATERIAL);
        }

        /* contador de FPS arriba a la izquierda (sin depth test) */
        int hn = hud_build(hud, (float)W, (float)H, fps);
        if (use_sh) {
            glDisable(GL_DEPTH_TEST);
            glUseProgram(hudp);
            glBindVertexArray(hvao);
            glBindBuffer(GL_ARRAY_BUFFER, hvbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, hn * sizeof(float), hud);
            glUniform4f(uColor, 0.02f, 0.02f, 0.04f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glUniform4f(uColor, 0.20f, 1.00f, 0.35f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 6, hn / 2 - 6);
        } else {
            ff_hud(hud, hn);
        }

        if (test && !sampled && cyc >= (float)T_ASM + 0.5f) {
            static unsigned char px[64 * 64 * 4];
            int cx0 = W / 2 - 32, cy0 = H / 2 - 32, lit = 0;
            if (cx0 < 0) cx0 = 0;
            if (cy0 < 0) cy0 = 0;
            glReadPixels(cx0, cy0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px);
            for (int i = 0; i < 64 * 64; i++) {
                int dr = abs((int)px[i * 4] - (int)(bgr * 255.0f + 0.5f));
                int dg = abs((int)px[i * 4 + 1] - (int)(bgg * 255.0f + 0.5f));
                int db = abs((int)px[i * 4 + 2] - (int)(bgb * 255.0f + 0.5f));
                if (dr + dg + db > 24) lit++;
            }
            CHECK(lit > 1000, "el cubo armado no se ve en el centro (%d de 4096 pixeles distintos del fondo)", lit);
            GLenum ge = glGetError();
            if (ge != GL_NO_ERROR) printf("AVISO: glGetError=0x%x tras dibujar\n", ge);
            sampled = 1;
            running = 0;
        }
        glXSwapBuffers(d, win);
        frames++;
        fcount++;
        if (tn - fps_t >= 0.5) {
            fps = (int)((double)fcount / (tn - fps_t) + 0.5);
            fcount = 0; fps_t = tn;
            char tt[160];
            snprintf(tt, sizeof tt, "Lorica - cubo de Rubik | GL %d.%d | %d FPS%s", am, an, fps, fl_on ? " | linterna" : "");
            XStoreName(d, win, tt);
            XFlush(d);
            printf("\rFPS %4d | GL %d.%d (%s) | linterna %-3s | sombras %-3s   ", fps, am, an,
                   use_sh ? "shaders" : "fija", fl_on ? "SI" : "no", shadows_now ? "SI" : "no");
            fflush(stdout);
        }
    }
    printf("\n");

    double total = now_s() - t0;
    if (test) {
        CHECK(sampled, "no se llego a la fase de cubo armado");
        CHECK(frames >= 3, "solo se dibujaron %d cuadros", frames);
        printf("FPS_PROMEDIO=%.1f\n", total > 0.0 ? frames / total : 0.0);
    }
    glXMakeCurrent(d, None, NULL);
    glXDestroyContext(d, ctx);
    XDestroyWindow(d, win);
    XCloseDisplay(d);
    if (test) printf(fails ? "RESULTADO: %d FALLOS\n" : "RESULTADO: TODO OK\n", fails);
    return fails != 0;
}
