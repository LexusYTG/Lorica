/* lvk.c - nucleo Vulkan de Lorica (ver lvk.h) */
#define _GNU_SOURCE
#include "lvk.h"
#include <dlfcn.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ---- tipos que solo usa este fichero ---- */
typedef struct { VkStructureType sType; const void *pNext; uint32_t handleType; } VkPhysicalDeviceExternalSemaphoreInfo_;
typedef struct { VkStructureType sType; void *pNext; uint32_t exportFromImportedHandleTypes, compatibleHandleTypes, externalSemaphoreFeatures; } VkExternalSemaphoreProperties_;
typedef struct { VkStructureType sType; const void *pNext; uint32_t handleType; } VkPhysicalDeviceExternalFenceInfo_;
typedef struct { VkStructureType sType; void *pNext; uint32_t exportFromImportedHandleTypes, compatibleHandleTypes, externalFenceFeatures; } VkExternalFenceProperties_;
typedef struct { VkStructureType sType; const void *pNext; VkExternalFenceHandleTypeFlags handleTypes; } VkExportFenceCreateInfo_;
typedef struct { VkStructureType sType; const void *pNext; VkFence fence; uint32_t handleType; } VkFenceGetFdInfoKHR_;
#define STYPE_EXT_SEM_INFO   1000076000
#define STYPE_EXT_SEM_PROPS  1000076001
#define STYPE_EXT_FENCE_INFO 1000112000
#define STYPE_EXT_FENCE_PROPS 1000112001
#define STYPE_EXPORT_FENCE   1000113000
#define STYPE_FENCE_GET_FD   1000115001
#define EXT_FEATURE_EXPORTABLE 1u
#define EXT_FEATURE_IMPORTABLE 2u

#define VKFN(ret, name, args) typedef ret (*PFN_##name) args;
VKFN(VkResult, vkCreateInstance, (const VkInstanceCreateInfo *, const void *, VkInstance *))
VKFN(VkResult, vkEnumerateInstanceVersion, (uint32_t *))
VKFN(void, vkDestroyInstance, (VkInstance, const void *))
VKFN(VkResult, vkEnumeratePhysicalDevices, (VkInstance, uint32_t *, VkPhysicalDevice *))
VKFN(void, vkGetPhysicalDeviceProperties, (VkPhysicalDevice, VkPhysicalDeviceProperties *))
VKFN(void, vkGetPhysicalDeviceQueueFamilyProperties, (VkPhysicalDevice, uint32_t *, VkQueueFamilyProperties *))
VKFN(void, vkGetPhysicalDeviceMemoryProperties, (VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *))
VKFN(VkResult, vkEnumerateDeviceExtensionProperties, (VkPhysicalDevice, const char *, uint32_t *, VkExtensionProperties *))
VKFN(void, vkGetPhysicalDeviceExternalSemaphoreProperties, (VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo_ *, VkExternalSemaphoreProperties_ *))
VKFN(void, vkGetPhysicalDeviceExternalFenceProperties, (VkPhysicalDevice, const VkPhysicalDeviceExternalFenceInfo_ *, VkExternalFenceProperties_ *))
VKFN(VkResult, vkCreateDevice, (VkPhysicalDevice, const VkDeviceCreateInfo *, const void *, VkDevice *))
VKFN(void, vkDestroyDevice, (VkDevice, const void *))
VKFN(void, vkGetDeviceQueue, (VkDevice, uint32_t, uint32_t, VkQueue *))
VKFN(VkResult, vkCreateBuffer, (VkDevice, const VkBufferCreateInfo *, const void *, VkBuffer *))
VKFN(void, vkDestroyBuffer, (VkDevice, VkBuffer, const void *))
VKFN(void, vkGetBufferMemoryRequirements, (VkDevice, VkBuffer, VkMemoryRequirements *))
VKFN(VkResult, vkAllocateMemory, (VkDevice, const VkMemoryAllocateInfo *, const void *, VkDeviceMemory *))
VKFN(void, vkFreeMemory, (VkDevice, VkDeviceMemory, const void *))
VKFN(VkResult, vkBindBufferMemory, (VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize))
VKFN(VkResult, vkMapMemory, (VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void **))
VKFN(void, vkUnmapMemory, (VkDevice, VkDeviceMemory))
VKFN(VkResult, vkGetMemoryFdKHR, (VkDevice, const VkMemoryGetFdInfoKHR *, int *))
VKFN(VkResult, vkCreateCommandPool, (VkDevice, const VkCommandPoolCreateInfo *, const void *, VkCommandPool *))
VKFN(void, vkDestroyCommandPool, (VkDevice, VkCommandPool, const void *))
VKFN(VkResult, vkAllocateCommandBuffers, (VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *))
VKFN(void, vkFreeCommandBuffers, (VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer *))
VKFN(VkResult, vkBeginCommandBuffer, (VkCommandBuffer, const VkCommandBufferBeginInfo *))
VKFN(VkResult, vkEndCommandBuffer, (VkCommandBuffer))
VKFN(VkResult, vkResetCommandBuffer, (VkCommandBuffer, VkCommandBufferResetFlags))
VKFN(VkResult, vkQueueSubmit, (VkQueue, uint32_t, const VkSubmitInfo *, VkFence))
VKFN(VkResult, vkCreateFence, (VkDevice, const VkFenceCreateInfo *, const void *, VkFence *))
VKFN(void, vkDestroyFence, (VkDevice, VkFence, const void *))
VKFN(VkResult, vkWaitForFences, (VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t))
VKFN(VkResult, vkGetFenceStatus, (VkDevice, VkFence))
VKFN(VkResult, vkResetFences, (VkDevice, uint32_t, const VkFence *))
VKFN(VkResult, vkGetFenceFdKHR, (VkDevice, const VkFenceGetFdInfoKHR_ *, int *))
VKFN(VkResult, vkCreateSemaphore, (VkDevice, const VkSemaphoreCreateInfo *, const void *, VkSemaphore *))
VKFN(void, vkDestroySemaphore, (VkDevice, VkSemaphore, const void *))
VKFN(VkResult, vkImportSemaphoreFdKHR, (VkDevice, const VkImportSemaphoreFdInfoKHR *, int))
VKFN(VkResult, vkCreateQueryPool, (VkDevice, const VkQueryPoolCreateInfo *, const void *, VkQueryPool *))
VKFN(void, vkDestroyQueryPool, (VkDevice, VkQueryPool, const void *))
VKFN(void, vkCmdResetQueryPool, (VkCommandBuffer, VkQueryPool, uint32_t, uint32_t))
VKFN(void, vkCmdWriteTimestamp, (VkCommandBuffer, VkPipelineStageFlags, VkQueryPool, uint32_t))
VKFN(VkResult, vkGetQueryPoolResults, (VkDevice, VkQueryPool, uint32_t, uint32_t, size_t, void *, VkDeviceSize, VkQueryResultFlags))
VKFN(VkResult, vkDeviceWaitIdle, (VkDevice))

