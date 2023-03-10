#include <rag_vk.h>

#include <stdio.h>
#include <stdint.h>
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
    ASSERT_VK(vkEnumerateInstanceLayerProperties(&layer_count, NULL));
    ASSERT(layer_count > 0);

    VkLayerProperties *layers = ALLOC_N(layer_count, VkLayerProperties);
    ASSERT(layers != NULL);
    ASSERT_VK(vkEnumerateInstanceLayerProperties(&layer_count, layers));

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
    ASSERT(!error);
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
    ASSERT_VK(vkCreateInstance(&instance_create_info, NULL, instance));
}

static void InitPDev(VkPhysicalDevice *pdev, VkInstance instance) {
    uint32_t pdev_count;
    ASSERT_VK(vkEnumeratePhysicalDevices(instance, &pdev_count, NULL));
    ASSERT(pdev_count > 0);

    VkPhysicalDevice *pds = ALLOC_N(pdev_count, VkPhysicalDevice);
    ASSERT(pds != NULL);
    ASSERT_VK(vkEnumeratePhysicalDevices(instance, &pdev_count, pds));

    // TODO: choose the most suitable device if pdev_count > 1
    *pdev = pds[0];
    free(pds);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(*pdev, &props);
    printf("Using VkPhysicalDevice #%u of type %u -- %s\n", props.deviceID, props.deviceType, props.deviceName);
}

/* Returns selected queue family index. */
static uint32_t InitDev(VkDevice *dev, VkPhysicalDevice pdev, bool need_gfx_queue) {
    uint32_t queue_count;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queue_count, NULL);
    ASSERT(queue_count > 0);

    VkQueueFamilyProperties *family_props = ALLOC_N(queue_count, VkQueueFamilyProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queue_count, family_props);

    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; i++) {
        bool g = family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool c = family_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
        bool t = family_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT;
        if ((!need_gfx_queue || g) && c && t) {
            printf("Using queue family #%u (count = %u, GCT = %d%d%d)\n", i, family_props[i].queueCount, g, c, t);
            index = i;
            break;
        }
    }
    ASSERT(index != UINT32_MAX);
    free(family_props);

    VkDeviceQueueCreateInfo queue_create_info = {0};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = index;
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
    ASSERT_VK(vkCreateDevice(pdev, &device_create_info, NULL, dev));
    return index;
}

void VulkanCtx_Init(VulkanCtx *ctx, bool need_gfx_queue) {
    InitInstance(&ctx->instance);
    InitPDev(&ctx->pdev, ctx->instance);

    uint32_t queue_family_idx = InitDev(&ctx->dev, ctx->pdev, need_gfx_queue);
    vkGetDeviceQueue(ctx->dev, queue_family_idx, 0, &ctx->queue);

    VkCommandPoolCreateInfo pool_create_info = {0};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create_info.queueFamilyIndex = queue_family_idx;
    ASSERT_VK(vkCreateCommandPool(ctx->dev, &pool_create_info, NULL, &ctx->cmd_pool));
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
    ASSERT_VK(vkCreateShaderModule(ctx->dev, &create_info, NULL, &module));

    free(buf);
    return module;
}

void test() {

}
