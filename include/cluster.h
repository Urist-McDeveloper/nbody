#ifndef NB_CLUSTER_H
#define NB_CLUSTER_H

#include "nbody.h"

#define MIN_SPIRALS 2       // minimal number of spirals in a cluster
#define MAX_SPIRALS 4       // maximum number of spirals in a cluster

#define CC_MIN_R    200.f   // minimal radius of cluster center
#define CC_MAX_R    600.f   // maximal radius of cluster center
#define CC_DENSITY  30.0f   // density of cluster center
#define NP_MIN_R    1.5f    // minimal radius of a normal particle
#define NP_MAX_R    9.5f    // maximal radius of a normal particle
#define NP_DENSITY  10.f    // density of normal particles

#ifndef PI
#define PI      3.1415927f  // homegrown constants are the best
#endif

/* Convert radius to mass (R is evaluated 3 times). */
#define R_TO_M(R, DENSITY)  ((4.f * PI * DENSITY / 3.f) * (R) * (R) * (R))

#define CC_R_TO_M(R)    R_TO_M(R, CC_DENSITY)   // convert cluster center's radius to mass
#define NP_R_TO_M(R)    R_TO_M(R, NP_DENSITY)   // convert normal particle's radius to mass
#define MIN_CS_MASS     CC_R_TO_M(CC_MIN_R)     // minimal possible mass of cluster center
#define MAX_NP_MASS     NP_R_TO_M(NP_MAX_R)     // maximal possible mass of a normal particle

_Static_assert(MAX_NP_MASS < MIN_CS_MASS,
               "maximal mass of normal particle must be less than minimal mass of cluster center");

#define MIN_PARTICLES_PER_CLUSTER   100         // minimal number of particles per cluster

/*
 * Create COUNT particles in two clusters.
 * COUNT must be greater than `2 * MIN_PARTICLES_PER_CLUSTER`
 */
Particle *MakeTwoClusters(uint32_t count);

#endif //NB_CLUSTER_H