static struct {
    PFN_vkGetInstanceProcAddr gipa; PFN_vkGetDeviceProcAddr gdpa;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkGetPhysicalDeviceMemoryProperties GetMemProps;
    PFN_vkDestroyDevice DestroyDevice; PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkCreateBuffer CreateBuffer; PFN_vkDestroyBuffer DestroyBuffer; PFN_vkGetBufferMemoryRequirements GetBufMemReq;
    PFN_vkAllocateMemory AllocateMemory; PFN_vkFreeMemory FreeMemory; PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkMapMemory MapMemory; PFN_vkUnmapMemory UnmapMemory; PFN_vkGetMemoryFdKHR GetMemoryFd;
    PFN_vkCreateCommandPool CreateCommandPool; PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers; PFN_vkFreeCommandBuffers FreeCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer; PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer; PFN_vkQueueSubmit QueueSubmit;
    PFN_vkCreateFence CreateFence; PFN_vkDestroyFence DestroyFence; PFN_vkWaitForFences WaitForFences;
    PFN_vkGetFenceStatus GetFenceStatus; PFN_vkResetFences ResetFences; PFN_vkGetFenceFdKHR GetFenceFd;
    PFN_vkCreateSemaphore CreateSemaphore; PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkImportSemaphoreFdKHR ImportSemaphoreFd;
    PFN_vkCreateQueryPool CreateQueryPool; PFN_vkDestroyQueryPool DestroyQueryPool;
    PFN_vkCmdResetQueryPool CmdResetQueryPool; PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
    PFN_vkGetQueryPoolResults GetQueryPoolResults; PFN_vkDeviceWaitIdle DeviceWaitIdle;
} F;

#define TS_SLOTS 256
typedef struct { VkCommandBuffer cb; VkFence fence; VkSemaphore sem; int used, submitted; } ts_slot;
typedef struct lvk_pending { VkCommandBuffer cb; VkFence fence; VkSemaphore sem; int fd; struct lvk_pending *next; } lvk_pending;

static struct {
    pthread_mutex_t mu;
    int state;                      /* 0 sin intentar, 1 listo, -1 fallo */
    void *lib; VkInstance inst; VkPhysicalDevice pd; VkDevice dev; VkQueue q;
    VkCommandPool pool; VkPhysicalDeviceMemoryProperties mem;
    lvk_caps_t caps;
    VkQueryPool tpool; ts_slot ts[TS_SLOTS];
    lvk_pending *pending;
} G = { .mu = PTHREAD_MUTEX_INITIALIZER };

static void logmsg(const char *m) { if (getenv("LIBGL_DEBUG") || getenv("LORICA_VK_DEBUG")) fprintf(stderr, "LORICA VK: %s\n", m); }
static int has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{ for (uint32_t i = 0; i < n; i++) if (!strcmp(e[i].extensionName, name)) return 1; return 0; }

#define LOADI(var, name) do { var = (PFN_##name)F.gipa(G.inst, #name); } while (0)
#define LOADD(field, name) do { F.field = (PFN_##name)F.gdpa(G.dev, #name); } while (0)

static void pending_reap(int force);

