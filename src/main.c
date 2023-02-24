#include <stdlib.h>
#include <time.h>

#include <raylib.h>

#include "body.h"
#include "world.h"

static const double SPEEDS[] = {0, 1, 2, 4, 8, 16, 32};

#define SPEEDS_LENGTH   (int)(sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED      (SPEEDS_LENGTH - 1)
#define MAX_SPEED       SPEEDS[LAST_SPEED]

#define BODY_COUNT      1500
#define PHYS_STEP       0.01
#define MAX_PHYS_STEP   (MAX_SPEED * PHYS_STEP)

static void drawBodies(World *world) {
    for (int i = 0; i < world->size; i++) {
        Body *b = &world->bodies[i];
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

    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetTargetFPS((int) round(1.0 / PHYS_STEP));
    InitWindow(0, 0, "RAG!");

    World *world = World_create(BODY_COUNT, GetScreenWidth(), GetScreenHeight());
    if (world == NULL) return 1;

    double phys_time = 0.0;
    int speed = 1;

    while (!WindowShouldClose()) {
        // handle input
        if (IsKeyPressed(KEY_Q)) {
            break;
        }
        if (IsKeyPressed(KEY_LEFT) && speed > 0) {
            speed--;
        }
        if (IsKeyPressed(KEY_RIGHT) && speed < LAST_SPEED) {
            speed++;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            speed = 0;
        }

        // update stuff
        phys_time += SPEEDS[speed] * GetFrameTime();
        if (phys_time > MAX_PHYS_STEP) {
            TraceLog(LOG_DEBUG, "skipping physics frames: %.2f -> %.2f", phys_time, MAX_PHYS_STEP);
            phys_time = MAX_PHYS_STEP;
        }
        doPhysics(world, &phys_time);

        // draw stuff
        BeginDrawing();
        ClearBackground(BLACK);
        drawBodies(world);
        DrawFPS(10, 10);
        DrawText(TextFormat("x%d", (int) SPEEDS[speed]), 10, 30, 20, GREEN);
        EndDrawing();
    }

    CloseWindow();
    World_destroy(world);
    return 0;
}
