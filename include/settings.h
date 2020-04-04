#ifndef SETTINGS_H
#define SETTINGS_H

#include <cstdint>

namespace settings {

    struct Settings {
        struct {
            char ntp_server[100];
            int utc_offset;
        } data;
        uint16_t checksum;
    } __attribute((packed));

    extern Settings settings;

    void sanitize();
    void load();
    void save();
    void print();
}

#endif
