#include <rag.h>
#include <rag_vk.h>

#include <string.h>
#include <stdlib.h>

#include "body.h"
#include "util.h"


/* Must match the value in shaders. */
#define LOCAL_SIZE_X 16

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t size;
    float dt;
} WorldData;

/* Simulation pipeline-related stuff. */
typedef struct WorldComp WorldComp;

/*
 * Initialize necessary Vulkan stuff and setup uniform buffer with WORLD_DATA.
 * Note that WORLD_DATA cannot be changed after initialization.
 *
 * CTX must remain a valid pointer to initialized VulkanCtx until WC is de-initialized.
 */
WorldComp *WorldComp_Create(const VulkanCtx *ctx, WorldData data);

/* De-initialize WC. */
void WorldComp_Destroy(WorldComp *wc);

/* Update WC. */
void WorldComp_DoUpdate(WorldComp *wc);

/* Copy bodies from GPU buffer into ARR. */
void WorldComp_GetBodies(WorldComp *wc, Body *arr);

/* Copy bodies from ARR into GPU buffer. */
void WorldComp_SetBodies(WorldComp *wc, Body *arr);


/* The simulated world with fixed boundaries and body count. */
struct World {
    Body *arr;          // array of Bodies
    int size;           // length of the array
    WorldComp *comp;    // Vulkan-related stuff
    bool gpu_sync;      // whether last change in GPU buffer is synced with the array
    bool arr_sync;      // whether last change in the array is synced with GPU buffer
};

/* Copy data from GPU buffer to RAM if necessary. */
static void SyncToArrFromGPU(World *w) {
    if (!w->gpu_sync) {
        WorldComp_GetBodies(w->comp, w->arr);
        w->gpu_sync = true;
        w->arr_sync = true;
    }
}

/* Copy data from RAM to GPU buffer if necessary. */
static void SyncFromArrToGPU(World *w) {
    if (!w->arr_sync) {
        WorldComp_SetBodies(w->comp, w->arr);
        w->gpu_sync = true;
        w->arr_sync = true;
    }
}


/* Allocate World of given SIZE and randomize positions within MIN and MAX. */
World *World_Create(int size, V2 min, V2 max) {
    World *world = ALLOC(World);
    Body *arr = ALLOC_N(size, Body);
    ASSERT(world != NULL && arr != NULL);

    for (int i = 0; i < size; i++) {
        Particle_InitRand(&(arr + i)->p, min, max);
        arr[i].vel = V2_ZERO;
        arr[i].acc = V2_ZERO;
    }

    *world = (World){
            .arr = arr,
            .size = size,
            .comp = NULL,
            .gpu_sync = true,
            .arr_sync = true,
    };
    return world;
}

/* Free previously allocated W. */
void World_Destroy(World *w) {
    if (w != NULL) {
        if (w->comp != NULL) {
            WorldComp_Destroy(w->comp);
        }
        free(w->arr);
        free(w);
    }
}

/* Update W using exact simulation. */
void World_Update(World *w, float dt) {
    SyncToArrFromGPU(w);
    w->arr_sync = false;

    Body *arr = w->arr;
    int size = w->size;

    #pragma omp parallel for firstprivate(arr, size) default(none)
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (i != j) {
                Body_ApplyGrav(&arr[i], arr[j].p);
            }
        }
    }

    #pragma omp parallel for firstprivate(arr, size, dt) default(none)
    for (int i = 0; i < size; i++) {
        Body_Move(&arr[i], dt);
    }
}

/* Get W's bodies and size into respective pointers. */
void World_GetBodies(World *w, Body **bodies, int *size) {
    SyncToArrFromGPU(w);
    *bodies = w->arr;
    *size = w->size;
}


/*
 * Setup Vulkan pipeline for W. Updates will use constant time delta DT.
 * CTX must remain a valid pointer to initialized VulkanCtx until W is destroyed.
 */
void World_InitVK(World *w, const VulkanCtx *ctx, float dt) {
    if (w->comp == NULL) {
        WorldData data = (WorldData){
                .size = w->size,
                .dt = dt,
        };
        w->comp = WorldComp_Create(ctx, data);
        w->arr_sync = false;
    }
}

/* Update W using Vulkan pipeline. Aborts if Vulkan has not been setup for W. */
void World_UpdateVK(World *w) {
    ASSERT(w->comp != NULL);

    SyncFromArrToGPU(w);
    w->gpu_sync = false;

    WorldComp_DoUpdate(w->comp);
}

