#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>
#include <rag.h>

#include "lib/fio.h"
#include "lib/util.h"

/* Assert that Vulkan library function returned VK_SUCCESS. */
#define ASSERT_VK(X) ASSERT((X) == VK_SUCCESS)

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

#endif

/* Initialize VkInstance. */
static void InitInstance(VkInstance *instance) {
#ifndef NDEBUG
    AssertDebugLayersSupported();
#endif
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "rag";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_create_info = {0};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
#ifndef NDEBUG
    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = DBG_LAYERS;
#endif
    ASSERT_VK(vkCreateInstance(&instance_create_info, NULL, instance));
}

/* Initialize VkPhysicalDevice. */
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

/* Initialize VkDevice and VkQueue. */
static void InitDevAndQueue(VkDevice *dev, VkQueue *queue, VkPhysicalDevice pdev) {
#ifndef NDEBUG
    AssertDebugLayersSupported();
#endif
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
    device_create_info.enabledLayerCount = 1;
    device_create_info.ppEnabledLayerNames = DBG_LAYERS;
#endif
    ASSERT_VK(vkCreateDevice(pdev, &device_create_info, NULL, dev));
    vkGetDeviceQueue(*dev, index, 0, queue);
}

/* Vulkan setup-related stuff. */
typedef struct VulkanCtx {
    VkInstance instance;            // Vulkan instance
    VkPhysicalDevice pdev;          // physical device
    VkDevice dev;                   // logical device
    VkQueue queue;                  // command queue
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

/* Load shader module from PATH. */
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

/* Simulation pipeline-related stuff. */
typedef struct BodyCompPipeline {
    const VulkanCtx *ctx;               // Vulkan context
    VkShaderModule grav_module;         // gravity compute shader
    VkShaderModule move_module;         // move compute shader
    VkDescriptorSetLayout ds_layout;    // ??
    VkPipelineLayout pipeline_layout;   // ??
    VkPipeline grav_pipeline;           // gravity pipeline
    VkPipeline move_pipeline;           // move pipeline
} BodyCompPipeline;

/*
 * Initialize BodyCompPipeline.
 * CTX must be a valid pointer to initialized VulkanCtx until BodyCompPipeline_DeInit() is called.
 */
void BodyCompPipeline_Init(BodyCompPipeline *bcp, const VulkanCtx *ctx) {
    bcp->ctx = ctx;
    bcp->grav_module = VulkanCtx_LoadShader(ctx, "shader/body_grav_cs.spv");
    bcp->move_module = VulkanCtx_LoadShader(ctx, "shader/body_move_cs.spv");

    VkDescriptorSetLayoutBinding *bindings = ALLOC_N(2, VkDescriptorSetLayoutBinding);
    ASSERT(bindings != NULL);

    bindings[0] = (VkDescriptorSetLayoutBinding){0};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1] = (VkDescriptorSetLayoutBinding){0};
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsl_info = {0};
    dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_info.bindingCount = 2;
    dsl_info.pBindings = bindings;

    ASSERT_VK(vkCreateDescriptorSetLayout(ctx->dev, &dsl_info, NULL, &bcp->ds_layout));
    free(bindings);

    VkPipelineLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &bcp->ds_layout;
    ASSERT_VK(vkCreatePipelineLayout(ctx->dev, &layout_info, NULL, &bcp->pipeline_layout));

    VkPipelineShaderStageCreateInfo grav_stage_info = {0};
    grav_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    grav_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    grav_stage_info.module = bcp->grav_module;
    grav_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo move_stage_info = {0};
    move_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    move_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    move_stage_info.module = bcp->move_module;
    move_stage_info.pName = "main";

    VkComputePipelineCreateInfo *pipeline_info = ALLOC_N(2, VkComputePipelineCreateInfo);
    ASSERT(pipeline_info != NULL);

    pipeline_info[0] = (VkComputePipelineCreateInfo){0};
    pipeline_info[0].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[0].stage = grav_stage_info;
    pipeline_info[0].layout = bcp->pipeline_layout;

    pipeline_info[1] = (VkComputePipelineCreateInfo){0};
    pipeline_info[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[1].stage = move_stage_info;
    pipeline_info[1].layout = bcp->pipeline_layout;

    ASSERT_VK(vkCreateComputePipelines(ctx->dev, NULL, 2, pipeline_info, NULL, &bcp->grav_pipeline));
    free(pipeline_info);
}

/* De-initialize BodyCompPipeline. */
void BodyCompPipeline_DeInit(BodyCompPipeline *bcp) {
    vkDestroyPipeline(bcp->ctx->dev, bcp->move_pipeline, NULL);
    vkDestroyPipeline(bcp->ctx->dev, bcp->grav_pipeline, NULL);
    vkDestroyPipelineLayout(bcp->ctx->dev, bcp->pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(bcp->ctx->dev, bcp->ds_layout, NULL);

    vkDestroyShaderModule(bcp->ctx->dev, bcp->move_module, NULL);
    vkDestroyShaderModule(bcp->ctx->dev, bcp->grav_module, NULL);
}

int main(void) {
    VulkanCtx ctx;
    VulkanCtx_Init(&ctx);

    BodyCompPipeline bcp;
    BodyCompPipeline_Init(&bcp, &ctx);

    Body *bodies;
    int size;
    World *w = World_Create(100, 100, 100);
    World_GetBodies(w, &bodies, &size);

    // TODO: stuff
    printf("Press ENTER to continue\n");
    fgetc(stdin);

    World_Destroy(w);
    BodyCompPipeline_DeInit(&bcp);
    VulkanCtx_DeInit(&ctx);
    return EXIT_SUCCESS;
}
