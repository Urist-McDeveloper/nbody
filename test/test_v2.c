#include <acutest.h>

#include <math.h>
#include <v2.h>

#define A   V2_of(1, 0)
#define B   V2_of(3, 4)

#define EPSILON     1e-9
#define EQ(a, b)    (fabs((a) - (b)) < EPSILON)

void add(void) {
    V2 ab = V2_add(A, B);
    V2 ba = V2_add(B, A);

    TEST_CHECK(EQ(ab.x, 4));
    TEST_CHECK(EQ(ab.y, 4));

    TEST_CHECK(EQ(ba.x, 4));
    TEST_CHECK(EQ(ba.y, 4));
}

void sub(void) {
    V2 ab = V2_sub(A, B);
    V2 ba = V2_sub(B, A);

    TEST_CHECK(EQ(ab.x, -2));
    TEST_CHECK(EQ(ab.y, -4));

    TEST_CHECK(EQ(ba.x, 2));
    TEST_CHECK(EQ(ba.y, 4));
}

void len(void) {
    TEST_CHECK(EQ(V2_len(A), 1));
    TEST_CHECK(EQ(V2_len(B), 5));
}

void scale(void) {
    V2 a = V2_scale(A, -1);
    V2 b = V2_scale(B, 1.5);

    TEST_CHECK(EQ(a.x, -1));
    TEST_CHECK(EQ(a.y, 0));

    TEST_CHECK(EQ(b.x, 4.5));
    TEST_CHECK(EQ(b.y, 6.0));
}

TEST_LIST = {
        TEST(add),
        TEST(sub),
        TEST(len),
        TEST(scale),
        TEST_LIST_END
};
