#include "world.h"

#include <stdlib.h>

#include "err.h"
#include "body.h"
#include "quadtree.h"

/* How velocity changes along the axis of bounce. */
#define BOUNCE_F    (-0.5)

/* How velocity changes along the other axis. */
#define FRICTION_F  0.75

/*
 * WORLD
 */

struct World {
    Body *bodies;
    QuadTree *tree;
    int size;
    int width;
    int height;
};

World *World_create(int size, int width, int height) {
    V2 min = V2_ZERO;
    V2 max = V2_of(width, height);
    QuadTree *tree = QuadTree_create(min, max);

    World *world = malloc(sizeof(*world));
    Body *bodies = malloc(sizeof(*bodies) * size);
    ASSERT(world != NULL && bodies != NULL);

    for (int i = 0; i < size; i++) {
        Particle_init(&(bodies + i)->p, min, max);
    }

    *world = (World) {
            .bodies = bodies,
            .tree = tree,
            .size = size,
            .width = width,
            .height = height,
    };
    return world;
}

World *World_copy(const World *w) {
    World *copy = World_create(w->size, w->width, w->height);
    for (int i = 0; i < w->size; i++) {
        copy->bodies[i] = w->bodies[i];
    }

    return copy;
}

void World_destroy(World *w) {
    if (w != NULL) {
        QuadTree_destroy(w->tree);
        free(w->bodies);
        free(w);
    }
}

void World_update(World *w, const double t, bool approx) {
    Body *bodies = w->bodies;
    QuadTree *tree = w->tree;
    int size = w->size;

    if (approx) {
        QuadTree_update(tree, bodies, size);

        #pragma omp parallel for firstprivate(bodies, tree, size) default(none)
        for (int i = 0; i < size; i++) {
            QuadTree_applyGrav(tree, &bodies[i]);
        }
    } else {
        #pragma omp parallel for firstprivate(bodies, tree, size) default(none)
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                if (i != j) {
                    Body_applyGrav(&bodies[i], bodies[j].p);
                }
            }
        }
    }

    int width = w->width;
    int height = w->height;

    #pragma omp parallel for shared(bodies) firstprivate(size, t, width, height) default(none)
    for (int i = 0; i < size; i++) {
        Body *b = &bodies[i];
        Body_move(b, t);

        Particle *p = &b->p;
        double min_x = p->radius;
        double min_y = p->radius;
        double max_x = width - min_x;
        double max_y = height - min_y;

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

void World_getBodies(const World *w, Body **bodies, int *size) {
    *bodies = w->bodies;
    *size = w->size;
}

/*
 * DEBUG
 */

const Node *World_getQuad(const World *w) {
    return QuadTree_getQuad(w->tree);
}
