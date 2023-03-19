#ifndef NB_GALAXY_H
#define NB_GALAXY_H

#include "nbody.h"

#ifndef PI
#define PI      3.1415927f  // homegrown constants are the best
#endif

#define MIN_SPIRALS 2       // minimum number of spirals in a galaxy
#define MAX_SPIRALS 4       // maximum number of spirals in a galaxy

#define GC_MIN_R    200.f   // minimum radius of galaxy cores
#define GC_MAX_R    600.f   // maximum radius of galaxy cores
#define GC_DENSITY  30.0f   // density of galaxy cores
#define NP_MIN_R    1.5f    // minimum radius of particles
#define NP_MAX_R    9.5f    // maximum radius of particles
#define NP_DENSITY  10.f    // density of particles

/* Convert radius to mass (R is evaluated 3 times). */
#define R_TO_M(R, DENSITY)  ((4.f * PI * DENSITY / 3.f) * (R) * (R) * (R))

#define GC_R_TO_M(R)    R_TO_M(R, GC_DENSITY)   // convert galaxy core's radius to mass
#define NP_R_TO_M(R)    R_TO_M(R, NP_DENSITY)   // convert normal particle's radius to mass
#define MIN_GC_MASS     GC_R_TO_M(GC_MIN_R)     // minimum possible mass of galaxy core

#define MIN_PARTICLES_PER_GALAXY    100         // minimum number of particles per galaxy

/*
 *  A galaxy has a minimum and maximum distance from its core at which the particles can be generated:
 *      min_dist is absolute, no particle can violate it;
 *      max_dist dictates how far away a particle can be generated before its position is further randomized.
 *
 *  Let N be the number of particles and R be the core's radius. Then:
 *      min_dist = R * MIN_PARTICLE_DIST_CR_F;
 *      max_dist = R * MAX_PARTICLE_DIST_CR_F + sqrt(N) * MAX_PARTICLE_DIST_PC_F;
 *
 *  In other words:
 *      MIN_PARTICLE_DIST_CR_F affects the minimum distance between a particle and the core;
 *      MAX_PARTICLE_DIST_CR_F affects the baseline "radius" of a galaxy;
 *      MAX_PARTICLE_DIST_PC_F affects how much particles "push" galaxy's radius outwards;
 */
#define MIN_PARTICLE_DIST_CR_F  5.f
#define MAX_PARTICLE_DIST_CR_F  10.f
#define MAX_PARTICLE_DIST_PC_F  300.f

/*
 *  The algorithm that assigns position to galaxies:
 *      for galaxy N=0: position is (0, 0);
 *      for galaxy N>0:
 *          1.  pick a random "parent" galaxy from range [0, N);
 *          2.  pick a random R within MIN_SEP and MAX_SEP;
 *          3.  pick a random point P which is R units away from parent's core;
 *          4.  if no other galaxies intersect that point, then N's position is P;
 *          5.  else start from step 1;
 *      where
 *          MIN_SEP = MIN_GALAXY_SEPARATION * (N.max_dist + parent.max_dist);
 *          MAX_SEP = MAX_GALAXY_SEPARATION * (N.max_dist + parent.max_dist);
 */
#define MIN_GALAXY_SEPARATION   1.4f
#define MAX_GALAXY_SEPARATION   2.0f

/* `particle_count` must not be less than `MIN_PARTICLES_PER_GALAXY * galaxy_count`. */
Particle *MakeGalaxies(uint32_t particle_count, uint32_t galaxy_count);

#endif //NB_GALAXY_H
