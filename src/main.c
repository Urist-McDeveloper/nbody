#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "raylib.h"
#include "body.h"

#define WIDTH       800
#define HEIGHT      600
#define BODY_COUNT  450

static Body bodies[BODY_COUNT];
static const double SPEEDS[] = {0, 1, 2, 4, 8, 16, 32};

#define SPEEDS_LENGTH   (int)(sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED      (SPEEDS_LENGTH - 1)
#define MAX_SPEED       SPEEDS[LAST_SPEED]

#define PHYS_STEP       0.01
#define MAX_PHYS_STEP   (MAX_SPEED * PHYS_STEP)

void DrawBodies(void) {
    for (int i = 0; i < BODY_COUNT; i++) {
        Body *b = &bodies[i];
        DrawCircle(
                (int) round(b->pos.x),
                (int) round(b->pos.y),
                (float) b->r,
                RAYWHITE
        );
    }
}

void DoPhysics(double *phys_time) {
    if (*phys_time < PHYS_STEP) return;

    while (*phys_time >= PHYS_STEP) {
        *phys_time -= PHYS_STEP;

#pragma omp parallel for if(BODY_COUNT > 100) shared(bodies) default(none)
        for (int i = 0; i < BODY_COUNT; i++) {
            for (int j = 0; j < BODY_COUNT; j++) {
                if (i != j) {
                    Body_ApplyGravUni(&bodies[i], &bodies[j]);
                }
            }
        }

#pragma omp parallel for if(BODY_COUNT > 100) shared(bodies) default(none)
        for (int i = 0; i < BODY_COUNT; i++) {
            Body *b = &bodies[i];
            Body_Move(b, PHYS_STEP);

            if (b->pos.x < 0 || b->pos.x > WIDTH) {
                b->pos.x = b->pos.x < 0 ? 0 : WIDTH;
                b->vel.x *= -0.5;
            }
            if (b->pos.y < 0 || b->pos.y > HEIGHT) {
                b->pos.y = b->pos.y < 0 ? 0 : HEIGHT;
                b->vel.y *= -0.5;
            }
        }
    }
}

int main(void) {
    srand(time(NULL));

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "RAG!");

    for (int i = 0; i < BODY_COUNT; i++) {
        Body_Init(&bodies[i], WIDTH, HEIGHT);
    }

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
        DoPhysics(&phys_time);

        // draw stuff
        BeginDrawing();
        ClearBackground(BLACK);
        DrawBodies();
        DrawFPS(10, 10);
        DrawText(TextFormat("x%d", (int) SPEEDS[speed]), 10, 30, 20, GREEN);
        EndDrawing();
    }
    CloseWindow();
}
