#ifndef RAG_PARTICLE_PACK_H
#define RAG_PARTICLE_PACK_H

#include <rag.h>
#include <stddef.h>

/* Some number of particles pack together for vectorization. */
typedef struct ParticlePack ParticlePack;

/* Allocate ParticlePack that can fit COUNT particles. */
void AllocPackArray(uint32_t count, ParticlePack **arr, uint32_t *len);

/* Pack COUNT particles into PACKS. */
void PackParticles(uint32_t count, const Particle *ps, ParticlePack *packs);

/* Update P with PACKS. */
void PackedUpdate(Particle *p, float dt, size_t packs_len, ParticlePack *packs);

#endif //RAG_PARTICLE_PACK_H
