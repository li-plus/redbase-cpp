#include "rm/bitmap.h"
#include <bitset>
#include <gtest/gtest.h>

constexpr int MAX_N = 4096;

void check_equal(uint8_t *bm, std::bitset<MAX_N> &mock) {
    for (int i = 0; i < MAX_N; i++) {
        EXPECT_EQ(Bitmap::test(bm, i), mock.test(i));
    }
    int first_zero = Bitmap::first_bit(false, bm, MAX_N);
    if (first_zero < MAX_N) {
        EXPECT_FALSE(Bitmap::test(bm, first_zero));
    }
    for (int i = 0; i < first_zero; i++) {
        EXPECT_TRUE(Bitmap::test(bm, i));
    }
    int first_one = Bitmap::first_bit(true, bm, MAX_N);
    if (first_one < MAX_N) {
        EXPECT_TRUE(Bitmap::test(bm, first_one));
    }
    for (int i = 0; i < first_one; i++) {
        EXPECT_FALSE(Bitmap::test(bm, i));
    }
}

TEST(Bitmap, basic) {
    srand((unsigned)time(nullptr));

    uint8_t bm[MAX_N / Bitmap::WIDTH];
    Bitmap::init(bm, MAX_N / Bitmap::WIDTH);
    std::bitset<MAX_N> mock;

    for (int round = 0; round < 10000; round++) {
        int choice = rand() % 2;
        int pos = rand() % MAX_N;
        if (choice == 0) {
            Bitmap::set(bm, pos);
            mock.set(pos);
        } else {
            Bitmap::reset(bm, pos);
            mock.reset(pos);
        }
        check_equal(bm, mock);
    }
}