static int init_locked(void)
{
    static const char *const libs[] = { NULL /*env*/, "libvulkan.so.1", "libvulkan.so", "/system/lib64/libvulkan.so",
                                        "/system/lib/libvulkan.so", "/vendor/lib64/libvulkan.so", NULL };
    const char *envlib = getenv("LORICA_VK_LIB");
    PFN_vkCreateInstance create_instance; PFN_vkEnumerateInstanceVersion enum_ver;
    uint32_t iver = VK_MAKE_API_VERSION(0, 1, 0, 0);
    VkApplicationInfo app; VkInstanceCreateInfo ici; VkResult r;
    uint32_t npd = 0, nq = 0, ne = 0; VkQueueFamilyProperties qp[16];
    VkExtensionProperties *ext = NULL; const char *en[8]; uint32_t nen = 0; float prio = 1.0f;

    if (getenv("LORICA_VK") && !strcmp(getenv("LORICA_VK"), "0")) return -1;
    for (int i = 0; i < 7 && !G.lib; i++) {
        const char *n = (i == 0) ? envlib : libs[i];
        if (n) G.lib = dlopen(n, RTLD_NOW | RTLD_LOCAL);
    }
    if (!G.lib) { logmsg("no se pudo abrir libvulkan"); return -1; }
    F.gipa = (PFN_vkGetInstanceProcAddr)dlsym(G.lib, "vkGetInstanceProcAddr");
    if (!F.gipa) F.gipa = (PFN_vkGetInstanceProcAddr)dlsym(G.lib, "vk_icdGetInstanceProcAddr");
    if (!F.gipa) { logmsg("sin vkGetInstanceProcAddr"); return -1; }
    create_instance = (PFN_vkCreateInstance)F.gipa(NULL, "vkCreateInstance");
    enum_ver = (PFN_vkEnumerateInstanceVersion)F.gipa(NULL, "vkEnumerateInstanceVersion");
    if (!create_instance) return -1;
    if (enum_ver) enum_ver(&iver);
    if (iver < VK_API_VERSION_1_1) { logmsg("instancia Vulkan < 1.1: no se usa"); return -1; }

    memset(&app, 0, sizeof app); app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "lorica"; app.apiVersion = VK_API_VERSION_1_1;
    memset(&ici, 0, sizeof ici); ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.pApplicationInfo = &app;
    r = create_instance(&ici, NULL, &G.inst);
    if (r != VK_SUCCESS) { logmsg("vkCreateInstance fallo"); return -1; }

    { PFN_vkEnumeratePhysicalDevices epd; PFN_vkGetPhysicalDeviceProperties gp; PFN_vkGetPhysicalDeviceQueueFamilyProperties gq;
      PFN_vkEnumerateDeviceExtensionProperties ede; PFN_vkGetPhysicalDeviceExternalSemaphoreProperties gesp;
      PFN_vkGetPhysicalDeviceExternalFenceProperties gefp; PFN_vkCreateDevice cd; VkPhysicalDevice pds[8];
      VkPhysicalDeviceProperties props; uint32_t best = 0; int bestscore = -1;
      LOADI(epd, vkEnumeratePhysicalDevices); LOADI(gp, vkGetPhysicalDeviceProperties);
      LOADI(gq, vkGetPhysicalDeviceQueueFamilyProperties); LOADI(F.GetMemProps, vkGetPhysicalDeviceMemoryProperties);
      LOADI(ede, vkEnumerateDeviceExtensionProperties); LOADI(gesp, vkGetPhysicalDeviceExternalSemaphoreProperties);
      LOADI(gefp, vkGetPhysicalDeviceExternalFenceProperties); LOADI(cd, vkCreateDevice);
      LOADI(F.DestroyInstance, vkDestroyInstance); LOADI(F.gdpa, vkGetDeviceProcAddr);
      if (!epd || !gp || !gq || !F.GetMemProps || !ede || !cd || !F.gdpa) return -1;
      npd = 8; r = epd(G.inst, &npd, pds);
      if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || !npd) { logmsg("sin dispositivos fisicos"); return -1; }
      for (uint32_t i = 0; i < npd; i++) {   /* preferir GPU real sobre CPU; 1.1+ */
          gp(pds[i], &props);
          if (props.apiVersion < VK_API_VERSION_1_1) continue;
          int score = (props.deviceType == 1 || props.deviceType == 2) ? 2 : (props.deviceType == 4 ? 0 : 1);
          if (score > bestscore) { bestscore = score; best = i; }
      }
      if (bestscore < 0) { logmsg("ningun dispositivo Vulkan >= 1.1"); return -1; }
      G.pd = pds[best]; gp(G.pd, &props);
      G.caps.api_version = props.apiVersion; G.caps.vendor_id = props.vendorID;
      strncpy(G.caps.device_name, props.deviceName, sizeof G.caps.device_name - 1);
      G.caps.timestamp_period_ns = props.limits.timestampPeriod;
      G.caps.max_storage_range = props.limits.maxStorageBufferRange;
      G.caps.max_compute_invocations = props.limits.maxComputeWorkGroupInvocations;
      nq = 16; gq(G.pd, &nq, qp);
      G.caps.queue_family = 0xffffffffu;
      for (uint32_t i = 0; i < nq; i++)      /* cola con compute (la grafica la incluye) y timestamps */
          if ((qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && G.caps.queue_family == 0xffffffffu) {
              G.caps.queue_family = i; G.caps.timestamp_bits = qp[i].timestampValidBits; }
      if (G.caps.queue_family == 0xffffffffu) { logmsg("sin cola de compute"); return -1; }
      G.caps.timestamps = G.caps.timestamp_bits > 0 && props.limits.timestampPeriod > 0.0f;
      F.GetMemProps(G.pd, &G.mem);

      ede(G.pd, NULL, &ne, NULL); ext = (VkExtensionProperties *)calloc(ne ? ne : 1, sizeof *ext);
      if (!ext) return -1;
      ede(G.pd, NULL, &ne, ext);
      G.caps.has_ext_mem_fd = has_ext(ext, ne, "VK_KHR_external_memory_fd");
      G.caps.has_ext_sem_fd = has_ext(ext, ne, "VK_KHR_external_semaphore_fd");
      G.caps.has_ext_fence_fd = has_ext(ext, ne, "VK_KHR_external_fence_fd");
      G.caps.has_ahb = has_ext(ext, ne, "VK_ANDROID_external_memory_android_hardware_buffer");
      G.caps.has_dma_buf = has_ext(ext, ne, "VK_EXT_external_memory_dma_buf");
      G.caps.has_foreign_queue = has_ext(ext, ne, "VK_EXT_queue_family_foreign");
      if (G.caps.has_ext_mem_fd) en[nen++] = "VK_KHR_external_memory_fd";
      if (G.caps.has_ext_sem_fd) en[nen++] = "VK_KHR_external_semaphore_fd";
      if (G.caps.has_ext_fence_fd) en[nen++] = "VK_KHR_external_fence_fd";
      if (G.caps.has_ahb) { en[nen++] = "VK_ANDROID_external_memory_android_hardware_buffer";
          /* dependencias de la extension AHB que NO son core en 1.1 */
          if (has_ext(ext, ne, "VK_KHR_sampler_ycbcr_conversion") && G.caps.api_version < VK_API_VERSION_1_1) en[nen++] = "VK_KHR_sampler_ycbcr_conversion";
          if (G.caps.has_foreign_queue) en[nen++] = "VK_EXT_queue_family_foreign"; }
      else if (G.caps.has_foreign_queue) en[nen++] = "VK_EXT_queue_family_foreign";
      if (G.caps.has_dma_buf && G.caps.has_ext_mem_fd) en[nen++] = "VK_EXT_external_memory_dma_buf";
      free(ext);

      if (gesp) { VkPhysicalDeviceExternalSemaphoreInfo_ si = { STYPE_EXT_SEM_INFO, NULL, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT };
          VkExternalSemaphoreProperties_ sp; memset(&sp, 0, sizeof sp); sp.sType = STYPE_EXT_SEM_PROPS;
          gesp(G.pd, &si, &sp);
          G.caps.sync_fd_import = G.caps.has_ext_sem_fd && (sp.externalSemaphoreFeatures & EXT_FEATURE_IMPORTABLE);
          (void)EXT_FEATURE_EXPORTABLE; }
      if (gefp) { VkPhysicalDeviceExternalFenceInfo_ fi = { STYPE_EXT_FENCE_INFO, NULL, VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT };
          VkExternalFenceProperties_ fp; memset(&fp, 0, sizeof fp); fp.sType = STYPE_EXT_FENCE_PROPS;
          gefp(G.pd, &fi, &fp);
          G.caps.sync_fd_export = G.caps.has_ext_fence_fd && (fp.externalFenceFeatures & EXT_FEATURE_EXPORTABLE); }

      { VkDeviceQueueCreateInfo qci; VkDeviceCreateInfo dci;
        memset(&qci, 0, sizeof qci); qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = G.caps.queue_family; qci.queueCount = 1; qci.pQueuePriorities = &prio;
        memset(&dci, 0, sizeof dci); dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = nen; dci.ppEnabledExtensionNames = en;
        r = cd(G.pd, &dci, NULL, &G.dev);
        if (r != VK_SUCCESS) { logmsg("vkCreateDevice fallo"); return -1; } }
    }
    LOADD(DestroyDevice, vkDestroyDevice); LOADD(GetDeviceQueue, vkGetDeviceQueue);
    LOADD(CreateBuffer, vkCreateBuffer); LOADD(DestroyBuffer, vkDestroyBuffer);
    LOADD(GetBufMemReq, vkGetBufferMemoryRequirements); LOADD(AllocateMemory, vkAllocateMemory);
    LOADD(FreeMemory, vkFreeMemory); LOADD(BindBufferMemory, vkBindBufferMemory);
    LOADD(MapMemory, vkMapMemory); LOADD(UnmapMemory, vkUnmapMemory); LOADD(GetMemoryFd, vkGetMemoryFdKHR);
    LOADD(CreateCommandPool, vkCreateCommandPool); LOADD(DestroyCommandPool, vkDestroyCommandPool);
    LOADD(AllocateCommandBuffers, vkAllocateCommandBuffers); LOADD(FreeCommandBuffers, vkFreeCommandBuffers);
    LOADD(BeginCommandBuffer, vkBeginCommandBuffer); LOADD(EndCommandBuffer, vkEndCommandBuffer);
    LOADD(ResetCommandBuffer, vkResetCommandBuffer); LOADD(QueueSubmit, vkQueueSubmit);
    LOADD(CreateFence, vkCreateFence); LOADD(DestroyFence, vkDestroyFence); LOADD(WaitForFences, vkWaitForFences);
    LOADD(GetFenceStatus, vkGetFenceStatus); LOADD(ResetFences, vkResetFences); LOADD(GetFenceFd, vkGetFenceFdKHR);
    LOADD(CreateSemaphore, vkCreateSemaphore); LOADD(DestroySemaphore, vkDestroySemaphore);
    LOADD(ImportSemaphoreFd, vkImportSemaphoreFdKHR); LOADD(CreateQueryPool, vkCreateQueryPool);
    LOADD(DestroyQueryPool, vkDestroyQueryPool); LOADD(CmdResetQueryPool, vkCmdResetQueryPool);
    LOADD(CmdWriteTimestamp, vkCmdWriteTimestamp); LOADD(GetQueryPoolResults, vkGetQueryPoolResults);
    LOADD(DeviceWaitIdle, vkDeviceWaitIdle);
    if (!F.GetDeviceQueue || !F.CreateBuffer || !F.AllocateMemory || !F.QueueSubmit || !F.CreateCommandPool ||
        !F.CreateFence || !F.WaitForFences || !F.GetFenceStatus) { logmsg("faltan funciones de dispositivo"); return -1; }
    if (G.caps.has_ext_mem_fd && !F.GetMemoryFd) G.caps.has_ext_mem_fd = 0;
    if (!F.ImportSemaphoreFd || !F.CreateSemaphore) G.caps.sync_fd_import = 0;
    if (!F.GetFenceFd) G.caps.sync_fd_export = 0;
    if (!F.CmdWriteTimestamp || !F.CmdResetQueryPool || !F.GetQueryPoolResults || !F.CreateQueryPool) G.caps.timestamps = 0;
    F.GetDeviceQueue(G.dev, G.caps.queue_family, 0, &G.q);
    { VkCommandPoolCreateInfo pci; memset(&pci, 0, sizeof pci); pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pci.queueFamilyIndex = G.caps.queue_family;
      if (F.CreateCommandPool(G.dev, &pci, NULL, &G.pool) != VK_SUCCESS) return -1; }
    if (G.caps.timestamps) {
        VkQueryPoolCreateInfo qi; memset(&qi, 0, sizeof qi); qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qi.queryType = VK_QUERY_TYPE_TIMESTAMP; qi.queryCount = TS_SLOTS;
        if (F.CreateQueryPool(G.dev, &qi, NULL, &G.tpool) != VK_SUCCESS) G.caps.timestamps = 0;
    }
    G.caps.available = 1;
    if (getenv("LIBGL_DEBUG") || getenv("LORICA_VK_DEBUG"))
        fprintf(stderr, "LORICA VK: %s api %u.%u  memfd=%d semfd=%d fencefd=%d ahb=%d syncfd(imp=%d exp=%d) ts=%d(%u bits, %.2f ns)\n",
                G.caps.device_name, G.caps.api_version >> 22, (G.caps.api_version >> 12) & 0x3ff,
                G.caps.has_ext_mem_fd, G.caps.has_ext_sem_fd, G.caps.has_ext_fence_fd, G.caps.has_ahb,
                G.caps.sync_fd_import, G.caps.sync_fd_export, G.caps.timestamps, G.caps.timestamp_bits, G.caps.timestamp_period_ns);
    return 1;
}

