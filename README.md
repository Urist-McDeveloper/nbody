# 2D N-body simulation on CPU and GPU

Written in C, powered by Vulkan and AVX (or SSE), shown on screen with [raylib](https://github.com/raysan5/raylib).

*(videos below are quite heavily compressed)*

https://user-images.githubusercontent.com/112800528/225752992-43dac741-f90d-4638-ab47-29df0921942b.mp4

https://user-images.githubusercontent.com/112800528/225753149-73836b71-6744-4fcb-b3d9-e97e08782134.mp4

## Build and run

### Build prerequisites

1. C compiler that supports:
    * C11 standard;
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

* `USE_AVX` (default `ON`) -- whether CPU simulation should use AVX or SSE.

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
* The entirety of [include/galaxy.h](include/galaxy.h#L6)

## TODO list

- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Write Vulkan renderer so that particle data never has to leave GPU
- [ ] Allow setting simulation parameters through command line arguments
- [ ] Write tests that actually test something
- [ ] Maybe refactor [src/lib/galaxy.c](src/lib/galaxy.c) because it's a bit painful to look at

Done:

- [x] Make GPU simulation respect simulation step change
- [x] Use specialization constants to make sure CPU and GPU simulations always have the same parameters
- [x] Add pushing force which is stronger than gravity at short distances
- [x] Make GPU buffers device-local for performance improvements
- [x] Allow performing multiple updates in a single UpdateWorld_GPU call by chaining pipeline dispatches
- [x] Use AVX for CPU simulation
- [x] Add CMake option to disable AVX and fall back to SSE
- [x] A moving camera would be nice
