#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <nbody.h>
#include <galaxy.h>

#define US_PER_S    (1000 * 1000)
#define NS_PER_US   (1000)

static void now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static int64_t diff_us(struct timespec from, struct timespec to) {
    return (to.tv_sec - from.tv_sec) * US_PER_S + (to.tv_nsec - from.tv_nsec) / NS_PER_US;
}

#define UPDATE_STEP 1.f
#define WARMUP_ITER 10
#define BENCH_ITER  100

static int64_t bench(World *w, void (*update)(World *, float, uint32_t)) {
    struct timespec start;
    struct timespec end;

    update(w, UPDATE_STEP, WARMUP_ITER);
    now(&start);
    update(w, UPDATE_STEP, BENCH_ITER);
    now(&end);

    return diff_us(start, end) / BENCH_ITER;
}

/* Must be sorted in ascending order. */
static const int SIZES[] = {250, 500, 800, 1200, 2000, 4000, 10000, 20000, 50000, 100000};
static const int SIZES_LEN = sizeof(SIZES) / sizeof(SIZES[0]);

int main(int argc, char **argv) {
    srand(11037);   // fixed seed for reproducible benchmarks

    bool use_cpu = true, use_gpu = true;
    if (argc > 1) {
        if (memcmp(argv[1], "--cpu", 5) == 0) use_gpu = false;
        if (memcmp(argv[1], "--gpu", 5) == 0) use_cpu = false;
    }

    World *cpu_w = NULL, *gpu_w = NULL;
    for (int i = 0; i < SIZES_LEN; i++) {
        int world_size = SIZES[i];
        Particle *particles = MakeGalaxies(world_size, 2);

        if (use_cpu) cpu_w = CreateWorld(particles, world_size);
        if (use_gpu) gpu_w = CreateWorld(particles, world_size);

        if (i == 0) {
            printf("\t      N");
            if (use_cpu) printf("\t    CPU");
            if (use_gpu) printf("\t    GPU");
            printf("\n");
        }

        printf("\t%7d", world_size);
        if (use_cpu) printf("\t%7ld", bench(cpu_w, UpdateWorld_CPU));
        if (use_gpu) printf("\t%7ld", bench(gpu_w, UpdateWorld_GPU));
        printf("\n");

        free(particles);
        if (use_cpu) DestroyWorld(cpu_w);
        if (use_gpu) DestroyWorld(gpu_w);
    }
}
