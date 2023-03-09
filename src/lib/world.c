#include "rag.h"

#include <stdlib.h>

#include "body.h"
#include "quadtree.h"
#include "../util.h"

/* How velocity changes along the axis of bounce. */
#define BOUNCE_F    (-0.5f)

/* How velocity changes along the other axis. */
#define FRICTION_F  0.75f

struct World {
    Body *bodies;
    QuadTree *tree;
    int size;
    int width;
    int height;
};

World *World_Create(int size, int width, int height) {
    V2 min = V2_ZERO;
    V2 max = V2_From(width, height);
    QuadTree *tree = QuadTree_Create(min, max);

    World *world = ALLOC(World);
    Body *bodies = ALLOC_N(size, Body);
    ASSERT(world != NULL && bodies != NULL);

    for (int i = 0; i < size; i++) {
        Particle_InitRand(&(bodies + i)->p, min, max);
    }

    *world = (World){
            .bodies = bodies,
            .tree = tree,
            .size = size,
            .width = width,
            .height = height,
    };
    return world;
}

World *World_Copy(const World *w) {
    World *copy = World_Create(w->size, w->width, w->height);
    for (int i = 0; i < w->size; i++) {
        copy->bodies[i] = w->bodies[i];
    }

    return copy;
}

void World_Destroy(World *w) {
    if (w != NULL) {
        QuadTree_Destroy(w->tree);
        free(w->bodies);
        free(w);
    }
}

void World_Update(World *w, float t, bool approx) {
    Body *bodies = w->bodies;
    QuadTree *tree = w->tree;
    int size = w->size;

    if (approx) {
        QuadTree_Update(tree, bodies, size);

        #pragma omp parallel for firstprivate(bodies, tree, size) default(none)
        for (int i = 0; i < size; i++) {
            QuadTree_ApplyGrav(tree, &bodies[i]);
        }
    } else {
        #pragma omp parallel for firstprivate(bodies, tree, size) default(none)
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                if (i != j) {
                    Body_ApplyGrav(&bodies[i], bodies[j].p);
                }
            }
        }
    }

    float width = (float)w->width;
    float height = (float)w->height;

    #pragma omp parallel for shared(bodies) firstprivate(size, t, width, height) default(none)
    for (int i = 0; i < size; i++) {
        Body *b = &bodies[i];
        Body_Move(b, t);

        Particle *p = &b->p;
        float min_x = p->radius;
        float min_y = p->radius;
        float max_x = width - min_x;
        float max_y = height - min_y;

        if (p->pos.x < min_x || p->pos.x > max_x) {
            p->pos.x = (p->pos.x < min_x) ? min_x : max_x;
            b->vel.x *= BOUNCE_F;
            b->vel.y *= FRICTION_F;
        }
        if (p->pos.y < min_y || p->pos.y > max_y) {
            p->pos.y = (p->pos.y < min_y) ? min_y : max_y;
            b->vel.y *= BOUNCE_F;
            b->vel.x *= FRICTION_F;
        }
    }
}

void World_GetBodies(const World *w, Body **bodies, int *size) {
    *bodies = w->bodies;
    *size = w->size;
}

BHQuad World_GetQuad(const World *w) {
    return QuadTree_GetQuad(w->tree);
}
