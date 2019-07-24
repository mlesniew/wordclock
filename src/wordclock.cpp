#include <LedControl.h>

#define BMP_ZERO       (bitmap + 0)
#define BMP_ONE        (bitmap + 8)
#define BMP_TWO        (bitmap + 16)
#define BMP_THREE      (bitmap + 24)
#define BMP_FOUR       (bitmap + 32)
#define BMP_FIVE       (bitmap + 40)
#define BMP_SIX        (bitmap + 48)
#define BMP_SEVEN      (bitmap + 56)
#define BMP_EIGHT      (bitmap + 64)
#define BMP_NINE       (bitmap + 72)
#define BMP_TEN        (bitmap + 80)
#define BMP_ELEVEN     (bitmap + 88)
#define BMP_TWELVE     (bitmap + 96)
#define BMP_AQUARTER   (bitmap + 104)
#define BMP_HALF       (bitmap + 112)
#define BMP_PAST       (bitmap + 120)
#define BMP_TO         (bitmap + 128)
#define BMP_DOT        (bitmap + 136)

const unsigned char bitmap[] = {
     15,   0,   0,   0,   0,   0,   0,   0,        //    0  -- ZERO
      0,   0,   0,   7,   0,   0,   0,   0,        //    8  -- ONE
      0,   0,   0,  28,   0,   0,   0,   0,        //   16  -- TWO
      0,   0,   0,   0, 248,   0,   0,   0,        //   24  -- THREE
      0,   0,   0,   0,   0,   0,  15,   0,        //   32  -- FOUR
      0,   0,   0,   0,   0, 240,   0,   0,        //   40  -- FIVE
      0,   0,   0, 224,   0,   0,   0,   0,        //   48  -- SIX
      0,   0,   0,   0,   0,   0,   0, 248,        //   56  -- SEVEN
      0,   0,   0,   0,   0,  31,   0,   0,        //   64  -- EIGHT
      0,   0,   0,   0,   0,   0,   0,  15,        //   72  -- NINE
      0,   0,   0,   0,   7,   0,   0,   0,        //   80  -- TEN
      0,   0,   0,   0,   0,   0,  48, 120,        //   88  -- ELEVEN
      0,   0,   0,   0,   0,   0, 240,  48,        //   96  -- TWELVE
     64, 127,   0,   0,   0,   0,   0,   0,        //  104  -- AQUARTER
    240,   0,   0,   0,   0,   0,   0,   0,        //  112  -- HALF
      0,   0, 240,   0,   0,   0,   0,   0,        //  120  -- PAST
      0,   0,  24,   0,   0,   0,   0,   0,        //  128  -- TO
      0,   0,   7,   0,   0,   0,   0,   0,        //  136  -- DOT
};

typedef unsigned char pix_t;
pix_t screen[8];

LedControl lc(
    D7, // din
    D5, // clk
    D8, // load
    1   // matrix count
);

void clear(pix_t * buf = screen)
{
    int y = 8;
    while (y--)
        *(buf++) = 0;
}

void add_bmp(const pix_t * src, pix_t * dst = screen)
{
    int y = 8;
    while (y--)
        *(dst++) |= *(src++);
}

void compose_time(char h, char m, pix_t * buf = screen)
{
    const pix_t * pasto = nullptr;

    if (m < 30)
    {
        // count up from the past hour
        pasto = BMP_PAST;
    } else {
        // count down to the next hour
        m = 60 - m;
        h++;
        pasto = BMP_TO;
    }

    // hours
    h = h % 12;
    if (h == 0)
    {
        // there's no zero o'clock
        h = 12;
    }
    add_bmp(BMP_ZERO + h * 8, buf);

    // minutes
    if (m < 8) {
        // assume full hour
        return;
    } else if (m < 15 + 8) {
        add_bmp(BMP_AQUARTER, buf);
    } else {
        add_bmp(BMP_HALF, buf);
    }

    add_bmp(pasto, buf);
}

unsigned char reverse_bits(unsigned char b) {
    // https://stackoverflow.com/a/2602885
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

void copy_to_display(const pix_t * src = screen)
{
    for (int y = 0; y < 8; ++y)
        lc.setRow(0, 7 - y, reverse_bits(src[y]));
}

void transition(const pix_t * dst, const pix_t * src = screen)
{
    constexpr auto speed = 20;

    // vanish
    for (int p = 0; p < 8; ++p)
    {
        for (int y = 0; y < 8; ++y)
            screen[y] <<= 1;
        copy_to_display();
        delay(speed);
    }

    // shift in the new bmp
    for (int p = 0; p < 8; ++p)
    {
        for (int y = 0; y < 8; ++y)
        {
            screen[y] <<= 1;
            screen[y] |= (dst[y] >> (7 - p)) & 1;
        }
        copy_to_display();
        delay(speed);
    }
}

void setup() {
    Serial.begin(9600);

    // enable display
    lc.shutdown(0, false);
    // set brightness
    lc.setIntensity(0, 2);
    // clear the display
    lc.clearDisplay(0);

    clear();

    Serial.println("Setup complete");
}

void loop() {
    for (int h = 0; h < 24; ++h)
        for (int m = 0; m < 60; m += 15)
        {
            pix_t tmp[8];
            clear(tmp);
            compose_time(h, m, tmp);
            transition(tmp);
            delay(3000);
        }
}

