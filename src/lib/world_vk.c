#include "world_vk.h"

#include "util.h"
#include "vulkan_ctx.h"

/* Compute shader work group size. */
#define LOCAL_SIZE_X 16

struct SimPipeline {
    WorldData world_data;
    uint32_t new_idx;                       // which storage buffer is "new"
    const VulkanCtx *ctx;
    VkShaderModule shader;
    // Memory
    VkDeviceMemory dev_mem;                 // device-local memory
    VkDeviceMemory host_mem;                // host-accessible memory
    VkBuffer uniform;
    VkBuffer storage[2];                    // one for new data, one for old
    VkBuffer transfer_buf;                  // buffer from host-accessible memory
    VkDeviceSize uniform_size;
    VkDeviceSize storage_size;
    // Descriptor
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkDescriptorSet sets[2];                // for new_idx of 0 and 1
    // Pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    // Commands
    VkCommandBuffer pipeline_cb;            // transient buffer for pipeline dispatch
    VkCommandBuffer htd_uniform_copy_cb;    // transfer_buf to uniform
    VkCommandBuffer htd_storage_copy_cb[2]; // transfer_buf to storage[new_idx]
    VkCommandBuffer dth_storage_copy_cb[2]; // storage[new_idx] to transfer_buf
    // Sync
    VkBufferMemoryBarrier mem_barriers[2];  // between write and read of storage[new_idx]
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
    SubmitAndWait(sim, sim->dth_storage_copy_cb[sim->new_idx]);

    void *mapped;
    ASSERT_VKR(vkMapMemory(sim->ctx->dev, sim->host_mem, 0, sim->storage_size, 0, &mapped),
               "Failed to map memory");

    // copy data from host-accessible buffer
    memcpy(arr, mapped, sim->storage_size);
    vkUnmapMemory(sim->ctx->dev, sim->host_mem);
}

void SetSimParticles(SimPipeline *sim, Particle *arr) {
    void *mapped;
    ASSERT_VKR(vkMapMemory(sim->ctx->dev, sim->host_mem, 0, sim->storage_size, 0, &mapped),
               "Failed to map memory");

    // copy data into host-accessible buffer
    memcpy(mapped, arr, sim->storage_size);
    vkUnmapMemory(sim->ctx->dev, sim->host_mem);

    // copy data from host-accessible buffer into device-local buffer
    SubmitAndWait(sim, sim->htd_storage_copy_cb[sim->new_idx]);
}

static void CreateWriteDescriptorSet(VkDescriptorSet set,
                                     const VkDescriptorType *types,
                                     const VkDescriptorBufferInfo *info,
                                     VkWriteDescriptorSet *writes) {
    for (int i = 0; i < 3; i++) {
        writes[i] = (VkWriteDescriptorSet){0};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = types[i];
        writes[i].pBufferInfo = &info[i];
    }
}

