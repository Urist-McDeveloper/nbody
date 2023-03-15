#include "vulkan_ctx.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "fio.h"
#include "util.h"

#ifndef NDEBUG
static const char *const DBG_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static const int DBG_LAYERS_COUNT = sizeof(DBG_LAYERS) / sizeof(DBG_LAYERS[0]);

static void AssertDebugLayersSupported() {
    static bool done = false;
    if (done) return;

    uint32_t layer_count;
    ASSERT_VK(vkEnumerateInstanceLayerProperties(&layer_count, NULL), "Failed to enumerate instance layers");

    VkLayerProperties *layers = ALLOC(layer_count, VkLayerProperties);
    ASSERT(layers != NULL, "Failed to alloc %u VkLayerProperties", layer_count);
    ASSERT_VK(vkEnumerateInstanceLayerProperties(&layer_count, layers), "Failed to enumerate instance layers");

    bool error = false;
    for (int i = 0; i < DBG_LAYERS_COUNT; i++) {
        bool found = false;
        for (uint32_t j = 0; j < layer_count && !found; j++) {
            if (strcmp(DBG_LAYERS[i], layers[j].layerName) == 0) {
                found = true;
            }
        }
        if (!found) {
            fprintf(stderr, "Debug layer not supported: %s\n", DBG_LAYERS[i]);
            error = true;
        }
    }
    if (error) abort();

    free(layers);
    done = true;
}

#endif //NDEBUG

static void InitInstance(VkInstance *instance) {
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "nbody-sim";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_create_info = {0};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
#ifndef NDEBUG
    AssertDebugLayersSupported();
    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = DBG_LAYERS;
#endif
    ASSERT_VK(vkCreateInstance(&instance_create_info, NULL, instance), "Failed to create instance");
}

static void InitPDev(VkPhysicalDevice *pdev, VkInstance instance) {
    uint32_t pdev_count;
    ASSERT_VK(vkEnumeratePhysicalDevices(instance, &pdev_count, NULL), "Failed to enumerate physical devices");
    ASSERT(pdev_count > 0, "Physical device count is 0");

    VkPhysicalDevice *pds = ALLOC(pdev_count, VkPhysicalDevice);
    ASSERT(pds != NULL, "Failed to alloc %u VkPhysicalDevices", pdev_count);
    ASSERT_VK(vkEnumeratePhysicalDevices(instance, &pdev_count, pds), "Failed to enumerate physical devices");

    // TODO: choose the most suitable device if pdev_count > 1
    *pdev = pds[0];
    free(pds);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(*pdev, &props);
    printf("Using VkPhysicalDevice #%u of type %u -- %s\n", props.deviceID, props.deviceType, props.deviceName);
}

/* Returns selected queue family index. */
static uint32_t InitDev(VkDevice *dev, VkPhysicalDevice pdev) {
    uint32_t family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, NULL);
    ASSERT(family_count > 0, "Queue family count is 0");

    VkQueueFamilyProperties *family_props = ALLOC(family_count, VkQueueFamilyProperties);
    ASSERT(family_props != NULL, "Failed to alloc %u VkQueueFamilyProperties", family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, family_props);

    uint32_t qf_idx = UINT32_MAX;
    printf("Selecting queue family:");

    for (uint32_t i = 0; i < family_count; i++) {
        bool g = family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool c = family_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
        bool t = family_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT;

        printf("\t#%u: count = %u, flags =", i, family_props[i].queueCount);
        if (g) printf(" graphics");
        if (c) printf(" compute");
        if (t) printf(" transfer");
        printf("\n");

        // prefer compute only
        if (c && t && (!g || qf_idx == UINT32_MAX)) {
            qf_idx = i;
        }
    }
    ASSERT(qf_idx != UINT32_MAX, "Could not find suitable queue family");

    printf("Using queue family #%u\n", qf_idx);
    free(family_props);

    VkDeviceQueueCreateInfo queue_create_info = {0};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = qf_idx;
    queue_create_info.queueCount = 1;

    const float priority = 1.f;
    queue_create_info.pQueuePriorities = &priority;

    VkDeviceCreateInfo device_create_info = {0};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
#ifndef NDEBUG
    AssertDebugLayersSupported();
    device_create_info.enabledLayerCount = 1;
    device_create_info.ppEnabledLayerNames = DBG_LAYERS;
#endif
    ASSERT_VK(vkCreateDevice(pdev, &device_create_info, NULL, dev), "Failed to create device");
    return qf_idx;
}

VulkanCtx *CreateVulkanCtx() {
    VulkanCtx *ctx = ALLOC(1, VulkanCtx);
    ASSERT(ctx != NULL, "Failed to alloc VulkanCtx");

    InitInstance(&ctx->instance);
    InitPDev(&ctx->pdev, ctx->instance);

    ctx->queue_family_idx = InitDev(&ctx->dev, ctx->pdev);
    vkGetDeviceQueue(ctx->dev, ctx->queue_family_idx, 0, &ctx->queue);

    VkCommandPoolCreateInfo pool_create_info = {0};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create_info.queueFamilyIndex = ctx->queue_family_idx;
    ASSERT_VK(vkCreateCommandPool(ctx->dev, &pool_create_info, NULL, &ctx->cmd_pool), "Failed to create command pool");

    return ctx;
}

