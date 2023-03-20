#include <stdlib.h>
#include <time.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <nbody.h>
#include <galaxy.h>

#include "renderer.h"
#include "lib/util.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#define PARTICLE_COUNT  40000       // number of simulated particles
#define GALAXY_COUNT    8           // number of created galaxies

#define PHYS_STEP       0.01f       // fixed time step used by simulation
#define MAX_OVERWORK    3           // maximum updates per second = MAX_OVERWORK * current_speed

#define CAMERA_SCROLL_ZOOM   0.1f   // how much 1 mouse wheel scroll affects zoom

static const int SPEEDS[] = {1, 2, 4, 8, 16, 32, 64, 128};          // number of updates per tick
static const float STEPS[] = {0.1f, 0.25f, 0.5f, 1.f, 2.f, 4.f};    // fixed step multiplier

#define SPEEDS_LENGTH   (sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED_IDX  (SPEEDS_LENGTH - 1)

#define STEPS_LENGTH    (sizeof(STEPS) / sizeof(STEPS[0]))
#define LAST_STEP_IDX   (STEPS_LENGTH - 1)
#define DEF_STEP_IDX    3

static void InitVulkan();

static Camera InitCamera(const Particle *particles, uint32_t count);

static void error_callback(int code, const char *msg) {
    fprintf(stderr, "GLFW error %08x: %s\n", code, msg);
}

static struct {
    Camera camera;
    double phys_time;
    double frame_time;
    double last_frame_time;
    uint32_t speed_idx;
    uint32_t step_idx;
    bool reverse;
    bool paused;
    bool mmb_pressed;
} state;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_R) {
        if (action == GLFW_PRESS)
            state.reverse = true;

        if (action == GLFW_RELEASE)
            state.reverse = false;
    }
    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        case GLFW_KEY_LEFT:
            if (state.speed_idx > 0) state.speed_idx--;
            return;
        case GLFW_KEY_DOWN:
            if (state.step_idx > 0) state.step_idx--;
            return;
        case GLFW_KEY_RIGHT:
            if (state.speed_idx < LAST_SPEED_IDX) state.speed_idx++;
            return;
        case GLFW_KEY_UP:
            if (state.step_idx < LAST_STEP_IDX) state.step_idx++;
            return;
        case GLFW_KEY_SPACE:
            state.paused = !state.paused;
            state.phys_time = 0;
            return;
        default:
            return;
    }
}

static void scroll_callback(GLFWwindow *window, double dx, double dy) {
    (void)window;
    (void)dx;

    state.camera.zoom *= 1.f + (float)dy * CAMERA_SCROLL_ZOOM;
}

static void cursor_callback(GLFWwindow *window, double pos_x, double pos_y) {
    (void)window;

    float dx = (float)pos_x - state.camera.offset.x;
    float dy = (float)pos_y - state.camera.offset.y;

    state.camera.offset.x += dx;
    state.camera.offset.y += dy;

    if (!state.mmb_pressed) {
        state.camera.target.x += dx / state.camera.zoom;
        state.camera.target.y += dy / state.camera.zoom;
    }
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    (void)window;
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        state.mmb_pressed = action == GLFW_PRESS;
    }
}

int main() {
    srand((unsigned int)time(NULL));

    glfwSetErrorCallback(error_callback);
    ASSERT(GLFW_TRUE == glfwInit(), "Failed to init GLFW");
    ASSERT(GLFW_TRUE == glfwVulkanSupported(), "glfwVulkanSupported() returned false");
    InitVulkan();

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "N-Body Simulation", NULL, NULL);
    ASSERT(window != NULL, "Failed to create GLFW window");

    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    Particle *particles = MakeGalaxies(PARTICLE_COUNT, GALAXY_COUNT);
    World *world = CreateWorld(particles, PARTICLE_COUNT);
    state.camera = InitCamera(particles, PARTICLE_COUNT);
    free(particles);

    Renderer *renderer = CreateRenderer(window, GetWorldParticleBuffer(world));
    VkSemaphore sim_to_rnd, rnd_to_sim;

    VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    ASSERT_VK(vkCreateSemaphore(vk_ctx.dev, &semaphore_info, NULL, &sim_to_rnd), "Failed to create semaphore");
    ASSERT_VK(vkCreateSemaphore(vk_ctx.dev, &semaphore_info, NULL, &rnd_to_sim), "Failed to create semaphore");

    bool window_shown = false;
    bool rnd_signaled = false;

    state.step_idx = DEF_STEP_IDX;
    state.frame_time = glfwGetTime();

    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        state.last_frame_time = state.frame_time;
        state.frame_time = glfwGetTime();
        state.phys_time += (state.frame_time - state.last_frame_time) * SPEEDS[state.speed_idx];

        uint32_t updates = 0;
        if (!state.paused) {
            while (state.phys_time > PHYS_STEP) {
                state.phys_time -= PHYS_STEP;
                updates++;
            }
        }

        if (updates > 0) {
            uint32_t max_updates = MAX_OVERWORK * SPEEDS[state.speed_idx];
            if (updates > max_updates) {
                updates = max_updates;
            }

            float step = PHYS_STEP * STEPS[state.step_idx];
            if (state.reverse) {
                step = -step;
            }

            VkSemaphore wait = rnd_signaled ? rnd_to_sim : NULL;
            rnd_signaled = false;

            UpdateWorld_GPU(world, wait, sim_to_rnd, step, updates);
        }

        VkSemaphore wait = updates > 0 ? sim_to_rnd : NULL;
        VkSemaphore signal = rnd_signaled ? NULL : rnd_to_sim;
        rnd_signaled = true;

        Draw(renderer, state.camera, wait, signal, PARTICLE_COUNT);

        if (!window_shown) {
            glfwShowWindow(window);
            window_shown = true;
        }
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

static Camera InitCamera(const Particle *particles, uint32_t count) {
    Camera camera = {
            .offset = MakeVec2(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f),
            .dims = MakeVec2(WINDOW_WIDTH, WINDOW_HEIGHT),
            .zoom = 1.f,
    };
    if (count == 0) return camera;

    Vec2 min = particles[0].pos, max = particles[0].pos;
    for (uint32_t i = 1; i < count; i++) {
        min.x = fminf(min.x, particles[i].pos.x);
        min.y = fminf(min.y, particles[i].pos.y);
        max.x = fmaxf(max.x, particles[i].pos.x);
        max.y = fmaxf(max.y, particles[i].pos.y);
    }

    float zoom_x = 0.9f * (float)WINDOW_WIDTH / (max.x - min.x);
    float zoom_y = 0.9f * (float)WINDOW_HEIGHT / (max.y - min.y);

    if (zoom_x < 1.f || zoom_y < 1.f) {
        camera.zoom = fminf(zoom_x, zoom_y);
    }

    Vec2 target = ScaleVec2(AddVec2(min, max), 0.5f);
    camera.target.x = target.x;
    camera.target.y = target.y;

    return camera;
}
