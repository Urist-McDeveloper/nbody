#include <rag.h>

#include <stdlib.h>

#include "body.h"
#include "util.h"

/* How velocity changes along the axis of bounce. */
#define BOUNCE_F    (-0.5f)

/* How velocity changes along the other axis. */
#define FRICTION_F  0.75f

struct World {
    Body *bodies;
    int size;
    int width;
    int height;
};

World *World_Create(int size, int width, int height) {
    V2 min = V2_ZERO;
    V2 max = V2_From(width, height);

    World *world = ALLOC(World);
    Body *bodies = ALLOC_N(size, Body);
    ASSERT(world != NULL && bodies != NULL);

    for (int i = 0; i < size; i++) {
        Particle_InitRand(&(bodies + i)->p, min, max);
        bodies[i].vel = V2_ZERO;
        bodies[i].acc = V2_ZERO;
    }

    *world = (World){
            .bodies = bodies,
            .size = size,
            .width = width,
            .height = height,
    };
    return world;
}

void World_Destroy(World *w) {
    if (w != NULL) {
        free(w->bodies);
        free(w);
    }
}

void World_Update(World *w, float dt) {
    Body *bodies = w->bodies;
    int size = w->size;

    #pragma omp parallel for firstprivate(bodies, size) default(none)
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (i != j) {
                Body_ApplyGrav(&bodies[i], bodies[j].p);
            }
        }
    }

    float width = (float)w->width;
    float height = (float)w->height;

    #pragma omp parallel for shared(bodies) firstprivate(size, dt, width, height) default(none)
    for (int i = 0; i < size; i++) {
        Body *b = &bodies[i];
        Body_Move(b, dt);

        Particle *p = &b->p;
        float min_x = p->radius;
        float min_y = p->radius;
        float max_x = width - min_x;
        float max_y = height - min_y;

        if (p->pos.x < min_x) {
            p->pos.x = min_x;
            b->vel.x *= BOUNCE_F;
            b->vel.y *= FRICTION_F;
        }
        if (p->pos.x > max_x) {
            p->pos.x = max_x;
            b->vel.x *= BOUNCE_F;
            b->vel.y *= FRICTION_F;
        }
        if (p->pos.y < min_y) {
            p->pos.y = min_y;
            b->vel.y *= BOUNCE_F;
            b->vel.x *= FRICTION_F;
        }
        if (p->pos.y > max_y) {
            p->pos.y = max_y;
            b->vel.y *= BOUNCE_F;
            b->vel.x *= FRICTION_F;
        }
    }
}

void World_GetBodies(const World *w, Body **bodies, int *size) {
    *bodies = w->bodies;
    *size = w->size;
}
