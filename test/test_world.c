#include <acutest.h>
#include <rag.h>

#define PART(x, y) (Particle){ .pos = V2_From(x, y), .mass = 1.0, .radius = 0.1 }
#define BODY(x, y) (Body){ .p = PART(x, y), .vel = V2_ZERO, .acc = V2_ZERO }

void create_and_destroy(void) {
    World *w = World_Create(100, 100, 100);
    TEST_ASSERT(w != NULL);
    World_Destroy(w);
}

void update_approx(void) {
    World *w = World_Create(2, 10, 10);
    TEST_ASSERT(w != NULL);

    int size;
    Body *bodies;
    World_GetBodies(w, &bodies, &size);

    bodies[0] = BODY(1, 1);
    bodies[1] = BODY(9, 9);
    World_UpdateBH(w, 1.f);

    TEST_CHECK(bodies[0].vel.x > 0);
    TEST_CHECK(bodies[0].vel.y > 0);
    TEST_CHECK(bodies[1].vel.x < 0);
    TEST_CHECK(bodies[1].vel.y < 0);

    World_Destroy(w);
}

void update_exact(void) {
    World *w = World_Create(4, 10, 10);
    TEST_ASSERT(w != NULL);

    int size;
    Body *bodies;
    World_GetBodies(w, &bodies, &size);

    bodies[0] = BODY(1, 1);
    bodies[1] = BODY(9, 1);
    bodies[2] = BODY(1, 9);
    bodies[3] = BODY(9, 9);
    World_UpdateExact(w, 1.f);

    TEST_CHECK(bodies[0].vel.x > 0);
    TEST_CHECK(bodies[0].vel.y > 0);

    TEST_CHECK(bodies[1].vel.x < 0);
    TEST_CHECK(bodies[1].vel.y > 0);

    TEST_CHECK(bodies[2].vel.x > 0);
    TEST_CHECK(bodies[2].vel.y < 0);

    TEST_CHECK(bodies[3].vel.x < 0);
    TEST_CHECK(bodies[3].vel.y < 0);

    World_Destroy(w);
}

TEST_LIST = {
        TEST(create_and_destroy),
        TEST(update_approx),
        TEST(update_exact),
        TEST_LIST_END
};
