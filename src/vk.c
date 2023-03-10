#include <rag_vk.h>

#include "lib/fio.h"
#include "lib/util.h"

/* Simulation pipeline-related stuff. */
typedef struct BodyCompPipeline {
    const VulkanCtx *ctx;
    VkShaderModule grav_module;
    VkShaderModule move_module;
    VkDescriptorSetLayout ds_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline grav_pipeline;
    VkPipeline move_pipeline;
} BodyCompPipeline;

/*
 * Initialize BodyCompPipeline.
 * CTX must remain a valid pointer to initialized VulkanCtx until BCP is de-initialized.
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
    VulkanCtx_Init(&ctx, false);

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
}
