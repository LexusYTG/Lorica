/* gl31_buffer.c - buffers, buffer storage (nativo o compartido con Vulkan), UBOs, SSBOs, mapeo
 *
 * glBufferStorage (GL 4.4) elige en ejecucion el camino de GPU disponible:
 *   1. GL_EXT_buffer_storage del backend: flags y mapeo persistente nativos.
 *   2. Sin ella, y con GL_EXT_memory_object_fd + Vulkan 1.1: Vulkan asigna memoria host-coherente
 *      exportable, GLES la importa como almacenamiento del buffer, y el puntero persistente es el
 *      mapeo de Vulkan (mismos bytes que ve la GPU de GLES, sin copias).
 *   3. Sin ninguna: INVALID_OPERATION (no existe camino de GPU).
 * LORICA_BUFSTORE=native|vk fuerza uno de los dos primeros (pruebas / drivers con la extension rota). */
#include "gl31.h"
#include "gl31_vk.h"
#include "../../vk/lvk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUFSTORE_VALID_FLAGS (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | \
                              GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT | GL_CLIENT_STORAGE_BIT)

typedef struct {
    GLuint id; GLsizeiptr size; GLbitfield flags;
    int vk;                          /* 1: memoria de Vulkan importada en GL */
    GLuint memobj; lvk_buffer vb;
    int mapped; GLbitfield map_access; GLintptr map_off; GLsizeiptr map_len;
} bufinfo_t;                         /* solo existe para buffers con almacenamiento inmutable */

static bufinfo_t* g_bi; static int g_nbi, g_cbi;
static pthread_mutex_t g_bimu = PTHREAD_MUTEX_INITIALIZER;

/* los llamantes toman g_bimu */
static bufinfo_t* bi_find(GLuint id)
{
    int i;
    if (!id) return NULL;
    for (i = 0; i < g_nbi; i++) if (g_bi[i].id == id) return &g_bi[i];
    return NULL;
}

static bufinfo_t* bi_add(GLuint id)
{
    bufinfo_t* b;
    if (g_nbi == g_cbi) {
        int nc = g_cbi ? g_cbi * 2 : 32;
        bufinfo_t* n = (bufinfo_t*)realloc(g_bi, (size_t)nc * sizeof *n);
        if (!n) return NULL;
        g_bi = n; g_cbi = nc;
    }
    b = &g_bi[g_nbi++];
    memset(b, 0, sizeof *b);
    b->id = id;
    return b;
}

/* libera los recursos de Vulkan y quita la entrada (g_bimu tomado) */
static void bi_remove(bufinfo_t* b)
{
    if (b->vk) {
        if (b->memobj && BE(glDeleteMemoryObjectsEXT)) BE(glDeleteMemoryObjectsEXT)(1, &b->memobj);
        lvk_buffer_destroy(&b->vb);     /* GL conserva su propia referencia a la memoria importada */
    }
    *b = g_bi[--g_nbi];
}

static GLuint bound_buffer(GLenum target)
{
    gl31_state_t* s = gl31_state();
    GLint v = 0; GLenum q = 0;
    switch (target) {
        case GL_ARRAY_BUFFER:          return s->array_buffer;
        case GL_UNIFORM_BUFFER:        return s->uniform_buffer;
        case GL_COPY_READ_BUFFER:      return s->copy_read_buffer;
        case GL_COPY_WRITE_BUFFER:     return s->copy_write_buffer;
        case GL_PIXEL_PACK_BUFFER:     return s->pixel_pack_buffer;
        case GL_PIXEL_UNPACK_BUFFER:   return s->pixel_unpack_buffer;
        case GL_ELEMENT_ARRAY_BUFFER:  q = GL_ELEMENT_ARRAY_BUFFER_BINDING; break;
        case GL_TRANSFORM_FEEDBACK_BUFFER: q = GL_TRANSFORM_FEEDBACK_BUFFER_BINDING; break;
        case GL_SHADER_STORAGE_BUFFER: q = GL_SHADER_STORAGE_BUFFER_BINDING; break;
        case GL_ATOMIC_COUNTER_BUFFER: q = GL_ATOMIC_COUNTER_BUFFER_BINDING; break;
        case GL_DRAW_INDIRECT_BUFFER:  q = GL_DRAW_INDIRECT_BUFFER_BINDING; break;
        case GL_DISPATCH_INDIRECT_BUFFER: q = GL_DISPATCH_INDIRECT_BUFFER_BINDING; break;
        default: return 0;
    }
    BE(glGetIntegerv)(q, &v);
    return (GLuint)v;
}

