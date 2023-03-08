#ifndef RAG_QUADTREE_H
#define RAG_QUADTREE_H

#include <stdbool.h>
#include "body.h"

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

/*
 * DEBUG
 */

/* Get quad from T. */
const Node *QuadTree_GetQuad(const QuadTree *t);

/* Get quad from N, or NULL if N is a leaf. */
const Node *Node_GetQuad(const Node *n);

/* Get Nth node from QUAD. */
const Node *Node_FromQuad(const Node *quad, int n);

/* Whether N has any bodies in it. */
bool Node_IsEmpty(const Node *n);

/* Put bounding box of N into FROM and TO. */
void Node_GetBox(const Node *n, V2 *from, V2 *to);

#endif //RAG_QUADTREE_H
