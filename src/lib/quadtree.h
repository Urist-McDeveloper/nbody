#ifndef RAG_QUADTREE_H
#define RAG_QUADTREE_H

#include <rag.h>

typedef struct Node Node;
typedef struct QuadTree QuadTree;

/* Allocate quadtree of given size. */
QuadTree *QuadTree_Create(V2 from, V2 to);

/* Deallocate quadtree R. */
void QuadTree_Destroy(QuadTree *t);

/* Update quadtree with array B of length N. */
void QuadTree_Update(QuadTree *t, const Body *b, int n);

/* Apply gravity of T to B. */
void QuadTree_ApplyGrav(const QuadTree *t, Body *b);

/* Get quad from T. */
BHQuad QuadTree_GetQuad(const QuadTree *t);

#endif //RAG_QUADTREE_H
