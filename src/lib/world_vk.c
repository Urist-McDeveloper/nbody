#include "world_vk.h"

#include "util.h"
#include "vulkan_ctx.h"

/* Compute shader work group size. */
#define LOCAL_SIZE_X 16

struct WorldComp {
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

static void SubmitAndWait(WorldComp *comp, VkCommandBuffer cmd_buf) {
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    ASSERT_VKR(vkQueueSubmit(comp->ctx->queue, 1, &submit_info, comp->fence), "Failed to submit command buffer");
    ASSERT_VKR(vkWaitForFences(comp->ctx->dev, 1, &comp->fence, VK_TRUE, UINT64_MAX), "Failed to wait for fences");
    ASSERT_VKR(vkResetFences(comp->ctx->dev, 1, &comp->fence), "Failed to reset fence");
}

void WorldComp_GetParticles(WorldComp *comp, Particle *arr) {
    // copy data from device-local buffer into host-accessible buffer
    SubmitAndWait(comp, comp->dth_storage_copy_cb[comp->new_idx]);

    void *mapped;
    ASSERT_VKR(vkMapMemory(comp->ctx->dev, comp->host_mem, 0, comp->storage_size, 0, &mapped),
               "Failed to map memory");

    // copy data from host-accessible buffer
    memcpy(arr, mapped, comp->storage_size);
    vkUnmapMemory(comp->ctx->dev, comp->host_mem);
}

void WorldComp_SetParticles(WorldComp *comp, Particle *arr) {
    void *mapped;
    ASSERT_VKR(vkMapMemory(comp->ctx->dev, comp->host_mem, 0, comp->storage_size, 0, &mapped),
               "Failed to map memory");

    // copy data into host-accessible buffer
    memcpy(mapped, arr, comp->storage_size);
    vkUnmapMemory(comp->ctx->dev, comp->host_mem);

    // copy data from host-accessible buffer into device-local buffer
    SubmitAndWait(comp, comp->htd_storage_copy_cb[comp->new_idx]);
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

WorldComp *WorldComp_Create(const VulkanCtx *ctx, WorldData data) {
    WorldComp *comp = ALLOC(1, WorldComp);
    ASSERT_MSG(comp != NULL, "Failed to alloc WorldComp");

    comp->world_data = data;
    comp->new_idx = 0;
    comp->ctx = ctx;

    /*
     * Shaders.
     */

    comp->shader = VulkanCtx_LoadShader(ctx, "shader/particle_cs.spv");

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
    *(float *)(shader_spec_data + 12) = NB_FRICTION;

    VkSpecializationInfo shader_spec_info = {0};
    shader_spec_info.mapEntryCount = 4;
    shader_spec_info.pMapEntries = shader_spec_map;
    shader_spec_info.dataSize = 16;
    shader_spec_info.pData = shader_spec_data;

    VkPipelineShaderStageCreateInfo shader_stage_info = {0};
    shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_info.module = comp->shader;
    shader_stage_info.pName = "main";
    shader_stage_info.pSpecializationInfo = &shader_spec_info;

    /*
     * Memory buffers.
     */

    const VkDeviceSize uniform_size = SIZE_OF_ALIGN_16(WorldData);
    const VkDeviceSize storage_size = data.size * sizeof(Particle);
    const VkDeviceSize dev_mem_size = uniform_size + 2 * storage_size;

    comp->uniform_size = uniform_size;
    comp->storage_size = storage_size;

    comp->host_mem = VulkanCtx_AllocMemory(ctx, storage_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    comp->dev_mem = VulkanCtx_AllocMemory(ctx, dev_mem_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkBufferUsageFlags transfer_buf_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags uniform_buf_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags storage_buf_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | transfer_buf_flags;

    comp->uniform = VulkanCtx_CreateBuffer(ctx, uniform_size, uniform_buf_flags);
    comp->storage[0] = VulkanCtx_CreateBuffer(ctx, storage_size, storage_buf_flags);
    comp->storage[1] = VulkanCtx_CreateBuffer(ctx, storage_size, storage_buf_flags);
    comp->transfer_buf = VulkanCtx_CreateBuffer(ctx, storage_size, transfer_buf_flags);

    vkBindBufferMemory(ctx->dev, comp->uniform, comp->dev_mem, 0);
    vkBindBufferMemory(ctx->dev, comp->storage[0], comp->dev_mem, uniform_size);
    vkBindBufferMemory(ctx->dev, comp->storage[1], comp->dev_mem, uniform_size + storage_size);
    vkBindBufferMemory(ctx->dev, comp->transfer_buf, comp->host_mem, 0);

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
    ASSERT_VKR(vkCreateDescriptorSetLayout(ctx->dev, &ds_layout_info, NULL, &comp->ds_layout),
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
    ASSERT_VKR(vkCreateDescriptorPool(ctx->dev, &ds_pool_info, NULL, &comp->ds_pool),
               "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo ds_alloc_info = {0};
    ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_alloc_info.descriptorPool = comp->ds_pool;
    ds_alloc_info.descriptorSetCount = 1;
    ds_alloc_info.pSetLayouts = &comp->ds_layout;

    ASSERT_VKR(vkAllocateDescriptorSets(comp->ctx->dev, &ds_alloc_info, &comp->sets[0]),
               "Failed to allocate descriptor set #0");
    ASSERT_VKR(vkAllocateDescriptorSets(comp->ctx->dev, &ds_alloc_info, &comp->sets[1]),
               "Failed to allocate descriptor set #0");

    /*
     * Update descriptor set.
     */

    VkDescriptorBufferInfo uniform_info = {0};
    uniform_info.buffer = comp->uniform;
    uniform_info.offset = 0;
    uniform_info.range = uniform_size;

    VkDescriptorBufferInfo storage0_info = {0};
    storage0_info.buffer = comp->storage[0];
    storage0_info.offset = 0;
    storage0_info.range = storage_size;

    VkDescriptorBufferInfo storage1_info = {0};
    storage1_info.buffer = comp->storage[1];
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
    CreateWriteDescriptorSet(comp->sets[0], types, set0, write_sets);
    CreateWriteDescriptorSet(comp->sets[1], types, set1, write_sets + 3);

    vkUpdateDescriptorSets(comp->ctx->dev, 6, write_sets, 0, NULL);

    /*
     * Pipeline.
     */

    VkPipelineLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &comp->ds_layout;
    ASSERT_VKR(vkCreatePipelineLayout(ctx->dev, &layout_info, NULL, &comp->pipeline_layout),
               "Failed to create pipeline layout");

    VkComputePipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = shader_stage_info;
    pipeline_info.layout = comp->pipeline_layout;
    ASSERT_VKR(vkCreateComputePipelines(ctx->dev, NULL, 1, &pipeline_info, NULL, &comp->pipeline),
               "Failed to create compute pipeline");

    /*
     * Command buffers.
     */

    VkCommandBuffer buffers[6];
    VulkanCtx_AllocCommandBuffers(ctx, 6, buffers);

    comp->pipeline_cb = buffers[0];
    comp->htd_uniform_copy_cb = buffers[1];
    comp->htd_storage_copy_cb[0] = buffers[2];
    comp->htd_storage_copy_cb[1] = buffers[3];
    comp->dth_storage_copy_cb[0] = buffers[4];
    comp->dth_storage_copy_cb[1] = buffers[5];

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
    ASSERT_VKR(vkBeginCommandBuffer(comp->htd_uniform_copy_cb, &cmd_begin_info), "Failed to begin command cmd");
    vkCmdCopyBuffer(comp->htd_uniform_copy_cb, comp->transfer_buf, comp->uniform, 1, &uniform_copy);
    ASSERT_VKR(vkEndCommandBuffer(comp->htd_uniform_copy_cb), "Failed to end command buffer");

    // host-to-dev storage copy
    for (int i = 0; i < 2; i++) {
        VkCommandBuffer cmd = comp->htd_storage_copy_cb[i];
        ASSERT_VKR(vkBeginCommandBuffer(cmd, &cmd_begin_info), "Failed to begin command buffer");
        vkCmdCopyBuffer(cmd, comp->transfer_buf, comp->storage[i], 1, &storage_copy);
        ASSERT_VKR(vkEndCommandBuffer(cmd), "Failed to end command buffer");
    }

    // dev-to-host storage copy
    for (int i = 0; i < 2; i++) {
        VkCommandBuffer cmd = comp->dth_storage_copy_cb[i];
        ASSERT_VKR(vkBeginCommandBuffer(cmd, &cmd_begin_info), "Failed to begin command cmd");
        vkCmdCopyBuffer(cmd, comp->storage[i], comp->transfer_buf, 1, &storage_copy);
        ASSERT_VKR(vkEndCommandBuffer(cmd), "Failed to end command cmd");
    }

    /*
     * Synchronization.
     */

    for (int i = 0; i < 2; i++) {
        comp->mem_barriers[i] = (VkBufferMemoryBarrier){0};
        comp->mem_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        comp->mem_barriers[i].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        comp->mem_barriers[i].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        comp->mem_barriers[i].buffer = comp->storage[i];
        comp->mem_barriers[i].offset = 0;
        comp->mem_barriers[i].size = comp->storage_size;
    }

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ASSERT_VKR(vkCreateFence(ctx->dev, &fence_info, NULL, &comp->fence), "Failed to create fence");

    return comp;
}

void WorldComp_Destroy(WorldComp *comp) {
    if (comp != NULL) {
        VkDevice dev = comp->ctx->dev;
        VkCommandPool cmd_pool = comp->ctx->cmd_pool;

        vkDestroyFence(dev, comp->fence, NULL);
        vkFreeCommandBuffers(dev, cmd_pool, 1, &comp->pipeline_cb);
        vkFreeCommandBuffers(dev, cmd_pool, 1, &comp->htd_uniform_copy_cb);
        vkFreeCommandBuffers(dev, cmd_pool, 2, comp->htd_storage_copy_cb);
        vkFreeCommandBuffers(dev, cmd_pool, 2, comp->dth_storage_copy_cb);

        vkDestroyPipeline(dev, comp->pipeline, NULL);
        vkDestroyPipelineLayout(dev, comp->pipeline_layout, NULL);

        vkDestroyDescriptorPool(dev, comp->ds_pool, NULL);
        vkDestroyDescriptorSetLayout(dev, comp->ds_layout, NULL);

        vkDestroyBuffer(dev, comp->transfer_buf, NULL);
        vkDestroyBuffer(dev, comp->storage[0], NULL);
        vkDestroyBuffer(dev, comp->storage[1], NULL);
        vkDestroyBuffer(dev, comp->uniform, NULL);
        vkFreeMemory(dev, comp->host_mem, NULL);
        vkFreeMemory(dev, comp->dev_mem, NULL);

        vkDestroyShaderModule(dev, comp->shader, NULL);
        free(comp);
    }
}

void WorldComp_DoUpdate(WorldComp *comp, float dt, const uint32_t n) {
    // update uniform buffer if DT has changed
    if (comp->world_data.dt != dt) {
        comp->world_data.dt = dt;

        void *mapped;
        ASSERT_VKR(vkMapMemory(comp->ctx->dev, comp->host_mem, 0, comp->uniform_size, 0, &mapped),
                   "Failed to map device memory");

        // copy data into host-accessible buffer
        *(WorldData *)mapped = comp->world_data;
        vkUnmapMemory(comp->ctx->dev, comp->host_mem);

        // copy data from host-accessible buffer into device-local buffer
        SubmitAndWait(comp, comp->htd_uniform_copy_cb);
    }

    VkCommandBuffer cmd_buf = comp->pipeline_cb;
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ASSERT_VKR(vkBeginCommandBuffer(cmd_buf, &begin_info), "Failed to begin pipeline command buffer");

    uint32_t new_idx = comp->new_idx;
    uint32_t group_count = comp->world_data.size / LOCAL_SIZE_X;
    if (comp->world_data.size % LOCAL_SIZE_X != 0) group_count++;

    for (uint32_t i = 0; i < n; i++) {
        // first dispatch does not need synchronization
        if (i != 0) {
            vkCmdPipelineBarrier(cmd_buf,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, NULL,
                                 1, &comp->mem_barriers[new_idx],
                                 0, NULL);
        }
        // rotate new_idx AFTER pipeline barrier
        new_idx = (new_idx + 1) % 2;

        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                comp->pipeline_layout, 0,
                                1, &comp->sets[new_idx],
                                0, 0);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp->pipeline);
        vkCmdDispatch(cmd_buf, group_count, 1, 1);
    }
    comp->new_idx = new_idx;

    ASSERT_VKR(vkEndCommandBuffer(cmd_buf), "Failed to end pipeline command buffer");
    SubmitAndWait(comp, cmd_buf);
    ASSERT_VKR(vkResetCommandBuffer(cmd_buf, 0), "Failed to reset pipeline command buffer");
}
