#include <CRC.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <LedControl.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#define HOSTNAME "wordclock"
#define DEFAULT_NTP_SERVER "pool.ntp.org"
#define DEFAULT_UTC_OFFSET 60

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

struct Settings {
    struct {
        char ntp_server[100];
        int utc_offset;
    } data;
    uint16_t checksum;
} __attribute((packed));

Settings settings;

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

void sanitize_settings() {
    if (settings.data.utc_offset > 60 * 14)
        settings.data.utc_offset = 60 * 14;

    if (settings.data.utc_offset < -60 * 12)
        settings.data.utc_offset = -60 * 12;

    if (strlen(settings.data.ntp_server) == 0)
        strcpy(settings.data.ntp_server, DEFAULT_NTP_SERVER);
}

void load_settings() {
    EEPROM.begin(sizeof(Settings));
    EEPROM.get(0, settings);

    const auto expected_checksum = CRC::crc16(&settings.data, sizeof(settings.data));
    if (settings.checksum != expected_checksum) {
        printf("Invalid CRC of settings in flash, using defaults.\n");
        settings.data.ntp_server[0] = '\0';
        settings.data.utc_offset = DEFAULT_UTC_OFFSET;
    } else {
        printf("Loaded settings from flash, CRC correct.");
        settings.data.ntp_server[99] = '\0';  // just in case
    }

    sanitize_settings();
}

void save_settings() {
    settings.checksum = CRC::crc16(&settings.data, sizeof(settings.data));
    EEPROM.put(0, settings);
    EEPROM.commit();
}

void print_settings() {
    printf("Settings:\n");
    printf("  server: %s\n", settings.data.ntp_server);
    printf("  offset: %i:%02i\n", settings.data.utc_offset / 60, settings.data.utc_offset % 60);
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

    load_settings();
    print_settings();

    setup_wifi();

    SPIFFS.begin();

    server.serveStatic("/", SPIFFS, "/index.html");

    server.on("/version", []{
            server.send(200, "text/plain", __DATE__ " " __TIME__);
            });

    server.on("/uptime", []{
            server.send(200, "text/plain", String(millis() / 1000));
            });

    server.on("/settings/save", []{
            auto ntp_server = server.arg("ntpServer");
            auto offset = server.arg("timeOffset");

            ntp_server.trim();
            offset.trim();

            ntp_server.toCharArray(settings.data.ntp_server, 100);
            settings.data.utc_offset = offset.toInt();
            sanitize_settings();
            print_settings();
            save_settings();

            ntp_client.setPoolServerName(settings.data.ntp_server);
            ntp_client.setTimeOffset(settings.data.utc_offset * 60);

            server.send(200, "text/plain", "");
            });

    ntp_client.begin();
    ntp_client.setPoolServerName(settings.data.ntp_server);
    ntp_client.setTimeOffset(settings.data.utc_offset * 60);

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
