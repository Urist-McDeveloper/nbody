#ifndef RAG_WORLD_H
#define RAG_WORLD_H

#include <stdbool.h>

#include "body.h"
#include "quadtree.h"

typedef struct World World;

/* Allocate World with given SIZE, WIDTH and HEIGHT. */
World *World_Create(int size, int width, int height);

/* Create an (almost) identical copy of W. */
World *World_Copy(const World *w);

/* Free previously allocated W. */
void World_Destroy(World *w);

/* Update W assuming T seconds passed. */
void World_Update(World *w, float t, bool approx);

/* Get W's bodies and size into respective pointers. */
void World_GetBodies(const World *w, Body **bodies, int *size);

/*
 * DEBUG
 */

const Node *World_GetQuad(const World *w);

#endif //RAG_WORLD_H
