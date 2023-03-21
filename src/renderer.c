#include "renderer.h"
#include "lib/util.h"

#include <nbody.h>
#include <galaxy.h>

#include "shader/particle.vs.h"
#include "shader/particle.fs.h"

#define SAMPLE_COUNT    VK_SAMPLE_COUNT_1_BIT

/*
 * Utilities.
 */

static const char *FormatToStr(VkFormat format);

static const char *ColorSpaceToStr(VkColorSpaceKHR color_space);

static const char *PresentModeToStr(VkPresentModeKHR mode);

static VkSurfaceFormatKHR PickSurfaceFormat(const VkSurfaceFormatKHR *formats, uint32_t count) {
    uint32_t idx = 0;
    printf("\t- Selecting surface formats\n");

    for (uint32_t i = 0; i < count; i++) {
        VkSurfaceFormatKHR f = formats[i];
        printf("\t\t- #%u: format = %s, space = %s\n",
               i, FormatToStr(f.format), ColorSpaceToStr(f.colorSpace));

        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            idx = i;
        }
    }
    printf("\t\t- Using format #%u\n", idx);
    return formats[idx];
}

static VkPresentModeKHR PickPresentMode(const VkPresentModeKHR *modes, uint32_t count) {
    uint32_t idx = 0;
    printf("\t- Selecting present modes\n");
    for (uint32_t i = 0; i < count; i++) {
        printf("\t\t- #%u: %s\n", i, PresentModeToStr(modes[i]));

        // use mailbox if available
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            idx = i;
        }
        // otherwise use FIFO
        if (idx == 0 && modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
            idx = i;
        }
    }
    printf("\t\t- Using mode #%u\n", idx);
    return modes[idx];
}

static void PickFormatAndMode(VkSurfaceKHR surface, VkSurfaceFormatKHR *format, VkPresentModeKHR *mode) {
    uint32_t format_count, mode_count;
    ASSERT_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_ctx.pdev, surface, &format_count, NULL),
              "Failed to get surface formats");
    ASSERT_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_ctx.pdev, surface, &mode_count, NULL),
              "Failed to get present modes");
    ASSERT(format_count > 0, "Surface supports 0 formats");
    ASSERT(mode_count > 0, "Surface supports 0 modes");

    VkSurfaceFormatKHR *formats = ALLOC(format_count, VkSurfaceFormatKHR);
    VkPresentModeKHR *modes = ALLOC(mode_count, VkPresentModeKHR);
    ASSERT(formats != NULL && modes != NULL,
           "Failed to alloc %u VkSurfaceFormatKHR and %u VkPresentModeKHR",
           format_count, mode_count);
    ASSERT_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_ctx.pdev, surface, &format_count, formats),
              "Failed to get surface formats");
    ASSERT_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_ctx.pdev, surface, &mode_count, modes),
              "Failed to get present modes");

    *format = PickSurfaceFormat(formats, format_count);
    *mode = PickPresentMode(modes, mode_count);

    free(formats);
    free(modes);
}

static VkExtent2D GetImageExtent(GLFWwindow *window, VkSurfaceCapabilitiesKHR cap) {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    VkExtent2D result = {.width = (uint32_t)w, .height = (uint32_t)h};
    if (result.width < cap.minImageExtent.width) {
        result.width = cap.minImageExtent.width;
    }
    if (result.width > cap.maxImageExtent.width) {
        result.width = cap.maxImageExtent.width;
    }
    if (result.height < cap.minImageExtent.height) {
        result.height = cap.minImageExtent.height;
    }
    if (result.height > cap.maxImageExtent.height) {
        result.height = cap.maxImageExtent.height;
    }
    printf("\t- Extent is %ux%u\n", result.width, result.height);
    return result;
}

/*
 * Implementation.
 */

