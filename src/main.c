#include <stdlib.h>
#include <stdio.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <nbody.h>
#include <galaxy.h>

#include "renderer.h"
#include "lib/util.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#define PARTICLE_COUNT  6000        // number of simulated particles
#define PHYS_STEP       0.01f       // fixed time step used by simulation

static void InitVulkan();

static void error_callback(int code, const char *msg) {
    fprintf(stderr, "GLFW error %08x: %s\n", code, msg);
}

int main() {
    glfwSetErrorCallback(error_callback);
    ASSERT(GLFW_TRUE == glfwInit(), "Failed to init GLFW");
    ASSERT(GLFW_TRUE == glfwVulkanSupported(), "glfwVulkanSupported() returned false");
    InitVulkan();

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "N-Body Simulation", NULL, NULL);
    ASSERT(window != NULL, "Failed to create GLFW window");

    Particle *particles = MakeGalaxies(PARTICLE_COUNT, 3);
    World *world = CreateWorld(particles, PARTICLE_COUNT);
    free(particles);

    Renderer *renderer = CreateRenderer(window, GetWorldParticleBuffer(world));
    VkEvent event;

    VkEventCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
    };
    ASSERT_VK(vkCreateEvent(vk_ctx.dev, &info, NULL, &event), "Failed to create Vulkan event");

    double last_frame_time = glfwGetTime();
    double phys_time = 0.0;

    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double frame_time = glfwGetTime();
        double frame_time_delta = frame_time - last_frame_time;

        phys_time += frame_time_delta;
        uint32_t updates = 0;

        while (phys_time > PHYS_STEP) {
            phys_time -= PHYS_STEP;
            updates++;
        }

        if (updates > 0) {
            ASSERT_VK(vkResetEvent(vk_ctx.dev, event), "Failed to reset Vulkan event");
            UpdateWorld_GPU(world, event, PHYS_STEP, updates);
            Draw(renderer, event, PARTICLE_COUNT);
        }
        last_frame_time = frame_time;
    }

    DestroyRenderer(renderer);
    glfwDestroyWindow(window);
    DestroyWorld(world);
    glfwTerminate();
    return 0;
}

static void InitVulkan() {
    uint32_t ext_count;
    const char **ext = glfwGetRequiredInstanceExtensions(&ext_count);
    ASSERT(ext != NULL, "GLFW failed to provide required Vulkan instance extensions");

    InitGlobalVulkanContext(true, ext, ext_count);
    ASSERT(GLFW_TRUE == glfwGetPhysicalDevicePresentationSupport(vk_ctx.instance,
                                                                 vk_ctx.pdev,
                                                                 vk_ctx.queue_family_idx),
           "glfwGetPhysicalDevicePresentationSupport() returned false");
}
