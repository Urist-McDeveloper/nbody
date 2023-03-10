#include <rag_vk.h>

#include "lib/fio.h"
#include "lib/util.h"

#define LOCAL_SIZE_X 16

/* Simulation pipeline-related stuff. */
typedef struct BodyCompute {
    // Context
    const VulkanCtx *ctx;
    // Shaders
    VkShaderModule grav_module;
    VkShaderModule move_module;
    // Descriptor sets
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkDescriptorSet ds;
    // Memory buffers
    VkDeviceMemory memory;
    VkBuffer uniform;
    VkBuffer storage;
    // Pipelines
    VkPipelineLayout pipeline_layout;
    VkPipeline grav_pipeline;
    VkPipeline move_pipeline;
    // Command buffers
    VkCommandBuffer cmd_buf;
} BodyCompute;

/*
 * Initialize BodyCompute.
 * CTX must remain a valid pointer to initialized VulkanCtx until BCP is de-initialized.
 */
void BodyCompute_Init(BodyCompute *bc, const VulkanCtx *ctx, uint32_t world_size) {
    bc->ctx = ctx;

    /*
     * Shader modules.
     */

    bc->grav_module = VulkanCtx_LoadShader(ctx, "shader/body_grav_cs.spv");
    bc->move_module = VulkanCtx_LoadShader(ctx, "shader/body_move_cs.spv");

    /*
     * Descriptor set layout.
     */

    VkDescriptorSetLayoutBinding bindings[2];

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

    VkDescriptorSetLayoutCreateInfo ds_layout_info = {0};
    ds_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ds_layout_info.bindingCount = 2;
    ds_layout_info.pBindings = bindings;
    ASSERT_VK(vkCreateDescriptorSetLayout(ctx->dev, &ds_layout_info, NULL, &bc->ds_layout));

    /*
     * Descriptor pool.
     */

    VkDescriptorPoolSize ds_pool_size[2];

    ds_pool_size[0] = (VkDescriptorPoolSize){0};
    ds_pool_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ds_pool_size[0].descriptorCount = 1;

    ds_pool_size[1] = (VkDescriptorPoolSize){0};
    ds_pool_size[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ds_pool_size[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo ds_pool_info = {0};
    ds_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ds_pool_info.maxSets = 1;
    ds_pool_info.poolSizeCount = 2;
    ds_pool_info.pPoolSizes = ds_pool_size;
    ASSERT_VK(vkCreateDescriptorPool(ctx->dev, &ds_pool_info, NULL, &bc->ds_pool));

    /*
     * Descriptor set.
     */

    VkDescriptorSetAllocateInfo ds_alloc_info = {0};
    ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_alloc_info.descriptorPool = bc->ds_pool;
    ds_alloc_info.descriptorSetCount = 1;
    ds_alloc_info.pSetLayouts = &bc->ds_layout;
    ASSERT_VK(vkAllocateDescriptorSets(ctx->dev, &ds_alloc_info, &bc->ds));

    /*
     * Memory buffers.
     */

    VkDeviceSize uniform_size = 16;
    VkDeviceSize storage_size = world_size * sizeof(Body);

    VulkanCtx_AllocMemory(ctx, &bc->memory, uniform_size + storage_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VulkanCtx_CreateBuffer(ctx, &bc->uniform, uniform_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    VulkanCtx_CreateBuffer(ctx, &bc->storage, storage_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    vkBindBufferMemory(ctx->dev, bc->uniform, bc->memory, 0);
    vkBindBufferMemory(ctx->dev, bc->storage, bc->memory, uniform_size);

    /*
     * Pipelines.
     */

    VkPipelineLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &bc->ds_layout;
    ASSERT_VK(vkCreatePipelineLayout(ctx->dev, &layout_info, NULL, &bc->pipeline_layout));

    VkPipelineShaderStageCreateInfo grav_stage_info = {0};
    grav_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    grav_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    grav_stage_info.module = bc->grav_module;
    grav_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo move_stage_info = {0};
    move_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    move_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    move_stage_info.module = bc->move_module;
    move_stage_info.pName = "main";

    VkComputePipelineCreateInfo pipeline_info[2];

    pipeline_info[0] = (VkComputePipelineCreateInfo){0};
    pipeline_info[0].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[0].stage = grav_stage_info;
    pipeline_info[0].layout = bc->pipeline_layout;

    pipeline_info[1] = (VkComputePipelineCreateInfo){0};
    pipeline_info[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[1].stage = move_stage_info;
    pipeline_info[1].layout = bc->pipeline_layout;

    VkPipeline pipelines[2];
    ASSERT_VK(vkCreateComputePipelines(ctx->dev, NULL, 2, pipeline_info, NULL, pipelines));

    bc->grav_pipeline = pipelines[0];
    bc->move_pipeline = pipelines[1];

    /*
     * Command buffer.
     */

    VulkanCtx_AllocCommandBuffers(ctx, 1, &bc->cmd_buf);

    VkCommandBufferBeginInfo cmd_begin_info = {0};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    ASSERT_VK(vkBeginCommandBuffer(bc->cmd_buf, &cmd_begin_info));

    vkCmdBindDescriptorSets(bc->cmd_buf,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            bc->pipeline_layout, 0,
                            1, &bc->ds,
                            0, 0);

    VkBufferMemoryBarrier storage_barrier = {0};
    storage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    storage_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    storage_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    storage_barrier.srcQueueFamilyIndex = ctx->queue_family_idx;
    storage_barrier.dstQueueFamilyIndex = ctx->queue_family_idx;
    storage_barrier.buffer = bc->storage;
    storage_barrier.offset = 0;
    storage_barrier.size = storage_size;

    uint32_t group_count = world_size / LOCAL_SIZE_X;
    if (world_size % LOCAL_SIZE_X != 0) group_count++;

    vkCmdBindPipeline(bc->cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, bc->grav_pipeline);
    vkCmdDispatch(bc->cmd_buf, group_count, 1, 1);

    vkCmdPipelineBarrier(bc->cmd_buf,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, NULL,
                         1, &storage_barrier,
                         0, NULL);

    vkCmdBindPipeline(bc->cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, bc->move_pipeline);
    vkCmdDispatch(bc->cmd_buf, group_count, 1, 1);

    ASSERT_VK(vkEndCommandBuffer(bc->cmd_buf));
}

/* De-initialize BodyCompute. */
void BodyCompute_DeInit(BodyCompute *bc) {
    VkDevice dev = bc->ctx->dev;

    vkDestroyPipeline(dev, bc->move_pipeline, NULL);
    vkDestroyPipeline(dev, bc->grav_pipeline, NULL);
    vkDestroyPipelineLayout(dev, bc->pipeline_layout, NULL);

    vkFreeMemory(dev, bc->memory, NULL);
    vkDestroyBuffer(dev, bc->uniform, NULL);
    vkDestroyBuffer(dev, bc->storage, NULL);

    vkDestroyDescriptorPool(dev, bc->ds_pool, NULL);
    vkDestroyDescriptorSetLayout(dev, bc->ds_layout, NULL);

    vkDestroyShaderModule(dev, bc->move_module, NULL);
    vkDestroyShaderModule(dev, bc->grav_module, NULL);
}

#define WORLD_SIZE 100

int main(void) {
    VulkanCtx ctx;
    VulkanCtx_Init(&ctx, false);

    BodyCompute bc;
    BodyCompute_Init(&bc, &ctx, WORLD_SIZE);

    Body *bodies;
    int size;
    World *w = World_Create(WORLD_SIZE, 800, 600);
    World_GetBodies(w, &bodies, &size);

    // TODO: stuff
    printf("Press ENTER to continue\n");
    fgetc(stdin);

    World_Destroy(w);
    BodyCompute_DeInit(&bc);
    VulkanCtx_DeInit(&ctx);
}
