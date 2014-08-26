#ifndef _APPDATA_H
#define _APPDATA_H

#include <stdint.h>
#include <stdbool.h>

#include "HexData.h"


struct AppData
{
    HexData *code;
    HexData *config;
    HexData *protection;
    HexData *eeprom;

    uint32_t device_config;
    uint32_t security_WOL;
    uint32_t checksum; // only bottom two bytes are stored in hex file

    // metadata
    uint16_t hex_file_version;
    uint32_t device_id;
    uint8_t  silicon_revision;
    uint8_t  debug_enable;
    uint32_t reserved;

//    uint8_t metadata[12]; // not stored in PSoC

    // ...

    AppData();

    void clear(void);

//    void set_device_id(uint32_t device_id) { m_device_id = device_id }; // for writing to hex file
//    void get_device_id(void) { return m_device_id; }; // generally read from read_hex_file

    bool read_hex_file(const char *filename, uint32_t default_base_address=0);
    bool write_hex_file(const char *filename=NULL) const; // NULL = stdout
    void dump(bool shortform, const char *filename=NULL) const;

    void _set_metadata(uint8_t metadata[12]) const; // utility function
    uint32_t calc_checksum(bool truncate=false) const;
    bool extra_flash_used_for_config(void) const;
};

#endif
