# Direct 2D N-body simulation

Written in C, powered by Vulkan and AVX, shown on screen with [raylib](https://github.com/raysan5/raylib).

## Build and run

### Build prerequisites

1. C compiler that supports:
   * C11 standard (specifically `aligned_alloc` in stdlib);
   * AVX intrinsics;
   * (*optional*) OpenMP.
2. Vulkan SDK, including `glslc` and validation layers. Only Vulkan 1.0 features are used.
3. CMake version 3.20 or later.

Target `rag-bench` uses Linux-only monotonic clock and therefore is not available on other platforms.

### How to build

Like any other CMake project.

### How to run

1. Make sure that the compiled shader is located at `./shader/body_cs.spv`.
2. Run `rag`.

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

- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Write Vulkan renderer so that particle data never has to leave GPU
- [ ] Also moving viewport
- [ ] Allow setting simulation parameters through command line arguments

Done:

- [x] Make GPU simulation respect simulation step change
- [x] Use specialization constants to make sure CPU and GPU simulations always have the same parameters
- [x] Add pushing force which is stronger than gravity at short distances
- [x] Make GPU buffers device-local for performance improvements
- [x] Allow performing multiple updates in a single World_UpdateVK call by chaining pipeline dispatches
- [x] Use AVX for CPU simulation