struct Renderer {
    GLFWwindow *window;
    VkShaderModule vert, frag;
    VkRenderPass render_pass;
    // swapchain
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    uint32_t image_count;
    uint32_t image_index;
    VkImage *images;
    VkImageView *views;
    VkFramebuffer *framebuffers;
    // descriptors
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkDescriptorSet ds_set;
    // pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    // commands and synchronization
    VkCommandBuffer cmd;
    VkFence fence;
    VkSemaphore cmd_sem;
    VkSemaphore swapchain_sem;
};

typedef struct {
    float camera_proj[3][2];
    float zoom;
    float dt;
} PushConstants;

static void SetupFramebuffers(Renderer *r) {
    r->framebuffers = REALLOC(r->framebuffers, r->image_count, VkFramebuffer);
    ASSERT(r->framebuffers != NULL, "Failed to realloc %u VkFramebuffer", r->image_count);

    for (uint32_t i = 0; i < r->image_count; i++) {
        VkFramebufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = r->render_pass,
                .attachmentCount = 1,
                .pAttachments = &r->views[i],
                .width = r->extent.width,
                .height = r->extent.height,
                .layers = 1,
        };
        ASSERT_VK(vkCreateFramebuffer(vk_ctx.dev, &buffer_info, NULL, &r->framebuffers[i]),
                  "Failed to create framebuffer #%u", i);
    }
}

Renderer *CreateRenderer(GLFWwindow *window, const VulkanBuffer *particle_data) {
    Renderer *r = ALLOC(1, Renderer);
    ASSERT(r != NULL, "Failed to alloc Renderer");

    *r = (Renderer){.window = window};
    ASSERT_VK(glfwCreateWindowSurface(vk_ctx.instance, r->window, NULL, &r->surface),
              "Failed to create Vulkan surface");

    /*
     * Swapchain.
     */

    RecreateSwapchain(r);

    /*
     * Render pass.
     */

    VkAttachmentDescription color_attachment = {
            .format = r->format.format,
            .samples = SAMPLE_COUNT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_attachment_ref = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_ref,
    };
    VkRenderPassCreateInfo render_pass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
    };
    ASSERT_VK(vkCreateRenderPass(vk_ctx.dev, &render_pass_info, NULL, &r->render_pass), "Failed to create render pass");
    SetupFramebuffers(r);

    /*
     * Shaders.
     */

    VkShaderModuleCreateInfo vert_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sizeof(particle_vs_spv),
            .pCode = (uint32_t *)particle_vs_spv,
    };
    VkShaderModuleCreateInfo frag_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sizeof(particle_fs_spv),
            .pCode = (uint32_t *)particle_fs_spv,
    };
    ASSERT_VK(vkCreateShaderModule(vk_ctx.dev, &vert_info, NULL, &r->vert), "Failed to create vertex shader module");
    ASSERT_VK(vkCreateShaderModule(vk_ctx.dev, &frag_info, NULL, &r->frag), "Failed to create vertex shader module");

    VkSpecializationMapEntry spec_map_entry = {
            .constantID = 1,
            .offset = 0,
            .size = sizeof(float),
    };
    float spec_const = MIN_GC_MASS;
    VkSpecializationInfo spec_info = {
            .mapEntryCount = 1,
            .pMapEntries = &spec_map_entry,
            .dataSize = sizeof(float),
            .pData = &spec_const,
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = r->vert,
                    .pName = "main",
                    .pSpecializationInfo = &spec_info,
            },
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = r->frag,
                    .pName = "main",
            },
    };

    /*
     * Descriptors.
     */

    VkDescriptorSetLayoutBinding data_binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayoutCreateInfo ds_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &data_binding,
    };
    ASSERT_VK(vkCreateDescriptorSetLayout(vk_ctx.dev, &ds_layout_info, NULL, &r->ds_layout),
              "Failed to create descriptor set layout");

    VkDescriptorPoolSize ds_pool_size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo ds_pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &ds_pool_size,
    };
    ASSERT_VK(vkCreateDescriptorPool(vk_ctx.dev, &ds_pool_info, NULL, &r->ds_pool),
              "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo ds_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = r->ds_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &r->ds_layout,
    };
    ASSERT_VK(vkAllocateDescriptorSets(vk_ctx.dev, &ds_alloc_info, &r->ds_set),
              "Failed to allocate descriptor set");

    VkDescriptorBufferInfo data_info;
    FillDescriptorBufferInfo(particle_data, &data_info);

    VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = r->ds_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &data_info,
    };
    vkUpdateDescriptorSets(vk_ctx.dev, 1, &write_set, 0, NULL);

    /*
     * Pipeline.
     */

    VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(PushConstants),
    };
    VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &r->ds_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range,
    };
    ASSERT_VK(vkCreatePipelineLayout(vk_ctx.dev, &layout_info, NULL, &r->pipeline_layout),
              "Failed to create pipeline layout");

    VkDynamicState dynamic_state[2] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamic_state,
    };
    VkPipelineViewportStateCreateInfo viewport_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
    };

    VkPipelineVertexInputStateCreateInfo input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo assembly_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    };
    VkPipelineRasterizationStateCreateInfo raster_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.f,
    };
    VkPipelineMultisampleStateCreateInfo multisample_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = SAMPLE_COUNT,
    };
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                              VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blend_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment,
    };
    VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shader_stages,
            .pVertexInputState = &input_info,
            .pInputAssemblyState = &assembly_info,
            .pViewportState = &viewport_info,
            .pRasterizationState = &raster_info,
            .pMultisampleState = &multisample_info,
            .pColorBlendState = &color_blend_info,
            .pDynamicState = &dynamic_info,
            .layout = r->pipeline_layout,
            .renderPass = r->render_pass,
            .subpass = 0,
    };
    ASSERT_VK(vkCreateGraphicsPipelines(vk_ctx.dev, NULL, 1, &pipeline_info, NULL, &r->pipeline),
              "Failed to create graphics pipeline");

    /*
     * Commands and synchronization.
     */

    AllocCommandBuffers(1, &r->cmd);
    VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    ASSERT_VK(vkCreateFence(vk_ctx.dev, &fence_info, NULL, &r->fence), "Failed to create fence");

    VkSemaphoreCreateInfo sem_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    ASSERT_VK(vkCreateSemaphore(vk_ctx.dev, &sem_info, NULL, &r->cmd_sem), "Failed to create semaphore");
    ASSERT_VK(vkCreateSemaphore(vk_ctx.dev, &sem_info, NULL, &r->swapchain_sem), "Failed to create semaphore");

    return r;
}