int lvk_init(void)
{
    int s;
    pthread_mutex_lock(&G.mu);
    if (G.state == 0) {
        G.state = init_locked();
        if (G.state != 1) { /* dejar el estado limpio ante un fallo a medias */
            if (G.dev && F.DestroyDevice) F.DestroyDevice(G.dev, NULL);
            if (G.inst && F.DestroyInstance) F.DestroyInstance(G.inst, NULL);
            G.dev = NULL; G.inst = NULL; G.caps.available = 0;
        }
    }
    s = G.state;
    pthread_mutex_unlock(&G.mu);
    return s == 1;
}

const lvk_caps_t *lvk_caps(void) { return &G.caps; }

void lvk_shutdown(void)
{
    pthread_mutex_lock(&G.mu);
    if (G.state == 1) {
        F.DeviceWaitIdle(G.dev);
        pending_reap(1);
        for (int i = 0; i < TS_SLOTS; i++) {
            if (G.ts[i].fence) F.DestroyFence(G.dev, G.ts[i].fence, NULL);
            if (G.ts[i].sem && F.DestroySemaphore) F.DestroySemaphore(G.dev, G.ts[i].sem, NULL);
            if (G.ts[i].cb) F.FreeCommandBuffers(G.dev, G.pool, 1, &G.ts[i].cb);
        }
        if (G.tpool) F.DestroyQueryPool(G.dev, G.tpool, NULL);
        F.DestroyCommandPool(G.dev, G.pool, NULL);
        F.DestroyDevice(G.dev, NULL); F.DestroyInstance(G.inst, NULL);
    }
    memset(&G.ts, 0, sizeof G.ts); G.tpool = 0; G.pool = 0; G.dev = NULL; G.inst = NULL; G.pending = NULL;
    memset(&G.caps, 0, sizeof G.caps);
    G.state = 0;     /* permite reiniciar tras un shutdown */
    pthread_mutex_unlock(&G.mu);
}

