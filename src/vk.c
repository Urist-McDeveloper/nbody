#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>
#include "util.h"

/* Assert that Vulkan library function returned VK_SUCCESS. */
#define ASSERT_VK(X) ASSERT((X) == VK_SUCCESS)

#ifndef NDEBUG
/* Debug layers. */
static const char *const DBG_LAYERS[] = { "VK_LAYER_KHRONOS_validation" };
/* Debug layers count. */
static const int DBG_LAYERS_COUNT = sizeof(DBG_LAYERS) / sizeof(DBG_LAYERS[0]);
#endif  // NDEBUG

/* Initialize VkInstance. */
static void InitInstance(VkInstance *instance) {
#ifndef NDEBUG
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
#endif
    VkApplicationInfo app_info = { 0 };
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "rag";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_create_info = { 0 };
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
#ifndef NDEBUG
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

static void InitDevAndQueue(VkDevice *dev, VkQueue *queue, VkPhysicalDevice pdev) {
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
        if (c && t) {
            printf("Using queue family #%u (count = %u, GCT = %d%d%d)\n", i, family_props[i].queueCount, g, c, t);
            index = i;
            break;
        }
    }
    ASSERT(index != UINT32_MAX);
    free(family_props);

    VkDeviceQueueCreateInfo queue_create_info = { 0 };
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
    device_create_info.enabledLayerCount = 1;
    device_create_info.ppEnabledLayerNames = DBG_LAYERS;
#endif

    ASSERT_VK(vkCreateDevice(pdev, &device_create_info, NULL, dev));
    vkGetDeviceQueue(*dev, index, 0, queue);
}

/* Holder struct for everything Vulkan-related. */
typedef struct VulkanCtx {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkQueue queue;
    VkDevice dev;
} VulkanCtx;

/* Initialize VulkanCtx. */
void VulkanCtx_Init(VulkanCtx *ctx) {
    InitInstance(&ctx->instance);
    InitPDev(&ctx->pdev, ctx->instance);
    InitDevAndQueue(&ctx->dev, &ctx->queue, ctx->pdev);
}

/* De-initialize VulkanCtx. */
void VulkanCtx_DeInit(VulkanCtx *ctx) {
    vkDestroyDevice(ctx->dev, NULL);
    vkDestroyInstance(ctx->instance, NULL);
}

/*
 * MAIN
 */

int main(void) {
    VulkanCtx ctx;
    VulkanCtx_Init(&ctx);

    VulkanCtx_DeInit(&ctx);
    return EXIT_SUCCESS;
}