void DestroyRenderer(Renderer *r) {
    // wait for previous frame to finish
    ASSERT_VK(vkWaitForFences(vk_ctx.dev, 1, &r->fence, VK_TRUE, UINT64_MAX), "Failed to wait for fences");

    if (r != NULL) {
        FreeCommandBuffers(1, &r->cmd);
        vkDestroySemaphore(vk_ctx.dev, r->swapchain_sem, NULL);
        vkDestroySemaphore(vk_ctx.dev, r->cmd_sem, NULL);
        vkDestroyFence(vk_ctx.dev, r->fence, NULL);

        vkDestroyPipeline(vk_ctx.dev, r->pipeline, NULL);
        vkDestroyPipelineLayout(vk_ctx.dev, r->pipeline_layout, NULL);

        vkDestroyShaderModule(vk_ctx.dev, r->frag, NULL);
        vkDestroyShaderModule(vk_ctx.dev, r->vert, NULL);
        vkDestroyRenderPass(vk_ctx.dev, r->render_pass, NULL);

        for (uint32_t i = 0; i < r->image_count; i++) {
            vkDestroyFramebuffer(vk_ctx.dev, r->framebuffers[i], NULL);
            vkDestroyImageView(vk_ctx.dev, r->views[i], NULL);
        }
        free(r->framebuffers);
        free(r->views);
        free(r->images);
        vkDestroySwapchainKHR(vk_ctx.dev, r->swapchain, NULL);
        vkDestroySurfaceKHR(vk_ctx.instance, r->surface, NULL);

        free(r);
    }
}

