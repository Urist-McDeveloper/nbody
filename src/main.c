#include <stdlib.h>
#include <time.h>

#include <raylib.h>

#include "body.h"
#include "quadtree.h"
#include "world.h"

static const double SPEEDS[] = {0, 1, 2, 4, 8, 16, 32};

#define SPEEDS_LENGTH   (int)(sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED_IDX  (SPEEDS_LENGTH - 1)

#define BODY_COUNT      1500
#define PHYS_STEP       0.01

#define MAX_PHYS_OVERWORK 3
#define MAX_SKIPPED_PHYS_FRAMES 15

static int dtoi(double f) {
    return (int) round(f);
}

static void drawBodies(World *world) {
    Body *bodies;
    int size;
    World_getBodies(world, &bodies, &size);

    for (int i = 0; i < size; i++) {
        Particle *p = &(bodies + i)->p;
        DrawCircle(
                dtoi(p->pos.x),
                dtoi(p->pos.y),
                (float) p->radius,
                RAYWHITE
        );
    }
}

#define DrawLineD(X0, Y0, X1, Y1) DrawLine(dtoi(X0), dtoi(Y0), dtoi(X1), dtoi(Y1), BLUE)

static void drawQuad(const Node *quad) {
    // null-check is mandatory
    if (quad == NULL) return;

    for (int i = 0; i < 4; i++) {
        const Node *n = Node_fromQuad(quad, i);

        // ignore empty nodes
        if (Node_isEmpty(n)) continue;

        const Node *inner = Node_getQuad(n);
        if (inner != NULL) {
            // draw inner quad
            drawQuad(Node_getQuad(n));
        } else {
            // draw bounding box
            V2 from, to;
            Node_getBox(n, &from, &to);

            DrawLineD(from.x, from.y, to.x, from.y);    // top
            DrawLineD(from.x, from.y, from.x, to.y);    // left
            DrawLineD(from.x, to.y, to.x, to.y);        // bottom
            DrawLineD(to.x, from.y, to.x, to.y);        // right
        }
    }
}

int main(void) {
    srand(time(NULL));

    SetConfigFlags(/*FLAG_FULLSCREEN_MODE |*/ FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetTargetFPS((int) round(1.0 / PHYS_STEP));
    InitWindow(800, 600, "RAG!");

    World *world = World_create(BODY_COUNT, GetScreenWidth(), GetScreenHeight());

    int speed_idx = 1;
    bool approx = true;

    double phys_time = 0.0;
    int skipped_phys_frames = 0;

    while (!WindowShouldClose()) {
        // handle input
        if (IsKeyPressed(KEY_Q)) {
            break;
        }
        if (IsKeyPressed(KEY_TAB)) {
            approx = !approx;
        }
        if (IsKeyPressed(KEY_LEFT) && speed_idx > 0) {
            speed_idx--;
        }
        if (IsKeyPressed(KEY_RIGHT) && speed_idx < LAST_SPEED_IDX) {
            speed_idx++;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            speed_idx = 0;
            phys_time = 0;
            skipped_phys_frames = 0;
        }
        if (speed_idx == 0 && IsKeyDown(KEY_ENTER)) {
            World_update(world, PHYS_STEP, approx);
        }

        // update stuff
        if (speed_idx > 1 && skipped_phys_frames > MAX_SKIPPED_PHYS_FRAMES) {
            speed_idx--;
            skipped_phys_frames = MAX_PHYS_OVERWORK;
        }
        if (speed_idx > 0) {
            phys_time += SPEEDS[speed_idx] * GetFrameTime();
            double max_phys_time = MAX_PHYS_OVERWORK * SPEEDS[speed_idx] * PHYS_STEP;

            if (phys_time > max_phys_time) {
                TraceLog(LOG_DEBUG, "skipping physics frames: %.2f -> %.2f", phys_time, max_phys_time);
                phys_time = max_phys_time;
                skipped_phys_frames++;
            } else {
                skipped_phys_frames = 0;
            }

            while (phys_time >= PHYS_STEP) {
                phys_time -= PHYS_STEP;
                World_update(world, PHYS_STEP, approx);
            }
        }

        // draw stuff
        BeginDrawing();
        ClearBackground(BLACK);

        drawBodies(world);
        if (speed_idx == 0) {
            drawQuad(World_getQuad(world));
        }

        DrawText(approx ? "Barnes-Hut simulation" : "Exact simulation", 10, 10, 20, GREEN);
        DrawText(TextFormat("x%d", (int) SPEEDS[speed_idx]), 10, 30, 20, GREEN);
        DrawFPS(10, 50);

        if (skipped_phys_frames > MAX_PHYS_OVERWORK) {
            DrawText("SKIPPING FRAMES", 10, 70, 20, RED);
        }

        EndDrawing();
    }

    CloseWindow();
    World_destroy(world);
    return 0;
}
