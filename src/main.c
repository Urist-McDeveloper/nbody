#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include <nbody.h>
#include <galaxy.h>
#include <raylib.h>

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#define PARTICLE_COUNT  6000        // number of simulated particles
#define PHYS_STEP       0.01f       // fixed time step used by simulation
#define MAX_OVERWORK    3           // maximum simulation updates per frame == `MAX_OVERWORK * current_speed`

#define CAMERA_SPEED_DELTA  800.f   // how far camera with 1x zoom can move per second
#define CAMERA_ZOOM_DELTA   0.1f    // how much zoom delta is 1 mouse wheel scroll

static const Color BG_COLOR = {.r = 22, .g = 22, .b = 22, .a = 255};    // background color
static const Color CC_COLOR = {.r = 222, .g = 222, .b = 222, .a = 255}; // galaxy core color
static const Color NP_COLOR = {.r = 175, .g = 195, .b = 175, .a = 255}; // particle color
static const Color EP_COLOR = {.r = 145, .g = 145, .b = 233, .a = 255}; // "empty" (massless) particle color

static const float SPEEDS[] = {1, 2, 4, 8, 16, 32, 64, 128};        // number of updates per tick
static const float STEPS[] = {0.1f, 0.25f, 0.5f, 1.f, 2.f, 4.f};    // fixed step multiplier

#define SPEEDS_LENGTH   (sizeof(SPEEDS) / sizeof(SPEEDS[0]))
#define LAST_SPEED_IDX  (SPEEDS_LENGTH - 1)

#define STEPS_LENGTH    (sizeof(STEPS) / sizeof(STEPS[0]))
#define LAST_STEP_IDX   (STEPS_LENGTH - 1)
#define DEF_STEP_IDX    3

/* Create camera that will fit all particles on screen. */
static Camera2D CreateCamera(const Particle *ps, uint32_t count);

/* Draw all particles of WORLD. */
static void DrawParticles(World *world, float min_radius);