/* 1 = camino nativo, 2 = camino Vulkan, 0 = ninguno */
static int storage_path(void)
{
    const char* e = getenv("LORICA_BUFSTORE");
    int want_vk = e && !strcmp(e, "vk"), want_native = e && !strcmp(e, "native");
    if (!want_vk && gl31_caps.buffer_storage) return 1;
    if (want_native) return 0;
    if (gl31_caps.memobj_fd && lvk_init() && lvk_caps()->has_ext_mem_fd) return 2;
    return gl31_caps.buffer_storage ? 1 : 0;
}

/* acceso a los bytes de un buffer respaldado por Vulkan (NULL si no lo es) */
static void* vk_ptr(GLuint id, GLsizeiptr* size)
{
    void* p = NULL; bufinfo_t* b;
    pthread_mutex_lock(&g_bimu);
    b = bi_find(id);
    if (b && b->vk) { p = b->vb.map; if (size) *size = b->size; }
    pthread_mutex_unlock(&g_bimu);
    return p;
}

/* 1 si el buffer enlazado en `target` tiene almacenamiento inmutable (glBufferStorage) */
int gl31_buffer_immutable_info(GLenum target, GLbitfield* flags, int* vk)
{
    bufinfo_t* bi; int r = 0;
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(bound_buffer(target));
    if (bi) { r = 1; if (flags) *flags = bi->flags; if (vk) *vk = bi->vk; }
    pthread_mutex_unlock(&g_bimu);
    return r;
}

void gl31_buffer_shutdown(void)
{
    pthread_mutex_lock(&g_bimu);
    while (g_nbi) bi_remove(&g_bi[g_nbi - 1]);
    free(g_bi); g_bi = NULL; g_cbi = 0;
    pthread_mutex_unlock(&g_bimu);
}

void gl31_glGenBuffers(GLsizei n, GLuint* buffers)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenBuffers)(n, buffers);
}

void gl31_glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
    GLsizei i; int b;
    gl31_state_t* s = gl31_state();
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        GLuint id = buffers[i];
        if (!id) continue;
        if (s->array_buffer == id) s->array_buffer = 0;
        if (s->uniform_buffer == id) s->uniform_buffer = 0;
        if (s->copy_read_buffer == id) s->copy_read_buffer = 0;
        if (s->copy_write_buffer == id) s->copy_write_buffer = 0;
        if (s->pixel_pack_buffer == id) s->pixel_pack_buffer = 0;
        if (s->pixel_unpack_buffer == id) s->pixel_unpack_buffer = 0;
        for (b = 0; b < GL31_MAX_UBO_BINDINGS; b++)
            if (s->ubo[b].buffer == id) memset(&s->ubo[b], 0, sizeof(s->ubo[b]));
    }
    BE(glDeleteBuffers)(n, buffers);
    pthread_mutex_lock(&g_bimu);
    for (i = 0; i < n; i++) { bufinfo_t* bi = bi_find(buffers[i]); if (bi) bi_remove(bi); }
    pthread_mutex_unlock(&g_bimu);
}

