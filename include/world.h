#ifndef RAG_WORLD_H
#define RAG_WORLD_H

#include <stdbool.h>

#include "body.h"
#include "quadtree.h"

typedef struct World World;

/* Allocate World with given SIZE, WIDTH and HEIGHT. */
World *World_create(int size, int width, int height);

/* Create an (almost) identical copy of W. */
World *World_copy(const World *w);

/* Free previously allocated W. */
void World_destroy(World *w);

/* Update W assuming T seconds passed. */
void World_update(World *w, double t, bool approx);

/* Get W's bodies and size into respective pointers. */
void World_getBodies(const World *w, Body **bodies, int *size);

/*
 * DEBUG
 */

const Node *World_getQuad(const World *w);

#endif //RAG_WORLD_H
