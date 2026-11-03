/* lvk_types.h - subconjunto minimo de Vulkan 1.0/1.1 (ABI exacto del registro Khronos).
 * Lorica carga libvulkan con dlopen: no hay dependencia de cabeceras ni de enlace.
 * Solo se declara lo que usa src/vk. Valores copiados de vulkan_core.h (1.1). */
#ifndef LVK_TYPES_H
#define LVK_TYPES_H
#include <stdint.h>
#include <stddef.h>

#define LVK_DEFINE_HANDLE(o) typedef struct o##_T *o
#if defined(__LP64__) || defined(_WIN64) || defined(__aarch64__) || defined(__x86_64__)
#define LVK_DEFINE_NDH(o) typedef struct o##_T *o
#else
#define LVK_DEFINE_NDH(o) typedef uint64_t o
#endif
#define LVKAPI

LVK_DEFINE_HANDLE(VkInstance); LVK_DEFINE_HANDLE(VkPhysicalDevice); LVK_DEFINE_HANDLE(VkDevice);
LVK_DEFINE_HANDLE(VkQueue); LVK_DEFINE_HANDLE(VkCommandBuffer);
LVK_DEFINE_NDH(VkDeviceMemory); LVK_DEFINE_NDH(VkBuffer); LVK_DEFINE_NDH(VkImage); LVK_DEFINE_NDH(VkImageView);
LVK_DEFINE_NDH(VkCommandPool); LVK_DEFINE_NDH(VkFence); LVK_DEFINE_NDH(VkSemaphore); LVK_DEFINE_NDH(VkQueryPool);
LVK_DEFINE_NDH(VkShaderModule); LVK_DEFINE_NDH(VkPipeline); LVK_DEFINE_NDH(VkPipelineLayout);
LVK_DEFINE_NDH(VkDescriptorSetLayout); LVK_DEFINE_NDH(VkDescriptorPool); LVK_DEFINE_NDH(VkDescriptorSet);
LVK_DEFINE_NDH(VkPipelineCache);

typedef uint32_t VkFlags; typedef uint32_t VkBool32; typedef uint64_t VkDeviceSize;
typedef VkFlags VkInstanceCreateFlags, VkDeviceCreateFlags, VkDeviceQueueCreateFlags, VkMemoryMapFlags,
    VkQueryPoolCreateFlags, VkQueryPipelineStatisticFlags, VkCommandPoolCreateFlags, VkCommandBufferUsageFlags,
    VkImageUsageFlags, VkImageCreateFlags, VkBufferUsageFlags, VkBufferCreateFlags, VkQueueFlags,
    VkMemoryPropertyFlags, VkMemoryHeapFlags, VkAccessFlags, VkPipelineStageFlags, VkFenceCreateFlags,
    VkSemaphoreCreateFlags, VkSampleCountFlags, VkImageAspectFlags, VkQueryResultFlags, VkShaderStageFlags,
    VkDescriptorPoolCreateFlags, VkDependencyFlags, VkFormatFeatureFlags, VkCommandBufferResetFlags,
    VkShaderModuleCreateFlags, VkPipelineShaderStageCreateFlags, VkPipelineLayoutCreateFlags,
    VkDescriptorSetLayoutCreateFlags, VkPipelineCreateFlags, VkExternalMemoryHandleTypeFlags,
    VkExternalSemaphoreHandleTypeFlags, VkSemaphoreImportFlags, VkExternalFenceHandleTypeFlags, VkFenceImportFlags;

typedef enum VkResult { VK_SUCCESS = 0, VK_NOT_READY = 1, VK_TIMEOUT = 2, VK_INCOMPLETE = 5,
    VK_ERROR_OUT_OF_HOST_MEMORY = -1, VK_ERROR_OUT_OF_DEVICE_MEMORY = -2, VK_ERROR_INITIALIZATION_FAILED = -3,
    VK_ERROR_DEVICE_LOST = -4, VK_ERROR_EXTENSION_NOT_PRESENT = -7, VK_ERROR_FEATURE_NOT_PRESENT = -8 } VkResult;

