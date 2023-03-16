#include "cluster.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>
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

static bool RandBool() {
    return rand() & 1;
}

Particle *MakeTwoClusters(uint32_t count) {
    ASSERT(count >= 2 * MIN_PARTICLES_PER_CLUSTER,
           "Need at least %u particles to make two clusters, called with %u",
           2 * MIN_PARTICLES_PER_CLUSTER, count);

    Particle *particles = ALLOC(count, Particle);
    ASSERT(particles != NULL, "Failed to alloc %u particles", count);

    // randomize number of particles in each cluster
    uint32_t rand_range = count - 2 * MIN_PARTICLES_PER_CLUSTER;
    uint32_t first_size = MIN_PARTICLES_PER_CLUSTER + RandUInt(0, 1 + rand_range);
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

        float min_particle_dist = 4.f * center_radius;
        data.radius = 4.f * min_particle_dist + 200.f * sqrtf((float)data.size);

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

        // Formula of a spiral in polar coordinates: r(t) == b * t, where b is some constant.
        // I want cluster spirals to end at t1 = 2*PI at distance r1 = `data.dist`. Which means:
        //
        //      r1 == r(t1) == b * t1 == b * 2*PI => b = r1 / 2*PI
        //
        // Also I want the spiral to start at distance r0 = `min_particle_dist`. Which means:
        //
        //      r0 == t(t0) == b * t0 == t0 * r1 / 2*PI => t0 = 2*PI * r0 / r1
        float b = data.radius / (2 * PI);
        float t0 = 2 * PI * min_particle_dist / data.radius;
        float t1 = 2 * PI;

        float spiral_offsets[MAX_SPIRALS];              // offset of each spiral
        float initial_offset = RandFloat(0, 2 * PI);    // to make each cluster's spiral have different rotations

        uint32_t spiral_count = RandUInt(MIN_SPIRALS, 1 + MAX_SPIRALS);
        float spiral_angle_dist = 2 * PI / (float)spiral_count;

        for (uint32_t j = 0; j < spiral_count; j++) {
            spiral_offsets[j] = initial_offset + (float)j * spiral_angle_dist;
        }

        // initialize cluster particles
        for (uint32_t j = 1; j < data.size; j++) {
            // initial angle and distance
            float t = RandFloat(t0, t1);
            float r = b * t;

            // add some randomness to both angle and distance to make the spiral look more natural
            float t_offset = RandFloat(0, 0.5f * sqrtf(spiral_angle_dist));
            float r_offset = RandFloat(0, sqrtf(fminf(b, r - min_particle_dist)));

            float dist = r + (RandBool() ? -1.f : 1.f) * (r_offset * r_offset);
            float ang = t + (RandBool() ? -1.f : 1.f) * (t_offset * t_offset);

            // particle position relative to cluster's center
            float spiral_offset = spiral_offsets[RandUInt(0, spiral_count)];
            float dx = dist * cosf(ang + spiral_offset);
            float dy = dist * sinf(ang + spiral_offset);

            float radius, mass;
            // the further away from the cluster center, the higher the chance of massless particle
            if (RandFloat(0.f, 1.f) < (dist - min_particle_dist) / dist_range) {
                radius = 0.5f;
                mass = 0.f;
            } else {
                radius = RandFloat(NP_MIN_R, NP_MAX_R);
                mass = NP_R_TO_M(radius);
            }

            // position coordinates
            float px = data.center.x + dx;
            float py = data.center.y + dy;

            // orbital velocity
            float speed = sqrtf(NB_G * data.mass / dist);
            V2 vel = ScaleV2(V2_FROM(dy, -dx), speed / dist);

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
