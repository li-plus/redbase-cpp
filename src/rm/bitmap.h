#pragma once

#include <cstring>
#include <cinttypes>

static constexpr int BITMAP_WIDTH = 8;
static constexpr unsigned BITMAP_HIGHEST_BIT = 0x80u;

class Bitmap {
public:
    static void init(uint8_t *bm, int size) {
        memset(bm, 0, size);
    }

    static void set(uint8_t *bm, int pos) {
        bm[get_bucket(pos)] |= get_bit(pos);
    }

    static void reset(uint8_t *bm, int pos) {
        bm[get_bucket(pos)] &= (uint8_t) ~get_bit(pos);
    }

    static bool test(const uint8_t *bm, int pos) {
        return (bm[get_bucket(pos)] & get_bit(pos)) != 0;
    }

    static int next_bit(bool bit, const uint8_t *bm, int max_n, int curr) {
        for (int i = curr + 1; i < max_n; i++) {
            if (test(bm, i) == bit) {
                return i;
            }
        }
        return max_n;
    }

    static int first_bit(bool bit, const uint8_t *bm, int max_n) {
        return next_bit(bit, bm, max_n, -1);
    }

private:
    static int get_bucket(int pos) {
        return pos / BITMAP_WIDTH;
    }

    static uint8_t get_bit(int pos) {
        return BITMAP_HIGHEST_BIT >> (uint8_t) (pos % BITMAP_WIDTH);
    }
};