void RecreateSwapchain(Renderer *r) {
    printf(r->swapchain == NULL ? "Creating swapchain\n" : "Recreating swapchain\n");

    VkSurfaceCapabilitiesKHR cap;
    ASSERT_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_ctx.pdev, r->surface, &cap),
              "Failed to get surface capabilities");

    VkPresentModeKHR mode;
    PickFormatAndMode(r->surface, &r->format, &mode);

    uint32_t image_count = 3;
    if (cap.minImageCount > 0 && image_count < cap.minImageCount) {
        image_count = cap.minImageCount + 1;
    }
    if (cap.maxImageCount > 0 && image_count > cap.maxImageCount) {
        image_count = cap.maxImageCount;
    }
    printf("\t- Using %u images\n", image_count);

    r->extent = GetImageExtent(r->window, cap);
    VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = r->surface,
            .minImageCount = image_count,
            .imageFormat = r->format.format,
            .imageColorSpace = r->format.colorSpace,
            .imageExtent = r->extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = cap.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = mode,
            .clipped = VK_TRUE,
            .oldSwapchain = r->swapchain,
    };
    ASSERT_VK(vkCreateSwapchainKHR(vk_ctx.dev, &swapchain_info, NULL, &r->swapchain),
              "Failed to create swapchain");
    ASSERT_VK(vkGetSwapchainImagesKHR(vk_ctx.dev, r->swapchain, &r->image_count, NULL),
              "Failed to get swapchain images");

    r->images = REALLOC(r->images, r->image_count, VkImage);
    ASSERT(r->images != NULL, "Failed to realloc %u VkImage", r->image_count);

    ASSERT_VK(vkGetSwapchainImagesKHR(vk_ctx.dev, r->swapchain, &r->image_count, r->images),
              "Failed to get swapchain images");

    r->views = REALLOC(r->views, r->image_count, VkImageView);
    ASSERT(r->views != NULL, "Failed to realloc %u VkImageView", r->image_count);

    for (uint32_t i = 0; i < r->image_count; i++) {
        VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = r->images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = r->format.format,
                .components = {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },

        };
        ASSERT_VK(vkCreateImageView(vk_ctx.dev, &view_info, NULL, &r->views[i]),
                  "Failed to create image view #%u", i);
    }

    if (r->framebuffers != NULL) {
        SetupFramebuffers(r);
    }
}

