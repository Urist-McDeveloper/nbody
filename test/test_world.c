#include <acutest.h>
#include <rag.h>

#define PART(x, y) (Particle){ .pos = V2_From(x, y), .mass = 1.0, .radius = 0.1 }
#define BODY(x, y) (Body){ .p = PART(x, y), .vel = V2_ZERO, .acc = V2_ZERO }

void create_and_destroy(void) {
    World *w = World_Create(100, V2_ZERO, V2_From(10, 10));
    TEST_ASSERT(w != NULL);
    World_Destroy(w);
}

void update(void) {
    World *w = World_Create(2, V2_ZERO, V2_From(10, 10));
    TEST_ASSERT(w != NULL);

    int size;
    Body *bodies;
    World_GetBodies(w, &bodies, &size);

    bodies[0] = BODY(1, 1);
    bodies[1] = BODY(9, 9);
    World_Update(w, 1.f);

    TEST_CHECK(bodies[0].vel.x > 0);
    TEST_CHECK(bodies[0].vel.y > 0);
    TEST_CHECK(bodies[1].vel.x < 0);
    TEST_CHECK(bodies[1].vel.y < 0);

    World_Destroy(w);
}

TEST_LIST = {
        TEST(create_and_destroy),
        TEST(update),
        TEST_LIST_END
};
