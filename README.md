# 2D N-body simulation on CPU and GPU

Written in C, powered by Vulkan and AVX (or SSE), shown on screen with [raylib](https://github.com/raysan5/raylib).

*(videos below are quite heavily compressed)*

https://user-images.githubusercontent.com/112800528/225752992-43dac741-f90d-4638-ab47-29df0921942b.mp4

https://user-images.githubusercontent.com/112800528/225753149-73836b71-6744-4fcb-b3d9-e97e08782134.mp4

## Build and run

### Build prerequisites

1. C compiler:
   * C99 standard;
   * unless SIMD is disabled through build options:
      * AVX or SSE intrinsics (`immintrin.h` and `xmmintrin.h` respectively);
      * `stdlib.h` must define one of:
         * `aligned_alloc` (C11 standard);
         * `_aligned_malloc` (Windows);
         * `posix_memalign` (POSIX)
   * (*optional*) OpenMP.
2. Vulkan SDK, including `glslc` and validation layers. Only Vulkan 1.0 features are used.
3. CMake version 3.20 or later.

If you don't have raylib installed on your system, it will be built with this project. For more information
on building raylib please refer to https://github.com/raysan5/raylib/tree/4.2.0#build-and-installation.

#### Build is tested on

* Linux:
   * GCC 12.1;
   * Clang 15;
   * TCC 0.9:
      * raylib is provided externally;
      * `SIMD_SET` is `none`.
* Windows:
   * MinGW-w64 10.

#### Misc.

Target `nbody-bench` uses Linux-only monotonic clock and therefore is not available on other platforms.

### How to build

```shell
git clone --recurse-submodules https://github.com/Urist-McDeveloper/nbody.git
cd nbody
```

Then build like any other CMake project. Example on Linux:

```shell
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

#### Build options

* `SIMD_SET` (default `AVX`) -- which SIMD instruction set to use; possible values: `AVX`, `SSE` or `none`.

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

* `WINDOW_WIDTH` and `WINDOW_HEIGHT` ([src/main.c](src/main.c#L10)) -- self explanatory;
* `PARTICLE_COUNT` ([src/main.c](src/main.c#L13)) -- you guessed it, particle count;
* The entirety of [include/galaxy.h](include/galaxy.h)

## TODO list

- [ ] Select optimal VkPhysicalDevice, not the first one in the list
- [ ] Write Vulkan renderer so that particle data never has to leave GPU
- [ ] Allow setting simulation parameters through command line arguments
- [ ] Write tests that actually test something

Done:

- [x] Make GPU simulation respect simulation step change
- [x] Use specialization constants to make sure CPU and GPU simulations always have the same parameters
- [x] Make GPU buffers device-local for performance improvements
- [x] Allow performing multiple updates in a single UpdateWorld_GPU call by chaining pipeline dispatches
- [x] Use AVX for CPU simulation
- [x] Add CMake option to disable AVX and fall back to SSE
- [x] A moving camera would be nice
