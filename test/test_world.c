#include <acutest.h>

#include <world.h>
#include <body.h>
#include <v2.h>

#define PART(x, y) (Particle){ .pos = V2_of(x, y), .mass = 1.0, .radius = 0.1 }
#define BODY(x, y) (Body){ .p = PART(x, y), .vel = V2_ZERO, .acc = V2_ZERO }

void create_and_destroy(void) {
    World *w = World_create(100, 100, 100);
    TEST_ASSERT(w != NULL);
    World_destroy(w);
}

void update_approx(void) {
    World *w = World_create(2, 10, 10);
    TEST_ASSERT(w != NULL);

    int size;
    Body *bodies;
    World_getBodies(w, &bodies, &size);

    bodies[0] = BODY(1, 1);
    bodies[1] = BODY(9, 9);
    World_update(w, 1.0, true);

    TEST_CHECK(bodies[0].vel.x > 0);
    TEST_CHECK(bodies[0].vel.y > 0);
    TEST_CHECK(bodies[1].vel.x < 0);
    TEST_CHECK(bodies[1].vel.y < 0);

    World_destroy(w);
}

void update_exact(void) {
    World *w = World_create(4, 10, 10);
    TEST_ASSERT(w != NULL);

    int size;
    Body *bodies;
    World_getBodies(w, &bodies, &size);

    bodies[0] = BODY(1, 1);
    bodies[1] = BODY(9, 1);
    bodies[2] = BODY(1, 9);
    bodies[3] = BODY(9, 9);
    World_update(w, 1.0, false);

    TEST_CHECK(bodies[0].vel.x > 0);
    TEST_CHECK(bodies[0].vel.y > 0);

    TEST_CHECK(bodies[1].vel.x < 0);
    TEST_CHECK(bodies[1].vel.y > 0);

    TEST_CHECK(bodies[2].vel.x > 0);
    TEST_CHECK(bodies[2].vel.y < 0);

    TEST_CHECK(bodies[3].vel.x < 0);
    TEST_CHECK(bodies[3].vel.y < 0);

    World_destroy(w);
}

TEST_LIST = {
        TEST(create_and_destroy),
        TEST(update_approx),
        TEST(update_exact),
        TEST_LIST_END
};