void gl31_glBindBuffer(GLenum target, GLuint buffer)
{
    gl31_state_t* s = gl31_state();
    switch (target) {
        case GL_ARRAY_BUFFER:         s->array_buffer = buffer; break;
        case GL_UNIFORM_BUFFER:       s->uniform_buffer = buffer; break;
        case GL_COPY_READ_BUFFER:     s->copy_read_buffer = buffer; break;
        case GL_COPY_WRITE_BUFFER:    s->copy_write_buffer = buffer; break;
        case GL_PIXEL_PACK_BUFFER:    s->pixel_pack_buffer = buffer; break;
        case GL_PIXEL_UNPACK_BUFFER:  s->pixel_unpack_buffer = buffer; break;
        case GL_ELEMENT_ARRAY_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
            break;
        case GL_SHADER_STORAGE_BUFFER:
        case GL_ATOMIC_COUNTER_BUFFER:
        case GL_DRAW_INDIRECT_BUFFER:
        case GL_DISPATCH_INDIRECT_BUFFER:
            if (!gl31_caps.es31) {
                gl31_stub_warn("SSBO / atomic counters / buffers indirectos (backend sin ES 3.1)");
                gl31_set_error(GL_INVALID_ENUM);
                return;
            }
            break;
        default:  /* incluye GL_TEXTURE_BUFFER (no soportado) */
            gl31_set_error(GL_INVALID_ENUM);
            return;
    }
    BE(glBindBuffer)(target, buffer);
}

void gl31_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    int imm;
    if (size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    pthread_mutex_lock(&g_bimu);
    imm = bi_find(bound_buffer(target)) != NULL;
    pthread_mutex_unlock(&g_bimu);
    if (imm) { gl31_set_error(GL_INVALID_OPERATION); return; }   /* almacenamiento inmutable */
    BE(glBufferData)(target, size, data, usage);
}

void gl31_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    GLuint id; bufinfo_t* bi; int vk = 0, ok = 1;
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    id = bound_buffer(target);
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (bi) {
        if (!(bi->flags & GL_DYNAMIC_STORAGE_BIT) || bi->mapped) ok = 0;
        else if (offset + size > bi->size) { pthread_mutex_unlock(&g_bimu); gl31_set_error(GL_INVALID_VALUE); return; }
        vk = bi->vk;
    }
    pthread_mutex_unlock(&g_bimu);
    if (!ok) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (!size) return;
    if (vk) {
        /* El almacenamiento es memoria importada: la escritura se hace en la GPU de GLES, ordenada con
         * los draws anteriores (glBufferSubData lo exige), via buffer temporal + CopyBufferSubData. */
        GLuint tmp = 0; GLenum rt = (target == GL_COPY_READ_BUFFER) ? GL_COPY_WRITE_BUFFER : GL_COPY_READ_BUFFER;
        gl31_state_t* st = gl31_state();
        GLuint prev = (rt == GL_COPY_READ_BUFFER) ? st->copy_read_buffer : st->copy_write_buffer;
        BE(glGenBuffers)(1, &tmp);
        BE(glBindBuffer)(rt, tmp);
        BE(glBufferData)(rt, size, data, GL_STREAM_COPY);
        BE(glCopyBufferSubData)(rt, target, 0, offset, size);
        BE(glBindBuffer)(rt, prev);
        BE(glDeleteBuffers)(1, &tmp);
        return;
    }
    BE(glBufferSubData)(target, offset, size, data);
}

