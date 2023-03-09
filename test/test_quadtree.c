#include <acutest.h>
#include <rag.h>

#include "../src/lib/quadtree.h"

static const V2 FROM = V2_ZERO;
static const V2 TO = V2_From(10, 10);

#define PART(x, y) (Particle){ .pos = V2_From(x, y), .mass = 1, .radius = 2 }
#define BODY(x, y) (Body){ .p = PART(x, y), .vel = V2_ZERO, .acc = V2_ZERO }

void create_and_destroy(void) {
    QuadTree *t = QuadTree_Create(FROM, TO);
    TEST_ASSERT(t != NULL);
    QuadTree_Destroy(t);
}

void update_few(void) {
    const Body FEW[] = {
            BODY(1, 1),   // top left
            BODY(9, 1),   // top right
            BODY(1, 9),   // bottom left
            BODY(9, 9),   // bottom right
    };

    QuadTree *t = QuadTree_Create(FROM, TO);
    TEST_ASSERT(t != NULL);

    QuadTree_Update(t, FEW, 4);
    BHQuad quad = QuadTree_GetQuad(t);

    if (TEST_CHECK(quad != NULL)) {
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 0)));
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 1)));
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 2)));
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 3)));

        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 0)) == NULL);
        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 1)) == NULL);
        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 2)) == NULL);
        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 3)) == NULL);
    }
    QuadTree_Destroy(t);
}

void update_many() {
    const Body MANY[] = {
            BODY(1, 1),   // top left
            BODY(2, 2),   // top left
            BODY(9, 1),   // top right
            BODY(8, 2),   // top right
            BODY(1, 9),   // bottom left
            BODY(2, 8),   // bottom left
            BODY(9, 9),   // bottom right
            BODY(8, 8),   // bottom right
    };

    QuadTree *t = QuadTree_Create(FROM, TO);
    TEST_ASSERT(t != NULL);

    QuadTree_Update(t, MANY, 8);
    BHQuad quad = QuadTree_GetQuad(t);

    if (TEST_CHECK(quad != NULL)) {
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 0)));
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 1)));
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 2)));
        TEST_CHECK(!BHNode_IsEmpty(BHQuad_GetNode(quad, 3)));

        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 0)) != NULL);
        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 1)) != NULL);
        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 2)) != NULL);
        TEST_CHECK(BHNode_GetQuad(BHQuad_GetNode(quad, 3)) != NULL);
    }
    QuadTree_Destroy(t);
}

void apply_gravity_few(void) {
    Body bodies[] = {
            BODY(1, 1),   // top left
            BODY(9, 1),   // top right
            BODY(1, 9),   // bottom left
            BODY(9, 9),   // bottom right
    };

    QuadTree *t = QuadTree_Create(FROM, TO);
    TEST_ASSERT(t != NULL);

    QuadTree_Update(t, bodies, 4);
    for (int i = 0; i < 4; i++) {
        QuadTree_ApplyGrav(t, &bodies[i]);
    }

    TEST_CHECK(bodies[0].acc.x > 0);
    TEST_CHECK(bodies[0].acc.y > 0);

    TEST_CHECK(bodies[1].acc.x < 0);
    TEST_CHECK(bodies[1].acc.y > 0);

    TEST_CHECK(bodies[2].acc.x > 0);
    TEST_CHECK(bodies[2].acc.y < 0);

    TEST_CHECK(bodies[3].acc.x < 0);
    TEST_CHECK(bodies[3].acc.y < 0);

    QuadTree_Destroy(t);
}

void apply_gravity_many(void) {
    Body bodies[] = {
            BODY(1, 1),   // top left
            BODY(2, 2),   // top left
            BODY(9, 1),   // top right
            BODY(8, 2),   // top right
            BODY(1, 9),   // bottom left
            BODY(2, 8),   // bottom left
            BODY(9, 9),   // bottom right
            BODY(8, 8),   // bottom right
    };

    QuadTree *t = QuadTree_Create(FROM, TO);
    TEST_ASSERT(t != NULL);

    QuadTree_Update(t, bodies, 8);
    for (int i = 0; i < 8; i++) {
        QuadTree_ApplyGrav(t, &bodies[i]);
    }

    TEST_CHECK(bodies[0].acc.x > 0);
    TEST_CHECK(bodies[0].acc.y > 0);
    TEST_CHECK(bodies[1].acc.x > 0);
    TEST_CHECK(bodies[1].acc.y > 0);

    TEST_CHECK(bodies[2].acc.x < 0);
    TEST_CHECK(bodies[2].acc.y > 0);
    TEST_CHECK(bodies[3].acc.x < 0);
    TEST_CHECK(bodies[3].acc.y > 0);

    TEST_CHECK(bodies[4].acc.x > 0);
    TEST_CHECK(bodies[4].acc.y < 0);
    TEST_CHECK(bodies[5].acc.x > 0);
    TEST_CHECK(bodies[5].acc.y < 0);

    TEST_CHECK(bodies[6].acc.x < 0);
    TEST_CHECK(bodies[6].acc.y < 0);
    TEST_CHECK(bodies[7].acc.x < 0);
    TEST_CHECK(bodies[7].acc.y < 0);

    QuadTree_Destroy(t);
}

TEST_LIST = {
        TEST(create_and_destroy),
        TEST(update_few),
        TEST(update_many),
        TEST(apply_gravity_few),
        TEST(apply_gravity_many),
        TEST_LIST_END
};
