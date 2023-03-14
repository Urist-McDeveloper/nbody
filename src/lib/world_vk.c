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
    VkDeviceMemory dev_mem;                 // device-local memory
    VkDeviceMemory host_mem;                // host-accessible memory
    VkBuffer uniform;
    VkBuffer storage[2];                    // storage[0] for old data, storage[1] for new
    VkBuffer transfer_buf;                  // buffer from host-accessible memory
    VkDeviceSize uniform_size;
    VkDeviceSize storage_size;
    // Descriptor
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkDescriptorSet set;
    // Pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    // Commands
    VkCommandBuffer pipeline_cb;            // transient buffer for pipeline dispatches
    VkCommandBuffer htd_uniform_copy_cb;    // copy transfer_buf to uniform
    VkCommandBuffer htd_storage_copy_cb;    // copy transfer_buf to storage[1]
    VkCommandBuffer dth_storage_copy_cb;    // copy storage[1] to transfer_buf
    VkFence fence;
};

static void SubmitAndWait(SimPipeline *sim, VkCommandBuffer cmd_buf) {
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    ASSERT_VKR(vkQueueSubmit(sim->ctx->queue, 1, &submit_info, sim->fence), "Failed to submit command buffer");
    ASSERT_VKR(vkWaitForFences(sim->ctx->dev, 1, &sim->fence, VK_TRUE, UINT64_MAX), "Failed to wait for fences");
    ASSERT_VKR(vkResetFences(sim->ctx->dev, 1, &sim->fence), "Failed to reset fence");
}

void GetSimParticles(SimPipeline *sim, Particle *arr) {
    // copy data from device-local buffer into host-accessible buffer
    SubmitAndWait(sim, sim->dth_storage_copy_cb);

    void *mapped;
    ASSERT_VKR(vkMapMemory(sim->ctx->dev, sim->host_mem, 0, sim->storage_size, 0, &mapped), "Failed to map memory");

    // copy data from host-accessible buffer
    memcpy(arr, mapped, sim->storage_size);
    vkUnmapMemory(sim->ctx->dev, sim->host_mem);
}

void SetSimParticles(SimPipeline *sim, Particle *arr) {
    void *mapped;
    ASSERT_VKR(vkMapMemory(sim->ctx->dev, sim->host_mem, 0, sim->storage_size, 0, &mapped), "Failed to map memory");

    // copy data into host-accessible buffer
    memcpy(mapped, arr, sim->storage_size);
    vkUnmapMemory(sim->ctx->dev, sim->host_mem);

    // copy data from host-accessible buffer into device-local buffer
    SubmitAndWait(sim, sim->htd_storage_copy_cb);
}

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
    const VkDeviceSize dev_mem_size = uniform_size + 2 * storage_size;

    sim->uniform_size = uniform_size;
    sim->storage_size = storage_size;

    sim->host_mem = AllocVkDeviceMemory(ctx, storage_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    sim->dev_mem = AllocVkDeviceMemory(ctx, dev_mem_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkBufferUsageFlags transfer_buf_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags uniform_buf_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags storage_buf_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | transfer_buf_flags;

    sim->uniform = CreateVkBuffer(ctx, uniform_size, uniform_buf_flags);
    sim->storage[0] = CreateVkBuffer(ctx, storage_size, storage_buf_flags);
    sim->storage[1] = CreateVkBuffer(ctx, storage_size, storage_buf_flags);
    sim->transfer_buf = CreateVkBuffer(ctx, storage_size, transfer_buf_flags);

    vkBindBufferMemory(ctx->dev, sim->uniform, sim->dev_mem, 0);
    vkBindBufferMemory(ctx->dev, sim->storage[0], sim->dev_mem, uniform_size);
    vkBindBufferMemory(ctx->dev, sim->storage[1], sim->dev_mem, uniform_size + storage_size);
    vkBindBufferMemory(ctx->dev, sim->transfer_buf, sim->host_mem, 0);

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

    VkDescriptorBufferInfo uniform_info = {0};
    uniform_info.buffer = sim->uniform;
    uniform_info.offset = 0;
    uniform_info.range = uniform_size;

    VkDescriptorBufferInfo storage_info[2] = {0};
    storage_info[0].buffer = sim->storage[0];
    storage_info[0].offset = 0;
    storage_info[0].range = storage_size;
    storage_info[1].buffer = sim->storage[1];
    storage_info[1].offset = 0;
    storage_info[1].range = storage_size;

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
     * Command buffers.
     */

    VkCommandBuffer buffers[4];
    AllocVkCommandBuffers(ctx, 4, buffers);

    sim->pipeline_cb = buffers[0];
    sim->htd_uniform_copy_cb = buffers[1];
    sim->htd_storage_copy_cb = buffers[2];
    sim->dth_storage_copy_cb = buffers[3];

    VkCommandBufferBeginInfo cmd_begin_info = {0};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkBufferCopy uniform_copy = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = uniform_size
    };
    VkBufferCopy storage_copy = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = storage_size
    };

    // host-to-dev uniform copy
    ASSERT_VKR(vkBeginCommandBuffer(sim->htd_uniform_copy_cb, &cmd_begin_info), "Failed to begin command cmd");
    vkCmdCopyBuffer(sim->htd_uniform_copy_cb, sim->transfer_buf, sim->uniform, 1, &uniform_copy);
    ASSERT_VKR(vkEndCommandBuffer(sim->htd_uniform_copy_cb), "Failed to end command buffer");

    // host-to-dev storage copy
    ASSERT_VKR(vkBeginCommandBuffer(sim->htd_storage_copy_cb, &cmd_begin_info), "Failed to begin command buffer");
    vkCmdCopyBuffer(sim->htd_storage_copy_cb, sim->transfer_buf, sim->storage[1], 1, &storage_copy);
    ASSERT_VKR(vkEndCommandBuffer(sim->htd_storage_copy_cb), "Failed to end command buffer");

    // dev-to-host storage copy
    ASSERT_VKR(vkBeginCommandBuffer(sim->dth_storage_copy_cb, &cmd_begin_info), "Failed to begin command cmd");
    vkCmdCopyBuffer(sim->dth_storage_copy_cb, sim->storage[1], sim->transfer_buf, 1, &storage_copy);
    ASSERT_VKR(vkEndCommandBuffer(sim->dth_storage_copy_cb), "Failed to end command cmd");

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ASSERT_VKR(vkCreateFence(ctx->dev, &fence_info, NULL, &sim->fence), "Failed to create fence");

    return sim;
}

