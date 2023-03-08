#include <acutest.h>

#include <math.h>
#include <v2.h>

#define A   V2_From(1, 0)
#define B   V2_From(3, 4)

#define EPSILON     1e-9
#define EQ(a, b)    (fabsf((a) - (b)) < EPSILON)

void add(void) {
    V2 ab = V2_Add(A, B);
    V2 ba = V2_Add(B, A);

    TEST_CHECK(EQ(ab.x, 4));
    TEST_CHECK(EQ(ab.y, 4));

    TEST_CHECK(EQ(ba.x, 4));
    TEST_CHECK(EQ(ba.y, 4));
}

void sub(void) {
    V2 ab = V2_Sub(A, B);
    V2 ba = V2_Sub(B, A);

    TEST_CHECK(EQ(ab.x, -2));
    TEST_CHECK(EQ(ab.y, -4));

    TEST_CHECK(EQ(ba.x, 2));
    TEST_CHECK(EQ(ba.y, 4));
}

void mul(void) {
    V2 a = V2_Mul(A, -1);
    V2 b = V2_Mul(B, 1.5f);

    TEST_CHECK(EQ(a.x, -1));
    TEST_CHECK(EQ(a.y, 0));

    TEST_CHECK(EQ(b.x, 4.5f));
    TEST_CHECK(EQ(b.y, 6.0f));
}

void mag(void) {
    TEST_CHECK(EQ(V2_Mag(A), 1));
    TEST_CHECK(EQ(V2_Mag(B), 5));
}

void sq_mag(void) {
    TEST_CHECK(EQ(V2_SqMag(A), 1));
    TEST_CHECK(EQ(V2_SqMag(B), 25));
}

TEST_LIST = {
        TEST(add),
        TEST(sub),
        TEST(mul),
        TEST(mag),
        TEST(sq_mag),
        TEST_LIST_END
};
