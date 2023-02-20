#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <raylib.h>

#include "body.h"

#define WIDTH   800
#define HEIGHT  600

#define PHYS_STEP   0.01
#define BODY_COUNT  100

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

void AccBodies() {
    for (int i = 0; i < BODY_COUNT - 1; i++) {
        for (int j = i + 1; j < BODY_COUNT; j++) {
            Body_ApplyGrav(&bodies[i], &bodies[j]);
        }
    }
}

void MoveBodies(double t) {
    for (int i = 0; i < BODY_COUNT; i++) {
        Body *b = &bodies[i];
        Body_Move(b, t);

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

int main(void) {
    srand(time(NULL));

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "RAG!");

    for (int i = 0; i < BODY_COUNT; i++) {
        Body_Init(&bodies[i], WIDTH, HEIGHT);
    }

    double phys_time = 0.0;
    while (!WindowShouldClose()) {
        phys_time += GetFrameTime();
        while (phys_time >= PHYS_STEP) {
            AccBodies();
            MoveBodies(PHYS_STEP);
            phys_time -= PHYS_STEP;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawBodies();
        DrawFPS(10, 10);
        EndDrawing();
    }
    CloseWindow();
}
