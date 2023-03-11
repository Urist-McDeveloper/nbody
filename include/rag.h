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

/* Simulation particle. */
typedef struct Body {
    V2 pos, vel, acc;
    float mass, radius;
} Body;

/*
 * Gravitational constant; controls pulling force.
 *      g = RAG_G * mass / dist^2
 */
#define RAG_G   10.0f

/*
 * "Negative" gravity; controls pushing force.
 *      n = RAG_N * mass / dist^3
 */
#define RAG_N   (-0.8f * RAG_G * RAG_G * RAG_G)

/* A fraction of velocity that becomes friction. */
#define RAG_FRICTION    (-0.01f)

/* The simulated world with fixed boundaries and body count. */
typedef struct World World;

/* Create World of given SIZE and randomize particle positions within MIN and MAX. */
World *World_Create(int size, V2 min, V2 max);

/* Destroy W. */
void World_Destroy(World *w);

/* Update W using exact simulation. */
void World_Update(World *w, float dt);

/* Put W's body array and its size into respective pointers. */
void World_GetBodies(World *w, Body **bodies, int *size);

#endif //RAG_H
