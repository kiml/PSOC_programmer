#ifndef _DEVICEDATA_H
#define _DEVICEDATA_H

/*
    Copyright (C) 2014 Kim Lester
    http://www.dfusion.com.au/

    This Program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This Program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this Program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
