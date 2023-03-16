#ifndef NB_GALAXY_H
#define NB_GALAXY_H

#include "nbody.h"

#define MIN_SPIRALS 2       // minimal number of spirals in a galaxy
#define MAX_SPIRALS 4       // maximum number of spirals in a galaxy

#define GC_MIN_R    200.f   // minimal radius of galaxy core
#define GC_MAX_R    600.f   // maximal radius of galaxy core
#define GC_DENSITY  30.0f   // density of galaxy core
#define NP_MIN_R    1.5f    // minimal radius of a normal particle
#define NP_MAX_R    9.5f    // maximal radius of a normal particle
#define NP_DENSITY  10.f    // density of normal particles

#ifndef PI
#define PI      3.1415927f  // homegrown constants are the best
#endif

/* Convert radius to mass (R is evaluated 3 times). */
#define R_TO_M(R, DENSITY)  ((4.f * PI * DENSITY / 3.f) * (R) * (R) * (R))

#define GC_R_TO_M(R)    R_TO_M(R, GC_DENSITY)   // convert galaxy core's radius to mass
#define NP_R_TO_M(R)    R_TO_M(R, NP_DENSITY)   // convert normal particle's radius to mass
#define MIN_GS_MASS     GC_R_TO_M(GC_MIN_R)     // minimal possible mass of galaxy core
#define MAX_NP_MASS     NP_R_TO_M(NP_MAX_R)     // maximal possible mass of a normal particle

_Static_assert(MAX_NP_MASS < MIN_GS_MASS,
               "maximal mass of normal particle must be less than minimal mass of galaxy core");

#define MIN_PARTICLES_PER_GALAXY    100         // minimal number of particles per galaxy

/*
 * Create COUNT particles in two galaxies.
 * COUNT must be greater than `2 * MIN_PARTICLES_PER_GALAXY`
 */
Particle *MakeTwoGalaxies(uint32_t count);

#endif //NB_GALAXY_H
