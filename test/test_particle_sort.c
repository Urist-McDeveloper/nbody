#include <acutest.h>
#include <stdint.h>

/*
 * Sort SIZE elements of ARR so that zeros come after all non-zeros.
 * Returns number of non-zero elements.
 *
 * This exact algorithm is used in world.c to sort particles by mass.
 */
uint32_t sort_zeros(int *arr, uint32_t size) {
    uint32_t i = 0, j = size;
    while (1) {
        while (i < j && arr[i] != 0) i++;   // arr[i] is the first zero element
        while (i < j && arr[--j] == 0);     // arr[j] is the last non-zero element

        // if i == j then array is sorted
        if (i == j) break;

        // swap arr[i] and arr[j]
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
    return j;
}

void test_on_non_zeros() {
    int arr[5] = {1, 2, 3, 4, 5};
    uint32_t result = sort_zeros(arr, 5);

    TEST_CHECK(result == 5);
    TEST_CHECK(arr[0] == 1);
    TEST_CHECK(arr[1] == 2);
    TEST_CHECK(arr[2] == 3);
    TEST_CHECK(arr[3] == 4);
    TEST_CHECK(arr[4] == 5);
}

void test_on_zeros() {
    int arr[5] = {0, 0, 0, 0, 0};
    uint32_t result = sort_zeros(arr, 5);

    TEST_CHECK(result == 0);
    TEST_CHECK(arr[0] == 0);
    TEST_CHECK(arr[1] == 0);
    TEST_CHECK(arr[2] == 0);
    TEST_CHECK(arr[3] == 0);
    TEST_CHECK(arr[4] == 0);
}

void test_on_sorted() {
    int arr[5] = {1, 2, 3, 0, 0};
    uint32_t result = sort_zeros(arr, 5);

    TEST_CHECK(result == 3);
    TEST_CHECK(arr[0] == 1);
    TEST_CHECK(arr[1] == 2);
    TEST_CHECK(arr[2] == 3);
    TEST_CHECK(arr[3] == 0);
    TEST_CHECK(arr[4] == 0);
}

void test_on_reverse_sorted_odd() {
    int arr[5] = {0, 0, 1, 2, 3};
    uint32_t result = sort_zeros(arr, 5);

    TEST_CHECK(result == 3);
    TEST_CHECK(arr[0] == 3);
    TEST_CHECK(arr[1] == 2);
    TEST_CHECK(arr[2] == 1);
    TEST_CHECK(arr[3] == 0);
    TEST_CHECK(arr[4] == 0);
}

void test_on_reverse_sorted_even() {
    int arr[6] = {0, 0, 0, 1, 2, 3};
    uint32_t result = sort_zeros(arr, 6);

    TEST_CHECK(result == 3);
    TEST_CHECK(arr[0] == 3);
    TEST_CHECK(arr[1] == 2);
    TEST_CHECK(arr[2] == 1);
    TEST_CHECK(arr[3] == 0);
    TEST_CHECK(arr[4] == 0);
    TEST_CHECK(arr[5] == 0);
}

void test_on_unsorted_odd() {
    int arr[5] = {0, 1, 2, 0, 3};
    uint32_t result = sort_zeros(arr, 5);

    TEST_CHECK(result == 3);
    TEST_CHECK(arr[0] == 3);
    TEST_CHECK(arr[1] == 1);
    TEST_CHECK(arr[2] == 2);
    TEST_CHECK(arr[3] == 0);
    TEST_CHECK(arr[4] == 0);
}

void test_on_unsorted_even() {
    int arr[6] = {0, 1, 2, 0, 3, 0};
    uint32_t result = sort_zeros(arr, 6);

    TEST_CHECK(result == 3);
    TEST_CHECK(arr[0] == 3);
    TEST_CHECK(arr[1] == 1);
    TEST_CHECK(arr[2] == 2);
    TEST_CHECK(arr[3] == 0);
    TEST_CHECK(arr[4] == 0);
    TEST_CHECK(arr[5] == 0);
}

TEST_LIST = {
        TEST(test_on_non_zeros),
        TEST(test_on_zeros),
        TEST(test_on_sorted),
        TEST(test_on_reverse_sorted_odd),
        TEST(test_on_reverse_sorted_even),
        TEST(test_on_unsorted_odd),
        TEST(test_on_unsorted_even),
        TEST_LIST_END
};
