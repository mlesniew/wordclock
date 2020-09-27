#ifndef GFX_H
#define GFX_H

typedef unsigned char pix_t;
extern pix_t screen[8];

void clear(pix_t * buf = screen);
void blit(const pix_t * src = screen);

void add_bmp(const pix_t * src, pix_t * dst = screen);
int bmp_equal(const pix_t * a, const pix_t * b = screen);

bool get_pixel(uint8_t x, uint8_t y, const uint8_t * matrix);
void put_pixel(uint8_t x, uint8_t y, bool val);

#endif
