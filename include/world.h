#ifndef RAG_WORLD_H
#define RAG_WORLD_H

#include "body.h"
#include "quadtree.h"

typedef struct World World;

/* Allocate World with given SIZE, WIDTH and HEIGHT. */
World *World_create(int size, int width, int height);

/* Free previously allocated World. */
void World_destroy(World *world);

/* Update WORLD assuming T seconds passed. */
void World_update(World *world, double t);

/* Get WORLD's bodies and size into respective pointers. */
void World_getBodies(const World *world, Body **bodies, int *size);

/*
 * DEBUG
 */

const Node *World_getQuad(const World *world);

#endif //RAG_WORLD_H
