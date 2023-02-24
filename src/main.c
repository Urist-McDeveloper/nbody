#include <stdlib.h>
#include <time.h>

#include <raylib.h>

#include "body.h"
#include "world.h"

static const double SPEEDS[] = {0, 1, 2, 4, 8, 16, 32};

#define SPEEDS_LENGTH   (int)(sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED_IDX  (SPEEDS_LENGTH - 1)

#define BODY_COUNT      1500
#define PHYS_STEP       0.01

#define MAX_PHYS_OVERWORK 3
#define MAX_SKIPPED_PHYS_FRAMES 15

static void drawBodies(World *world) {
    Body *bodies;
    int size;
    World_getBodies(world, &bodies, &size);

    for (int i = 0; i < size; i++) {
        Body *b = &bodies[i];
        DrawCircle(
                (int) round(b->pos.x),
                (int) round(b->pos.y),
                (float) b->r,
                RAYWHITE
        );
    }
}

static void doPhysics(World *world, double *phys_time) {
    while (*phys_time >= PHYS_STEP) {
        *phys_time -= PHYS_STEP;
        World_update(world, PHYS_STEP);
    }
}

int main(void) {
    srand(time(NULL));

    SetConfigFlags(/*FLAG_FULLSCREEN_MODE |*/ FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetTargetFPS((int) round(1.0 / PHYS_STEP));
    InitWindow(800, 600, "RAG!");

    World *world = World_create(BODY_COUNT, GetScreenWidth(), GetScreenHeight());

    double phys_time = 0.0;
    int speed_idx = 1;
    int skipped_phys_frames = 0;

    while (!WindowShouldClose()) {
        // handle input
        if (IsKeyPressed(KEY_Q)) {
            break;
        }
        if (IsKeyPressed(KEY_LEFT) && speed_idx > 0) {
            speed_idx--;
        }
        if (IsKeyPressed(KEY_RIGHT) && speed_idx < LAST_SPEED_IDX) {
            speed_idx++;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            speed_idx = 0;
        }

        // update stuff
        if (speed_idx > 1 && skipped_phys_frames > MAX_SKIPPED_PHYS_FRAMES) {
            speed_idx--;
            skipped_phys_frames = MAX_PHYS_OVERWORK;
        }

        phys_time += SPEEDS[speed_idx] * GetFrameTime();
        double max_phys_time = MAX_PHYS_OVERWORK * SPEEDS[speed_idx] * PHYS_STEP;

        if (phys_time > max_phys_time) {
            TraceLog(LOG_DEBUG, "skipping physics frames: %.2f -> %.2f", phys_time, max_phys_time);
            phys_time = max_phys_time;
            skipped_phys_frames++;
        } else {
            skipped_phys_frames = 0;
        }
        doPhysics(world, &phys_time);

        // draw stuff
        BeginDrawing();
        ClearBackground(BLACK);

        drawBodies(world);
        DrawFPS(10, 10);

        DrawText(TextFormat("x%d", (int) SPEEDS[speed_idx]), 10, 30, 20, GREEN);
        if (skipped_phys_frames > MAX_PHYS_OVERWORK) {
            DrawText("SKIPPING FRAMES", 10, 50, 20, RED);
        }

        EndDrawing();
    }

    CloseWindow();
    World_destroy(world);
    return 0;
}
