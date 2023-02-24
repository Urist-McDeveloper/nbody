#include "world.h"

#include <stdlib.h>

/* Minimum number of bodies to use OMP. */
#define MIN_OMP_SIZE    100

/* How velocity changes along the axis of bounce. */
#define BOUNCE_F    (-0.5)

/* How velocity changes along the other axis. */
#define FRICTION_F  0.75

World *World_create(int size, int width, int height) {
    World *world = malloc(sizeof(*world));
    Body *bodies = malloc(sizeof(*bodies) * size);

    if (world == NULL || bodies == NULL)
        return NULL;

    for (int i = 0; i < size; i++) {
        Body_init(&bodies[i], width, height);
    }

    *world = (World) {
            .bodies = bodies,
            .size = size,
            .width = width,
            .height = height,
    };
    return world;
}

void World_destroy(World *world) {
    if (world != NULL) {
        free(world->bodies);
        free(world);
    }
}

void World_update(World *world, const double t) {
    Body *bodies = world->bodies;
    int size = world->size;

    #pragma omp parallel for if (size > MIN_OMP_SIZE) shared(bodies) firstprivate(size) default(none)
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (i == j) continue;
            Body_applyGrav(&bodies[i], &bodies[j]);
        }
    }

    int width = world->width;
    int height = world->height;

    #pragma omp parallel for if (size > MIN_OMP_SIZE) shared(bodies) firstprivate(size, t, width, height) default(none)
    for (int i = 0; i < size; i++) {
        Body *b = &bodies[i];
        Body_move(b, t);

        double min_x = b->r;
        double min_y = b->r;
        double max_x = width - min_x;
        double max_y = height - min_y;

        if (b->pos.x < min_x || b->pos.x > max_x) {
            b->pos.x = (b->pos.x < min_x) ? min_x : max_x;
            b->vel.x *= BOUNCE_F;
            b->vel.y *= FRICTION_F;
        }
        if (b->pos.y < min_y || b->pos.y > max_y) {
            b->pos.y = (b->pos.y < min_y) ? min_y : max_y;
            b->vel.y *= BOUNCE_F;
            b->vel.x *= FRICTION_F;
        }
    }
}
