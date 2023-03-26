# 2D N-body simulation on CPU and GPU

Written in C, powered by Vulkan and AVX (or SSE).

*(videos below are quite heavily compressed)*

https://user-images.githubusercontent.com/112800528/225752992-43dac741-f90d-4638-ab47-29df0921942b.mp4

https://user-images.githubusercontent.com/112800528/225753149-73836b71-6744-4fcb-b3d9-e97e08782134.mp4

## How to build

1. Clone this repository:
   ```shell
   git clone --recurse-submodules https://github.com/Urist-McDeveloper/nbody.git
   cd nbody
   ```
2. Build like any other CMake project. Example for Linux:
   ```shell
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   make
   ```

#### Build prerequisites

1. C compiler that supports C99 standard and (optionally) OpenMP
2. Vulkan SDK, including `glslc` and validation layers
3. CMake version 3.20 or later

Build prerequisites for GLFW can be found in
[the official documentation](https://www.glfw.org/docs/latest/compile.html#compile_deps).

#### Build options

* `SIMD_SET` -- which SIMD instruction set to use:
  * possible values: `AVX` (default), `SSE`, `NONE`
  * if AVX or SSE is used, compiler must support `immintrin.h` or `xmmintrin.h` respectively
  * if AVX or SSE is used, compiler must support one of:
    * `aligned_alloc` in `stdlib.h` (C11 standard)
    * `posix_memalign` in `stdlib.h` (POSIX)
    * `_aligned_malloc` in `malloc.h` (Windows)
* `USE_EXTERNAL_GLFW` -- whether to use external GLFW or build it from source:
  * possible values: `OFF` (default), `ON`

#### Build is tested on

* Linux:
   * GCC 12.1
   * Clang 15
   * TCC 0.9:
      * `SIMD_SET=NONE`
      * `USE_EXTERNAL_GLFW=ON`
* Windows:
   * MinGW-w64 10

#### Misc.

Target `nbody-bench` uses Linux-only monotonic clock and therefore is not available on other platforms.

## What to do

Base controls:

* `Q` or `ESC` to quit
* `SPACE` to pause/unpause

Camera controls:

* `WASD` to move
* Press middle mouse button to drag the screen
* Scroll mouse wheel to zoom

Simulation controls:

* `R` to reverse time
* `LEFT` to decrease simulation speed (fewer updates per second)
* `RIGHT` to increase simulation speed (more updates per second)
* `UP` to increase simulation step (less accurate, simulation speeds up)
* `DOWN` to decrease simulation step (more accurate, simulation slows down)

### How to change parameters

By changing some macros:

* `WINDOW_WIDTH` and `WINDOW_HEIGHT` ([src/main.c](src/main.c#L13))
* `PARTICLE_COUNT` and `GALAXY_COUNT` ([src/main.c](src/main.c#L16))
* The entirety of [include/galaxy.h](include/galaxy.h)

## TODO list

- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Allow setting simulation parameters through command line arguments
- [ ] Write tests that actually test something
- [ ] Make CPU simulation usable again
- [ ] Refactor the whole project: remove the split between lib and main

Done:

- [x] Make GPU simulation respect simulation step change
- [x] Use specialization constants to make sure CPU and GPU simulations always have the same parameters
- [x] Make GPU buffers device-local for performance improvements
- [x] Allow performing multiple updates in a single UpdateWorld_GPU call by chaining pipeline dispatches
- [x] Use AVX for CPU simulation
- [x] Add CMake option to disable AVX and fall back to SSE
- [x] A moving camera would be nice
- [x] Write Vulkan renderer so that particle data never has to leave GPU
