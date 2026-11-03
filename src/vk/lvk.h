/* lvk.h - nucleo Vulkan de Lorica.
 *
 * Papel: coprocesador de GPU para lo que GLES no puede expresar, trabajando sobre
 * memoria compartida con GLES y sincronizado en GPU (sync fd), sin pasar por la CPU.
 * Requisitos: Vulkan 1.1 o superior, sin extensiones opcionales. Solo se usan,
 * si existen, las extensiones de PLATAFORMA imprescindibles para compartir con GL
 * (external_memory_fd / AHB, external_semaphore_fd, external_fence_fd).
 * Todo se carga con dlopen: Lorica no se enlaza contra libvulkan. */
#ifndef LVK_H
#define LVK_H
#include "lvk_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lvk_caps {
    int      available;              /* instancia + dispositivo + cola listos */
    uint32_t api_version;            /* apiVersion del dispositivo fisico */
    char     device_name[256];
    uint32_t vendor_id;
    uint32_t queue_family;
    int      has_ext_mem_fd;         /* VK_KHR_external_memory_fd */
    int      has_ext_sem_fd;         /* VK_KHR_external_semaphore_fd */
    int      has_ext_fence_fd;       /* VK_KHR_external_fence_fd */
    int      has_ahb;                /* VK_ANDROID_external_memory_android_hardware_buffer */
    int      has_dma_buf;            /* VK_EXT_external_memory_dma_buf */
    int      has_foreign_queue;      /* VK_EXT_queue_family_foreign */
    int      sync_fd_export;         /* semaforo exportable como SYNC_FD (GPU->GL) */
    int      sync_fd_import;         /* semaforo importable como SYNC_FD (GL->GPU) */
    int      timestamps;             /* timestampValidBits > 0 en la cola elegida */
    uint32_t timestamp_bits;
    float    timestamp_period_ns;
    uint32_t max_storage_range;
    uint32_t max_compute_invocations;
} lvk_caps_t;

/* Inicializa (idempotente, seguro entre hilos). Devuelve 1 si Vulkan queda usable.
 * Variables: LORICA_VK=0 lo desactiva; LORICA_VK_LIB fuerza la ruta de libvulkan. */
int  lvk_init(void);
void lvk_shutdown(void);
const lvk_caps_t *lvk_caps(void);

/* ---- buffers ---- */
typedef struct lvk_buffer {
    VkBuffer       buf;
    VkDeviceMemory mem;
    VkDeviceSize   size;        /* tamano pedido */
    VkDeviceSize   alloc_size;  /* tamano real de la asignacion */
    void          *map;         /* NULL si no es visible desde host */
    int            exportable;
} lvk_buffer;

#define LVK_BUF_HOST      1u   /* visible y coherente en host (mapeo persistente) */
#define LVK_BUF_EXPORT    2u   /* exportable como fd opaco para GL_EXT_memory_object_fd */
int  lvk_buffer_create(lvk_buffer *b, VkDeviceSize size, VkBufferUsageFlags usage, unsigned flags);
int  lvk_buffer_export_fd(const lvk_buffer *b);   /* fd nuevo (el llamador lo cierra/entrega) o -1 */
void lvk_buffer_destroy(lvk_buffer *b);

/* ---- comandos ---- */
/* Graba y ejecuta una tanda en la cola de Lorica. wait_sync_fd >= 0: la tanda espera
 * ese sync fd en GPU (se consume el fd, incluso en error). Si out_sync_fd != NULL se
 * devuelve un sync fd que se senaliza al terminar (o -1 si ya termino). Si
 * out_sync_fd == NULL y block != 0 la llamada espera a la GPU. */
typedef void (*lvk_record_fn)(VkCommandBuffer cb, void *user);
int  lvk_run(lvk_record_fn rec, void *user, int wait_sync_fd, int *out_sync_fd, int block);

/* ---- timestamps de GPU (GL_TIMESTAMP / GL_TIME_ELAPSED) ---- */
/* Marca un instante: la GPU escribe su reloj cuando termina todo lo anterior. Si
 * wait_sync_fd >= 0 el instante es "cuando GL termino hasta ese fence". Devuelve el
 * slot (>=0) o -1. */
int  lvk_ts_mark(int wait_sync_fd);
/* 1 = listo y *ns relleno (nanosegundos del reloj de la GPU); 0 = aun no; -1 = error */
int  lvk_ts_get(int slot, uint64_t *ns, int block);
void lvk_ts_release(int slot);

/* utilidades */
uint32_t lvk_find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid);

#ifdef __cplusplus
}
#endif
#endif