int main(void) {
    srand(time(NULL));

    Particle *particles = MakeGalaxies(PARTICLE_COUNT, 3);
    World *world = CreateWorld(particles, PARTICLE_COUNT);

    Camera2D camera = CreateCamera(particles, PARTICLE_COUNT);
    free(particles);

    SetTargetFPS((int)(1.f / PHYS_STEP));
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "N-Body Simulation");

    bool pause = false;
    bool overlay = true;
    bool use_gpu = PARTICLE_COUNT > 500;

    uint32_t speed_idx = 0;
    uint32_t step_idx = DEF_STEP_IDX;

    float phys_time = 0.f;
    uint32_t skipped_phys_frames = 0;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_Q)) break;
        if (IsKeyPressed(KEY_LEFT_ALT)) {
            overlay = !overlay;
        }

        int fps = GetFPS();
        if (fps > 0) {
            // move with WASD
            float cam_target_delta = CAMERA_SPEED_DELTA / (camera.zoom * (float)GetFPS());
            if (IsKeyDown(KEY_A)) {
                camera.target.x -= cam_target_delta;
            }
            if (IsKeyDown(KEY_D)) {
                camera.target.x += cam_target_delta;
            }
            if (IsKeyDown(KEY_W)) {
                camera.target.y -= cam_target_delta;
            }
            if (IsKeyDown(KEY_S)) {
                camera.target.y += cam_target_delta;
            }
        }

        // zoom with mouse wheel
        float mouse_wheel_move = GetMouseWheelMoveV().y;
        if (mouse_wheel_move > 0) {
            camera.zoom *= 1.f + CAMERA_ZOOM_DELTA;
        }
        if (mouse_wheel_move < 0) {
            camera.zoom *= 1.f - CAMERA_ZOOM_DELTA;
        }

        // drag the camera around
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 delta = GetMouseDelta();
            camera.target.x -= delta.x / camera.zoom;
            camera.target.y -= delta.y / camera.zoom;
        }

        // change camera offset to mouse position to zoom where the pointer is
        float mouse_dx = (float)GetMouseX() - camera.offset.x;
        float mouse_dy = (float)GetMouseY() - camera.offset.y;
        camera.offset.x += mouse_dx;
        camera.offset.y += mouse_dy;
        camera.target.x += mouse_dx / camera.zoom;
        camera.target.y += mouse_dy / camera.zoom;

        // simulation
        if (IsKeyPressed(KEY_TAB)) {
            use_gpu = !use_gpu;
            phys_time = 0;
            skipped_phys_frames = 0;
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
            phys_time += SPEEDS[speed_idx] * GetFrameTime();
            float max_overwork = SPEEDS[speed_idx] * PHYS_STEP * MAX_OVERWORK;

            if (phys_time > max_overwork) {
                phys_time = max_overwork;
                skipped_phys_frames++;
            } else {
                skipped_phys_frames = 0;
            }

            int updates = 0;
            while (phys_time >= PHYS_STEP) {
                phys_time -= PHYS_STEP;
                updates++;
            }

            float step = PHYS_STEP * STEPS[step_idx];
            if (use_gpu) {
                UpdateWorld_GPU(world, step, updates);
            } else {
                UpdateWorld_CPU(world, step, updates);
            }
        }

        // draw stuff
        BeginDrawing();
        {
            ClearBackground(BG_COLOR);
            BeginMode2D(camera);
            {
                float min_radius = 0.5f / camera.zoom;
                DrawParticles(world, min_radius);
            }
            EndMode2D();

            if (overlay) {
                if (pause) {
                    DrawText(use_gpu ? "GPU simulation (paused)" : "CPU simulation (paused)", 10, 10, 20, GREEN);
                } else {
                    DrawText(use_gpu ? "GPU simulation" : "CPU simulation", 10, 10, 20, GREEN);
                }
                DrawText(TextFormat("step x%.2f  speed x%d", STEPS[step_idx], (int)SPEEDS[speed_idx]), 10, 30, 20,
                         GREEN);
                DrawFPS(10, 50);

                if (skipped_phys_frames > MAX_OVERWORK) {
                    DrawText("SKIPPING FRAMES", 10, 70, 20, RED);
                }
            }
        }
        EndDrawing();
    }

    CloseWindow();
    DestroyWorld(world);
}

static Camera2D CreateCamera(const Particle *ps, uint32_t count) {
    Camera2D camera = {
            .offset = (Vector2){.x = WINDOW_WIDTH / 2.f, .y = WINDOW_HEIGHT / 2.f},
            .zoom = 1.f,
    };
    if (count == 0) return camera;

    V2 min = ps[0].pos, max = ps[0].pos;
    for (uint32_t i = 1; i < count; i++) {
        min.x = fminf(min.x, ps[i].pos.x);
        min.y = fminf(min.y, ps[i].pos.y);
        max.x = fmaxf(max.x, ps[i].pos.x);
        max.y = fmaxf(max.y, ps[i].pos.y);
    }

    float zoom_x = 0.9f * (float)WINDOW_WIDTH / (max.x - min.x);
    float zoom_y = 0.9f * (float)WINDOW_HEIGHT / (max.y - min.y);

    if (zoom_x < 1.f || zoom_y < 1.f) {
        camera.zoom = fminf(zoom_x, zoom_y);
    }

    V2 target = ScaleV2(AddV2(min, max), 0.5f);
    camera.target.x = target.x;
    camera.target.y = target.y;

    return camera;
}

static Color ColorForMass(float mass) {
    if (mass <= 0) {
        return EP_COLOR;
    } else if (mass < MIN_GC_MASS) {
        return NP_COLOR;
    } else {
        return CC_COLOR;
    }
}

static void DrawParticles(World *world, float min_radius) {
    uint32_t size;
    const Particle *arr = GetWorldParticles(world, &size);

    for (uint32_t i = 0; i < size; i++) {
        Particle p = arr[i];
        DrawCircle(
                (int)p.pos.x,
                (int)p.pos.y,
                fmaxf(p.radius, min_radius),
                ColorForMass(p.mass)
        );
    }
}