struct WorldComp {
    const VulkanCtx *ctx;
    // Shaders
    VkShaderModule grav_module;
    VkShaderModule move_module;
    // Descriptor
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkDescriptorSet ds;
    // Memory
    VkDeviceMemory memory;
    VkBuffer uniform;
    VkBuffer storage;
    VkDeviceSize uniform_size;
    VkDeviceSize storage_size;
    // Pipelines
    VkPipelineLayout pipeline_layout;
    VkPipeline grav_pipeline;
    VkPipeline move_pipeline;
    // Commands
    VkCommandBuffer cmd_buf;
    VkFence fence;
};

void WorldComp_GetBodies(WorldComp *wc, Body *arr) {
    void *mapped;
    ASSERT_VK(vkMapMemory(wc->ctx->dev, wc->memory, wc->uniform_size, wc->storage_size, 0, &mapped));

    memcpy(arr, mapped, wc->storage_size);
    vkUnmapMemory(wc->ctx->dev, wc->memory);
}

void WorldComp_SetBodies(WorldComp *wc, Body *arr) {
    void *mapped;
    ASSERT_VK(vkMapMemory(wc->ctx->dev, wc->memory, wc->uniform_size, wc->storage_size, 0, &mapped));

    memcpy(mapped, arr, wc->storage_size);
    vkUnmapMemory(wc->ctx->dev, wc->memory);
}

void WorldComp_DoUpdate(WorldComp *wc) {
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &wc->cmd_buf;

    ASSERT_VK(vkQueueSubmit(wc->ctx->queue, 1, &submit_info, wc->fence));
    ASSERT_VK(vkWaitForFences(wc->ctx->dev, 1, &wc->fence, VK_TRUE, UINT64_MAX));
    ASSERT_VK(vkResetFences(wc->ctx->dev, 1, &wc->fence));
}

void WorldComp_Destroy(WorldComp *wc) {
    if (wc != NULL) {
        VkDevice dev = wc->ctx->dev;
        vkDestroyFence(dev, wc->fence, NULL);

        vkDestroyPipeline(dev, wc->move_pipeline, NULL);
        vkDestroyPipeline(dev, wc->grav_pipeline, NULL);
        vkDestroyPipelineLayout(dev, wc->pipeline_layout, NULL);

        vkFreeMemory(dev, wc->memory, NULL);
        vkDestroyBuffer(dev, wc->uniform, NULL);
        vkDestroyBuffer(dev, wc->storage, NULL);

        vkDestroyDescriptorPool(dev, wc->ds_pool, NULL);
        vkDestroyDescriptorSetLayout(dev, wc->ds_layout, NULL);

        vkDestroyShaderModule(dev, wc->move_module, NULL);
        vkDestroyShaderModule(dev, wc->grav_module, NULL);
        free(wc);
    }
}

