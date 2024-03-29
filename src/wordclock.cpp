#include <Arduino.h>
#include <LedControl.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <OneButton.h>

#include "gfx.h"
#include "matrix.h"
#include "transition.h"

// Allow displaying "HALF TO X" (besides "HALF PAST X")
//
// If ALLOW_HALF_TO is defined:
//   * "HALF PAST X" is displayed between x:24 and x:29
//   * "HALF TO X" is displayed from x:30 to x:37.
// If ALLOW_HALF_TO is not defined:
//   * "HALF PAST X" is displayed between x:24 and x:37
#define ALLOW_HALF_TO

LedControl lc(2, 4, 3, 1);  /* DIN, CLK, LOAD, Matrix count */
ThreeWire tw(5, 6, 7);      /* IO, SCLK, CE */
RtcDS1302<ThreeWire> rtc(tw);
OneButton btn = OneButton(10, true, true);  /* pin, active low, enable pull-up */

unsigned char hour, minute;
bool adjusting = false;

void compose_time(char h, char m, pix_t * buf = screen) {
    const pix_t * pasto = nullptr;

#ifdef ALLOW_HALF_TO
    if (m < 30)
#else
    if (m < 38)
#endif
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
    // enable display
    lc.shutdown(0, false);
    lc.setIntensity(0, 15);

    // after reset the display is all lit up, force it to be like that always
    for (int y = 0; y < 8; ++y)
        screen[y] = 0xff;
    blit();

    Serial.begin(9600);

    Serial.print(F("\n\n"
       "888       888                      888          888                   888\n"
       "888   o   888                      888          888                   888\n"
       "888  d8b  888                      888          888                   888\n"
       "888 d888b 888  .d88b.  888d888 .d88888  .d8888b 888  .d88b.   .d8888b 888  888\n"
       "888d88888b888 d88''88b 888P'  d88' 888 d88P'    888 d88''88b d88P'    888 .88P\n"
       "88888P Y88888 888  888 888    888  888 888      888 888  888 888      888888K\n"
       "8888P   Y8888 Y88..88P 888    Y88b 888 Y88b.    888 Y88..88P Y88b.    888 '88b\n"
       "888P     Y888  'Y88P'  888     'Y88888  'Y8888P 888  'Y88P'   'Y8888P 888  888\n"
       "\nBuilt on " __DATE__ " at " __TIME__ ".\n\n"));

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
                hour += (minute / 60);
                minute %= 60;
                hour %= 12;
            } else {
                transition(screen);
            }
        });

    btn.attachLongPressStart([] {
        if (adjusting) {
            rtc.SetDateTime(RtcDateTime(2020, 1, 1, hour, minute, 0));
            adjusting = false;
        } else {
            minute = (minute / 15 * 15);
            adjusting = true;
        }
        blit();
    });

    Serial.println(F("Setup complete."));

    // before entering the loop, wait 3 seconds to allow display inspection
    delay(3000);

    Serial.println(F("Entering loop..."));
}


void loop() {
    btn.tick();

    if (adjusting) {
        clear();
        if ((millis() >> 8) & 1) {
            compose_time(hour, minute);
        }
        blit();
    } else {
        pix_t img[8];
        clear(img);

#ifndef DEMO_MODE
        const auto now = rtc.GetDateTime();
        hour = now.Hour();
        minute = now.Minute();
#else
        minute += 1;
        hour += (minute / 60);
        minute %= 60;
        hour %= 12;
#endif

        compose_time(hour, minute, img);
        if (!bmp_equal(img, screen)) {
            Serial.println(F("Updating display..."));
            transition(img);
        }

        delay(200);
    }
}
