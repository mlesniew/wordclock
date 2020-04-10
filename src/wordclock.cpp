#include <ESP8266WebServer.h>
#include <FS.h>
#include <LedControl.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#include "settings.h"
#include "matrix.h"

#define HOSTNAME "wordclock"

typedef unsigned char pix_t;
pix_t screen[8];

LedControl lc(
    D8, // din
    D7, // clk
    D6, // load
    1   // matrix count
);

WiFiUDP ntpUDP;
NTPClient ntp_client(ntpUDP);
Ticker ticker;
ESP8266WebServer server;

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

void copy_to_display(const pix_t * src = screen)
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

void reboot()
{
    printf("Reboot...\n");
    while (true)
    {
        ESP.restart();
        delay(10 * 1000);
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

    copy_to_display();
}


void setup_wifi()
{
    WiFi.hostname(HOSTNAME);
    WiFiManager wifiManager;

    wifiManager.setConfigPortalTimeout(60);
    if (!wifiManager.autoConnect(HOSTNAME, "password"))
    {
        printf("AutoConnect failed, retrying in 15 minutes...\n");
        delay(15 * 60 * 1000);
        reboot();
    }
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
    // set brightness
    lc.setIntensity(0, 15);
    // clear the display
    lc.clearDisplay(0);

    ticker.attach(0.05, static_effect);

    settings::load();

    setup_wifi();

    SPIFFS.begin();

    server.serveStatic("/", SPIFFS, "/index.html");

    server.on("/version", []{
            server.send(200, "text/plain", __DATE__ " " __TIME__);
            });

    server.on("/uptime", []{
            server.send(200, "text/plain", String(millis() / 1000));
            });

    server.on("/settings", []{
            char buf[250];
            snprintf(buf, sizeof(buf),
                     "{\"ntpServer\":\"%s\", \"timeOffset\": %i}",
                     settings::settings.data.ntp_server,
                     settings::settings.data.utc_offset);
            server.send(200, "application/json", buf);
            });

    server.on("/settings/save", []{
            auto ntp_server = server.arg("ntpServer");
            auto offset = server.arg("timeOffset");

            ntp_server.trim();
            offset.trim();

            ntp_server.toCharArray(settings::settings.data.ntp_server, 100);
            settings::settings.data.utc_offset = offset.toInt();
            settings::sanitize();
            settings::print();
            settings::save();

            ntp_client.setPoolServerName(settings::settings.data.ntp_server);
            ntp_client.setTimeOffset(settings::settings.data.utc_offset * 60);

            server.sendHeader("Location", "/", true);
            server.send(302, "text/plain", "Settings saved.");
            });

    ntp_client.begin();
    ntp_client.setPoolServerName(settings::settings.data.ntp_server);
    ntp_client.setTimeOffset(settings::settings.data.utc_offset * 60);

    Serial.println("Setup complete");
    server.begin();
}

void loop() {
    bool got_time = ntp_client.update();
    static bool ever_got_time = false;

    if (got_time)
    {
        if (!ever_got_time)
        {
            ticker.detach();
            ever_got_time = true;
        }
        pix_t img[8];
        clear(img);
        compose_time(ntp_client.getHours(), ntp_client.getMinutes(), img);
        if (!bmp_equal(img, screen)) {
            Serial.println("Updating display...");
            transition(img);
        }
    }
    server.handleClient();
}
