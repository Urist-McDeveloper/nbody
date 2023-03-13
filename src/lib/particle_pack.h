#ifndef NB_PARTICLE_PACK_H
#define NB_PARTICLE_PACK_H

#include <nbody.h>

/* Some number of particles packed together for vectorization. */
typedef struct ParticlePack ParticlePack;

/* Allocate ParticlePack that can fit COUNT particles. */
void AllocPackArray(uint32_t count, ParticlePack **arr, uint32_t *len);

/* Pack COUNT particles into PACKS. */
void PackParticles(uint32_t count, const Particle *ps, ParticlePack *packs);

/* Update P with PACKS. */
void PackedUpdate(Particle *p, float dt, uint32_t packs_len, ParticlePack *packs);

#endif //NB_PARTICLE_PACK_H