WorldComp *WorldComp_Create(const VulkanCtx *ctx, WorldData data) {
    WorldComp *wc = ALLOC(WorldComp);
    ASSERT(wc != NULL);

    wc->ctx = ctx;
    wc->grav_module = VulkanCtx_LoadShader(ctx, "shader/body_grav_cs.spv");
    wc->move_module = VulkanCtx_LoadShader(ctx, "shader/body_move_cs.spv");

    /*
     * Descriptor set and friends.
     */

    VkDescriptorSetLayoutBinding bindings[2] = {0};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ds_layout_info = {0};
    ds_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ds_layout_info.bindingCount = 2;
    ds_layout_info.pBindings = bindings;
    ASSERT_VK(vkCreateDescriptorSetLayout(ctx->dev, &ds_layout_info, NULL, &wc->ds_layout));

    VkDescriptorPoolSize ds_pool_size[2] = {0};
    ds_pool_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ds_pool_size[0].descriptorCount = 1;
    ds_pool_size[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ds_pool_size[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo ds_pool_info = {0};
    ds_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ds_pool_info.maxSets = 1;
    ds_pool_info.poolSizeCount = 2;
    ds_pool_info.pPoolSizes = ds_pool_size;
    ASSERT_VK(vkCreateDescriptorPool(ctx->dev, &ds_pool_info, NULL, &wc->ds_pool));

    VkDescriptorSetAllocateInfo ds_alloc_info = {0};
    ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_alloc_info.descriptorPool = wc->ds_pool;
    ds_alloc_info.descriptorSetCount = 1;
    ds_alloc_info.pSetLayouts = &wc->ds_layout;
    ASSERT_VK(vkAllocateDescriptorSets(ctx->dev, &ds_alloc_info, &wc->ds));

    /*
     * Memory buffers.
     */

    wc->uniform_size = SIZE_OF_ALIGN_16(WorldData);
    wc->storage_size = data.size * sizeof(Body);

    wc->memory = VulkanCtx_AllocMemory(ctx, wc->uniform_size + wc->storage_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    wc->uniform = VulkanCtx_CreateBuffer(ctx, wc->uniform_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    wc->storage = VulkanCtx_CreateBuffer(ctx, wc->storage_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    vkBindBufferMemory(ctx->dev, wc->uniform, wc->memory, 0);
    vkBindBufferMemory(ctx->dev, wc->storage, wc->memory, wc->uniform_size);

    void *mapped;
    ASSERT_VK(vkMapMemory(ctx->dev, wc->memory, 0, wc->uniform_size, 0, &mapped));

    memcpy(mapped, &data, sizeof(WorldData));
    vkUnmapMemory(ctx->dev, wc->memory);

    /*
     * Bind buffers to descriptor sets.
     */

    VkDescriptorBufferInfo uniform_info = {0};
    uniform_info.buffer = wc->uniform;
    uniform_info.offset = 0;
    uniform_info.range = wc->uniform_size;

    VkDescriptorBufferInfo storage_info = {0};
    storage_info.buffer = wc->storage;
    storage_info.offset = 0;
    storage_info.range = wc->storage_size;

    VkWriteDescriptorSet write_sets[2] = {0};

    write_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_sets[0].dstSet = wc->ds;
    write_sets[0].dstBinding = 0;
    write_sets[0].descriptorCount = 1;
    write_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_sets[0].pBufferInfo = &uniform_info;

    write_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_sets[1].dstSet = wc->ds;
    write_sets[1].dstBinding = 1;
    write_sets[1].descriptorCount = 1;
    write_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_sets[1].pBufferInfo = &storage_info;

    vkUpdateDescriptorSets(ctx->dev, 2, write_sets, 0, NULL);

    /*
     * Pipelines.
     */

    VkPipelineLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &wc->ds_layout;
    ASSERT_VK(vkCreatePipelineLayout(ctx->dev, &layout_info, NULL, &wc->pipeline_layout));

    VkPipelineShaderStageCreateInfo grav_stage_info = {0};
    grav_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    grav_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    grav_stage_info.module = wc->grav_module;
    grav_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo move_stage_info = {0};
    move_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    move_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    move_stage_info.module = wc->move_module;
    move_stage_info.pName = "main";

    VkComputePipelineCreateInfo pipeline_info[2] = {0};

    pipeline_info[0].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[0].stage = grav_stage_info;
    pipeline_info[0].layout = wc->pipeline_layout;

    pipeline_info[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[1].stage = move_stage_info;
    pipeline_info[1].layout = wc->pipeline_layout;

    VkPipeline pipelines[2];
    ASSERT_VK(vkCreateComputePipelines(ctx->dev, NULL, 2, pipeline_info, NULL, pipelines));

    wc->grav_pipeline = pipelines[0];
    wc->move_pipeline = pipelines[1];

    /*
     * Command buffers.
     */

    VulkanCtx_AllocCommandBuffers(ctx, 1, &wc->cmd_buf);

    VkCommandBufferBeginInfo cmd_begin_info = {0};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    ASSERT_VK(vkBeginCommandBuffer(wc->cmd_buf, &cmd_begin_info));

    vkCmdBindDescriptorSets(wc->cmd_buf,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            wc->pipeline_layout, 0,
                            1, &wc->ds,
                            0, 0);

    uint32_t group_count = data.size / LOCAL_SIZE_X;
    if (data.size % LOCAL_SIZE_X != 0) group_count++;

    vkCmdBindPipeline(wc->cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, wc->grav_pipeline);
    vkCmdDispatch(wc->cmd_buf, group_count, 1, 1);

    VkBufferMemoryBarrier storage_barrier = {0};
    storage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    storage_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    storage_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    storage_barrier.srcQueueFamilyIndex = ctx->queue_family_idx;
    storage_barrier.dstQueueFamilyIndex = ctx->queue_family_idx;
    storage_barrier.buffer = wc->storage;
    storage_barrier.offset = 0;
    storage_barrier.size = wc->storage_size;

    vkCmdPipelineBarrier(wc->cmd_buf,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, NULL,
                         1, &storage_barrier,
                         0, NULL);

    vkCmdBindPipeline(wc->cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, wc->move_pipeline);
    vkCmdDispatch(wc->cmd_buf, group_count, 1, 1);

    ASSERT_VK(vkEndCommandBuffer(wc->cmd_buf));

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ASSERT_VK(vkCreateFence(ctx->dev, &fence_info, NULL, &wc->fence));

    return wc;
}
