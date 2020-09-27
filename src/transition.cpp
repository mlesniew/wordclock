#include <Arduino.h>
#include "transition.h"
#include "gfx.h"

uint8_t get_lit(uint8_t * lit, uint8_t * matrix) {
    uint8_t lit_count = 0;
    for (uint8_t y = 0; y < 8; ++y)
        for (uint8_t x = 0; x < 8; ++x) {
            if (get_pixel(x, y, matrix)) {
                lit[lit_count] = (y * 8) + x;
                ++lit_count;
            }
        }
    return lit_count;
}

void shuffle_pixels(uint8_t * lit, uint8_t lit_count) {
    for (uint8_t i = 0; i < lit_count; ++i) {
        uint8_t j = random(i, lit_count);
        uint8_t t = lit[i];
        lit[i] = lit[j];
        lit[j] = t;
    }
}

void transition_out(uint8_t * src = screen) {
    uint8_t lit[64];
    uint8_t lit_count = get_lit(lit, src);

    shuffle_pixels(lit, lit_count);

    const uint8_t loops = 12;
    for (uint8_t loop = 0; loop < loops; ++loop) {
        shuffle_pixels(lit, lit_count);
        for (uint8_t i = 0; i < lit_count; ++i) {
            uint8_t idx = lit[i];
            uint8_t x = idx & 7;
            uint8_t y = idx >> 3;
            bool lit = random(0, loops) > loop;
            put_pixel(x, y, lit);
        }
        blit();
        delay(500 / loops);
    }
}

void transition_in(uint8_t * dst) {
    uint8_t lit[64];
    uint8_t lit_count = get_lit(lit, dst);

    const uint8_t loops = 12;
    for (uint8_t loop = 0; loop < loops; ++loop) {
        shuffle_pixels(lit, lit_count);
        for (uint8_t i = 0; i < lit_count; ++i) {
            uint8_t idx = lit[i];
            uint8_t x = idx & 7;
            uint8_t y = idx >> 3;
            bool lit = random(0, loops) <= loop;
            put_pixel(x, y, lit);
        }
        blit();
        delay(500 / loops);
    }
}

void transition(uint8_t * dst) {
    transition_out();
    transition_in(dst);
}
