#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include <rag.h>

#define US_PER_S    (1000 * 1000)
#define NS_PER_US   (1000)

static void now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static int64_t diff_us(struct timespec *from, struct timespec *to) {
    return (to->tv_sec - from->tv_sec) * US_PER_S + (to->tv_nsec - from->tv_nsec) / NS_PER_US;
}

static int int64_t_cmp(const void *a_ptr, const void *b_ptr) {
    int a = (int)*((int64_t *)a_ptr);
    int b = (int)*((int64_t *)b_ptr);
    return a - b;
}

#define WORLD_WIDTH     1000
#define WORLD_HEIGHT    1000

#define UPDATE_STEP 0.01
#define WARMUP_ITER 100
#define BENCH_ITER  1000

static int64_t dt[BENCH_ITER];

static int64_t bench(World *w, bool approx) {
    for (int i = 0; i < WARMUP_ITER; i++) {
        World_Update(w, UPDATE_STEP, approx);
    }

    struct timespec start;
    struct timespec end;

    for (int i = 0; i < BENCH_ITER; i++) {
        now(&start);
        World_Update(w, UPDATE_STEP, approx);
        now(&end);

        dt[i] = diff_us(&start, &end);
    }

    qsort(dt, BENCH_ITER, sizeof(*dt), int64_t_cmp);
    int middle = BENCH_ITER / 2;

#if BENCH_ITER % 2 == 0
    return (dt[middle - 1] + dt[middle]) / 2;
#else
    return dt[middle];
#endif
}

static const int WS[] = {10, 100, 250, 500, 800, 1200};
static const int WS_LEN = sizeof(WS) / sizeof(WS[0]);

int main(void) {
    srand(11037);
    printf("\t   N\tApprox\t Exact\tSpeedup\n");

    for (int i = 0; i < WS_LEN; i++) {
        World *w1 = World_Create(WS[i], WORLD_WIDTH, WORLD_HEIGHT);
        World *w2 = World_Copy(w1);

        int64_t approx = bench(w1, true);
        int64_t exact = bench(w2, false);

        World_Destroy(w1);
        World_Destroy(w2);

        printf("\t%4d\t%6ld\t%6ld\t%7.2f\n", WS[i], approx, exact, (double)exact / (double)approx);
    }
}
