#include <acutest.h>

#include <body.h>
#include <v2.h>

#define PART(x, y) (Particle){ .pos = V2_of(x, y), .mass = 1.0, .radius = 0.1 }
#define BODY(x, y) (Body){ .p = PART(x, y), .vel = V2_ZERO, .acc = V2_ZERO }

void apply_grav(void) {
    Body a = BODY(0, 0);
    Body b = BODY(1, 1);
    Body_applyGrav(&a, b.p);

    TEST_CHECK(a.acc.x > 0);
    TEST_CHECK(a.acc.y > 0);
    TEST_CHECK(b.acc.x == 0);
    TEST_CHECK(b.acc.y == 0);
}

void move(void) {
    Body a = BODY(0, 0);
    Body b = BODY(1, 1);
    Body_applyGrav(&a, b.p);
    Body_applyGrav(&b, a.p);

    TEST_CHECK(a.acc.x > 0);
    TEST_CHECK(a.acc.y > 0);
    TEST_CHECK(b.acc.x < 0);
    TEST_CHECK(b.acc.y < 0);

    Body_move(&a, 1.0);
    Body_move(&b, 1.0);

    TEST_CHECK(a.p.pos.x > 0);
    TEST_CHECK(a.p.pos.y > 0);
    TEST_CHECK(b.p.pos.x < 1);
    TEST_CHECK(b.p.pos.y < 1);

    TEST_CHECK(a.vel.x > 0);
    TEST_CHECK(a.vel.y > 0);
    TEST_CHECK(b.vel.x < 0);
    TEST_CHECK(b.vel.y < 0);

    TEST_CHECK(a.acc.x == 0);
    TEST_CHECK(a.acc.y == 0);
    TEST_CHECK(b.acc.x == 0);
    TEST_CHECK(b.acc.y == 0);
}

TEST_LIST = {
        TEST(apply_grav),
        TEST(move),
        TEST_LIST_END
};
