#ifndef _DEVICEDATA_H
#define _DEVICEDATA_H

#include <stdint.h>
#include <stdbool.h>
#include <string>


struct DeviceData
{
    // NV flash
    int flash_size;
    int flash_rows_per_array;
    int flash_num_arrays;
//    int flash_num_rows; // derived convenience

    int flash_code_bytes_per_row;
    int flash_code_max_size; // dervied
    uint32_t flash_code_base_address;

    int flash_config_bytes_per_row;
    int flash_config_max_size; // derived
    uint32_t flash_config_base_address;

    int flash_rows_per_protection_byte;

    int eeprom_size;
    int eeprom_bytes_per_row;
    uint32_t eeprom_base_address;

    // methods

    bool read_file(const std::string filename, const std::string devname);

    bool validate(void) const;
    void dump(void) const;
};

#endif