/* glBufferStorage (GL 4.4): ver la cabecera del fichero para los caminos de GPU. */
void gl31_glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags)
{
    GLuint id; bufinfo_t* bi; int path;
    if (size <= 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (flags & ~(GLbitfield)BUFSTORE_VALID_FLAGS) { gl31_set_error(GL_INVALID_VALUE); return; }
    if ((flags & GL_MAP_PERSISTENT_BIT) && !(flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))) { gl31_set_error(GL_INVALID_VALUE); return; }
    if ((flags & GL_MAP_COHERENT_BIT) && !(flags & GL_MAP_PERSISTENT_BIT)) { gl31_set_error(GL_INVALID_VALUE); return; }
    id = bound_buffer(target);
    if (!id) { gl31_set_error(GL_INVALID_OPERATION); return; }
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    pthread_mutex_unlock(&g_bimu);
    if (bi) { gl31_set_error(GL_INVALID_OPERATION); return; }     /* ya inmutable */

    path = storage_path();
    if (path == 0 && (flags & (GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT))) {
        gl31_stub_warn("glBufferStorage(PERSISTENT/COHERENT): el backend no tiene EXT_buffer_storage ni EXT_memory_object_fd+Vulkan");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (path == 1) {
        BE(glBufferStorage)(target, size, data, flags);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    } else if (path == 2) {
        lvk_buffer vb; GLuint mo = 0; int fd; GLenum err;
        if (!lvk_buffer_create(&vb, (VkDeviceSize)size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                LVK_BUF_HOST | LVK_BUF_EXPORT)) {
            if (getenv("LORICA_VK_DEBUG")) fprintf(stderr, "LORICA VK: lvk_buffer_create fallo (%ld bytes)\n", (long)size);
            gl31_set_error(GL_OUT_OF_MEMORY); return; }
        if (data) memcpy(vb.map, data, (size_t)size);
        fd = lvk_buffer_export_fd(&vb);
        if (fd < 0) { if (getenv("LORICA_VK_DEBUG")) fprintf(stderr, "LORICA VK: export de fd fallo\n");
            lvk_buffer_destroy(&vb); gl31_set_error(GL_OUT_OF_MEMORY); return; }
        while (BE(glGetError)() != GL_NO_ERROR) {}
        BE(glCreateMemoryObjectsEXT)(1, &mo);
        BE(glImportMemoryFdEXT)(mo, (GLuint64)vb.alloc_size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);   /* GL toma el fd */
        BE(glBufferStorageMemEXT)(target, size, mo, 0);
        err = BE(glGetError)();
        if (err != GL_NO_ERROR && getenv("LORICA_VK_DEBUG"))
            fprintf(stderr, "LORICA VK: import en GL fallo, error 0x%x (alloc %llu, size %ld)\n", err, (unsigned long long)vb.alloc_size, (long)size);
        if (err != GL_NO_ERROR) {
            if (mo) BE(glDeleteMemoryObjectsEXT)(1, &mo);
            lvk_buffer_destroy(&vb);
            gl31_set_error(err == GL_OUT_OF_MEMORY ? GL_OUT_OF_MEMORY : GL_INVALID_OPERATION);
            return;
        }
        pthread_mutex_lock(&g_bimu);
        bi = bi_add(id);
        if (bi) { bi->vk = 1; bi->memobj = mo; bi->vb = vb; }
        pthread_mutex_unlock(&g_bimu);
        if (!bi) { BE(glDeleteMemoryObjectsEXT)(1, &mo); lvk_buffer_destroy(&vb); gl31_set_error(GL_OUT_OF_MEMORY); return; }
    } else {
        /* sin storage nativo y sin mapeo persistente pedido: glBufferData con la sugerencia de uso */
        BE(glBufferData)(target, size, data,
                         (flags & (GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT)) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (!bi) bi = bi_add(id);
    if (bi) { bi->size = size; bi->flags = flags; }
    pthread_mutex_unlock(&g_bimu);
}

void gl31_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    gl31_state_t* s = gl31_state();
    if (target == GL_UNIFORM_BUFFER) {
        gl31_state_load_limits();
        if ((GLint)index >= s->max_ubo_bindings) { gl31_set_error(GL_INVALID_VALUE); return; }
        s->ubo[index].buffer = buffer;
        s->ubo[index].offset = 0;
        s->ubo[index].size = 0;
        s->uniform_buffer = buffer;
    }
    if (target == GL_SHADER_STORAGE_BUFFER) gl31_ssbo_record(index, buffer, 0, 0);
    BE(glBindBufferBase)(target, index, buffer);
}

void gl31_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    gl31_state_t* s = gl31_state();
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (target == GL_UNIFORM_BUFFER) {
        gl31_state_load_limits();
        if ((GLint)index >= s->max_ubo_bindings) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (buffer && (offset % s->ubo_alignment) != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        s->ubo[index].buffer = buffer;
        s->ubo[index].offset = offset;
        s->ubo[index].size = size;
        s->uniform_buffer = buffer;
    }
    if (target == GL_SHADER_STORAGE_BUFFER) gl31_ssbo_record(index, buffer, offset, size);
    BE(glBindBufferRange)(target, index, buffer, offset, size);
}

void gl31_glCopyBufferSubData(GLenum rt, GLenum wt, GLintptr ro, GLintptr wo, GLsizeiptr size)
{
    if (ro < 0 || wo < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glCopyBufferSubData)(rt, wt, ro, wo, size);
}

void* gl31_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    GLuint id = bound_buffer(target); bufinfo_t* bi; void* ret = NULL;
    GLbitfield persist = access & (GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (offset < 0 || length < 0) { gl31_set_error(GL_INVALID_VALUE); return NULL; }
    if ((access & GL_MAP_COHERENT_BIT) && !(access & GL_MAP_PERSISTENT_BIT)) { gl31_set_error(GL_INVALID_OPERATION); return NULL; }
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (!bi) {
        pthread_mutex_unlock(&g_bimu);
        if (persist) { gl31_set_error(GL_INVALID_OPERATION); return NULL; }   /* requiere glBufferStorage con esos flags */
        return BE(glMapBufferRange)(target, offset, length, access);
    }
    /* los bits de acceso deben estar permitidos por los flags de almacenamiento */
    if (((access & GL_MAP_READ_BIT) && !(bi->flags & GL_MAP_READ_BIT)) ||
        ((access & GL_MAP_WRITE_BIT) && !(bi->flags & GL_MAP_WRITE_BIT)) ||
        ((access & GL_MAP_PERSISTENT_BIT) && !(bi->flags & GL_MAP_PERSISTENT_BIT)) ||
        ((access & GL_MAP_COHERENT_BIT) && !(bi->flags & GL_MAP_COHERENT_BIT)) ||
        !(access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) || bi->mapped) {
        pthread_mutex_unlock(&g_bimu); gl31_set_error(GL_INVALID_OPERATION); return NULL;
    }
    if (offset + length > bi->size || length == 0) { pthread_mutex_unlock(&g_bimu); gl31_set_error(GL_INVALID_VALUE); return NULL; }
    if (bi->vk) {
        bi->mapped = 1; bi->map_access = access; bi->map_off = offset; bi->map_len = length;
        ret = (char*)bi->vb.map + offset;
        pthread_mutex_unlock(&g_bimu);
        return ret;
    }
    pthread_mutex_unlock(&g_bimu);
    return BE(glMapBufferRange)(target, offset, length, access);
}

void* gl31_glMapBuffer(GLenum target, GLenum access)
{
    GLint64 size = 0;
    GLbitfield flags;
    switch (access) {
        case GL_READ_ONLY:  flags = GL_MAP_READ_BIT; break;
        case GL_WRITE_ONLY: flags = GL_MAP_WRITE_BIT; break;
        case GL_READ_WRITE: flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;
        default: gl31_set_error(GL_INVALID_ENUM); return NULL;
    }
    if (BE(glGetBufferParameteri64v)) BE(glGetBufferParameteri64v)(target, GL_BUFFER_SIZE, &size);
    else { GLint v = 0; BE(glGetBufferParameteriv)(target, GL_BUFFER_SIZE, &v); size = v; }
    if (size <= 0) { gl31_set_error(GL_INVALID_OPERATION); return NULL; }
    return gl31_glMapBufferRange(target, 0, (GLsizeiptr)size, flags);
}

GLboolean gl31_glUnmapBuffer(GLenum target)
{
    GLuint id = bound_buffer(target); bufinfo_t* bi; int handled = 0, ok = GL_TRUE;
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (bi && bi->vk) {
        handled = 1;
        if (!bi->mapped) { gl31_set_error(GL_INVALID_OPERATION); ok = GL_FALSE; }
        bi->mapped = 0;
    }
    pthread_mutex_unlock(&g_bimu);
    return handled ? (GLboolean)ok : BE(glUnmapBuffer)(target);
}

void gl31_glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    GLuint id = bound_buffer(target); bufinfo_t* bi; int done = 0;
    if (!params) return;
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (bi) {
        done = 1;
        switch (pname) {
            case GL_BUFFER_IMMUTABLE_STORAGE: *params = GL_TRUE; break;
            case GL_BUFFER_STORAGE_FLAGS:     *params = (GLint)bi->flags; break;
            case GL_BUFFER_SIZE:              if (bi->vk) *params = (GLint)bi->size; else done = 0; break;
            case GL_BUFFER_MAPPED:            if (bi->vk) *params = bi->mapped ? GL_TRUE : GL_FALSE; else done = 0; break;
            case GL_BUFFER_ACCESS_FLAGS:      if (bi->vk) *params = bi->mapped ? (GLint)bi->map_access : 0; else done = 0; break;
            case GL_BUFFER_MAP_OFFSET:        if (bi->vk) *params = bi->mapped ? (GLint)bi->map_off : 0; else done = 0; break;
            case GL_BUFFER_MAP_LENGTH:        if (bi->vk) *params = bi->mapped ? (GLint)bi->map_len : 0; else done = 0; break;
            default: done = 0;
        }
    }
    pthread_mutex_unlock(&g_bimu);
    if (!done) BE(glGetBufferParameteriv)(target, pname, params);
}

GLboolean gl31_glIsBuffer(GLuint buffer) { return BE(glIsBuffer)(buffer); }

void gl31_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    GLuint id = bound_buffer(target); bufinfo_t* bi; int vk = 0, err = 0;
    if (offset < 0 || length < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (bi && bi->vk) {
        vk = 1;       /* memoria host-coherente: no hay nada que vaciar */
        if (!bi->mapped || !(bi->map_access & GL_MAP_FLUSH_EXPLICIT_BIT)) err = GL_INVALID_OPERATION;
        else if (offset + length > bi->map_len) err = GL_INVALID_VALUE;
    }
    pthread_mutex_unlock(&g_bimu);
    if (err) { gl31_set_error((GLenum)err); return; }
    if (!vk) BE(glFlushMappedBufferRange)(target, offset, length);
}

void gl31_glGetBufferPointerv(GLenum target, GLenum pname, void** params)
{
    GLuint id = bound_buffer(target); bufinfo_t* bi;
    if (!params) return;
    if (pname != GL_BUFFER_MAP_POINTER) { gl31_set_error(GL_INVALID_ENUM); return; }
    pthread_mutex_lock(&g_bimu);
    bi = bi_find(id);
    if (bi && bi->vk) {
        *params = bi->mapped ? (char*)bi->vb.map + bi->map_off : NULL;
        pthread_mutex_unlock(&g_bimu);
        return;
    }
    pthread_mutex_unlock(&g_bimu);
    BE(glGetBufferPointerv)(target, pname, params);
}

/* GL 4.4: enlazar varios buffers de una vez (emulado con un bucle) */
void gl31_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint* buffers)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    for (i = 0; i < count; i++) {
        gl31_glBindBufferBase(target, first + (GLuint)i, buffers ? buffers[i] : 0);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}

void gl31_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint* buffers,
                             const GLintptr* offsets, const GLsizeiptr* sizes)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (buffers && (!offsets || !sizes)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) {
        if (buffers && buffers[i]) gl31_glBindBufferRange(target, first + (GLuint)i, buffers[i], offsets[i], sizes[i]);
        else                       gl31_glBindBufferBase(target, first + (GLuint)i, 0);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}
