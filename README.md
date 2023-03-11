# Simple 2D N-body simulation
Exact Newtonian gravity simulated on CPU and GPU.

Written in C, built with Vulkan, shown on screen with [raylib](https://github.com/raysan5/raylib).

## Build and run

### Build prerequisites

1. C compiler that supports C99 standard. Not too hard to find these days.
   OpenMP support is optional, but it does drastically improve the speed of CPU simulation.
2. CMake version 3.20 or later.
3. Vulkan SDK, including `glslc` and validation layers. Only Vulkan 1.0 features are used.

Target `rag-bench` uses Linux-only monotonic clock and is therefore not available on other platforms.

### How to build

Like any other CMake project.

### How to run

1. Make sure that compiled shader is located at `./shader/body_cs.spv`.
2. Run `rag`, wherever it is respective to your CWD.

### What to do?

Press some buttons:

* `Q` or `ESC` to quit
* `SPACE` to pause/unpause
* `TAB` to switch between CPU and GPU simulation
* `LEFT` to decrease simulation speed (fewer updates per second)
* `RIGHT` to increase simulation speed (more updates per second)
* `UP` to increase simulation step (less accurate, simulation speeds up)
* `DOWN` to decrease simulation step (more accurate, simulation slows down)

## TODO list

- [ ] Make particles interact when they are close to each other instead of ignoring each other's existence
- [ ] Use push constants to make sure CPU and GPU simulations always have the same parameters
- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Switch to SDL and write Vulkan renderer so particle data never has to leave GPU
- [ ] Also moving viewport
- [ ] Allow chaining multiple pipeline calls in a single command buffer

Done:

- [x] Make GPU simulation respect simulation step change (2023-03-11)
