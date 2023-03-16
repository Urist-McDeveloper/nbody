# 2D N-body simulation on CPU and GPU

Written in C, powered by Vulkan and AVX (or SSE), shown on screen with [raylib](https://github.com/raysan5/raylib).

## Build and run

### Build prerequisites

1. C compiler that supports:
    * C11 standard (specifically `aligned_alloc` in stdlib and `_Static_assert` keyword);
    * AVX or SSE intrinsics;
    * (*optional*) OpenMP.
2. Vulkan SDK, including `glslc` and validation layers. Only Vulkan 1.0 features are used.
3. CMake version 3.20 or later.

Target `nbody-bench` uses Linux-only monotonic clock and therefore is not available on other platforms.

### How to build

Like any other CMake project. Example on Linux:

```shell
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Build options:

* `USE_AVX` (default `ON`) -- indicates whether CPU simulation should use AVX or SSE.

### How to run

1. Make sure that the compiled shader is located at `./shader/particle_cs.spv`.
2. Run `nbody`.

### What to do

Base controls:

* `Q` or `ESC` to quit
* `SPACE` to pause/unpause

Camera controls:

* `WASD` to move
* Press middle mouse button to drag the screen
* Scroll mouse wheel to zoom

Simulation controls:

* `TAB` to switch between CPU and GPU simulation
* `LEFT` to decrease simulation speed (fewer updates per second)
* `RIGHT` to increase simulation speed (more updates per second)
* `UP` to increase simulation step (less accurate, simulation speeds up)
* `DOWN` to decrease simulation step (more accurate, simulation slows down)

### How to change parameters

By changing some macros:

* `WINDOW_WIDTH` and `WINDOW_HEIGHT` ([src/main.c](src/main.c#L11)) -- self explanatory;
* `PARTICLE_COUNT` ([src/main.c](src/main.c#L14)) -- you guessed it, particle count;
* The entirety of [src/cluster.h](src/cluster.h#L6)

## TODO list

- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Write Vulkan renderer so that particle data never has to leave GPU
- [ ] Allow setting simulation parameters through command line arguments
- [ ] Write tests that actually test something
- [ ] Refactor [src/cluster.c](src/cluster.c) because it's painful to look at

Done:

- [x] Make GPU simulation respect simulation step change
- [x] Use specialization constants to make sure CPU and GPU simulations always have the same parameters
- [x] Add pushing force which is stronger than gravity at short distances
- [x] Make GPU buffers device-local for performance improvements
- [x] Allow performing multiple updates in a single UpdateWorld_GPU call by chaining pipeline dispatches
- [x] Use AVX for CPU simulation
- [x] Add CMake option to disable AVX and fall back to SSE
- [x] A moving camera would be nice