uint32_t lvk_find_memory_type(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid)
{
    for (uint32_t i = 0; i < G.mem.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (G.mem.memoryTypes[i].propertyFlags & want) == want &&
            !(G.mem.memoryTypes[i].propertyFlags & avoid)) return i;
    return 0xffffffffu;
}

/* ---------------- buffers ---------------- */
int lvk_buffer_create(lvk_buffer *b, VkDeviceSize size, VkBufferUsageFlags usage, unsigned flags)
{
    VkBufferCreateInfo bci; VkExternalMemoryBufferCreateInfo ebi; VkExportMemoryAllocateInfo exi;
    VkMemoryRequirements mr; VkMemoryAllocateInfo mai; uint32_t ti;
    memset(b, 0, sizeof *b);
    if (!G.caps.available || !size) return 0;
    if ((flags & LVK_BUF_EXPORT) && !G.caps.has_ext_mem_fd) return 0;
    memset(&bci, 0, sizeof bci); bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bci.size = size; bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (flags & LVK_BUF_EXPORT) {
        memset(&ebi, 0, sizeof ebi); ebi.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
        ebi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; bci.pNext = &ebi;
    }
    if (F.CreateBuffer(G.dev, &bci, NULL, &b->buf) != VK_SUCCESS) return 0;
    F.GetBufMemReq(G.dev, b->buf, &mr);
    if (flags & LVK_BUF_HOST) {
        ti = lvk_find_memory_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0);
    } else {
        ti = lvk_find_memory_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
        if (ti == 0xffffffffu) ti = lvk_find_memory_type(mr.memoryTypeBits, 0, 0);
    }
    if (ti == 0xffffffffu) { F.DestroyBuffer(G.dev, b->buf, NULL); memset(b, 0, sizeof *b); return 0; }
    memset(&mai, 0, sizeof mai); mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size; mai.memoryTypeIndex = ti;
    if (flags & LVK_BUF_EXPORT) {
        memset(&exi, 0, sizeof exi); exi.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; mai.pNext = &exi;
    }
    if (F.AllocateMemory(G.dev, &mai, NULL, &b->mem) != VK_SUCCESS ||
        F.BindBufferMemory(G.dev, b->buf, b->mem, 0) != VK_SUCCESS) {
        if (b->mem) F.FreeMemory(G.dev, b->mem, NULL);
        F.DestroyBuffer(G.dev, b->buf, NULL); memset(b, 0, sizeof *b); return 0;
    }
    b->size = size; b->alloc_size = mr.size; b->exportable = (flags & LVK_BUF_EXPORT) != 0;
    if (flags & LVK_BUF_HOST) {
        if (F.MapMemory(G.dev, b->mem, 0, VK_WHOLE_SIZE, 0, &b->map) != VK_SUCCESS) { lvk_buffer_destroy(b); return 0; }
    }
    return 1;
}

