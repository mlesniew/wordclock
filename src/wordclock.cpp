#include <Arduino.h>
#include <LedControl.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <OneButton.h>

#include "matrix.h"

typedef unsigned char pix_t;
pix_t screen[8];

LedControl lc(2, 4, 3, 1);  /* DIN, CLK, LOAD, Matrix count */
ThreeWire tw(5, 6, 7);  /* IO, SCLK, CE */
RtcDS1302<ThreeWire> rtc(tw);
OneButton btn = OneButton(10, true, true);  /* pin, active low, enable pull-up */

unsigned char hour, minute;
bool adjusting = false;

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

int bmp_equal(const pix_t * a, const pix_t * b = screen)
{
    int y = 8;
    while (y--)
    {
        if (*(a++) != *(b++))
            return false;
    }
    return true;
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
    add_bmp(bitmap[h], buf);

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

void blit(const pix_t * src = screen)
{
    for (int y = 0; y < 8; ++y)
        lc.setRow(0, y, src[y]);
}

void transition(const pix_t * dst, const pix_t * src = screen)
{
    constexpr auto speed = 20;

    // vanish
    for (int p = 0; p < 8; ++p)
    {
        for (int y = 0; y < 8; ++y)
            screen[y] <<= 1;
        blit();
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
        blit();
        delay(speed);
    }
}

void static_effect()
{
    static unsigned int p = 0;

    for (unsigned int y = 0; y < 8; ++y)
    {
        auto val = random(0xffff);
        val = (val & 0xff) & (val >> 8);
        screen[y] = val;
    }

    if ((p >> 1) < 8)
        screen[p >> 1] |= random(256);

    ++p;
    p &= 31;

    if ((p >> 1) < 8)
        screen[p >> 1] &= random(256);

    blit();
}

void print(const RtcDateTime& dt) {
    char datestring[20];

#if 0
    snprintf_P(datestring,
            20,
            PSTR("%02u:%02u:%02u"),
            dt.Hour(),
            dt.Minute(),
            dt.Second());
#else
    snprintf_P(datestring, 
            20,
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
#endif

    Serial.print(datestring);
}

void setup() {
    Serial.begin(9600);

    printf("\n\n"
       "888       888                      888          888                   888\n"
       "888   o   888                      888          888                   888\n"
       "888  d8b  888                      888          888                   888\n"
       "888 d888b 888  .d88b.  888d888 .d88888  .d8888b 888  .d88b.   .d8888b 888  888\n"
       "888d88888b888 d88''88b 888P'  d88' 888 d88P'    888 d88''88b d88P'    888 .88P\n"
       "88888P Y88888 888  888 888    888  888 888      888 888  888 888      888888K\n"
       "8888P   Y8888 Y88..88P 888    Y88b 888 Y88b.    888 Y88..88P Y88b.    888 '88b\n"
       "888P     Y888  'Y88P'  888     'Y88888  'Y8888P 888  'Y88P'   'Y8888P 888  888\n"
       "\nBuilt on " __DATE__ " at " __TIME__ ".\n\n");

    // enable display
    lc.shutdown(0, false);
    lc.setIntensity(0, 15);
    lc.clearDisplay(0);

    // setup rtc
    rtc.Begin();

    if (rtc.GetIsWriteProtected()) {
        rtc.SetIsWriteProtected(false);
    }

    if (!rtc.IsDateTimeValid()) {
        // set date to 2020-01-01T00:00:00
        rtc.SetDateTime(0);
    }

    if (!rtc.GetIsRunning()) {
        rtc.SetIsRunning(true);
    }

    const auto now = rtc.GetDateTime();

    adjusting = now.Year() < 2020;

    Serial.print(F("RTC time: "));
    print(now);
    Serial.println();

    btn.attachClick([] {
            if (adjusting) {
                minute += 15;
                if (minute >= 60) {
                    minute = 0;
                    hour++;
                }
                if (hour >= 24) {
                    hour = 0;
                }
            }
        });

    btn.attachLongPressStart([] {
        if (adjusting) {
            rtc.SetDateTime(RtcDateTime(2020, 1, 1, hour, minute, 0));
            adjusting = false;
        } else {
            const auto now = rtc.GetDateTime();
            hour = now.Hour();
            minute = (now.Minute() / 15 * 15);
            adjusting = true;
        }
    });

    Serial.println(F("Setup complete"));
}

void loop() {
    btn.tick();

    if (adjusting) {
        clear();
        if ((millis() >> 9) & 1) {
            compose_time(hour, minute);
        }
        blit();
    } else {
        pix_t img[8];
        clear(img);
        const auto now = rtc.GetDateTime();
        compose_time(now.Hour(), now.Minute(), img);
        if (!bmp_equal(img, screen)) {
            Serial.println(F("Updating display..."));
            transition(img);
        }
        delay(1000 / 3);
    }
}