typedef enum VkStructureType {
    VK_STRUCTURE_TYPE_APPLICATION_INFO = 0, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3,
    VK_STRUCTURE_TYPE_SUBMIT_INFO = 4, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5,
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9,
    VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO = 11, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12,
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15,
    VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18,
    VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO = 29, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30,
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33,
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35,
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42, VK_STRUCTURE_TYPE_MEMORY_BARRIER = 46,
    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER = 44, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO = 1000071002,
    VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES = 1000071003,
    VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO = 1000072000,
    VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO = 1000072001,
    VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO = 1000072002,
    VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR = 1000074000,
    VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR = 1000074002,
    VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO = 1000077000,
    VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR = 1000079000,
    VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR = 1000079001,
    VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO = 1000113000,
    VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR = 1000115001,
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO = 1000127001,
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS = 1000127000,
    VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 = 1000146000,
    VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 = 1000146003,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 = 1000059001,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 = 1000059000,
    VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID = 1000010000,
    VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID = 1000010001,
    VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID = 1000010002,
    VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID = 1000010003
} VkStructureType;

typedef struct VkExtent3D { uint32_t width, height, depth; } VkExtent3D;
typedef struct VkOffset3D { int32_t x, y, z; } VkOffset3D;
typedef struct VkExtensionProperties { char extensionName[256]; uint32_t specVersion; } VkExtensionProperties;
typedef struct VkApplicationInfo { VkStructureType sType; const void *pNext; const char *pApplicationName;
    uint32_t applicationVersion; const char *pEngineName; uint32_t engineVersion; uint32_t apiVersion; } VkApplicationInfo;
typedef struct VkInstanceCreateInfo { VkStructureType sType; const void *pNext; VkInstanceCreateFlags flags;
    const VkApplicationInfo *pApplicationInfo; uint32_t enabledLayerCount; const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames; } VkInstanceCreateInfo;
typedef struct VkPhysicalDeviceLimits {
    uint32_t maxImageDimension1D, maxImageDimension2D, maxImageDimension3D, maxImageDimensionCube, maxImageArrayLayers,
        maxTexelBufferElements, maxUniformBufferRange, maxStorageBufferRange, maxPushConstantsSize,
        maxMemoryAllocationCount, maxSamplerAllocationCount;
    VkDeviceSize bufferImageGranularity, sparseAddressSpaceSize;
    uint32_t maxBoundDescriptorSets, maxPerStageDescriptorSamplers, maxPerStageDescriptorUniformBuffers,
        maxPerStageDescriptorStorageBuffers, maxPerStageDescriptorSampledImages, maxPerStageDescriptorStorageImages,
        maxPerStageDescriptorInputAttachments, maxPerStageResources, maxDescriptorSetSamplers,
        maxDescriptorSetUniformBuffers, maxDescriptorSetUniformBuffersDynamic, maxDescriptorSetStorageBuffers,
        maxDescriptorSetStorageBuffersDynamic, maxDescriptorSetSampledImages, maxDescriptorSetStorageImages,
        maxDescriptorSetInputAttachments, maxVertexInputAttributes, maxVertexInputBindings,
        maxVertexInputAttributeOffset, maxVertexInputBindingStride, maxVertexOutputComponents,
        maxTessellationGenerationLevel, maxTessellationPatchSize, maxTessellationControlPerVertexInputComponents,
        maxTessellationControlPerVertexOutputComponents, maxTessellationControlPerPatchOutputComponents,
        maxTessellationControlTotalOutputComponents, maxTessellationEvaluationInputComponents,
        maxTessellationEvaluationOutputComponents, maxGeometryShaderInvocations, maxGeometryInputComponents,
        maxGeometryOutputComponents, maxGeometryOutputVertices, maxGeometryTotalOutputComponents,
        maxFragmentInputComponents, maxFragmentOutputAttachments, maxFragmentDualSrcAttachments,
        maxFragmentCombinedOutputResources, maxComputeSharedMemorySize;
    uint32_t maxComputeWorkGroupCount[3]; uint32_t maxComputeWorkGroupInvocations; uint32_t maxComputeWorkGroupSize[3];
    uint32_t subPixelPrecisionBits, subTexelPrecisionBits, mipmapPrecisionBits, maxDrawIndexedIndexValue,
        maxDrawIndirectCount; float maxSamplerLodBias, maxSamplerAnisotropy; uint32_t maxViewports;
    uint32_t maxViewportDimensions[2]; float viewportBoundsRange[2]; uint32_t viewportSubPixelBits;
    size_t minMemoryMapAlignment; VkDeviceSize minTexelBufferOffsetAlignment, minUniformBufferOffsetAlignment,
        minStorageBufferOffsetAlignment; int32_t minTexelOffset; uint32_t maxTexelOffset; int32_t minTexelGatherOffset;
    uint32_t maxTexelGatherOffset; float minInterpolationOffset, maxInterpolationOffset;
    uint32_t subPixelInterpolationOffsetBits, maxFramebufferWidth, maxFramebufferHeight, maxFramebufferLayers;
    VkSampleCountFlags framebufferColorSampleCounts, framebufferDepthSampleCounts, framebufferStencilSampleCounts,
        framebufferNoAttachmentsSampleCounts; uint32_t maxColorAttachments;
    VkSampleCountFlags sampledImageColorSampleCounts, sampledImageIntegerSampleCounts, sampledImageDepthSampleCounts,
        sampledImageStencilSampleCounts, storageImageSampleCounts; uint32_t maxSampleMaskWords;
    VkBool32 timestampComputeAndGraphics; float timestampPeriod; uint32_t maxClipDistances, maxCullDistances,
        maxCombinedClipAndCullDistances, discreteQueuePriorities; float pointSizeRange[2], lineWidthRange[2];
    float pointSizeGranularity, lineWidthGranularity; VkBool32 strictLines, standardSampleLocations;
    VkDeviceSize optimalBufferCopyOffsetAlignment, optimalBufferCopyRowPitchAlignment, nonCoherentAtomSize;
} VkPhysicalDeviceLimits;
typedef struct VkPhysicalDeviceSparseProperties { VkBool32 a, b, c, d, e; } VkPhysicalDeviceSparseProperties;
typedef struct VkPhysicalDeviceProperties { uint32_t apiVersion, driverVersion, vendorID, deviceID; int deviceType;
    char deviceName[256]; uint8_t pipelineCacheUUID[16]; VkPhysicalDeviceLimits limits;
    VkPhysicalDeviceSparseProperties sparseProperties; } VkPhysicalDeviceProperties;