void DestroyVulkanCtx(VulkanCtx *ctx) {
    if (ctx != NULL) {
        vkDestroyCommandPool(ctx->dev, ctx->cmd_pool, NULL);
        vkDestroyDevice(ctx->dev, NULL);
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
    }
}

VkShaderModule LoadVkShaderModule(const VulkanCtx *ctx, const char *path) {
    size_t buf_size;
    uint32_t *buf = FIO_ReadFile(path, &buf_size);

    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buf_size;
    create_info.pCode = buf;

    VkShaderModule module;
    ASSERT_VK(vkCreateShaderModule(ctx->dev, &create_info, NULL, &module),
              "Failed to create shader module from %s", path);

    free(buf);
    return module;
}

void AllocVkCommandBuffers(const VulkanCtx *ctx, uint32_t count, VkCommandBuffer *buffers) {
    VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = ctx->cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = count,
    };
    ASSERT_VK(vkAllocateCommandBuffers(ctx->dev, &allocate_info, buffers),
              "Failed to allocate %u command buffers", count);
}


/*
 * Memory management.
 */


static VulkanDeviceMemory CreateDeviceMemory(const VulkanCtx *ctx, VkDeviceSize size, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(ctx->pdev, &props);

    uint32_t mem_type_idx = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if (flags & props.memoryTypes[i].propertyFlags) {
            mem_type_idx = i;
            break;
        }
    }
    ASSERT(mem_type_idx != UINT32_MAX, "Failed to find suitable memory type for flags %#x", flags);

    VkMemoryAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = size,
            .memoryTypeIndex = mem_type_idx,
    };
    VkDeviceMemory memory;
    ASSERT_VK(vkAllocateMemory(ctx->dev, &allocate_info, NULL, &memory),
              "Failed to allocate %zu bytes of device memory #%u", size, mem_type_idx);

    void *mapped;
    if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        ASSERT_VK(vkMapMemory(ctx->dev, memory, 0, VK_WHOLE_SIZE, 0, &mapped), "Failed to map device memory");
    } else {
        mapped = NULL;
    }

    return (VulkanDeviceMemory){
            .ctx = ctx,
            .handle = memory,
            .flags = props.memoryTypes[mem_type_idx].propertyFlags,
            .size = size,
            .used = 0,
            .mapped = mapped,
    };
}

VulkanDeviceMemory CreateDeviceLocalMemory(const VulkanCtx *ctx, VkDeviceSize size) {
    return CreateDeviceMemory(ctx, size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

VulkanDeviceMemory CreateHostCoherentMemory(const VulkanCtx *ctx, VkDeviceSize size) {
    return CreateDeviceMemory(ctx, size, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

VulkanBuffer CreateVulkanBuffer(VulkanDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags usage) {
    ASSERT(memory->used + size <= memory->size,
           "Buffer creation requested %zu bytes but only %zu are available (size = %zu, used = %zu)",
           size, memory->size - memory->used, memory->size, memory->used);

    VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &memory->ctx->queue_family_idx,
    };
    VkBuffer buffer;
    ASSERT_VK(vkCreateBuffer(memory->ctx->dev, &create_info, NULL, &buffer), "Failed to create buffer");

    VkDeviceSize offset = memory->used;
    memory->used += size;

    void *mapped;
    if (memory->mapped != NULL) {
        mapped = ((char *)memory->mapped) + offset;
    } else {
        mapped = NULL;
    }

    ASSERT_VK(vkBindBufferMemory(memory->ctx->dev, buffer, memory->handle, offset), "Failed to bind VkBuffer");
    return (VulkanBuffer){
            .ctx = memory->ctx,
            .handle = buffer,
            .size = size,
            .mapped = mapped,
    };
}

void CopyVulkanBuffer(VkCommandBuffer cmd, const VulkanBuffer *src, const VulkanBuffer *dst) {
    ASSERT_DBG(src->size == dst->size, "src size (%zu) != dst size (%zu)", src->size, dst->size);
    VkBufferCopy buffer_copy = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = src->size,
    };
    vkCmdCopyBuffer(cmd, src->handle, dst->handle, 1, &buffer_copy);
}

void FillDescriptorBufferInfo(const VulkanBuffer *buffer, VkDescriptorBufferInfo *info) {
    *info = (VkDescriptorBufferInfo){
            .buffer = buffer->handle,
            .offset = 0,
            .range = buffer->size,
    };
}

void FillWriteReadBufferBarrier(const VulkanBuffer *buffer, VkBufferMemoryBarrier *barrier) {
    *barrier = (VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .srcQueueFamilyIndex = buffer->ctx->queue_family_idx,
            .dstQueueFamilyIndex = buffer->ctx->queue_family_idx,
            .buffer = buffer->handle,
            .offset = 0,
            .size = buffer->size,
    };
}
