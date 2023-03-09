#ifndef RAG_H
#define RAG_H

#include <stdbool.h>
#include <math.h>   // hypotf

/* 2D vector of floats. */
typedef struct V2 {
    float x, y;
} V2;

/* Zero-length vector. */
#define V2_ZERO         (V2){ .x = 0.0, .y = 0.0 }

/* Constructor macro. */
#define V2_From(X, Y)   (V2){ .x = (X), .y = (Y) }

/* Vector addition. */
static inline V2 V2_Add(V2 a, V2 b) {
    return V2_From(a.x + b.x, a.y + b.y);
}

/* Vector subtraction. */
static inline V2 V2_Sub(V2 a, V2 b) {
    return V2_From(a.x - b.x, a.y - b.y);
}

/* Scalar multiplication. */
static inline V2 V2_Mul(V2 v, float f) {
    return V2_From(v.x * f, v.y * f);
}

/* Vector magnitude. */
static inline float V2_Mag(V2 v) {
    return hypotf(v.x, v.y);
}

/* Vector magnitude squared. */
static inline float V2_SqMag(V2 v) {
    return (v.x * v.x) + (v.y * v.y);
}


/* A point in space with mass and radius. */
typedef struct Particle {
    V2 pos;
    float mass;
    float radius;
} Particle;

/* A moving Particle with acceleration and velocity. */
typedef struct Body {
    Particle p;
    V2 vel, acc;
} Body;


/* The simulation world with fixed boundaries and body count. */
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


/* A handle for quad of nodes (Barnes-Hut simulation). */
typedef const void *BHQuad;

/* A handle for a single node (Barnes-Hut simulation). */
typedef const void *BHNode;

/* Get top-level quad of W.  */
BHQuad World_GetQuad(const World *w);

/* Get Nth node (0 <= N < 4). */
BHNode BHQuad_GetNode(BHQuad q, int n);

/* Check whether N can be split into a quad.  */
bool BHNode_HasQuad(BHNode n);

/* Split N into a quad. Will return NULL if QuadNode_HasInnerQuad(N) is false. */
BHQuad BHNode_GetQuad(BHNode n);

/* Check whether N is empty (has no bodies in it). */
bool BHNode_IsEmpty(BHNode n);

/* Get bounding box of N. */
void BHNode_GetBox(BHNode n, V2 *from, V2 *to);

#endif //RAG_H
