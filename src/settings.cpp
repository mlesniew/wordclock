#include <cstdio>
#include <CRC.h>
#include <EEPROM.h>
#include "settings.h"

#define DEFAULT_NTP_SERVER "pool.ntp.org"
#define DEFAULT_UTC_OFFSET 60

namespace settings {

Settings settings;

void sanitize() {
    if (settings.data.utc_offset > 60 * 14)
        settings.data.utc_offset = 60 * 14;

    if (settings.data.utc_offset < -60 * 12)
        settings.data.utc_offset = -60 * 12;

    if (strlen(settings.data.ntp_server) == 0)
        strcpy(settings.data.ntp_server, DEFAULT_NTP_SERVER);
}

void load() {
    EEPROM.begin(sizeof(Settings));
    EEPROM.get(0, settings);

    const auto expected_checksum = CRC::crc16(&settings.data, sizeof(settings.data));
    if (settings.checksum != expected_checksum) {
        printf("Invalid CRC of settings in flash, using defaults.\n");
        settings.data.ntp_server[0] = '\0';
        settings.data.utc_offset = DEFAULT_UTC_OFFSET;
    } else {
        printf("Loaded settings from flash, CRC correct.\n");
        settings.data.ntp_server[99] = '\0';  // just in case
    }

    sanitize();
    print();
}

void save() {
    settings.checksum = CRC::crc16(&settings.data, sizeof(settings.data));
    EEPROM.put(0, settings);
    EEPROM.commit();
}

void print() {
    printf("Settings:\n");
    printf("  server: %s\n", settings.data.ntp_server);
    printf("  offset: %i:%02i\n", settings.data.utc_offset / 60, settings.data.utc_offset % 60);
}

}