void Draw(Renderer *r, Camera cam, float dt, uint32_t particle_count, VkSemaphore wait, VkSemaphore signal) {
    // wait for previous frame to finish
    ASSERT_VK(vkWaitForFences(vk_ctx.dev, 1, &r->fence, VK_TRUE, UINT64_MAX), "Failed to wait for fences");
    ASSERT_VK(vkResetFences(vk_ctx.dev, 1, &r->fence), "Failed to reset fence");
    ASSERT_VK(vkResetCommandBuffer(r->cmd, 0), "Failed to reset command buffer");

    vkAcquireNextImageKHR(vk_ctx.dev, r->swapchain, UINT64_MAX, r->swapchain_sem, NULL, &r->image_index);

    // start recording command buffer
    VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    ASSERT_VK(vkBeginCommandBuffer(r->cmd, &begin_info), "Failed to begin pipeline command buffer");

    // begin render pass
    VkClearValue clear_color = {{{0.01f, 0.01f, 0.01f, 1.f}}};
    VkRenderPassBeginInfo pass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = r->render_pass,
            .framebuffer = r->framebuffers[r->image_index],
            .renderArea = {
                    .offset = {0, 0},
                    .extent = r->extent,
            },
            .clearValueCount = 1,
            .pClearValues = &clear_color,
    };
    vkCmdBeginRenderPass(r->cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // bind pipeline and descriptor set
    vkCmdBindDescriptorSets(r->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layout, 0,
                            1, &r->ds_set,
                            0, 0);
    vkCmdBindPipeline(r->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipeline);

    // set dynamic data
    VkViewport viewport = {
            .width = (float)r->extent.width,
            .height = (float)r->extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
    };
    VkRect2D scissors = {
            .offset = {0, 0},
            .extent = r->extent,
    };
    vkCmdSetViewport(r->cmd, 0, 1, &viewport);
    vkCmdSetScissor(r->cmd, 0, 1, &scissors);

    // set camera matrix as a push constant
    Vec2 div = MakeVec2(2.f / cam.dims.x,
                        2.f / cam.dims.y);
    Vec2 div_z = ScaleVec2(div, cam.zoom);

    Vec2 screen_v = SubVec2(
            cam.offset,
            ScaleVec2(
                    AddVec2(cam.target, cam.offset),
                    1.f * cam.zoom
            )
    );
    Vec2 vk_v = AddVec2(
            MakeVec2(screen_v.x * div.x,
                     screen_v.y * div.y),
            MakeVec2(-1.f, -1.f)
    );

    PushConstants push = {
            .camera_proj = {
                    {div_z.x, 0.f,},
                    {0.f,     div_z.y},
                    {vk_v.x,  vk_v.y}
            },
            .zoom = cam.zoom,
            .dt = dt,
    };
    vkCmdPushConstants(r->cmd, r->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

    // draw stuff
    vkCmdDraw(r->cmd, particle_count, 1, 0, 0);

    // finish recording command buffer
    vkCmdEndRenderPass(r->cmd);
    ASSERT_VK(vkEndCommandBuffer(r->cmd), "Failed to end pipeline command buffer");

    // submit command buffer
    VkSemaphore wait_semaphores[] = {
            r->swapchain_sem,
            wait,
    };
    VkPipelineStageFlags wait_stages[] = {
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    };
    VkSemaphore signal_semaphores[] = {
            r->cmd_sem,
            signal,
    };
    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = wait == NULL ? 1 : 2,
            .pWaitSemaphores = wait_semaphores,
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &r->cmd,
            .signalSemaphoreCount = signal == NULL ? 1 : 2,
            .pSignalSemaphores = signal_semaphores,
    };
    ASSERT_VK(vkQueueSubmit(vk_ctx.queue, 1, &submit_info, r->fence), "Failed to submit command buffer");

    VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &r->cmd_sem,
            .swapchainCount = 1,
            .pSwapchains = &r->swapchain,
            .pImageIndices = &r->image_index,
    };
    ASSERT_VK(vkQueuePresentKHR(vk_ctx.queue, &present_info), "Failed to preset");
}

/*
 * Autogenerated enum-to-string converters.
 */

