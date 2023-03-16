#include "cluster.h"
#include "lib/util.h"

#include <stdlib.h>
#include <math.h>

typedef struct ClusterData {
    Particle *arr;
    uint32_t offset;
    uint32_t size;
    float mass;
    float radius;
    V2 center;
} ClusterData;

/* Random float in range [MIN, MAX). */
static float RandFloat(double min, double max) {
    return (float)(min + (max - min) * rand() / RAND_MAX);
}

/* Random uint32_t in range [MIN, MAX). */
static uint32_t RandUInt(uint32_t min, uint32_t max) {
    return min + ((uint32_t)rand() % (max - min));
}

Particle *MakeTwoClusters(uint32_t count) {
    ASSERT(count > 2 * MIN_PARTICLES_PER_CLUSTER,
           "Need at least %u particles to make two clusters, called with %u",
           2 * MIN_PARTICLES_PER_CLUSTER, count);

    Particle *particles = ALLOC(count, Particle);
    ASSERT(particles != NULL, "Failed to alloc %u particles", count);

    // randomize number of particles in each cluster
    uint32_t rand_range = count - 2 * MIN_PARTICLES_PER_CLUSTER;
    uint32_t first_size = MIN_PARTICLES_PER_CLUSTER + RandUInt(0, rand_range);
    uint32_t last_size = count - first_size;

    ClusterData clusters[2] = {
            {
                    .offset = 0,
                    .size = first_size,
            },
            {
                    .offset = first_size,
                    .size = last_size,
            },
    };

    // initialize clusters
    for (uint32_t i = 0; i < 2; i++) {
        ClusterData data = clusters[i];

        float center_radius = RandFloat(CC_MIN_R, CC_MAX_R);
        data.mass = CC_R_TO_M(center_radius);

        float min_particle_dist = 5.f * center_radius;
        data.radius = 2.f * min_particle_dist + 10.f * (float)data.size;

        // first cluster's position is always (0, 0)
        if (i != 0) {
            ClusterData other = clusters[0];

            float min_r = 1.2f * (other.radius + data.radius);
            float max_r = 1.4f * min_r;

            float angle = RandFloat(0, 2.f * PI);
            float dist = sqrtf(RandFloat(min_r * min_r, max_r * max_r));

            V2 delta = V2_FROM(cosf(angle) * dist, sinf(angle) * dist);
            data.center = AddV2(other.center, delta);
        }

        // cluster center particle
        data.arr = &particles[data.offset];
        data.arr[0] = (Particle){
                .pos = data.center,
                .mass = data.mass,
                .radius = center_radius,
        };

        // range of possible distances between particles and cluster center
        float dist_range = data.radius - min_particle_dist;

        // initialize cluster particles
        for (uint32_t j = 1; j < data.size; j++) {
            // random point within a circle; intentionally not uniformly distributed
            float angle = RandFloat(0, 2 * PI);
            float range = RandFloat(0, dist_range);
            float dist = min_particle_dist + range;

            float radius, mass;

            // the further away from the cluster center, the higher the chance of massless particle
            if (RandFloat(0.f, 1.f) < range / dist_range) {
                radius = 0.5f;
                mass = 0.f;
            } else {
                radius = RandFloat(NP_MIN_R, NP_MAX_R);
                mass = NP_R_TO_M(radius);
            }

            // position offsets from cluster center
            float dx = cosf(angle) * dist;
            float dy = sinf(angle) * dist;

            // position coordinates
            float px = data.center.x + dx;
            float py = data.center.y + dy;

            // orbital velocity
            float speed = sqrtf(NB_G * data.mass / dist);
            V2 vel = ScaleV2(V2_FROM(-dy, dx), speed / dist);

            data.arr[j] = (Particle){
                    .pos = V2_FROM(px, py),
                    .vel = vel,
                    .radius = radius,
                    .mass = mass,
            };
        }

        clusters[i] = data;
    }

    // make clusters move relative to each other ot avoid head-on collision
    V2 radv = SubV2(clusters[0].center, clusters[1].center);
    float d = sqrtf(radv.x * radv.x + radv.y * radv.y);
    V2 cluster_vel[] = {
            V2_FROM(-radv.y / d, radv.x / d),
            V2_FROM(radv.y / d, -radv.x / d),
    };

    for (uint32_t i = 0; i < 2; i++) {
        ClusterData data = clusters[i];
        V2 vel = ScaleV2(cluster_vel[i], RandFloat(100.f, 200.f));

        // every particle in a cluster must move together
        for (uint32_t j = 0; j < data.size; j++) {
            data.arr[j].vel = AddV2(data.arr[j].vel, vel);
        }
    }

    return particles;
}