SimPipeline *CreateSimPipeline(const VulkanCtx *ctx, WorldData data) {
    SimPipeline *sim = ALLOC(1, SimPipeline);
    ASSERT_MSG(sim != NULL, "Failed to alloc SimPipeline");

    sim->world_data = data;
    sim->new_idx = 0;
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
    ds_pool_size[0].descriptorCount = 2;
    ds_pool_size[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ds_pool_size[1].descriptorCount = 4;

    VkDescriptorPoolCreateInfo ds_pool_info = {0};
    ds_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ds_pool_info.maxSets = 2;
    ds_pool_info.poolSizeCount = 2;
    ds_pool_info.pPoolSizes = ds_pool_size;
    ASSERT_VKR(vkCreateDescriptorPool(ctx->dev, &ds_pool_info, NULL, &sim->ds_pool),
               "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo ds_alloc_info = {0};
    ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_alloc_info.descriptorPool = sim->ds_pool;
    ds_alloc_info.descriptorSetCount = 1;
    ds_alloc_info.pSetLayouts = &sim->ds_layout;

    ASSERT_VKR(vkAllocateDescriptorSets(sim->ctx->dev, &ds_alloc_info, &sim->sets[0]),
               "Failed to allocate descriptor set #0");
    ASSERT_VKR(vkAllocateDescriptorSets(sim->ctx->dev, &ds_alloc_info, &sim->sets[1]),
               "Failed to allocate descriptor set #0");

    /*
     * Update descriptor set.
     */

    VkDescriptorBufferInfo uniform_info = {0};
    uniform_info.buffer = sim->uniform;
    uniform_info.offset = 0;
    uniform_info.range = uniform_size;

    VkDescriptorBufferInfo storage0_info = {0};
    storage0_info.buffer = sim->storage[0];
    storage0_info.offset = 0;
    storage0_info.range = storage_size;

    VkDescriptorBufferInfo storage1_info = {0};
    storage1_info.buffer = sim->storage[1];
    storage1_info.offset = 0;
    storage1_info.range = storage_size;

    VkDescriptorType types[3] = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // uniform
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // old
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // new
    };
    VkDescriptorBufferInfo set0[3] = {
            uniform_info,   // uniform
            storage1_info,  // old
            storage0_info,  // new
    };
    VkDescriptorBufferInfo set1[3] = {
            uniform_info,   // uniform
            storage0_info,  // old
            storage1_info,  // new
    };

    VkWriteDescriptorSet write_sets[6];
    CreateWriteDescriptorSet(sim->sets[0], types, set0, write_sets);
    CreateWriteDescriptorSet(sim->sets[1], types, set1, write_sets + 3);

    vkUpdateDescriptorSets(sim->ctx->dev, 6, write_sets, 0, NULL);

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

    VkCommandBuffer buffers[6];
    AllocVkCommandBuffers(ctx, 6, buffers);

    sim->pipeline_cb = buffers[0];
    sim->htd_uniform_copy_cb = buffers[1];
    sim->htd_storage_copy_cb[0] = buffers[2];
    sim->htd_storage_copy_cb[1] = buffers[3];
    sim->dth_storage_copy_cb[0] = buffers[4];
    sim->dth_storage_copy_cb[1] = buffers[5];

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
    for (int i = 0; i < 2; i++) {
        VkCommandBuffer cmd = sim->htd_storage_copy_cb[i];
        ASSERT_VKR(vkBeginCommandBuffer(cmd, &cmd_begin_info), "Failed to begin command buffer");
        vkCmdCopyBuffer(cmd, sim->transfer_buf, sim->storage[i], 1, &storage_copy);
        ASSERT_VKR(vkEndCommandBuffer(cmd), "Failed to end command buffer");
    }

    // dev-to-host storage copy
    for (int i = 0; i < 2; i++) {
        VkCommandBuffer cmd = sim->dth_storage_copy_cb[i];
        ASSERT_VKR(vkBeginCommandBuffer(cmd, &cmd_begin_info), "Failed to begin command cmd");
        vkCmdCopyBuffer(cmd, sim->storage[i], sim->transfer_buf, 1, &storage_copy);
        ASSERT_VKR(vkEndCommandBuffer(cmd), "Failed to end command cmd");
    }

    /*
     * Synchronization.
     */

    for (int i = 0; i < 2; i++) {
        sim->mem_barriers[i] = (VkBufferMemoryBarrier){0};
        sim->mem_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        sim->mem_barriers[i].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        sim->mem_barriers[i].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        sim->mem_barriers[i].buffer = sim->storage[i];
        sim->mem_barriers[i].offset = 0;
        sim->mem_barriers[i].size = sim->storage_size;
    }

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ASSERT_VKR(vkCreateFence(ctx->dev, &fence_info, NULL, &sim->fence), "Failed to create fence");

    return sim;
}

void DestroySimPipeline(SimPipeline *sim) {
    if (sim != NULL) {
        VkDevice dev = sim->ctx->dev;
        VkCommandPool cmd_pool = sim->ctx->cmd_pool;

        vkDestroyFence(dev, sim->fence, NULL);
        vkFreeCommandBuffers(dev, cmd_pool, 1, &sim->pipeline_cb);
        vkFreeCommandBuffers(dev, cmd_pool, 1, &sim->htd_uniform_copy_cb);
        vkFreeCommandBuffers(dev, cmd_pool, 2, sim->htd_storage_copy_cb);
        vkFreeCommandBuffers(dev, cmd_pool, 2, sim->dth_storage_copy_cb);

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

    VkCommandBuffer cmd_buf = sim->pipeline_cb;
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ASSERT_VKR(vkBeginCommandBuffer(cmd_buf, &begin_info), "Failed to begin pipeline command buffer");

    uint32_t new_idx = sim->new_idx;
    uint32_t group_count = sim->world_data.size / LOCAL_SIZE_X;
    if (sim->world_data.size % LOCAL_SIZE_X != 0) group_count++;

    for (uint32_t i = 0; i < n; i++) {
        // first dispatch does not need synchronization
        if (i != 0) {
            vkCmdPipelineBarrier(cmd_buf,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, NULL,
                                 1, &sim->mem_barriers[new_idx],
                                 0, NULL);
        }
        // rotate new_idx AFTER pipeline barrier
        new_idx = (new_idx + 1) % 2;

        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sim->pipeline_layout, 0,
                                1, &sim->sets[new_idx],
                                0, 0);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, sim->pipeline);
        vkCmdDispatch(cmd_buf, group_count, 1, 1);
    }
    sim->new_idx = new_idx;

    ASSERT_VKR(vkEndCommandBuffer(cmd_buf), "Failed to end pipeline command buffer");
    SubmitAndWait(sim, cmd_buf);
    ASSERT_VKR(vkResetCommandBuffer(cmd_buf, 0), "Failed to reset pipeline command buffer");
}
