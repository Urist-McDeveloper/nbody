#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <raylib.h>

#include "body.h"

#define WIDTH   800
#define HEIGHT  600

#define PHYS_STEP   0.01
#define BODY_COUNT  350

static Body bodies[BODY_COUNT];

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

#pragma omp parallel for if(BODY_COUNT > 500) shared(bodies) default(none)
        for (int i = 0; i < BODY_COUNT; i++) {
            for (int j = 0; j < BODY_COUNT; j++) {
                if (i != j) {
                    Body_ApplyGravUni(&bodies[i], &bodies[j]);
                }
            }
        }

#pragma omp parallel for if(BODY_COUNT > 500) shared(bodies) default(none)
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
    SetTargetFPS((int) ceil(1.0 / PHYS_STEP));

    for (int i = 0; i < BODY_COUNT; i++) {
        Body_Init(&bodies[i], WIDTH, HEIGHT);
    }

    double phys_time = 0.0;
    double speed = 1.0;
    bool stop = false;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_LEFT) && speed > 0.2) {
            speed *= 0.8;
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            speed *= 1.2;
        }
        if (IsKeyPressed(KEY_ZERO)) {
            speed = 1.0;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            stop = !stop;
        }

        if (!stop) {
            phys_time += speed * GetFrameTime();
            DoPhysics(&phys_time);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawBodies();
        DrawFPS(10, 10);
        DrawText(TextFormat("x%.2f", stop ? 0 : speed), 10, 30, 20, GREEN);
        EndDrawing();
    }
    CloseWindow();
}
