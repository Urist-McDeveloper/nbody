#ifndef NB_PARTICLE_PACK_H
#define NB_PARTICLE_PACK_H

#include <nbody.h>
#include <stdint.h>

/* Some number of particles packed together for vectorization. */
typedef struct ParticlePack ParticlePack;

/*
 * Allocate ParticlePack array that can fit COUNT particles.
 * ARR is set to allocated array.
 * LEN is set to array size.
 */
void AllocPackArray(ParticlePack **arr, uint32_t *len, uint32_t count);

/* Pack COUNT particles into PACKS. */
void PackParticles(uint32_t count, const Particle *ps, ParticlePack *packs);

/* Update P with PACKS. */
void PackedUpdate(Particle *p, float dt, uint32_t packs_len, ParticlePack *packs);

#endif //NB_PARTICLE_PACK_H
