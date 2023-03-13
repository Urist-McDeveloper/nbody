#include <rag_vk.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "fio.h"
#include "util.h"

#ifndef NDEBUG
static const char *const DBG_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static const int DBG_LAYERS_COUNT = sizeof(DBG_LAYERS) / sizeof(DBG_LAYERS[0]);

static void AssertDebugLayersSupported() {
    static bool done = false;
    if (done) return;

    uint32_t layer_count;
    ASSERT_VKR(vkEnumerateInstanceLayerProperties(&layer_count, NULL), "Failed to enumerate instance layers");
    ASSERT_MSG(layer_count > 0, "Instance layer count is 0");

    VkLayerProperties *layers = ALLOC(layer_count, VkLayerProperties);
    ASSERT_FMT(layers != NULL, "Failed to alloc %u VkLayerProperties", layer_count);
    ASSERT_VKR(vkEnumerateInstanceLayerProperties(&layer_count, layers), "Failed to enumerate instance layers");

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
    app_info.pApplicationName = "rag";
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
    ASSERT_VKR(vkCreateInstance(&instance_create_info, NULL, instance), "Failed to create instance");
}

static void InitPDev(VkPhysicalDevice *pdev, VkInstance instance) {
    uint32_t pdev_count;
    ASSERT_VKR(vkEnumeratePhysicalDevices(instance, &pdev_count, NULL), "Failed to enumerate physical devices");
    ASSERT_MSG(pdev_count > 0, "Physical device count is 0");

    VkPhysicalDevice *pds = ALLOC(pdev_count, VkPhysicalDevice);
    ASSERT_FMT(pds != NULL, "Failed to alloc %u VkPhysicalDevices", pdev_count);
    ASSERT_VKR(vkEnumeratePhysicalDevices(instance, &pdev_count, pds), "Failed to enumerate physical devices");

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
    ASSERT_MSG(family_count > 0, "Queue family count is 0");

    VkQueueFamilyProperties *family_props = ALLOC(family_count, VkQueueFamilyProperties);
    ASSERT_FMT(family_props != NULL, "Failed to alloc %u VkQueueFamilyProperties", family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, family_props);

    uint32_t qf_idx = UINT32_MAX;
    for (uint32_t i = 0; i < family_count; i++) {
        bool g = family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool c = family_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
        bool t = family_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT;

        // prefer compute only queue family
        if (c && t && (!g || qf_idx == UINT32_MAX)) {
            qf_idx = i;
        }
    }
    ASSERT_MSG(qf_idx != UINT32_MAX, "Could not find suitable queue family");

    printf("Using queue family #%u (count = %u)\n", qf_idx, family_props[qf_idx].queueCount);
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
    ASSERT_VKR(vkCreateDevice(pdev, &device_create_info, NULL, dev), "Failed to create device");
    return qf_idx;
}

void VulkanCtx_Init(VulkanCtx *ctx) {
    InitInstance(&ctx->instance);
    InitPDev(&ctx->pdev, ctx->instance);

    ctx->queue_family_idx = InitDev(&ctx->dev, ctx->pdev);
    vkGetDeviceQueue(ctx->dev, ctx->queue_family_idx, 0, &ctx->queue);

    VkCommandPoolCreateInfo pool_create_info = {0};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create_info.queueFamilyIndex = ctx->queue_family_idx;
    ASSERT_VKR(vkCreateCommandPool(ctx->dev, &pool_create_info, NULL, &ctx->cmd_pool), "Failed to create command pool");
}

void VulkanCtx_DeInit(VulkanCtx *ctx) {
    vkDestroyCommandPool(ctx->dev, ctx->cmd_pool, NULL);
    vkDestroyDevice(ctx->dev, NULL);
    vkDestroyInstance(ctx->instance, NULL);
}

VkShaderModule VulkanCtx_LoadShader(const VulkanCtx *ctx, const char *path) {
    size_t buf_size;
    uint32_t *buf = FIO_ReadFile(path, &buf_size);

    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buf_size;
    create_info.pCode = buf;

    VkShaderModule module;
    ASSERT_VKR(vkCreateShaderModule(ctx->dev, &create_info, NULL, &module), "Failed to create shader module");

    free(buf);
    return module;
}

void VulkanCtx_AllocCommandBuffers(const VulkanCtx *ctx, uint32_t count, VkCommandBuffer *buffers) {
    VkCommandBufferAllocateInfo allocate_info = {0};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = ctx->cmd_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = count;
    ASSERT_VKR(vkAllocateCommandBuffers(ctx->dev, &allocate_info, buffers), "Failed to allocate command buffers");
}

VkDeviceMemory VulkanCtx_AllocMemory(const VulkanCtx *ctx, VkDeviceSize size, VkMemoryPropertyFlags flags) {
    ASSERT_MSG(flags != 0, "flags must not be 0");

    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(ctx->pdev, &props);

    uint32_t mem_type_idx = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if (flags & props.memoryTypes[i].propertyFlags) {
            mem_type_idx = i;
            break;
        }
    }
    ASSERT_FMT(mem_type_idx != UINT32_MAX, "Failed to find suitable memory type for flags %x", flags);

    VkMemoryAllocateInfo allocate_info = {0};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = size;
    allocate_info.memoryTypeIndex = mem_type_idx;

    VkDeviceMemory memory;
    ASSERT_VKR(vkAllocateMemory(ctx->dev, &allocate_info, NULL, &memory), "Failed to allocate device memory");
    return memory;
}

VkBuffer VulkanCtx_CreateBuffer(const VulkanCtx *ctx, VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 1;
    create_info.pQueueFamilyIndices = &ctx->queue_family_idx;

    VkBuffer buffer;
    ASSERT_VKR(vkCreateBuffer(ctx->dev, &create_info, NULL, &buffer), "Failed to create buffer");
    return buffer;
}