typedef struct VkQueueFamilyProperties { VkQueueFlags queueFlags; uint32_t queueCount; uint32_t timestampValidBits;
    VkExtent3D minImageTransferGranularity; } VkQueueFamilyProperties;
typedef struct VkMemoryType { VkMemoryPropertyFlags propertyFlags; uint32_t heapIndex; } VkMemoryType;
typedef struct VkMemoryHeap { VkDeviceSize size; VkMemoryHeapFlags flags; } VkMemoryHeap;
typedef struct VkPhysicalDeviceMemoryProperties { uint32_t memoryTypeCount; VkMemoryType memoryTypes[32];
    uint32_t memoryHeapCount; VkMemoryHeap memoryHeaps[16]; } VkPhysicalDeviceMemoryProperties;
typedef struct VkDeviceQueueCreateInfo { VkStructureType sType; const void *pNext; VkDeviceQueueCreateFlags flags;
    uint32_t queueFamilyIndex; uint32_t queueCount; const float *pQueuePriorities; } VkDeviceQueueCreateInfo;
typedef struct VkDeviceCreateInfo { VkStructureType sType; const void *pNext; VkDeviceCreateFlags flags;
    uint32_t queueCreateInfoCount; const VkDeviceQueueCreateInfo *pQueueCreateInfos; uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames; uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames;
    const void *pEnabledFeatures; } VkDeviceCreateInfo;
typedef struct VkMemoryAllocateInfo { VkStructureType sType; const void *pNext; VkDeviceSize allocationSize;
    uint32_t memoryTypeIndex; } VkMemoryAllocateInfo;
typedef struct VkMemoryRequirements { VkDeviceSize size, alignment; uint32_t memoryTypeBits; } VkMemoryRequirements;
typedef struct VkBufferCreateInfo { VkStructureType sType; const void *pNext; VkBufferCreateFlags flags;
    VkDeviceSize size; VkBufferUsageFlags usage; int sharingMode; uint32_t queueFamilyIndexCount;
    const uint32_t *pQueueFamilyIndices; } VkBufferCreateInfo;
typedef struct VkExternalMemoryBufferCreateInfo { VkStructureType sType; const void *pNext;
    VkExternalMemoryHandleTypeFlags handleTypes; } VkExternalMemoryBufferCreateInfo;
typedef struct VkExportMemoryAllocateInfo { VkStructureType sType; const void *pNext;
    VkExternalMemoryHandleTypeFlags handleTypes; } VkExportMemoryAllocateInfo;
typedef struct VkMemoryGetFdInfoKHR { VkStructureType sType; const void *pNext; VkDeviceMemory memory;
    uint32_t handleType; } VkMemoryGetFdInfoKHR;
typedef struct VkCommandPoolCreateInfo { VkStructureType sType; const void *pNext; VkCommandPoolCreateFlags flags;
    uint32_t queueFamilyIndex; } VkCommandPoolCreateInfo;
