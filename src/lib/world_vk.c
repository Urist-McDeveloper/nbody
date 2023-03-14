#include "world_vk.h"

#include "util.h"
#include "vulkan_ctx.h"

/* Compute shader work group size. */
#define LOCAL_SIZE_X 256

struct SimPipeline {
    WorldData world_data;
    const VulkanCtx *ctx;
    VkShaderModule shader;
    // Memory
    VulkanDeviceMemory dev_mem;     // device-local memory
    VulkanDeviceMemory host_mem;    // host-accessible memory
    VulkanBuffer uniform;           // uniform buffer in device-local memory
    VulkanBuffer storage[2];        // uniform buffer in device-local memory; [0] for old data, [1] for new
    VulkanBuffer transfer_buf[2];   // host-accessible transfer buffers; [0] for uniform, [1] for storage
    // Descriptor
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkDescriptorSet set;
    // Pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    // Commands and synchronization
    VkCommandBuffer cmd;
    VkFence fence;
};

SimPipeline *CreateSimPipeline(const VulkanCtx *ctx, WorldData data) {
    SimPipeline *sim = ALLOC(1, SimPipeline);
    ASSERT_MSG(sim != NULL, "Failed to alloc SimPipeline");

    sim->world_data = data;
    sim->ctx = ctx;

    /*
     * Shaders.
     */

    sim->shader = LoadVkShaderModule(ctx, "shader/particle_cs.spv");

    VkSpecializationMapEntry shader_spec_map[4];
    for (int i = 0; i < 4; i++) {
        shader_spec_map[i].constantID = i;
        shader_spec_map[i].offset = 4 * i;
        shader_spec_map[i].size = 4;
    }

    char shader_spec_data[16];
    *(uint32_t *)shader_spec_data = LOCAL_SIZE_X;
    *(float *)(shader_spec_data + 4) = NB_G;
    *(float *)(shader_spec_data + 8) = NB_N;
    *(float *)(shader_spec_data + 12) = NB_F;

    VkSpecializationInfo shader_spec_info = {0};
    shader_spec_info.mapEntryCount = 4;
    shader_spec_info.pMapEntries = shader_spec_map;
    shader_spec_info.dataSize = 16;
    shader_spec_info.pData = shader_spec_data;

    VkPipelineShaderStageCreateInfo shader_stage_info = {0};
    shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_info.module = sim->shader;
    shader_stage_info.pName = "main";
    shader_stage_info.pSpecializationInfo = &shader_spec_info;

    /*
     * Memory buffers.
     */

    const VkDeviceSize uniform_size = SIZE_OF_ALIGN_16(WorldData);
    const VkDeviceSize storage_size = data.size * sizeof(Particle);

    const VkDeviceSize host_mem_size = uniform_size + storage_size;
    const VkDeviceSize dev_mem_size = uniform_size + 2 * storage_size;

    sim->host_mem = CreateDeviceMemory(ctx, host_mem_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    sim->dev_mem = CreateDeviceMemory(ctx, dev_mem_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkBufferUsageFlags transfer_buf_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags uniform_buf_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags storage_buf_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | transfer_buf_flags;

    sim->uniform = CreateVulkanBuffer(&sim->dev_mem, uniform_size, uniform_buf_flags);
    sim->storage[0] = CreateVulkanBuffer(&sim->dev_mem, storage_size, storage_buf_flags);
    sim->storage[1] = CreateVulkanBuffer(&sim->dev_mem, storage_size, storage_buf_flags);
    sim->transfer_buf[0] = CreateVulkanBuffer(&sim->host_mem, uniform_size, transfer_buf_flags);
    sim->transfer_buf[1] = CreateVulkanBuffer(&sim->host_mem, storage_size, transfer_buf_flags);

    /*
     * Descriptors.
     */

    VkDescriptorSetLayoutBinding bindings[3] = {0};

    // uniform
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // old
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // new
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ds_layout_info = {0};
    ds_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ds_layout_info.bindingCount = 3;
    ds_layout_info.pBindings = bindings;
    ASSERT_VKR(vkCreateDescriptorSetLayout(ctx->dev, &ds_layout_info, NULL, &sim->ds_layout),
               "Failed to create descriptor set layout");

    VkDescriptorPoolSize ds_pool_size[2] = {0};
    ds_pool_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ds_pool_size[0].descriptorCount = 1;
    ds_pool_size[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ds_pool_size[1].descriptorCount = 2;

    VkDescriptorPoolCreateInfo ds_pool_info = {0};
    ds_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ds_pool_info.maxSets = 1;
    ds_pool_info.poolSizeCount = 2;
    ds_pool_info.pPoolSizes = ds_pool_size;
    ASSERT_VKR(vkCreateDescriptorPool(ctx->dev, &ds_pool_info, NULL, &sim->ds_pool),
               "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo ds_alloc_info = {0};
    ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_alloc_info.descriptorPool = sim->ds_pool;
    ds_alloc_info.descriptorSetCount = 1;
    ds_alloc_info.pSetLayouts = &sim->ds_layout;
    ASSERT_VKR(vkAllocateDescriptorSets(sim->ctx->dev, &ds_alloc_info, &sim->set), "Failed to allocate descriptor set");

    /*
     * Update descriptor set.
     */

    VkDescriptorBufferInfo uniform_info, storage_info[2];
    FillDescriptorBufferInfo(&sim->uniform, &uniform_info);
    FillDescriptorBufferInfo(&sim->storage[0], &storage_info[0]);
    FillDescriptorBufferInfo(&sim->storage[1], &storage_info[1]);

    VkWriteDescriptorSet write_sets[2] = {0};
    write_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_sets[0].dstSet = sim->set;
    write_sets[0].dstBinding = 0;
    write_sets[0].descriptorCount = 1;
    write_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_sets[0].pBufferInfo = &uniform_info;

    write_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_sets[1].dstSet = sim->set;
    write_sets[1].dstBinding = 1;
    write_sets[1].descriptorCount = 2;
    write_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_sets[1].pBufferInfo = storage_info;

    vkUpdateDescriptorSets(sim->ctx->dev, 2, write_sets, 0, NULL);

    /*
     * Pipeline.
     */

    VkPipelineLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &sim->ds_layout;
    ASSERT_VKR(vkCreatePipelineLayout(ctx->dev, &layout_info, NULL, &sim->pipeline_layout),
               "Failed to create pipeline layout");

    VkComputePipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = shader_stage_info;
    pipeline_info.layout = sim->pipeline_layout;
    ASSERT_VKR(vkCreateComputePipelines(ctx->dev, NULL, 1, &pipeline_info, NULL, &sim->pipeline),
               "Failed to create compute pipeline");

    /*
     * Command buffers and synchronization.
     */

    AllocVkCommandBuffers(ctx, 1, &sim->cmd);

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ASSERT_VKR(vkCreateFence(ctx->dev, &fence_info, NULL, &sim->fence), "Failed to create fence");

    return sim;
}

void DestroySimPipeline(SimPipeline *sim) {
    if (sim != NULL) {
        VkDevice dev = sim->ctx->dev;

        vkDestroyFence(dev, sim->fence, NULL);
        vkFreeCommandBuffers(dev, sim->ctx->cmd_pool, 1, &sim->cmd);

        vkDestroyPipeline(dev, sim->pipeline, NULL);
        vkDestroyPipelineLayout(dev, sim->pipeline_layout, NULL);

        vkDestroyDescriptorPool(dev, sim->ds_pool, NULL);
        vkDestroyDescriptorSetLayout(dev, sim->ds_layout, NULL);

        DestroyVulkanBuffer(&sim->transfer_buf[0]);
        DestroyVulkanBuffer(&sim->transfer_buf[1]);
        DestroyVulkanBuffer(&sim->storage[0]);
        DestroyVulkanBuffer(&sim->storage[1]);
        DestroyVulkanBuffer(&sim->uniform);
        DestroyVulkanMemory(&sim->host_mem);
        DestroyVulkanMemory(&sim->dev_mem);

        vkDestroyShaderModule(dev, sim->shader, NULL);
        free(sim);
    }
}

void PerformSimUpdate(SimPipeline *sim, uint32_t n, float dt, Particle *arr, bool new_data) {
    ASSERT_MSG(n > 0, "Performing 0 GPU simulation updates is not allowed");

    // start recording command buffer
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ASSERT_VKR(vkBeginCommandBuffer(sim->cmd, &begin_info), "Failed to begin pipeline command buffer");

    // update uniform buffer if DT has changed
    if (sim->world_data.dt != dt) {
        sim->world_data.dt = dt;

        SetVulkanBufferData(&sim->transfer_buf[0], &sim->world_data);
        CopyVulkanBuffer(sim->cmd, &sim->transfer_buf[0], &sim->uniform);

        // pipeline should wait until copy command is finished
        VkBufferMemoryBarrier uniform_copy_barrier;
        FillVulkanBufferWriteReadBarrier(&sim->uniform, &uniform_copy_barrier);

        vkCmdPipelineBarrier(sim->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, NULL,
                             1, &uniform_copy_barrier,
                             0, NULL);
    }

    // copy new data to storage[0] either from ARR or from storage[1]
    if (new_data) {
        SetVulkanBufferData(&sim->transfer_buf[1], arr);
        CopyVulkanBuffer(sim->cmd, &sim->transfer_buf[1], &sim->storage[0]);
    } else {
        CopyVulkanBuffer(sim->cmd, &sim->storage[1], &sim->storage[0]);
    }

    // wait for pipeline to finish before copying storage[1] into storage[0]
    VkBufferMemoryBarrier pipeline_barrier;
    FillVulkanBufferWriteReadBarrier(&sim->storage[1], &pipeline_barrier);

    // wait for copy command to finish before running pipeline
    VkBufferMemoryBarrier transfer_barrier;
    FillVulkanBufferWriteReadBarrier(&sim->storage[0], &transfer_barrier);

    // bind pipeline and descriptor set
    uint32_t group_count = sim->world_data.size / LOCAL_SIZE_X;
    if (sim->world_data.size % LOCAL_SIZE_X != 0) group_count++;

    vkCmdBindPipeline(sim->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sim->pipeline);
    vkCmdBindDescriptorSets(sim->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            sim->pipeline_layout, 0,
                            1, &sim->set,
                            0, 0);

    // run simulation N times
    for (uint32_t i = 0; i < n; i++) {
        // first dispatch already has new data in storage[0]
        if (i != 0) {
            // wait for pipeline to finish and copy new data from storage[1] to storage[0]
            vkCmdPipelineBarrier(sim->cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, NULL,
                                 1, &pipeline_barrier,
                                 0, NULL);
            CopyVulkanBuffer(sim->cmd, &sim->storage[1], &sim->storage[0]);
        }

        // wait for transfer to finish and run pipeline
        vkCmdPipelineBarrier(sim->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, NULL,
                             1, &transfer_barrier,
                             0, NULL);
        vkCmdDispatch(sim->cmd, group_count, 1, 1);
    }

    // wait for pipeline to finish and copy new data from storage[1] to transfer_buf[1]
    vkCmdPipelineBarrier(sim->cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, NULL,
                         1, &pipeline_barrier,
                         0, NULL);
    CopyVulkanBuffer(sim->cmd, &sim->storage[1], &sim->transfer_buf[1]);

    // finish recording command buffer
    ASSERT_VKR(vkEndCommandBuffer(sim->cmd), "Failed to end pipeline command buffer");

    // submit command buffer
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &sim->cmd;

    ASSERT_VKR(vkQueueSubmit(sim->ctx->queue, 1, &submit_info, sim->fence), "Failed to submit command buffer");
    ASSERT_VKR(vkWaitForFences(sim->ctx->dev, 1, &sim->fence, VK_TRUE, UINT64_MAX), "Failed to wait for fences");

    // reset fence and command buffer
    ASSERT_VKR(vkResetFences(sim->ctx->dev, 1, &sim->fence), "Failed to reset fence");
    ASSERT_VKR(vkResetCommandBuffer(sim->cmd, 0), "Failed to reset command buffer");

    // write new data from transfer_buf[1] to ARR
    GetVulkanBufferData(&sim->transfer_buf[1], arr);
}
