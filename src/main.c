#include <stdlib.h>
#include <time.h>

#include <rag.h>
#include <rag_vk.h>
#include <raylib.h>

static const float SPEEDS[] = {1, 2, 4, 8, 16, 32};

#define SPEEDS_LENGTH   (int)(sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED_IDX  (SPEEDS_LENGTH - 1)

static const float STEPS[] = {0.1f, 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f};

#define STEPS_LENGTH    (int)(sizeof(STEPS) / sizeof(STEPS[0]))
#define LAST_STEP_IDX   (STEPS_LENGTH - 1)
#define DEF_STEP_IDX    3

#define BODY_COUNT      1000
#define PHYS_STEP       0.01f

#define MAX_PHYS_OVERWORK 3

static int dtoi(double f) {
    return (int)round(f);
}

static void DrawBodies(World *world) {
    Body *bodies;
    int size;
    World_GetBodies(world, &bodies, &size);

    for (int i = 0; i < size; i++) {
        Particle *p = &(bodies + i)->p;
        DrawCircle(
                dtoi(p->pos.x),
                dtoi(p->pos.y),
                (float)p->radius,
                RAYWHITE
        );
    }
}

int main(void) {
    srand(time(NULL));

    SetConfigFlags(/*FLAG_FULLSCREEN_MODE |*/ FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetTargetFPS((int)round(1.0 / PHYS_STEP));
    InitWindow(800, 600, "RAG!");

    VulkanCtx vk_ctx;
    VulkanCtx_Init(&vk_ctx);

    World *world = World_Create(BODY_COUNT, V2_ZERO, V2_From(GetScreenWidth(), GetScreenHeight()));
    World_InitVK(world, &vk_ctx, PHYS_STEP);

    bool pause = false;
    bool use_gpu = false;

    int speed_idx = 0;
    int step_idx = DEF_STEP_IDX;

    float phys_time = 0.f;
    int skipped_phys_frames = 0;

    while (!WindowShouldClose()) {
        // handle input
        if (IsKeyPressed(KEY_Q)) {
            break;
        }
        if (IsKeyPressed(KEY_TAB)) {
            use_gpu = !use_gpu;
        }
        if (IsKeyPressed(KEY_LEFT) && speed_idx > 0) {
            speed_idx--;
        }
        if (IsKeyPressed(KEY_DOWN) && step_idx > 0) {
            step_idx--;
        }
        if (IsKeyPressed(KEY_RIGHT) && speed_idx < LAST_SPEED_IDX) {
            speed_idx++;
        }
        if (IsKeyPressed(KEY_UP) && step_idx < LAST_STEP_IDX) {
            step_idx++;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            if (pause) {
                pause = false;
            } else {
                pause = true;
                phys_time = 0;
                skipped_phys_frames = 0;
            }
        }

        // update stuff
        if (!pause) {
            float scale = SPEEDS[speed_idx] * STEPS[step_idx];
            float step = PHYS_STEP;

            // GPU simulation uses constant time delta (for now)
            if (!use_gpu) {
                step *= STEPS[step_idx];
            }

            phys_time += scale * GetFrameTime();
            float max_phys_time = MAX_PHYS_OVERWORK * scale * step;

            if (phys_time > max_phys_time) {
                phys_time = max_phys_time;
                skipped_phys_frames++;
            } else {
                skipped_phys_frames = 0;
            }

            while (phys_time >= step) {
                phys_time -= step;
                if (use_gpu) {
                    World_UpdateVK(world);
                } else {
                    World_Update(world, step);
                }
            }
        }

        // draw stuff
        BeginDrawing();
        ClearBackground(BLACK);

        DrawBodies(world);
        DrawText(use_gpu ? "Exact simulation on GPU" : "Exact simulation on CPU", 10, 10, 20, GREEN);
        DrawText(TextFormat("step x%.2f  speed x%d", STEPS[step_idx], (int)SPEEDS[speed_idx]), 10, 30, 20, GREEN);
        DrawFPS(10, 50);

        if (skipped_phys_frames > MAX_PHYS_OVERWORK) {
            DrawText("SKIPPING FRAMES", 10, 70, 20, RED);
        }
        EndDrawing();
    }

    CloseWindow();
    World_Destroy(world);
    VulkanCtx_DeInit(&vk_ctx);
}