int lvk_buffer_export_fd(const lvk_buffer *b)
{
    VkMemoryGetFdInfoKHR gi; int fd = -1;
    if (!b || !b->exportable || !F.GetMemoryFd) return -1;
    memset(&gi, 0, sizeof gi); gi.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR; gi.memory = b->mem;
    gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    return F.GetMemoryFd(G.dev, &gi, &fd) == VK_SUCCESS ? fd : -1;
}

void lvk_buffer_destroy(lvk_buffer *b)
{
    if (!b || !G.caps.available) return;
    if (b->map) F.UnmapMemory(G.dev, b->mem);
    if (b->buf) F.DestroyBuffer(G.dev, b->buf, NULL);
    if (b->mem) F.FreeMemory(G.dev, b->mem, NULL);
    memset(b, 0, sizeof *b);
}

/* ---------------- ejecucion ---------------- */
static int fd_signaled(int fd)
{
    struct pollfd p; p.fd = fd; p.events = POLLIN; p.revents = 0;
    return poll(&p, 1, 0) > 0;
}

static void pending_free(lvk_pending *p)
{
    if (p->cb) F.FreeCommandBuffers(G.dev, G.pool, 1, &p->cb);
    if (p->fence) F.DestroyFence(G.dev, p->fence, NULL);
    if (p->sem) F.DestroySemaphore(G.dev, p->sem, NULL);
    if (p->fd >= 0) close(p->fd);
    free(p);
}