static const char *FormatToStr(VkFormat format) {
    switch (format) {
        case VK_FORMAT_UNDEFINED:
            return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R4G4_UNORM_PACK8:
            return "VK_FORMAT_R4G4_UNORM_PACK8";
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
            return "VK_FORMAT_R5G6B5_UNORM_PACK16";
        case VK_FORMAT_B5G6R5_UNORM_PACK16:
            return "VK_FORMAT_B5G6R5_UNORM_PACK16";
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
            return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
            return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
        case VK_FORMAT_R8_UNORM:
            return "VK_FORMAT_R8_UNORM";
        case VK_FORMAT_R8_SNORM:
            return "VK_FORMAT_R8_SNORM";
        case VK_FORMAT_R8_USCALED:
            return "VK_FORMAT_R8_USCALED";
        case VK_FORMAT_R8_SSCALED:
            return "VK_FORMAT_R8_SSCALED";
        case VK_FORMAT_R8_UINT:
            return "VK_FORMAT_R8_UINT";
        case VK_FORMAT_R8_SINT:
            return "VK_FORMAT_R8_SINT";
        case VK_FORMAT_R8_SRGB:
            return "VK_FORMAT_R8_SRGB";
        case VK_FORMAT_R8G8_UNORM:
            return "VK_FORMAT_R8G8_UNORM";
        case VK_FORMAT_R8G8_SNORM:
            return "VK_FORMAT_R8G8_SNORM";
        case VK_FORMAT_R8G8_USCALED:
            return "VK_FORMAT_R8G8_USCALED";
        case VK_FORMAT_R8G8_SSCALED:
            return "VK_FORMAT_R8G8_SSCALED";
        case VK_FORMAT_R8G8_UINT:
            return "VK_FORMAT_R8G8_UINT";
        case VK_FORMAT_R8G8_SINT:
            return "VK_FORMAT_R8G8_SINT";
        case VK_FORMAT_R8G8_SRGB:
            return "VK_FORMAT_R8G8_SRGB";
        case VK_FORMAT_R8G8B8_UNORM:
            return "VK_FORMAT_R8G8B8_UNORM";
        case VK_FORMAT_R8G8B8_SNORM:
            return "VK_FORMAT_R8G8B8_SNORM";
        case VK_FORMAT_R8G8B8_USCALED:
            return "VK_FORMAT_R8G8B8_USCALED";
        case VK_FORMAT_R8G8B8_SSCALED:
            return "VK_FORMAT_R8G8B8_SSCALED";
        case VK_FORMAT_R8G8B8_UINT:
            return "VK_FORMAT_R8G8B8_UINT";
        case VK_FORMAT_R8G8B8_SINT:
            return "VK_FORMAT_R8G8B8_SINT";
        case VK_FORMAT_R8G8B8_SRGB:
            return "VK_FORMAT_R8G8B8_SRGB";
        case VK_FORMAT_B8G8R8_UNORM:
            return "VK_FORMAT_B8G8R8_UNORM";
        case VK_FORMAT_B8G8R8_SNORM:
            return "VK_FORMAT_B8G8R8_SNORM";
        case VK_FORMAT_B8G8R8_USCALED:
            return "VK_FORMAT_B8G8R8_USCALED";
        case VK_FORMAT_B8G8R8_SSCALED:
            return "VK_FORMAT_B8G8R8_SSCALED";
        case VK_FORMAT_B8G8R8_UINT:
            return "VK_FORMAT_B8G8R8_UINT";
        case VK_FORMAT_B8G8R8_SINT:
            return "VK_FORMAT_B8G8R8_SINT";
        case VK_FORMAT_B8G8R8_SRGB:
            return "VK_FORMAT_B8G8R8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM:
            return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SNORM:
            return "VK_FORMAT_R8G8B8A8_SNORM";
        case VK_FORMAT_R8G8B8A8_USCALED:
            return "VK_FORMAT_R8G8B8A8_USCALED";
        case VK_FORMAT_R8G8B8A8_SSCALED:
            return "VK_FORMAT_R8G8B8A8_SSCALED";
        case VK_FORMAT_R8G8B8A8_UINT:
            return "VK_FORMAT_R8G8B8A8_UINT";
        case VK_FORMAT_R8G8B8A8_SINT:
            return "VK_FORMAT_R8G8B8A8_SINT";
        case VK_FORMAT_R8G8B8A8_SRGB:
            return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM:
            return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SNORM:
            return "VK_FORMAT_B8G8R8A8_SNORM";
        case VK_FORMAT_B8G8R8A8_USCALED:
            return "VK_FORMAT_B8G8R8A8_USCALED";
        case VK_FORMAT_B8G8R8A8_SSCALED:
            return "VK_FORMAT_B8G8R8A8_SSCALED";
        case VK_FORMAT_B8G8R8A8_UINT:
            return "VK_FORMAT_B8G8R8A8_UINT";
        case VK_FORMAT_B8G8R8A8_SINT:
            return "VK_FORMAT_B8G8R8A8_SINT";
        case VK_FORMAT_B8G8R8A8_SRGB:
            return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
            return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
            return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
        case VK_FORMAT_R16_UNORM:
            return "VK_FORMAT_R16_UNORM";
        case VK_FORMAT_R16_SNORM:
            return "VK_FORMAT_R16_SNORM";
        case VK_FORMAT_R16_USCALED:
            return "VK_FORMAT_R16_USCALED";
        case VK_FORMAT_R16_SSCALED:
            return "VK_FORMAT_R16_SSCALED";
        case VK_FORMAT_R16_UINT:
            return "VK_FORMAT_R16_UINT";
        case VK_FORMAT_R16_SINT:
            return "VK_FORMAT_R16_SINT";
        case VK_FORMAT_R16_SFLOAT:
            return "VK_FORMAT_R16_SFLOAT";
        case VK_FORMAT_R16G16_UNORM:
            return "VK_FORMAT_R16G16_UNORM";
        case VK_FORMAT_R16G16_SNORM:
            return "VK_FORMAT_R16G16_SNORM";
        case VK_FORMAT_R16G16_USCALED:
            return "VK_FORMAT_R16G16_USCALED";
        case VK_FORMAT_R16G16_SSCALED:
            return "VK_FORMAT_R16G16_SSCALED";
        case VK_FORMAT_R16G16_UINT:
            return "VK_FORMAT_R16G16_UINT";
        case VK_FORMAT_R16G16_SINT:
            return "VK_FORMAT_R16G16_SINT";
        case VK_FORMAT_R16G16_SFLOAT:
            return "VK_FORMAT_R16G16_SFLOAT";
        case VK_FORMAT_R16G16B16_UNORM:
            return "VK_FORMAT_R16G16B16_UNORM";
        case VK_FORMAT_R16G16B16_SNORM:
            return "VK_FORMAT_R16G16B16_SNORM";
        case VK_FORMAT_R16G16B16_USCALED:
            return "VK_FORMAT_R16G16B16_USCALED";
        case VK_FORMAT_R16G16B16_SSCALED:
            return "VK_FORMAT_R16G16B16_SSCALED";
        case VK_FORMAT_R16G16B16_UINT:
            return "VK_FORMAT_R16G16B16_UINT";
        case VK_FORMAT_R16G16B16_SINT:
            return "VK_FORMAT_R16G16B16_SINT";
        case VK_FORMAT_R16G16B16_SFLOAT:
            return "VK_FORMAT_R16G16B16_SFLOAT";
        case VK_FORMAT_R16G16B16A16_UNORM:
            return "VK_FORMAT_R16G16B16A16_UNORM";
        case VK_FORMAT_R16G16B16A16_SNORM:
            return "VK_FORMAT_R16G16B16A16_SNORM";
        case VK_FORMAT_R16G16B16A16_USCALED:
            return "VK_FORMAT_R16G16B16A16_USCALED";
        case VK_FORMAT_R16G16B16A16_SSCALED:
            return "VK_FORMAT_R16G16B16A16_SSCALED";
        case VK_FORMAT_R16G16B16A16_UINT:
            return "VK_FORMAT_R16G16B16A16_UINT";
        case VK_FORMAT_R16G16B16A16_SINT:
            return "VK_FORMAT_R16G16B16A16_SINT";
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case VK_FORMAT_R32_UINT:
            return "VK_FORMAT_R32_UINT";
        case VK_FORMAT_R32_SINT:
            return "VK_FORMAT_R32_SINT";
        case VK_FORMAT_R32_SFLOAT:
            return "VK_FORMAT_R32_SFLOAT";
        case VK_FORMAT_R32G32_UINT:
            return "VK_FORMAT_R32G32_UINT";
        case VK_FORMAT_R32G32_SINT:
            return "VK_FORMAT_R32G32_SINT";
        case VK_FORMAT_R32G32_SFLOAT:
            return "VK_FORMAT_R32G32_SFLOAT";
        case VK_FORMAT_R32G32B32_UINT:
            return "VK_FORMAT_R32G32B32_UINT";
        case VK_FORMAT_R32G32B32_SINT:
            return "VK_FORMAT_R32G32B32_SINT";
        case VK_FORMAT_R32G32B32_SFLOAT:
            return "VK_FORMAT_R32G32B32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_UINT:
            return "VK_FORMAT_R32G32B32A32_UINT";
        case VK_FORMAT_R32G32B32A32_SINT:
            return "VK_FORMAT_R32G32B32A32_SINT";
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_R64_UINT:
            return "VK_FORMAT_R64_UINT";
        case VK_FORMAT_R64_SINT:
            return "VK_FORMAT_R64_SINT";
        case VK_FORMAT_R64_SFLOAT:
            return "VK_FORMAT_R64_SFLOAT";
        case VK_FORMAT_R64G64_UINT:
            return "VK_FORMAT_R64G64_UINT";
        case VK_FORMAT_R64G64_SINT:
            return "VK_FORMAT_R64G64_SINT";
        case VK_FORMAT_R64G64_SFLOAT:
            return "VK_FORMAT_R64G64_SFLOAT";
        case VK_FORMAT_R64G64B64_UINT:
            return "VK_FORMAT_R64G64B64_UINT";
        case VK_FORMAT_R64G64B64_SINT:
            return "VK_FORMAT_R64G64B64_SINT";
        case VK_FORMAT_R64G64B64_SFLOAT:
            return "VK_FORMAT_R64G64B64_SFLOAT";
        case VK_FORMAT_R64G64B64A64_UINT:
            return "VK_FORMAT_R64G64B64A64_UINT";
        case VK_FORMAT_R64G64B64A64_SINT:
            return "VK_FORMAT_R64G64B64A64_SINT";
        case VK_FORMAT_R64G64B64A64_SFLOAT:
            return "VK_FORMAT_R64G64B64A64_SFLOAT";
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
        case VK_FORMAT_D16_UNORM:
            return "VK_FORMAT_D16_UNORM";
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return "VK_FORMAT_X8_D24_UNORM_PACK32";
        case VK_FORMAT_D32_SFLOAT:
            return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_S8_UINT:
            return "VK_FORMAT_S8_UINT";
        case VK_FORMAT_D16_UNORM_S8_UINT:
            return "VK_FORMAT_D16_UNORM_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return "VK_FORMAT_D24_UNORM_S8_UINT";
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return "VK_FORMAT_D32_SFLOAT_S8_UINT";
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
        case VK_FORMAT_BC2_UNORM_BLOCK:
            return "VK_FORMAT_BC2_UNORM_BLOCK";
        case VK_FORMAT_BC2_SRGB_BLOCK:
            return "VK_FORMAT_BC2_SRGB_BLOCK";
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return "VK_FORMAT_BC3_UNORM_BLOCK";
        case VK_FORMAT_BC3_SRGB_BLOCK:
            return "VK_FORMAT_BC3_SRGB_BLOCK";
        case VK_FORMAT_BC4_UNORM_BLOCK:
            return "VK_FORMAT_BC4_UNORM_BLOCK";
        case VK_FORMAT_BC4_SNORM_BLOCK:
            return "VK_FORMAT_BC4_SNORM_BLOCK";
        case VK_FORMAT_BC5_UNORM_BLOCK:
            return "VK_FORMAT_BC5_UNORM_BLOCK";
        case VK_FORMAT_BC5_SNORM_BLOCK:
            return "VK_FORMAT_BC5_SNORM_BLOCK";
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
        case VK_FORMAT_BC7_UNORM_BLOCK:
            return "VK_FORMAT_BC7_UNORM_BLOCK";
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return "VK_FORMAT_BC7_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
            return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
            return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:
            return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
            return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
            return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
            return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";
        default:
            return "<unknown>";
    }
}

static const char *ColorSpaceToStr(VkColorSpaceKHR color_space) {
    switch (color_space) {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
            return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
        default:
            return "<unknown>";
    }
}

static const char *PresentModeToStr(VkPresentModeKHR mode) {
    switch (mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            return "VK_PRESENT_MODE_IMMEDIATE_KHR";
        case VK_PRESENT_MODE_MAILBOX_KHR:
            return "VK_PRESENT_MODE_MAILBOX_KHR";
        case VK_PRESENT_MODE_FIFO_KHR:
            return "VK_PRESENT_MODE_FIFO_KHR";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
            return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
            return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
        default:
            return "<unknown>";
    }
}
