#ifndef _HexFileFormat_H
#define _HexFileFormat_H

#include <stdint.h>


namespace HexFileFormat
{
    static const uint32_t VERSION            = 0x0001; // 2 bytes

    static const uint32_t FLASH_CODE_ADDRESS = 0x00000000;      // FIXME Consistency - remove flash or add to others ?
    static const uint32_t FLASH_CODE_SIZE    = 0x80000000; // max address space size

    static const uint32_t CONFIG_ADDRESS     = 0x80000000; // code or ECC/config typically
    static const uint32_t CONFIG_SIZE        = 0x10000000; // max address space size

    static const uint32_t DEVCONFIG_ADDRESS  = 0x90000000; // 4 bytes DEVCONFIG_SIZE
    static const uint32_t WOL_ADDRESS        = 0x90100000; // 4 bytes

    static const uint32_t EEPROM_ADDRESS     = 0x90200000;
    static const uint32_t EEPROM_SIZE        = 0x00100000;

    static const uint32_t CHECKSUM_ADDRESS   = 0x90300000; // 2 bytes

    static const uint32_t PROTECTION_ADDRESS = 0x90400000;
    static const uint32_t PROTECTION_SIZE    = 0x00100000;

    static const uint32_t METADATA_ADDRESS   = 0x90500000; // 12 bytes

    static const uint32_t METADATA_SIZE      = 12;
    static const uint32_t VERSION_ADDRESS           = METADATA_ADDRESS + 0; // 2 bytes
    static const uint32_t DEVICE_ID_ADDRESS         = METADATA_ADDRESS + 2; // 4 bytes
    static const uint32_t SILICON_REV_ADDRESS       = METADATA_ADDRESS + 6; // 1 byte
    static const uint32_t DEBUG_ENABLE_ADDRESS      = METADATA_ADDRESS + 7; // 1 byte
    static const uint32_t METADATA_RESERVED_ADDRESS = METADATA_ADDRESS + 8; // 4 bytes
}

#endif