typedef struct VkCommandBufferAllocateInfo { VkStructureType sType; const void *pNext; VkCommandPool commandPool;
    int level; uint32_t commandBufferCount; } VkCommandBufferAllocateInfo;
typedef struct VkCommandBufferBeginInfo { VkStructureType sType; const void *pNext; VkCommandBufferUsageFlags flags;
    const void *pInheritanceInfo; } VkCommandBufferBeginInfo;
typedef struct VkSubmitInfo { VkStructureType sType; const void *pNext; uint32_t waitSemaphoreCount;
    const VkSemaphore *pWaitSemaphores; const VkPipelineStageFlags *pWaitDstStageMask; uint32_t commandBufferCount;
    const VkCommandBuffer *pCommandBuffers; uint32_t signalSemaphoreCount; const VkSemaphore *pSignalSemaphores; } VkSubmitInfo;
typedef struct VkFenceCreateInfo { VkStructureType sType; const void *pNext; VkFenceCreateFlags flags; } VkFenceCreateInfo;
typedef struct VkSemaphoreCreateInfo { VkStructureType sType; const void *pNext; VkSemaphoreCreateFlags flags; } VkSemaphoreCreateInfo;
typedef struct VkExportSemaphoreCreateInfo { VkStructureType sType; const void *pNext;
    VkExternalSemaphoreHandleTypeFlags handleTypes; } VkExportSemaphoreCreateInfo;
typedef struct VkImportSemaphoreFdInfoKHR { VkStructureType sType; const void *pNext; VkSemaphore semaphore;
    VkSemaphoreImportFlags flags; uint32_t handleType; int fd; } VkImportSemaphoreFdInfoKHR;
typedef struct VkSemaphoreGetFdInfoKHR { VkStructureType sType; const void *pNext; VkSemaphore semaphore;
    uint32_t handleType; } VkSemaphoreGetFdInfoKHR;
typedef struct VkQueryPoolCreateInfo { VkStructureType sType; const void *pNext; VkQueryPoolCreateFlags flags;
    int queryType; uint32_t queryCount; VkQueryPipelineStatisticFlags pipelineStatistics; } VkQueryPoolCreateInfo;
typedef struct VkMemoryBarrier { VkStructureType sType; const void *pNext; VkAccessFlags srcAccessMask, dstAccessMask; } VkMemoryBarrier;
typedef struct VkBufferMemoryBarrier { VkStructureType sType; const void *pNext; VkAccessFlags srcAccessMask, dstAccessMask;
    uint32_t srcQueueFamilyIndex, dstQueueFamilyIndex; VkBuffer buffer; VkDeviceSize offset, size; } VkBufferMemoryBarrier;
typedef struct VkBufferCopy { VkDeviceSize srcOffset, dstOffset, size; } VkBufferCopy;
typedef struct VkShaderModuleCreateInfo { VkStructureType sType; const void *pNext; VkShaderModuleCreateFlags flags;
    size_t codeSize; const uint32_t *pCode; } VkShaderModuleCreateInfo;
typedef struct VkDescriptorSetLayoutBinding { uint32_t binding; int descriptorType; uint32_t descriptorCount;
    VkShaderStageFlags stageFlags; const void *pImmutableSamplers; } VkDescriptorSetLayoutBinding;
typedef struct VkDescriptorSetLayoutCreateInfo { VkStructureType sType; const void *pNext;
    VkDescriptorSetLayoutCreateFlags flags; uint32_t bindingCount; const VkDescriptorSetLayoutBinding *pBindings; } VkDescriptorSetLayoutCreateInfo;
typedef struct VkPushConstantRange { VkShaderStageFlags stageFlags; uint32_t offset, size; } VkPushConstantRange;
typedef struct VkPipelineLayoutCreateInfo { VkStructureType sType; const void *pNext; VkPipelineLayoutCreateFlags flags;
    uint32_t setLayoutCount; const VkDescriptorSetLayout *pSetLayouts; uint32_t pushConstantRangeCount;
    const VkPushConstantRange *pPushConstantRanges; } VkPipelineLayoutCreateInfo;
typedef struct VkPipelineShaderStageCreateInfo { VkStructureType sType; const void *pNext;
    VkPipelineShaderStageCreateFlags flags; int stage; VkShaderModule module; const char *pName;
    const void *pSpecializationInfo; } VkPipelineShaderStageCreateInfo;
