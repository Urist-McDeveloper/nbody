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

Press some buttons:

* `Q` or `ESC` to quit
* `SPACE` to pause/unpause
* `TAB` to switch between CPU and GPU simulation
* `LEFT` to decrease simulation speed (fewer updates per second)
* `RIGHT` to increase simulation speed (more updates per second)
* `UP` to increase simulation step (less accurate, simulation speeds up)
* `DOWN` to decrease simulation step (more accurate, simulation slows down)

### How to change parameters

By changing some macros:

* `WINDOW_WIDTH` and `WINDOW_HEIGHT` ([src/main.c](src/main.c#L39)) -- self explanatory;
* `PARTICLE_COUNT` ([src/main.c](src/main.c#L18)) -- you guessed it, particle count;
* `NB_G` ([include/nbody.h](include/nbody.h#L10)) -- gravitational constant;
* `NB_N` ([include/nbody.h](include/nbody.h#L16)) -- repulsion constant;
* `NB_F` ([include/nbody.h](include/nbody.h#L22)) -- velocity decay constant;
* `MIN_R` and `MAX_R` ([src/lib/world.c](src/lib/world.c#L11)) -- minimum and maximum radius of a Particle;

## TODO list

- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Write Vulkan renderer so that particle data never has to leave GPU
- [ ] A moving camera would be nice
- [ ] Allow setting simulation parameters through command line arguments
- [ ] Add CMake option to disable GPU simulation
- [ ] Write tests that actually test something

Done:

- [x] Make GPU simulation respect simulation step change
- [x] Use specialization constants to make sure CPU and GPU simulations always have the same parameters
- [x] Add pushing force which is stronger than gravity at short distances
- [x] Make GPU buffers device-local for performance improvements
- [x] Allow performing multiple updates in a single UpdateWorld_GPU call by chaining pipeline dispatches
- [x] Use AVX for CPU simulation
- [x] Add CMake option to disable AVX and fall back to SSE
