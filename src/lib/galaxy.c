#include "galaxy.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

typedef struct GalaxyData {
    Particle *core;         // galaxy core
    Particle *particles;    // array of particles; [0] is the core
    uint32_t size;          // number of particles (including the core)
    uint32_t offset;        // first index of this galaxy's particles from global particle array
    float min_dist;         // minimum distance between the core and particles
    float max_dist;         // maximum distance between the core and particles
} GalaxyData;

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

Particle *MakeGalaxies(uint32_t particles_count, uint32_t galaxies_count) {
    ASSERT(particles_count >= galaxies_count * MIN_PARTICLES_PER_GALAXY,
           "Need at least %u particles to make %u galaxies, called with %u",
           galaxies_count * MIN_PARTICLES_PER_GALAXY, galaxies_count, particles_count);

    Particle *particles = ALLOC(particles_count, Particle);
    ASSERT(particles != NULL, "Failed to alloc %u particles", particles_count);

    GalaxyData *galaxies = ALLOC(galaxies_count, GalaxyData);
    ASSERT(galaxies != NULL, "Failed to alloc %u galaxies", galaxies_count);

    // how many particles can be randomly distributed between galaxies
    uint32_t rand_range = particles_count - galaxies_count * MIN_PARTICLES_PER_GALAXY;

    // setup galaxy size and particles
    for (uint32_t i = 0; i < galaxies_count; i++) {
        uint32_t size;
        if (i == galaxies_count - 1) {
            // the last galaxy gets all that's left
            size = rand_range;
        } else {
            size = RandUInt(0, 1 + rand_range);
            rand_range -= size;
        }

        if (i == 0) {
            galaxies[i].offset = 0;
        } else {
            galaxies[i].offset = galaxies[i - 1].offset + galaxies[i - 1].size;
        }

        galaxies[i].size = MIN_PARTICLES_PER_GALAXY + size;
        galaxies[i].particles = particles + galaxies[i].offset;
        galaxies[i].core = &galaxies[i].particles[0];
    }

    // randomize core and calculate galaxy radius
    for (uint32_t i = 0; i < galaxies_count; i++) {
        float core_radius = RandFloat(GC_MIN_R, GC_MAX_R);
        float size_root = sqrtf((float)galaxies[i].size);

        galaxies[i].min_dist = core_radius * MIN_PARTICLE_DIST_CR_F;
        galaxies[i].max_dist = core_radius * MAX_PARTICLE_DIST_CR_F + size_root * MAX_PARTICLE_DIST_PC_F;

        *galaxies[i].core = (Particle){
                .radius = core_radius,
                .mass = GC_R_TO_M(core_radius),
        };
    }

    // randomize galaxy position; first galaxy is always stationary at (0, 0)
    for (uint32_t i = 1; i < galaxies_count; i++) {
        GalaxyData galaxy = galaxies[i];
        bool collision = true;

        while (collision) {
            // choose a random initialized galaxy as a starting point
            uint32_t parent_idx = RandUInt(0, i);
            GalaxyData parent = galaxies[parent_idx];

            // find minimum and maximum distance
            float min_sep = MIN_GALAXY_SEPARATION * (galaxy.max_dist + parent.max_dist);
            float max_sep = MAX_GALAXY_SEPARATION * (galaxy.max_dist + parent.max_dist);

            // choose a random point within a circle
            float dist = sqrtf(RandFloat(min_sep * min_sep, max_sep * max_sep));
            float angle = RandFloat(0, 2 * PI);

            galaxy.core->pos.x = parent.core->pos.x + dist * cosf(angle);
            galaxy.core->pos.y = parent.core->pos.y + dist * sinf(angle);

            // check if new position collides with any previous galaxy
            collision = false;
            for (uint32_t j = 0; j < i; j++) {
                if (j == parent_idx) continue;
                GalaxyData other = galaxies[j];

                float other_min_sep = MIN_GALAXY_SEPARATION * (galaxy.max_dist + other.max_dist);
                float other_sq_dist = SqMagV2(SubV2(galaxy.core->pos, other.core->pos));

                if (other_sq_dist < other_min_sep * other_min_sep) {
                    // other galaxy is too close to the chosen position
                    collision = true;
                    break;
                }
            }
        }
    }

    // give galaxies some velocity to avoid head-on collision
    for (uint32_t i = 1; i < galaxies_count; i++) {
        Particle *a = galaxies[i].core;

        for (uint32_t j = 0; j < i; j++) {
            if (i == j) continue;
            Particle *b = galaxies[j].core;

            V2 a_to_b = SubV2(b->pos, a->pos);      // vector from a to b
            float dist = MagV2(a_to_b);             // distance between a adn b
            V2 unit = ScaleV2(a_to_b, 1.f / dist);  // unit vector

            // calculate a fraction of "orbital speed" (won't actually work as orbital speed)
            float speed_a = 0.3f * sqrtf(NB_G * b->mass / dist);
            float speed_b = 0.3f * sqrtf(NB_G * a->mass / dist);

            V2 dv_a = ScaleV2(V2_FROM(unit.y, -unit.x), speed_a);
            V2 dv_b = ScaleV2(V2_FROM(-unit.y, unit.x), speed_b);

            a->vel = AddV2(a->vel, dv_a);
            b->vel = AddV2(b->vel, dv_b);
        }
    }

    // create particles
    for (uint32_t i = 0; i < galaxies_count; i++) {
        GalaxyData galaxy = galaxies[i];
        Particle core = *galaxy.core;

        // difference between minimum and maximum distance
        // used to decide whether particle is massless or not
        float dist_range = galaxy.max_dist - galaxy.min_dist;

        // make spirals for the galaxy
        float spiral_offsets[MAX_SPIRALS];              // offset of each spiral
        float initial_offset = RandFloat(0, 2 * PI);    // to make each galaxy's spirals have different rotations

        uint32_t spiral_count = RandUInt(MIN_SPIRALS, 1 + MAX_SPIRALS);
        float spiral_angle_dist = 2 * PI / (float)spiral_count;

        for (uint32_t j = 0; j < spiral_count; j++) {
            spiral_offsets[j] = initial_offset + (float)j * spiral_angle_dist;
        }

        /*
         *  Formula of a spiral in polar coordinates: r(t) == b * t, where b is some constant.
         *  I want the spiral to:
         *
         *      1.  end with angle T1 = 2*PI at distance R1 = `galaxy.max_dist`;
         *          (R1 == r(T1) == b * T1  =>  b == R1 / T1)
         *
         *      2.  start with angle T0 at distance R0 = `galaxy.min_dist`;
         *          (R0 == r(T0) == b * T0  =>  T0 == R0 / b)
         */
        float t1 = 2 * PI;
        float b = galaxy.max_dist / t1;
        float t0 = galaxy.min_dist / b;

        // start at 1 because first particle is the core
        for (uint32_t j = 1; j < galaxy.size; j++) {
            Particle *p = &galaxy.particles[j];
            *p = (Particle){0};

            // initial angle and distance
            float t = RandFloat(t0, t1);
            float r = b * t;

            // add some randomness to make the spiral look more natural
            // non-uniform distribution is used to make sure spirals keep their shape
            float t_offset = RandFloat(0, 0.6f * sqrtf(spiral_angle_dist));
            float r_offset = RandFloat(0, 0.6f * sqrtf(fminf(b, r - galaxy.min_dist)));

            float dist = r + (RandBool() ? -1.f : 1.f) * (r_offset * r_offset);
            float ang = t + (RandBool() ? -1.f : 1.f) * (t_offset * t_offset);

            // convert polar coordinates to cartesian
            float spiral_offset = spiral_offsets[RandUInt(0, spiral_count)];
            float dx = dist * cosf(ang + spiral_offset);
            float dy = dist * sinf(ang + spiral_offset);

            p->pos.x = core.pos.x + dx;
            p->pos.y = core.pos.y + dy;

            // the farther away from the core, the higher the chance of a particle being massless
            if (RandFloat(0.f, 1.f) < (dist - galaxy.min_dist) / dist_range) {
                p->radius = 0.5f;
                p->mass = 0.f;
            } else {
                p->radius = RandFloat(NP_MIN_R, NP_MAX_R);
                p->mass = NP_R_TO_M(p->radius);
            }

            // give the particle orbital velocity
            float speed = sqrtf(NB_G * core.mass / dist);
            p->vel.x = core.vel.x + speed * (dy / dist);
            p->vel.y = core.vel.y + speed * (-dx / dist);
        }
    }

    free(galaxies);
    return particles;
}
