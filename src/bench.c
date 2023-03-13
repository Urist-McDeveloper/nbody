#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <time.h>

#include <rag.h>
#include <rag_vk.h>

#define US_PER_S    (1000 * 1000)
#define NS_PER_US   (1000)

static void now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static int64_t diff_us(struct timespec *from, struct timespec *to) {
    return (to->tv_sec - from->tv_sec) * US_PER_S + (to->tv_nsec - from->tv_nsec) / NS_PER_US;
}

#define UPDATE_STEP 0.01
#define WARMUP_ITER 100
#define BENCH_ITER  1000

static int64_t bench(World *w, void (*update)(World *, float, uint32_t)) {
    struct timespec start;
    struct timespec end;

    update(w, UPDATE_STEP, WARMUP_ITER);
    now(&start);
    update(w, UPDATE_STEP, BENCH_ITER);
    now(&end);

    return diff_us(&start, &end) / BENCH_ITER;
}

static const int WS[] = {10, 100, 250, 500, 800, 1200, 2000, 4000};
static const int WS_LEN = sizeof(WS) / sizeof(WS[0]);

#define WORLD_WIDTH     1000
#define WORLD_HEIGHT    1000
#define WORLD_NEW(size) World_Create(size, V2_ZERO, V2_From(WORLD_WIDTH, WORLD_HEIGHT))

int main(int argc, char **argv) {
    srand(11037);

    bool use_cpu = true, use_gpu = true;
    if (argc > 1) {
        if (memcmp(argv[1], "--cpu", 5) == 0) use_gpu = false;
        if (memcmp(argv[1], "--gpu", 5) == 0) use_cpu = false;
    }

    VulkanCtx ctx;
    if (use_gpu) VulkanCtx_Init(&ctx);

    World *cpu_w;
    World *gpu_w;

    printf("\t   N");
    if (use_cpu) printf("\t  CPU");
    if (use_gpu) printf("\t  GPU");
    printf("\n");

    for (int i = 0; i < WS_LEN; i++) {
        int world_size = WS[i];

        if (use_cpu) cpu_w = WORLD_NEW(world_size);
        if (use_gpu) {
            gpu_w = WORLD_NEW(world_size);
            World_InitVK(gpu_w, &ctx);
        }

        printf("\t%4d", world_size);
        if (use_cpu) printf("\t%5ld", bench(cpu_w, World_Update));
        if (use_gpu) printf("\t%5ld", bench(gpu_w, World_UpdateVK));
        printf("\n");

        if (use_cpu) World_Destroy(cpu_w);
        if (use_gpu) World_Destroy(gpu_w);
    }
    if (use_gpu) VulkanCtx_DeInit(&ctx);
}
