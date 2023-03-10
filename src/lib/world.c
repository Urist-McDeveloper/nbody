#include <rag.h>

#include <stdlib.h>

#include "body.h"
#include "util.h"

/* The simulated world with fixed boundaries and body count. */
struct World {
    Body *bodies;
    int size;
};

/* Allocate World of given SIZE and randomize positions within MIN and MAX. */
World *World_Create(int size, V2 min, V2 max) {
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
    };
    return world;
}

/* Free previously allocated W. */
void World_Destroy(World *w) {
    if (w != NULL) {
        free(w->bodies);
        free(w);
    }
}

/* Update W using exact simulation. */
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

    #pragma omp parallel for firstprivate(bodies, size, dt) default(none)
    for (int i = 0; i < size; i++) {
        Body_Move(&bodies[i], dt);
    }
}

/* Get W's bodies and size into respective pointers. */
void World_GetBodies(const World *w, Body **bodies, int *size) {
    *bodies = w->bodies;
    *size = w->size;
}
