#include "sim_gpu.h"
#include "vulkan_ctx.h"
#include "util.h"

#include <stdbool.h>

/* Compute shader work group size. */
#define LOCAL_SIZE_X 256

struct SimPipeline {
    WorldData world_data;
    VkShaderModule shader;
    // Memory
    VulkanDeviceMemory dev_mem;     // device-local memory
    VulkanDeviceMemory host_mem;    // host-accessible memory
    VulkanBuffer uniform;           // uniform buffer in device-local memory
    VulkanBuffer storage[2];        // uniform buffer in device-local memory; [0] for old data, [1] for new
    VulkanBuffer transfer_buf[2];   // host-accessible transfer buffers; [0] for uniform, [1] for storage
    bool transfer_buf_synced;      // whether transfer_buf[1] has the same data as storage[1]
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

SimPipeline *CreateSimPipeline(WorldData data) {
    SimPipeline *sim = ALLOC(1, SimPipeline);
    ASSERT(sim != NULL, "Failed to alloc SimPipeline");

    InitGlobalVulkanContext();  // does nothing if global context was already initialized
    sim->world_data = data;
    sim->world_data.dt = 0;     // update uniform buffer when PerformSimUpdate is called

    /*
     * Shaders.
     */

    sim->shader = LoadShaderModule("shader/particle_cs.spv");

    VkSpecializationMapEntry shader_spec_map[4];
    for (int i = 0; i < 4; i++) {
        shader_spec_map[i] = (VkSpecializationMapEntry){
                .constantID = i,
                .offset = 4 * i,
                .size = 4,
        };
    }

    char shader_spec_data[16];
    *(uint32_t *)shader_spec_data = LOCAL_SIZE_X;
    *(float *)(shader_spec_data + 4) = NB_G;
    *(float *)(shader_spec_data + 8) = NB_N;
    *(float *)(shader_spec_data + 12) = NB_F;

    VkSpecializationInfo shader_spec_info = {
            .mapEntryCount = 4,
            .pMapEntries = shader_spec_map,
            .dataSize = 16,
            .pData = shader_spec_data,
    };
    VkPipelineShaderStageCreateInfo shader_stage_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = sim->shader,
            .pName = "main",
            .pSpecializationInfo = &shader_spec_info,
    };

    /*
     * Memory buffers.
     */

    const VkDeviceSize uniform_size = SIZE_OF_ALIGN_16(WorldData);
    const VkDeviceSize storage_size = data.total_len * sizeof(Particle);

    sim->host_mem = CreateHostCoherentMemory(uniform_size + storage_size);
    sim->dev_mem = CreateDeviceLocalMemory(uniform_size + 2 * storage_size);

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

    VkDescriptorSetLayoutBinding bindings[3] = {
            {       // uniform
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            {       // old
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            {       // new
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
    };
    VkDescriptorSetLayoutCreateInfo ds_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 3,
            .pBindings = bindings,
    };
    ASSERT_VK(vkCreateDescriptorSetLayout(vulkan_ctx.dev, &ds_layout_info, NULL, &sim->ds_layout),
              "Failed to create descriptor set layout");

    VkDescriptorPoolSize ds_pool_size[2] = {
            {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
            },
            {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 2,
            },
    };
    VkDescriptorPoolCreateInfo ds_pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 2,
            .pPoolSizes = ds_pool_size,
    };
    ASSERT_VK(vkCreateDescriptorPool(vulkan_ctx.dev, &ds_pool_info, NULL, &sim->ds_pool),
              "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo ds_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = sim->ds_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &sim->ds_layout,
    };
    ASSERT_VK(vkAllocateDescriptorSets(vulkan_ctx.dev, &ds_alloc_info, &sim->set),
              "Failed to allocate descriptor set");

    /*
     * Update descriptor set.
     */

    VkDescriptorBufferInfo uniform_info, storage_info[2];
    FillDescriptorBufferInfo(&sim->uniform, &uniform_info);
    FillDescriptorBufferInfo(&sim->storage[0], &storage_info[0]);
    FillDescriptorBufferInfo(&sim->storage[1], &storage_info[1]);

    VkWriteDescriptorSet write_sets[2] = {
            {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = sim->set,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &uniform_info,
            },
            {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = sim->set,
                    .dstBinding = 1,
                    .descriptorCount = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = storage_info,
            },
    };
    vkUpdateDescriptorSets(vulkan_ctx.dev, 2, write_sets, 0, NULL);

    /*
     * Pipeline.
     */

    VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &sim->ds_layout,
    };
    ASSERT_VK(vkCreatePipelineLayout(vulkan_ctx.dev, &layout_info, NULL, &sim->pipeline_layout),
              "Failed to create pipeline layout");

    VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = shader_stage_info,
            .layout = sim->pipeline_layout,
    };
    ASSERT_VK(vkCreateComputePipelines(vulkan_ctx.dev, NULL, 1, &pipeline_info, NULL, &sim->pipeline),
              "Failed to create compute pipeline");

    /*
     * Command buffers and synchronization.
     */

    AllocCommandBuffers(1, &sim->cmd);
    VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    ASSERT_VK(vkCreateFence(vulkan_ctx.dev, &fence_info, NULL, &sim->fence), "Failed to create fence");

    return sim;
}

void DestroySimPipeline(SimPipeline *sim) {
    if (sim != NULL) {
        VkDevice dev = vulkan_ctx.dev;

        vkDestroyFence(dev, sim->fence, NULL);
        vkFreeCommandBuffers(dev, vulkan_ctx.cmd_pool, 1, &sim->cmd);

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

void GetSimulationData(const SimPipeline *sim, Particle *ps) {
    CopyFromVulkanBuffer(&sim->transfer_buf[1], ps);
}

void SetSimulationData(SimPipeline *sim, const Particle *ps) {
    CopyIntoVulkanBuffer(&sim->transfer_buf[1], ps);
    sim->transfer_buf_synced = false;
}

void PerformSimUpdate(SimPipeline *sim, uint32_t n, float dt) {
    ASSERT(n > 0, "Performing 0 GPU simulation updates is not allowed");

    // start recording command buffer
    VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    ASSERT_VK(vkBeginCommandBuffer(sim->cmd, &begin_info), "Failed to begin pipeline command buffer");

    // update uniform buffer if DATA has changed
    if (sim->world_data.dt != dt) {
        sim->world_data.dt = dt;

        CopyIntoVulkanBuffer(&sim->transfer_buf[0], &sim->world_data);
        CopyVulkanBuffer(sim->cmd, &sim->transfer_buf[0], &sim->uniform);

        // pipeline should wait until copy command is finished
        VkBufferMemoryBarrier uniform_copy_barrier;
        FillWriteReadBufferBarrier(&sim->uniform, &uniform_copy_barrier);

        vkCmdPipelineBarrier(sim->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, NULL,
                             1, &uniform_copy_barrier,
                             0, NULL);
    }

    // copy latest data into storage[0]
    if (sim->transfer_buf_synced) {
        // transfer_buf[1] is identical to storage[1]
        CopyVulkanBuffer(sim->cmd, &sim->storage[1], &sim->storage[0]);
    } else {
        // transfer_buf[1] was modified externally
        CopyVulkanBuffer(sim->cmd, &sim->transfer_buf[1], &sim->storage[0]);
    }

    // wait for pipeline to finish before copying storage[1] into storage[0]
    VkBufferMemoryBarrier pipeline_barrier;
    FillWriteReadBufferBarrier(&sim->storage[1], &pipeline_barrier);

    // wait for copy command to finish before running pipeline
    VkBufferMemoryBarrier transfer_barrier;
    FillWriteReadBufferBarrier(&sim->storage[0], &transfer_barrier);

    // bind pipeline and descriptor set
    uint32_t group_count = sim->world_data.total_len / LOCAL_SIZE_X;
    if (sim->world_data.total_len % LOCAL_SIZE_X != 0) group_count++;

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
    ASSERT_VK(vkEndCommandBuffer(sim->cmd), "Failed to end pipeline command buffer");

    // submit command buffer
    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &sim->cmd,
    };
    ASSERT_VK(vkQueueSubmit(vulkan_ctx.queue, 1, &submit_info, sim->fence), "Failed to submit command buffer");
    ASSERT_VK(vkWaitForFences(vulkan_ctx.dev, 1, &sim->fence, VK_TRUE, UINT64_MAX), "Failed to wait for fences");

    // reset fence and command buffer
    ASSERT_VK(vkResetFences(vulkan_ctx.dev, 1, &sim->fence), "Failed to reset fence");
    ASSERT_VK(vkResetCommandBuffer(sim->cmd, 0), "Failed to reset command buffer");

    // storage[1] was copied to transfer_buf[1]
    sim->transfer_buf_synced = true;
}
