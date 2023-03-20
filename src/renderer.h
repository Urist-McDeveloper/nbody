#ifndef NB_RENDERER_H
#define NB_RENDERER_H

#include <nbody.h>
#include <GLFW/glfw3.h>

typedef struct Renderer Renderer;

/* Allocate and initialize. */
Renderer *CreateRenderer(GLFWwindow *window, const VulkanBuffer *particle_data);

/* De-initialize and deallocate. */
void DestroyRenderer(Renderer *renderer);

/* Must be called every time window is resized. */
void RecreateSwapchain(Renderer *r);

/* Submit a draw call. */
void Draw(Renderer *r, VkEvent wait_event, uint32_t particle_count);

#endif //NB_RENDERER_H
