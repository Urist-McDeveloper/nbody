#include <stdlib.h>
#include <stdio.h>

#include <GLFW/glfw3.h>

#include <nbody.h>
#include <galaxy.h>

#include "lib/util.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#define PARTICLE_COUNT  6000        // number of simulated particles

void error_callback(int code, const char *msg) {
    fprintf(stderr, "GLFW error %08x: %s\n", code, msg);
}

int main() {
    glfwSetErrorCallback(error_callback);
    ASSERT(GLFW_TRUE == glfwInit(), "Failed to init GLFW");

    Particle *particles = MakeGalaxies(PARTICLE_COUNT, 3);
    World *world = CreateWorld(particles, PARTICLE_COUNT);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "N-Body Simulation", NULL, NULL);
    ASSERT(window != NULL, "Failed to create GLFW window");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    free(world);
    glfwTerminate();
    return 0;
}