/* libera tandas terminadas (lock tomado). force: espera a la GPU y libera todo. */
static void pending_reap(int force)
{
    lvk_pending **pp = &G.pending;
    while (*pp) {
        lvk_pending *p = *pp; int done;
        if (force) done = 1;
        else if (p->fd >= 0) done = fd_signaled(p->fd);
        else done = (F.GetFenceStatus(G.dev, p->fence) == VK_SUCCESS);
        if (done) { *pp = p->next; pending_free(p); } else pp = &p->next;
    }
}

int lvk_run(lvk_record_fn rec, void *user, int wait_sync_fd, int *out_sync_fd, int block)
{
    lvk_pending *p; VkCommandBufferAllocateInfo cai; VkCommandBufferBeginInfo bi; VkSubmitInfo si;
    VkFenceCreateInfo fci; VkExportFenceCreateInfo_ efi; VkPipelineStageFlags wst = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    int ok = 0, want_fd = (out_sync_fd != NULL && G.caps.sync_fd_export);
    if (out_sync_fd) *out_sync_fd = -1;
    if (!G.caps.available) { if (wait_sync_fd >= 0) close(wait_sync_fd); return 0; }
    pthread_mutex_lock(&G.mu);
    pending_reap(0);
    p = (lvk_pending *)calloc(1, sizeof *p);
    if (!p) { if (wait_sync_fd >= 0) close(wait_sync_fd); pthread_mutex_unlock(&G.mu); return 0; }
    p->fd = -1;
    memset(&cai, 0, sizeof cai); cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cai.commandPool = G.pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    if (F.AllocateCommandBuffers(G.dev, &cai, &p->cb) != VK_SUCCESS) { p->cb = NULL; goto fail; }
    memset(&bi, 0, sizeof bi); bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (F.BeginCommandBuffer(p->cb, &bi) != VK_SUCCESS) goto fail;
    rec(p->cb, user);
    if (F.EndCommandBuffer(p->cb) != VK_SUCCESS) goto fail;
    memset(&fci, 0, sizeof fci); fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (want_fd) { memset(&efi, 0, sizeof efi); efi.sType = STYPE_EXPORT_FENCE; efi.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT; fci.pNext = &efi; }
    if (F.CreateFence(G.dev, &fci, NULL, &p->fence) != VK_SUCCESS) { p->fence = 0; goto fail; }
    memset(&si, 0, sizeof si); si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount = 1; si.pCommandBuffers = &p->cb;
    if (wait_sync_fd >= 0) {
        if (G.caps.sync_fd_import) {
            VkSemaphoreCreateInfo sci; VkImportSemaphoreFdInfoKHR isf;
            memset(&sci, 0, sizeof sci); sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            if (F.CreateSemaphore(G.dev, &sci, NULL, &p->sem) != VK_SUCCESS) { p->sem = 0; goto fail; }
            memset(&isf, 0, sizeof isf); isf.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR; isf.semaphore = p->sem;
            isf.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT; isf.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
            isf.fd = wait_sync_fd;
            if (F.ImportSemaphoreFd(G.dev, &isf, 0) != VK_SUCCESS) goto fail;   /* fd consumido solo si OK */
            wait_sync_fd = -1;
            si.waitSemaphoreCount = 1; si.pWaitSemaphores = &p->sem; si.pWaitDstStageMask = &wst;
        } else {   /* sin import de sync fd: espera en host (correcto, pero bloquea el hilo) */
            struct pollfd pf; pf.fd = wait_sync_fd; pf.events = POLLIN;
            while (poll(&pf, 1, -1) < 0 && errno == EINTR) {}
            close(wait_sync_fd); wait_sync_fd = -1;
        }
    }
    if (F.QueueSubmit(G.q, 1, &si, p->fence) != VK_SUCCESS) goto fail;
    if (want_fd) {
        VkFenceGetFdInfoKHR_ gi; int fd = -1; memset(&gi, 0, sizeof gi);
        gi.sType = STYPE_FENCE_GET_FD; gi.fence = p->fence; gi.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
        if (F.GetFenceFd(G.dev, &gi, &fd) == VK_SUCCESS) { *out_sync_fd = fd; if (fd >= 0) p->fd = dup(fd); }
    } else if (block || out_sync_fd) {
        /* sin fd exportable: el llamador pidio sync -> se garantiza con espera en host */
        F.WaitForFences(G.dev, 1, &p->fence, 1, ~0ULL);
    }
    p->next = G.pending; G.pending = p; p = NULL; ok = 1;
fail:
    if (wait_sync_fd >= 0) close(wait_sync_fd);
    if (p) pending_free(p);
    pthread_mutex_unlock(&G.mu);
    return ok;
}

