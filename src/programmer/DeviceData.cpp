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

#include <stdio.h>

#include "DeviceData.h"

#include "HierINIReader.h"


#if 0
DeviceData::DeviceData(const char *filename, const char *device)
    flash_max_code_size(0),
    flash_max_config_size(0),
    flash_code_bytes_per_row(0),
    flash_config_bytes_per_row(0),
    flash_code_base_address(0),
    flash_config_base_address(0),
    flash_rows_per_array(0),
    flash_num_arrays(0),
    flash_rows_per_protection_byte(0),
    eeprom_base_address(0),
    eeprom_size(0),
    eeprom_bytes_per_row(0)
{
}
#endif

bool DeviceData::read_file(const std::string filename, const std::string device)
{
    HierINIReader reader(filename);

    if (reader.ParseError() < 0) {
        //std::cout << "Can't read '%s'\n" << filename;
        return false;
    }

    flash_size = reader.GetInteger(device, "flash_size", 0 );
    flash_rows_per_array = reader.GetInteger(device, "flash_rows_per_array", 0);
    flash_num_arrays = reader.GetInteger(device, "flash_num_arrays", 0); // DERIVED - Easy to Calc ??

    flash_rows_per_protection_byte = reader.GetInteger(device, "flash_rows_per_protection_byte", 0);

    int total_flash_rows = flash_rows_per_array * flash_num_arrays;

    flash_code_bytes_per_row = reader.GetInteger(device, "flash_code_bytes_per_row", 0);
    flash_code_max_size = total_flash_rows * flash_code_bytes_per_row;
    flash_code_base_address = reader.GetUint32(device, "flash_code_base_address", 0);

    flash_config_bytes_per_row = reader.GetInteger(device, "flash_config_bytes_per_row", 0);
    flash_config_max_size = total_flash_rows * flash_config_bytes_per_row;
    flash_config_base_address = reader.GetUint32(device, "flash_config_base_address", 0);

    eeprom_size = reader.GetInteger(device, "eeprom_size", 0);
    eeprom_bytes_per_row = reader.GetInteger(device, "eeprom_bytes_per_row", 0);
    eeprom_base_address = reader.GetUint32(device, "eeprom_base_address", 0);

    return true;
}


bool DeviceData::validate(void) const
{
    // basic sanity checks
    if (flash_size == 0) return false;
    if (flash_rows_per_array == 0) return false;
    if (flash_num_arrays == 0) return false;
    if (flash_rows_per_protection_byte == 0) return false;

    if (flash_code_bytes_per_row == 0) return false;
    if (flash_code_max_size == 0) return false;

    if (flash_config_bytes_per_row == 0) return false;
    if (flash_config_max_size == 0) return false;

    if (eeprom_size == 0) return false;
    if (eeprom_bytes_per_row == 0) return false;

    // misc sanity checks
    if (flash_config_base_address == 0) return false;
    if (eeprom_base_address == 0) return false;
    if (flash_code_base_address == flash_config_base_address) return false;

    return true;
}


void DeviceData::dump(void) const
{
    fprintf(stderr, "Flash size: %d\n", flash_size);
    fprintf(stderr, "Flash rows per array: %d\n", flash_rows_per_array);
    fprintf(stderr, "Flash num arrays: %d\n", flash_num_arrays);
    fprintf(stderr, "Flash rows per protection byte: %d\n", flash_rows_per_protection_byte);

    fprintf(stderr, "Flash code bytes per row: %d\n", flash_code_bytes_per_row);
    fprintf(stderr, "Flash code max size: %d\n", flash_code_max_size);
    fprintf(stderr, "Flash code base address: 0x%08x\n", flash_code_base_address);

    fprintf(stderr, "Flash config bytes per row: %d\n", flash_config_bytes_per_row);
    fprintf(stderr, "Flash config max size: %d\n", flash_config_max_size);
    fprintf(stderr, "Flash config base address: 0x%08x\n", flash_config_base_address);

    fprintf(stderr, "EEPROM size: %d\n", eeprom_size);
    fprintf(stderr, "EEPROM bytes per row: %d\n", eeprom_bytes_per_row);
    fprintf(stderr, "EEPROM base address: 0x%08x\n", eeprom_base_address);

}
