#include <rag.h>
#include <rag_vk.h>

#include <string.h> // memcpy

#include "lib/fio.h"
#include "lib/util.h"

#define LOCAL_SIZE_X 16

typedef struct WorldData {
    uint32_t size;
    float dt;
    V2 min;
    V2 max;
} WorldData;

/* Simulation pipeline-related stuff. */
typedef struct BodyCompute {
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
    VkDeviceSize uniform_size;
    VkDeviceSize storage_size;
    // Pipelines
    VkPipelineLayout pipeline_layout;
    VkPipeline grav_pipeline;
    VkPipeline move_pipeline;
    // Command buffers
    VkCommandBuffer cmd_buf;
    VkFence fence;
} BodyCompute;

/*
 * Initialize necessary Vulkan stuff and setup buffers with BODIES and WORLD_DATA.
 * Note that number of bodies and size of the world cannot be changed after initialization.
 *
 * CTX must remain a valid pointer to initialized VulkanCtx until BCP is de-initialized.
 */
void BodyCompute_Init(BodyCompute *bc, const VulkanCtx *ctx, Body *bodies, WorldData world_data) {
    bc->ctx = ctx;

    /*
     * Shader modules.
     */

    bc->grav_module = VulkanCtx_LoadShader(ctx, "shader/body_grav_cs.spv");
    bc->move_module = VulkanCtx_LoadShader(ctx, "shader/body_move_cs.spv");

    /*
     * Descriptor set layout.
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
    ASSERT_VK(vkCreateDescriptorSetLayout(ctx->dev, &ds_layout_info, NULL, &bc->ds_layout));

    /*
     * Descriptor pool.
     */

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

    bc->uniform_size = VK_SIZE_OF_16(WorldData);
    bc->storage_size = world_data.size * sizeof(Body);
    VkDeviceSize memory_size = bc->uniform_size + bc->storage_size;

    bc->memory = VulkanCtx_AllocMemory(ctx, memory_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    bc->uniform = VulkanCtx_CreateBuffer(ctx, bc->uniform_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    bc->storage = VulkanCtx_CreateBuffer(ctx, bc->storage_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    vkBindBufferMemory(ctx->dev, bc->uniform, bc->memory, 0);
    vkBindBufferMemory(ctx->dev, bc->storage, bc->memory, bc->uniform_size);

    /*
     * Write initial state into buffers.
     */

    void *mapped;
    ASSERT_VK(vkMapMemory(ctx->dev, bc->memory, 0, memory_size, 0, &mapped));

    memcpy(mapped, &world_data, sizeof(WorldData));
    memcpy((char *)mapped + bc->uniform_size, bodies, bc->storage_size);
    vkUnmapMemory(ctx->dev, bc->memory);

    /*
     * Bind buffers to descriptor sets.
     */

    VkDescriptorBufferInfo uniform_info = {0};
    uniform_info.buffer = bc->uniform;
    uniform_info.offset = 0;
    uniform_info.range = bc->uniform_size;

    VkDescriptorBufferInfo storage_info = {0};
    storage_info.buffer = bc->storage;
    storage_info.offset = 0;
    storage_info.range = bc->storage_size;

    VkWriteDescriptorSet write_sets[2] = {0};

    write_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_sets[0].dstSet = bc->ds;
    write_sets[0].dstBinding = 0;
    write_sets[0].descriptorCount = 1;
    write_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_sets[0].pBufferInfo = &uniform_info;

    write_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_sets[1].dstSet = bc->ds;
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

    VkComputePipelineCreateInfo pipeline_info[2] = {0};
    pipeline_info[0].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[0].stage = grav_stage_info;
    pipeline_info[0].layout = bc->pipeline_layout;
    pipeline_info[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info[1].stage = move_stage_info;
    pipeline_info[1].layout = bc->pipeline_layout;

    VkPipeline pipelines[2];
    ASSERT_VK(vkCreateComputePipelines(ctx->dev, NULL, 2, pipeline_info, NULL, pipelines));

    bc->grav_pipeline = pipelines[0];
    bc->move_pipeline = pipelines[1];

    /*
     * Command buffers.
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

    uint32_t group_count = world_data.size / LOCAL_SIZE_X;
    if (world_data.size % LOCAL_SIZE_X != 0) group_count++;

    vkCmdBindPipeline(bc->cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, bc->grav_pipeline);
    vkCmdDispatch(bc->cmd_buf, group_count, 1, 1);

    VkBufferMemoryBarrier storage_barrier = {0};
    storage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    storage_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    storage_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    storage_barrier.srcQueueFamilyIndex = ctx->queue_family_idx;
    storage_barrier.dstQueueFamilyIndex = ctx->queue_family_idx;
    storage_barrier.buffer = bc->storage;
    storage_barrier.offset = 0;
    storage_barrier.size = bc->storage_size;

    vkCmdPipelineBarrier(bc->cmd_buf,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, NULL,
                         1, &storage_barrier,
                         0, NULL);

    vkCmdBindPipeline(bc->cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, bc->move_pipeline);
    vkCmdDispatch(bc->cmd_buf, group_count, 1, 1);

    ASSERT_VK(vkEndCommandBuffer(bc->cmd_buf));

    /*
     * Fences.
     */

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ASSERT_VK(vkCreateFence(ctx->dev, &fence_info, NULL, &bc->fence));
}

/* De-initialize BodyCompute. */
void BodyCompute_DeInit(BodyCompute *bc) {
    VkDevice dev = bc->ctx->dev;
    vkDestroyFence(dev, bc->fence, NULL);

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

/* Update world and copy results into BODIES. */
void BodyCompute_DoUpdate(BodyCompute *bc, Body *bodies) {
    VkDevice dev = bc->ctx->dev;

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &bc->cmd_buf;

    ASSERT_VK(vkQueueSubmit(bc->ctx->queue, 1, &submit_info, bc->fence));
    ASSERT_VK(vkWaitForFences(dev, 1, &bc->fence, VK_TRUE, UINT64_MAX));
    ASSERT_VK(vkResetFences(dev, 1, &bc->fence));

    void *mapped;
    ASSERT_VK(vkMapMemory(dev, bc->memory, bc->uniform_size, bc->storage_size, 0, &mapped));

    memcpy(bodies, mapped, bc->storage_size);
    vkUnmapMemory(dev, bc->memory);
}

#define WORLD_SIZE      4
#define WORLD_WIDTH     100
#define WORLD_HEIGHT    100

#define PART_FROM(X, Y) (Particle){ .pos = V2_From(X, Y), .mass = 10.0, .radius = 0.1 }
#define BODY_FROM(X, Y) (Body){ .p = PART_FROM(X, Y), .acc = V2_ZERO, .vel = V2_ZERO }

void ShowBodies(const Body *bodies) {
    printf("\n");
    for (int i = 0; i < WORLD_SIZE; i++) {
        Body b = bodies[i];
        printf("pos = (%6.2f, %6.2f)\tvel = (%6.2f, %6.2f)\n",
               b.p.pos.x, b.p.pos.y, b.vel.x, b.vel.y);
    }
}

int main(void) {
    VulkanCtx ctx;
    VulkanCtx_Init(&ctx, false);

    Body bodies[] = {
            BODY_FROM(10, 10),
            BODY_FROM(10, 90),
            BODY_FROM(90, 10),
            BODY_FROM(90, 90),
    };

    BodyCompute bc;
    BodyCompute_Init(&bc, &ctx, bodies, (WorldData){
            .size = WORLD_SIZE,
            .dt = 1.0f,
            .min = V2_ZERO,
            .max = V2_From(WORLD_WIDTH, WORLD_HEIGHT),
    });

    ShowBodies(bodies);
    BodyCompute_DoUpdate(&bc, bodies);
    ShowBodies(bodies);
    BodyCompute_DoUpdate(&bc, bodies);
    ShowBodies(bodies);

    BodyCompute_DeInit(&bc);
    VulkanCtx_DeInit(&ctx);
}
