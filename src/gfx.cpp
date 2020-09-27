#include <LedControl.h>
#include "gfx.h"

pix_t screen[8];
extern LedControl lc;

void blit(const pix_t * src) {
    for (int y = 0; y < 8; ++y)
        lc.setRow(0, y, src[y]);
}

void clear(pix_t * buf) {
    int y = 8;
    while (y--)
        *(buf++) = 0;
}

void add_bmp(const pix_t * src, pix_t * dst) {
    int y = 8;
    while (y--)
        *(dst++) |= *(src++);
}

int bmp_equal(const pix_t * a, const pix_t * b) {
    int y = 8;
    while (y--)
    {
        if (*(a++) != *(b++))
            return false;
    }
    return true;
}

bool get_pixel(uint8_t x, uint8_t y, const uint8_t * matrix) {
    return (matrix[y] >> x) & 1;
}

void put_pixel(uint8_t x, uint8_t y, bool val) {
    if (val)
        screen[y] |= 1 << x;
    else
        screen[y] &= ~(1 << x);
}

