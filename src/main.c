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

static void InitVulkan();

static void error_callback(int code, const char *msg) {
    fprintf(stderr, "GLFW error %08x: %s\n", code, msg);
}

int main() {
    glfwSetErrorCallback(error_callback);
    ASSERT(GLFW_TRUE == glfwInit(), "Failed to init GLFW");
    ASSERT(GLFW_TRUE == glfwVulkanSupported(), "glfwVulkanSupported() returned false");
    InitVulkan();

    World *world;
    GLFWwindow *window;
    Renderer *renderer;

    #pragma omp parallel num_threads(2) default(none) shared(world, window, renderer, stderr)
    #pragma omp master
    {
        #pragma omp task default(none) shared(world)
        {
            Particle *particles = MakeGalaxies(PARTICLE_COUNT, 3);
            world = CreateWorld(particles, PARTICLE_COUNT);
            free(particles);
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "N-Body Simulation", NULL, NULL);
        ASSERT(window != NULL, "Failed to create GLFW window");

        renderer = CreateRenderer(window);
    }

    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        Draw(renderer);
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