void DestroySimPipeline(SimPipeline *sim) {
    if (sim != NULL) {
        VkDevice dev = sim->ctx->dev;
        vkDestroyFence(dev, sim->fence, NULL);

        VkCommandBuffer buffers[4] = {sim->pipeline_cb, sim->htd_uniform_copy_cb,
                                      sim->htd_storage_copy_cb, sim->dth_storage_copy_cb};
        vkFreeCommandBuffers(dev, sim->ctx->cmd_pool, 4, buffers);

        vkDestroyPipeline(dev, sim->pipeline, NULL);
        vkDestroyPipelineLayout(dev, sim->pipeline_layout, NULL);

        vkDestroyDescriptorPool(dev, sim->ds_pool, NULL);
        vkDestroyDescriptorSetLayout(dev, sim->ds_layout, NULL);

        vkDestroyBuffer(dev, sim->transfer_buf, NULL);
        vkDestroyBuffer(dev, sim->storage[0], NULL);
        vkDestroyBuffer(dev, sim->storage[1], NULL);
        vkDestroyBuffer(dev, sim->uniform, NULL);
        vkFreeMemory(dev, sim->host_mem, NULL);
        vkFreeMemory(dev, sim->dev_mem, NULL);

        vkDestroyShaderModule(dev, sim->shader, NULL);
        free(sim);
    }
}

void PerformSimUpdate(SimPipeline *sim, float dt, uint32_t n) {
    // update uniform buffer if DT has changed
    if (sim->world_data.dt != dt) {
        sim->world_data.dt = dt;

        void *mapped;
        ASSERT_VKR(vkMapMemory(sim->ctx->dev, sim->host_mem, 0, sim->uniform_size, 0, &mapped),
                   "Failed to map device memory");

        // copy data into host-accessible buffer
        *(WorldData *)mapped = sim->world_data;
        vkUnmapMemory(sim->ctx->dev, sim->host_mem);

        // copy data from host-accessible buffer into device-local buffer
        SubmitAndWait(sim, sim->htd_uniform_copy_cb);
    }

    VkCommandBuffer cmd = sim->pipeline_cb;
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ASSERT_VKR(vkBeginCommandBuffer(cmd, &begin_info), "Failed to begin pipeline command buffer");

    uint32_t group_count = sim->world_data.size / LOCAL_SIZE_X;
    if (sim->world_data.size % LOCAL_SIZE_X != 0) group_count++;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sim->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            sim->pipeline_layout, 0,
                            1, &sim->set,
                            0, 0);

    VkBufferCopy storage_copy = {0};
    storage_copy.srcOffset = 0;
    storage_copy.dstOffset = 0;
    storage_copy.size = sim->storage_size;

    // wait for pipeline to finish before copying storage[1] into storage[0]
    VkBufferMemoryBarrier pipeline_finished_mem_barrier = {0};
    pipeline_finished_mem_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    pipeline_finished_mem_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    pipeline_finished_mem_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    pipeline_finished_mem_barrier.srcQueueFamilyIndex = sim->ctx->queue_family_idx;
    pipeline_finished_mem_barrier.dstQueueFamilyIndex = sim->ctx->queue_family_idx;
    pipeline_finished_mem_barrier.buffer = sim->storage[1];
    pipeline_finished_mem_barrier.offset = 0;
    pipeline_finished_mem_barrier.size = sim->storage_size;

    // wait for copy to finish before running pipeline
    VkBufferMemoryBarrier transfer_finished_mem_barrier = {0};
    transfer_finished_mem_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    transfer_finished_mem_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transfer_finished_mem_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transfer_finished_mem_barrier.srcQueueFamilyIndex = sim->ctx->queue_family_idx;
    transfer_finished_mem_barrier.dstQueueFamilyIndex = sim->ctx->queue_family_idx;
    transfer_finished_mem_barrier.buffer = sim->storage[0];
    transfer_finished_mem_barrier.offset = 0;
    transfer_finished_mem_barrier.size = sim->storage_size;

    for (uint32_t i = 0; i < n; i++) {
        // first dispatch does not need synchronization
        if (i != 0) {
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, NULL,
                                 1, &pipeline_finished_mem_barrier,
                                 0, NULL);
        }
        vkCmdCopyBuffer(cmd, sim->storage[1], sim->storage[0], 1, &storage_copy);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, NULL,
                             1, &transfer_finished_mem_barrier,
                             0, NULL);
        vkCmdDispatch(cmd, group_count, 1, 1);
    }

    ASSERT_VKR(vkEndCommandBuffer(cmd), "Failed to end pipeline command buffer");
    SubmitAndWait(sim, cmd);
    ASSERT_VKR(vkResetCommandBuffer(cmd, 0), "Failed to reset pipeline command buffer");
}
