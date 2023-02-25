#ifndef RAG_QUADTREE_H
#define RAG_QUADTREE_H

#include "body.h"

typedef struct Node Node;
typedef struct QuadTree QuadTree;

/* Allocate quadtree of given size. */
QuadTree *QuadTree_create(V2 from, V2 to);

/* Deallocate quadtree R. */
void QuadTree_destroy(QuadTree *t);

/* Update quadtree with array B of length N. */
void QuadTree_update(QuadTree *t, const Body *b, int n);

/* Apply gravity of T to B. */
void QuadTree_applyGrav(const QuadTree *t, Body *b);

#endif //RAG_QUADTREE_H
