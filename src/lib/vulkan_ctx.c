#include "vulkan_ctx.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "fio.h"
#include "util.h"

/* The one and only Vulkan context. */
struct VulkanContext vulkan_ctx = {0};

#ifndef NDEBUG
static const char *DBG_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static const int DBG_LAYERS_COUNT = sizeof(DBG_LAYERS) / sizeof(DBG_LAYERS[0]);

static void AssertDebugLayersSupported() {
    // run this function only once
    static bool done = false;
    if (done) return;
    done = true;

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
}

#endif //NDEBUG

static void InitInstance(VkInstance *instance) {
    VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "nbody-sim",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo instance_create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
    };
#ifdef __APPLE__
    const char *port_ext = "VK_KHR_portability_enumeration";
    instance_create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
    instance_create_info.enabledExtensionCount = 1,
    instance_create_info.ppEnabledExtensionNames = &port_ext,
#endif
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

/*
 * Check COUNT elements of EXTENSIONS and sort them so supported extensions come first.
 * Returns number of supported extensions.
 */
static uint32_t SortDeviceExtensionsBySupported(const char **extensions, uint32_t count, VkPhysicalDevice pdev) {
    uint32_t total_count;
    ASSERT_VK(vkEnumerateDeviceExtensionProperties(pdev, NULL, &total_count, NULL),
              "Failed to enumerate device extension properties");

    // early return to avoid allocating 0 bytes
    if (total_count == 0) return 0;

    VkExtensionProperties *properties = ALLOC(total_count, VkExtensionProperties);
    ASSERT(properties != NULL, "Failed to alloc %u VkExtensionProperties", total_count);
    ASSERT_VK(vkEnumerateDeviceExtensionProperties(pdev, NULL, &total_count, properties),
              "Failed to enumerate device extension properties");

    uint32_t supported_count = 0;
    for (uint32_t i = 0; i < total_count; i++) {
        for (uint32_t j = 0; j < count; j++) {
            if (strncmp(properties[i].extensionName, extensions[j], VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                if (supported_count != i) {
                    VkExtensionProperties tmp = properties[i];
                    properties[i] = properties[supported_count];
                    properties[supported_count] = tmp;
                }
                supported_count++;
                break;
            }
        }
    }
    return supported_count;
}

static void InitDev(VkDevice *dev, uint32_t *queue_family_idx, VkPhysicalDevice pdev) {
    uint32_t family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, NULL);
    ASSERT(family_count > 0, "Queue family count is 0");

    VkQueueFamilyProperties *family_props = ALLOC(family_count, VkQueueFamilyProperties);
    ASSERT(family_props != NULL, "Failed to alloc %u VkQueueFamilyProperties", family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, family_props);

    uint32_t qf_idx = UINT32_MAX;
    printf("Selecting queue family:\n");

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
    free(family_props);

    printf("Using queue family #%u\n", qf_idx);
    *queue_family_idx = qf_idx;

    const float queue_priority = 1.f;
    VkDeviceQueueCreateInfo queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = qf_idx,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
    };
    VkDeviceCreateInfo device_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue_create_info,
    };

    // per Vulkan spec, VK_KHR_portability_subset must be enabled if supported
    const char *portability_extension = "VK_KHR_portability_subset";
    if (1 == SortDeviceExtensionsBySupported(&portability_extension, 1, pdev)) {
        device_create_info.enabledExtensionCount = 1;
        device_create_info.ppEnabledExtensionNames = &portability_extension;
    }
#ifndef NDEBUG
    AssertDebugLayersSupported();
    device_create_info.enabledLayerCount = 1;
    device_create_info.ppEnabledLayerNames = DBG_LAYERS;
#endif
    ASSERT_VK(vkCreateDevice(pdev, &device_create_info, NULL, dev), "Failed to create device");
}

void InitGlobalVulkanContext() {
    // run this function only once
    static bool done = false;
    if (done) return;
    done = true;

    InitInstance(&vulkan_ctx.instance);
    InitPDev(&vulkan_ctx.pdev, vulkan_ctx.instance);
    InitDev(&vulkan_ctx.dev, &vulkan_ctx.queue_family_idx, vulkan_ctx.pdev);
    vkGetDeviceQueue(vulkan_ctx.dev, vulkan_ctx.queue_family_idx, 0, &vulkan_ctx.queue);

    VkCommandPoolCreateInfo pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = vulkan_ctx.queue_family_idx,
    };
    ASSERT_VK(vkCreateCommandPool(vulkan_ctx.dev, &pool_create_info, NULL, &vulkan_ctx.cmd_pool),
              "Failed to create global command pool");
}

void AllocCommandBuffers(uint32_t count, VkCommandBuffer *buffers) {
    VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = vulkan_ctx.cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = count,
    };
    ASSERT_VK(vkAllocateCommandBuffers(vulkan_ctx.dev, &allocate_info, buffers),
              "Failed to allocate %u command buffers", count);
}

/*
 * Memory management.
 */

static VulkanDeviceMemory CreateDeviceMemory(VkDeviceSize size, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(vulkan_ctx.pdev, &props);

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
    ASSERT_VK(vkAllocateMemory(vulkan_ctx.dev, &allocate_info, NULL, &memory),
              "Failed to allocate %llu bytes of device memory #%u", (unsigned long long)size, mem_type_idx);

    void *mapped = NULL;
    if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        ASSERT_VK(vkMapMemory(vulkan_ctx.dev, memory, 0, VK_WHOLE_SIZE, 0, &mapped), "Failed to map device memory");
    }

    return (VulkanDeviceMemory){
            .handle = memory,
            .size = size,
            .used = 0,
            .mapped = mapped,
    };
}

VulkanDeviceMemory CreateDeviceLocalMemory(VkDeviceSize size) {
    return CreateDeviceMemory(size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

VulkanDeviceMemory CreateHostCoherentMemory(VkDeviceSize size) {
    return CreateDeviceMemory(size, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

VulkanBuffer CreateVulkanBuffer(VulkanDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags usage) {
    ASSERT(memory->used + size <= memory->size,
           "Requested %llu bytes but only %llu are available (size = %llu, used = %llu)",
           (unsigned long long)size,
           (unsigned long long)(memory->size - memory->used),
           (unsigned long long)memory->size,
           (unsigned long long)memory->used);

    VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &vulkan_ctx.queue_family_idx,
    };
    VkBuffer buffer;
    ASSERT_VK(vkCreateBuffer(vulkan_ctx.dev, &create_info, NULL, &buffer), "Failed to create buffer");

    VkDeviceSize offset = memory->used;
    memory->used += size;

    void *mapped = NULL;
    if (memory->mapped != NULL) {
        mapped = ((char *)memory->mapped) + offset;
    }

    ASSERT_VK(vkBindBufferMemory(vulkan_ctx.dev, buffer, memory->handle, offset), "Failed to bind VkBuffer");
    return (VulkanBuffer){
            .handle = buffer,
            .size = size,
            .mapped = mapped,
    };
}

void CopyVulkanBuffer(VkCommandBuffer cmd, const VulkanBuffer *src, const VulkanBuffer *dst) {
    ASSERT_DBG(src->size == dst->size, "src size (%llu) != dst size (%llu)",
               (unsigned long long)src->size, (unsigned long long)dst->size);
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
            .srcQueueFamilyIndex = vulkan_ctx.queue_family_idx,
            .dstQueueFamilyIndex = vulkan_ctx.queue_family_idx,
            .buffer = buffer->handle,
            .offset = 0,
            .size = buffer->size,
    };
}