/* ---------------- timestamps ---------------- */
static int ts_submit(ts_slot *s, int slot, int wait_fd)
{
    VkCommandBufferBeginInfo bi; VkSubmitInfo si; VkPipelineStageFlags wst = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    memset(&si, 0, sizeof si); si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    if (!s->cb) {
        VkCommandBufferAllocateInfo cai; memset(&cai, 0, sizeof cai);
        cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cai.commandPool = G.pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
        if (F.AllocateCommandBuffers(G.dev, &cai, &s->cb) != VK_SUCCESS) { s->cb = NULL; return 0; }
    } else F.ResetCommandBuffer(s->cb, 0);
    if (!s->fence) {
        VkFenceCreateInfo fci; memset(&fci, 0, sizeof fci); fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (F.CreateFence(G.dev, &fci, NULL, &s->fence) != VK_SUCCESS) { s->fence = 0; return 0; }
    } else F.ResetFences(G.dev, 1, &s->fence);
    memset(&bi, 0, sizeof bi); bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (F.BeginCommandBuffer(s->cb, &bi) != VK_SUCCESS) return 0;
    F.CmdResetQueryPool(s->cb, G.tpool, (uint32_t)slot, 1);
    F.CmdWriteTimestamp(s->cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, G.tpool, (uint32_t)slot);
    if (F.EndCommandBuffer(s->cb) != VK_SUCCESS) return 0;
    si.commandBufferCount = 1; si.pCommandBuffers = &s->cb;
    if (wait_fd >= 0) {
        if (G.caps.sync_fd_import) {
            VkImportSemaphoreFdInfoKHR isf;
            if (!s->sem) { VkSemaphoreCreateInfo sci; memset(&sci, 0, sizeof sci); sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                if (F.CreateSemaphore(G.dev, &sci, NULL, &s->sem) != VK_SUCCESS) { s->sem = 0; return 0; } }
            memset(&isf, 0, sizeof isf); isf.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR; isf.semaphore = s->sem;
            isf.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT; isf.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT; isf.fd = wait_fd;
            if (F.ImportSemaphoreFd(G.dev, &isf, 0) != VK_SUCCESS) return 0;
            si.waitSemaphoreCount = 1; si.pWaitSemaphores = &s->sem; si.pWaitDstStageMask = &wst;
        } else {
            struct pollfd pf; pf.fd = wait_fd; pf.events = POLLIN;
            while (poll(&pf, 1, -1) < 0 && errno == EINTR) {}
            close(wait_fd);
        }
    }
    return F.QueueSubmit(G.q, 1, &si, s->fence) == VK_SUCCESS;
}

int lvk_ts_mark(int wait_fd)
{
    int slot = -1;
    if (!G.caps.available || !G.caps.timestamps) { if (wait_fd >= 0) close(wait_fd); return -1; }
    pthread_mutex_lock(&G.mu);
    for (int i = 0; i < TS_SLOTS; i++) if (!G.ts[i].used) { slot = i; break; }
    if (slot >= 0) {
        G.ts[slot].used = 1;
        /* si wait_fd fue consumido por un import correcto el fd ya no es nuestro; si falla, cerrarlo */
        if (ts_submit(&G.ts[slot], slot, wait_fd)) { G.ts[slot].submitted = 1; wait_fd = -1; }
        else { G.ts[slot].used = 0; slot = -1; }
    }
    if (wait_fd >= 0 && !G.caps.sync_fd_import) wait_fd = -1;  /* ya cerrado en la rama host */
    if (wait_fd >= 0) close(wait_fd);
    pthread_mutex_unlock(&G.mu);
    return slot;
}

int lvk_ts_get(int slot, uint64_t *ns, int block)
{
    ts_slot *s; uint64_t ticks = 0; VkResult r; int ret;
    if (slot < 0 || slot >= TS_SLOTS || !ns || !G.caps.available) return -1;
    pthread_mutex_lock(&G.mu);
    s = &G.ts[slot];
    if (!s->used || !s->submitted) { pthread_mutex_unlock(&G.mu); return -1; }
    r = block ? F.WaitForFences(G.dev, 1, &s->fence, 1, ~0ULL) : F.GetFenceStatus(G.dev, s->fence);
    if (r == VK_SUCCESS) {
        r = F.GetQueryPoolResults(G.dev, G.tpool, (uint32_t)slot, 1, sizeof ticks, &ticks, sizeof ticks, VK_QUERY_RESULT_64_BIT);
        if (r == VK_SUCCESS) {
            if (G.caps.timestamp_bits < 64) ticks &= ((1ULL << G.caps.timestamp_bits) - 1);
            *ns = (uint64_t)((double)ticks * (double)G.caps.timestamp_period_ns);
            ret = 1;
        } else ret = (r == VK_NOT_READY) ? 0 : -1;
    } else ret = (r == VK_NOT_READY || r == VK_TIMEOUT) ? 0 : -1;
    pthread_mutex_unlock(&G.mu);
    return ret;
}

void lvk_ts_release(int slot)
{
    if (slot < 0 || slot >= TS_SLOTS) return;
    pthread_mutex_lock(&G.mu);
    if (G.ts[slot].used) {
        /* no reutilizar mientras la GPU aun la usa */
        if (G.ts[slot].submitted && G.ts[slot].fence) F.WaitForFences(G.dev, 1, &G.ts[slot].fence, 1, ~0ULL);
        G.ts[slot].used = 0; G.ts[slot].submitted = 0;
    }
    pthread_mutex_unlock(&G.mu);
}
