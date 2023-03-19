#ifndef NB_RENDERER_H
#define NB_RENDERER_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

typedef struct Renderer Renderer;

/* Allocate and initialize. */
Renderer *CreateRenderer(GLFWwindow *window);

/* De-initialize and deallocate. */
void DestroyRenderer(Renderer *renderer);

/* Must be called every time window is resized. */
void RecreateSwapchain(Renderer *r);

#endif //NB_RENDERER_H
