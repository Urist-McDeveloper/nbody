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

void drawBodies(void) {
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

void doPhysics(double *phys_time) {
    if (*phys_time < PHYS_STEP) return;

    while (*phys_time >= PHYS_STEP) {
        *phys_time -= PHYS_STEP;

        #pragma omp parallel for if(BODY_COUNT > 100) shared(bodies) default(none)
        for (int i = 0; i < BODY_COUNT; i++) {
            for (int j = 0; j < BODY_COUNT; j++) {
                if (i != j) {
                    Body_applyGrav(&bodies[i], &bodies[j]);
                }
            }
        }

        #pragma omp parallel for if(BODY_COUNT > 100) shared(bodies) default(none)
        for (int i = 0; i < BODY_COUNT; i++) {
            Body *b = &bodies[i];
            Body_move(b, PHYS_STEP);

            double min_x = b->r;
            double min_y = b->r;
            double max_x = WIDTH - min_x;
            double max_y = HEIGHT - min_y;

            if (b->pos.x < min_x || b->pos.x > max_x) {
                b->pos.x = (b->pos.x < min_x) ? min_x : max_x;
                b->vel.x *= -0.5;
                b->vel.y *= 0.75;
            }
            if (b->pos.y < min_y || b->pos.y > max_y) {
                b->pos.y = (b->pos.y < min_y) ? min_y : max_y;
                b->vel.y *= -0.5;
                b->vel.x *= 0.75;
            }
        }
    }
}

int main(void) {
    srand(time(NULL));

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "RAG!");

    for (int i = 0; i < BODY_COUNT; i++) {
        Body_init(&bodies[i], WIDTH, HEIGHT);
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
        doPhysics(&phys_time);

        // draw stuff
        BeginDrawing();
        ClearBackground(BLACK);
        drawBodies();
        DrawFPS(10, 10);
        DrawText(TextFormat("x%d", (int) SPEEDS[speed]), 10, 30, 20, GREEN);
        EndDrawing();
    }
    CloseWindow();
}