typedef struct VkComputePipelineCreateInfo { VkStructureType sType; const void *pNext; VkPipelineCreateFlags flags;
    VkPipelineShaderStageCreateInfo stage; VkPipelineLayout layout; VkPipeline basePipelineHandle;
    int32_t basePipelineIndex; } VkComputePipelineCreateInfo;
typedef struct VkDescriptorPoolSize { int type; uint32_t descriptorCount; } VkDescriptorPoolSize;
typedef struct VkDescriptorPoolCreateInfo { VkStructureType sType; const void *pNext; VkDescriptorPoolCreateFlags flags;
    uint32_t maxSets; uint32_t poolSizeCount; const VkDescriptorPoolSize *pPoolSizes; } VkDescriptorPoolCreateInfo;
typedef struct VkDescriptorSetAllocateInfo { VkStructureType sType; const void *pNext; VkDescriptorPool descriptorPool;
    uint32_t descriptorSetCount; const VkDescriptorSetLayout *pSetLayouts; } VkDescriptorSetAllocateInfo;
typedef struct VkDescriptorBufferInfo { VkBuffer buffer; VkDeviceSize offset, range; } VkDescriptorBufferInfo;
typedef struct VkWriteDescriptorSet { VkStructureType sType; const void *pNext; VkDescriptorSet dstSet;
    uint32_t dstBinding, dstArrayElement, descriptorCount; int descriptorType; const void *pImageInfo;
    const VkDescriptorBufferInfo *pBufferInfo; const void *pTexelBufferView; } VkWriteDescriptorSet;

/* constantes */
#define VK_MAKE_API_VERSION(v,M,m,p) (((uint32_t)(v)<<29)|((uint32_t)(M)<<22)|((uint32_t)(m)<<12)|(uint32_t)(p))
#define VK_API_VERSION_1_1 VK_MAKE_API_VERSION(0,1,1,0)
#define VK_API_VERSION_1_2 VK_MAKE_API_VERSION(0,1,2,0)
#define VK_NULL_HANDLE 0
#define VK_QUEUE_GRAPHICS_BIT 1u
#define VK_QUEUE_COMPUTE_BIT 2u
#define VK_QUEUE_TRANSFER_BIT 4u
#define VK_BUFFER_USAGE_TRANSFER_SRC_BIT 1u
#define VK_BUFFER_USAGE_TRANSFER_DST_BIT 2u
#define VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 0x10u
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x20u
#define VK_BUFFER_USAGE_INDEX_BUFFER_BIT 0x40u
#define VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 0x80u
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 1u
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 2u
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 4u
#define VK_MEMORY_PROPERTY_HOST_CACHED_BIT 8u
#define VK_SHARING_MODE_EXCLUSIVE 0
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 1u
#define VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 2u
#define VK_QUERY_TYPE_TIMESTAMP 2
#define VK_QUERY_RESULT_64_BIT 1u
#define VK_QUERY_RESULT_WAIT_BIT 2u
#define VK_ACCESS_SHADER_READ_BIT 0x20u
#define VK_ACCESS_SHADER_WRITE_BIT 0x40u
#define VK_ACCESS_TRANSFER_READ_BIT 0x800u
#define VK_ACCESS_TRANSFER_WRITE_BIT 0x1000u
#define VK_ACCESS_HOST_READ_BIT 0x2000u
#define VK_ACCESS_HOST_WRITE_BIT 0x4000u
#define VK_ACCESS_MEMORY_READ_BIT 0x8000u
#define VK_ACCESS_MEMORY_WRITE_BIT 0x10000u
#define VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT 1u
#define VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT 0x800u
#define VK_PIPELINE_STAGE_TRANSFER_BIT 0x1000u
#define VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT 0x2000u
#define VK_PIPELINE_STAGE_HOST_BIT 0x4000u
#define VK_PIPELINE_BIND_POINT_COMPUTE 1
#define VK_SHADER_STAGE_COMPUTE_BIT 0x20u
#define VK_DESCRIPTOR_TYPE_STORAGE_BUFFER 7
#define VK_FENCE_CREATE_SIGNALED_BIT 1u
#define VK_QUEUE_FAMILY_IGNORED (~0u)
#define VK_WHOLE_SIZE (~0ULL)
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT 1u
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT 0x200u
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID 0x400u
#define VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT 1u
#define VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT 8u
#define VK_SEMAPHORE_IMPORT_TEMPORARY_BIT 1u
#define VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT 8u

/* punteros a funciones (solo las usadas) */
typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance, const char *);
typedef PFN_vkVoidFunction (*PFN_vkGetDeviceProcAddr)(VkDevice, const char *);
#endif
