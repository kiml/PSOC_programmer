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
#include <stdio.h>
#include <assert.h>

#include "AppData.h" 

#include "HexFileFormat.h"
#include "utils.h" 


#define DC_ECCEN_BIT    (1 << 27)

static int debug = 0;

AppData::AppData() :
    code(0), config(0), eeprom(0), protection(0),
    device_config(0),
    security_WOL(0),
    checksum(0),
    hex_file_version(HexFileFormat::VERSION),
    device_id(0),
    silicon_revision(0),
    debug_enable(0),
    reserved(0)
{
}


void AppData::clear(void)
{
    if (code) delete code; // code.clear();
    if (config) delete config; // config.clear();
    if (protection) delete protection; // protection.clear();
    if (eeprom) delete eeprom; // eeprom.clear();

    security_WOL = 0;
    device_config = 0;
    checksum = 0;

    hex_file_version = HexFileFormat::VERSION;
    device_id = 0;
    silicon_revision = 0;
    debug_enable = 0;
    reserved = 0;
}


bool AppData::read_hex_file(const char *filename, uint32_t default_base_address)
{
    // default base addr is only used in obscure cases when reading snippet hex files without a high_address record

    HexData raw;
    if (!raw.read_hex(filename, default_base_address)) return false;

    HexData *canon = raw.canonicalise();
    if (!canon) return false;

//    canon->dump(stdout);

    clear();

    code = canon->extract(HexFileFormat::FLASH_CODE_ADDRESS, HexFileFormat::FLASH_CODE_MAX_SIZE);
    config = canon->extract(HexFileFormat::CONFIG_ADDRESS, HexFileFormat::CONFIG_MAX_SIZE);
    protection = canon->extract(HexFileFormat::PROTECTION_ADDRESS, HexFileFormat::PROTECTION_MAX_SIZE);
    eeprom = canon->extract(HexFileFormat::EEPROM_ADDRESS, HexFileFormat::EEPROM_MAX_SIZE);

    checksum = canon->uint_at(HexFileFormat::CHECKSUM_ADDRESS, 2, HexData::BIGENDIAN);

    device_config = canon->uint_at(HexFileFormat::DEVCONFIG_ADDRESS, 4, HexData::LITTLEENDIAN);
    security_WOL = canon->uint_at(HexFileFormat::WOL_ADDRESS, 4, HexData::BIGENDIAN); // This seems to be encoded in hex file as BE

    // metadata
    hex_file_version = canon->uint_at(HexFileFormat::VERSION_ADDRESS, 2, HexData::BIGENDIAN);
    device_id = canon->uint_at(HexFileFormat::DEVICE_ID_ADDRESS, 4, HexData::BIGENDIAN);
    silicon_revision = canon->uint_at(HexFileFormat::SILICON_REV_ADDRESS, 1, HexData::BIGENDIAN);
    debug_enable = canon->uint_at(HexFileFormat::DEBUG_ENABLE_ADDRESS, 1, HexData::BIGENDIAN);
    reserved = canon->uint_at(HexFileFormat::METADATA_RESERVED_ADDRESS, 4, HexData::BIGENDIAN);

//    dump(true,NULL);

#if 0
    // Note: may not want to output message here.
    uint32_t calc_cksum = calc_checksum(true);
    if (calc_cksum != checksum)
        fprintf(stderr, "Warning: Checksum mismatch! Calculated 0x%04x, expected 0x%04x\n", calc_cksum, checksum);
#endif

    delete canon;
    return true;
}


bool AppData::write_hex_file(const char *filename) const
{
    // NOTE: Does not currently calculate checksum - must be precalculated.

    // Doc: 001-81290  Appendix A.1.1
    //fprintf(stderr, "write_hex_file()\n");

    FILE *fp = stdout;

    if (filename != NULL)
    {
        fp = fopen(filename, "w");
        if (fp == NULL)
        {
            fprintf(stderr, "Failed to open hex file \'%s\' for writing\n", filename);
            return false;
        }
    }

    unsigned int width = 32;

    // Written in order of increasing Hex File Addresses
    if (code) code->write_hex_data(fp,width);

    if (config) config->write_hex_data(fp, width);

    uint8_t encoded_int[4];
    uint32_to_b4_LE(device_config, encoded_int);
    HexData::write_hex_record(fp, HexFileFormat::DEVCONFIG_ADDRESS, RT_DATA, encoded_int, 4);

    uint32_to_b4_BE(security_WOL, encoded_int); // This seems to be encoded in hex file as BE !?
    HexData::write_hex_record(fp, HexFileFormat::WOL_ADDRESS, RT_DATA, encoded_int, 4);

    if (eeprom) eeprom->write_hex_data(fp, width);

    // we only save bottom two bytes
    uint16_to_b2_BE(checksum & 0xFFFF, encoded_int);
    HexData::write_hex_record(fp, HexFileFormat::CHECKSUM_ADDRESS, RT_DATA, encoded_int, 2);

    // NOTE: Ignoring docs that imply protection should be written as one hex row (note max is 255/6).
    if (protection) protection->write_hex_data(fp, width);

    uint8_t metadata[HexFileFormat::METADATA_SIZE];
    _set_metadata(metadata);
    HexData::write_hex_record(fp, HexFileFormat::METADATA_ADDRESS, RT_DATA, (uint8_t *)metadata, HexFileFormat::METADATA_SIZE);

    HexData::write_end_record(fp);

    fclose(fp);
    //fprintf(stderr, "write_hex_file(%s) END\n", filename);
    return true;
}


void AppData::_set_metadata(uint8_t metadata[HexFileFormat::METADATA_SIZE]) const
{
    memset(metadata, 0, HexFileFormat::METADATA_SIZE);

    uint16_to_b2_BE(hex_file_version, metadata+0);
    uint32_to_b4_BE(device_id, metadata+2);

    //        0006 Silicon revision (1 byte)
    //                  1 ES1 (TM)
    //                  2 ES2 (LP)
    metadata[6] = silicon_revision;

    //        0007 Debug Enable (1 byte) (advise only)
    //                  0 debugging disabled in code
    //                  1 debugging enabled in code
    metadata[7] = debug_enable;

    //        0008 Internal use by PSoC programmer (4 bytes)
    uint32_to_b4_BE(reserved, metadata+8);
}


uint32_t AppData::calc_checksum(bool truncate) const
{
    // truncate to lowest 16 bits
    // Calculates code checksum. FIXME may need to include config data if ECC == 0
    // FIXME: should I just store it in checksum field !?
    // Simple whole of program summation checksum used by PSoC hex files

    // Note checksum of each row would include both main code (256) and any CONFIG/ECC code (32)
    // FIXME: should only include code not all data in hex file
    int checksum = 0;

    // code blocks
    int nblocks, i;

    nblocks = code ? code->blockset.size() : 0;

    for(i=0; i<nblocks; i++)
    {
        const Block *block = code->blockset[i];
        int len = block->length();
        int j;
        for (j=0; j<len; j++)
        {
            checksum += block->data[j];
        }
    }

    // config blocks
    nblocks = config ? config->blockset.size() : 0;

    for(i=0; i<nblocks; i++)
    {
        const Block *block = config->blockset[i];
        int len = block->length();
        int j;
        for (j=0; j<len; j++)
        {
            checksum += block->data[j];
        }
    }

    if (truncate) checksum &= 0xFFFF;

    // FIXME: what about data in CONFIG/ECC space !?
    if (debug) fprintf(stderr,"AppData: Calc checksum is: 0x%x\n", checksum);

    return checksum;
}



bool AppData::extra_flash_used_for_config(void) const
{
    // assumes device_config is set
   return ((device_config & DC_ECCEN_BIT) == 0) ? true : false;
}


void AppData::dump(bool shortform, const char *filename) const
{
    FILE *fp = stderr;
    if (filename != NULL)
    {
        fprintf(stderr, "creating dump file:%s\n", filename);
        fp = fopen(filename, "w");
        assert(fp != NULL);
    }

    fprintf(fp, "DUMP:\n");
    fprintf(fp, "Code:\n");
    if (code) code->dump(fp, shortform ? 1024 : 0); // 0 = all
    else fprintf(fp, "NONE\n");

    fprintf(fp, "Config:\n");
    if (config) config->dump(fp, shortform? 1024: 0); // 0 = all
    else fprintf(fp, "NONE\n");

    fprintf(fp, "EEPROM:\n");
    if (eeprom) eeprom->dump(fp, shortform? 1024: 0); // 0 = all
    else fprintf(fp, "NONE\n");

    fprintf(fp, "Protection:\n");
    if (protection) protection->dump(fp, shortform? 1024: 0); // 0 = all
    else fprintf(fp, "NONE\n");

    fprintf(fp, "Device Config: 0x%04x\n", device_config);
    fprintf(fp, "  (b31-28) DIG_PHS_DLY: 0x%0x\n", (device_config & 0xf0000000) >> 28);
    fprintf(fp, "  (b27) ECCEN: %d (area avail for config: %d)\n", ((device_config & 0x08000000) ? 1 : 0), extra_flash_used_for_config());
    fprintf(fp, "  (b26-25) DPS: %d\n", (device_config & 0x06000000) >> 25);
    fprintf(fp, "  (b27) CFGSPEED: %d\n", (device_config & 0x01000000) ? 1 : 0);
    fprintf(fp, "  (b23) XRESMEN: P1[2] is %s\n", (device_config & 0x800000) ? "XRES" : "GPIO");
    fprintf(fp, "  (b22) DEBUG_EN: %d\n", (device_config & 0x4000000) ? 1 : 0);
    fprintf(fp, "\n");
    //dump_data(fp, device_config, 4, NULL);
    fprintf(fp, "WOL: 0x%04x\n", security_WOL);
    //dump_data(fp, security_WOL, 4, NULL);

    fprintf(fp, "code checksum: 0x%04x\n", checksum);
    uint32_t calc_cksum = calc_checksum(true);
    if (calc_cksum != checksum)
        fprintf(stderr, "  Warning: Checksum mismatch! Calculated 0x%04x, expected 0x%04x\n", calc_cksum, checksum);
    fprintf(fp, "device_id: 0x%08x\n", device_id);
    fprintf(fp, "hex_file_version: 0x%02x\n", hex_file_version);
    fprintf(fp, "silicon_revision: %d\n", silicon_revision);
    fprintf(fp, "debug_enable: %d\n", debug_enable);
    fprintf(fp, "reserved: 0x%08x\n", reserved);

    fprintf(fp, "END DUMP\n");

    if (fp != stderr)
        fclose(fp);
}


#if 0
void AppData::program_geom(const struct device_geometry_s &device_geom, int code_len, int *num_arrays, int *remainder_rows)
{
    //int num_bytes_per_array = device_geom.flash_max_code_size / device_geom.flash_num_arrays;
    int num_bytes_per_array = device_geom.flash_rows_per_array * device_geom.flash_code_bytes_per_row;
    //int remainder_bytes = fdata->code_length % num_bytes_per_array;
//    int code_len = fdata->code.length();
    int remainder_bytes = code_len % num_bytes_per_array;

    if (code_len == 0) fprintf(stderr, "Warning: No program code in memory so geometry will be empty\n");

    *num_arrays = code_len / num_bytes_per_array;
    *remainder_rows = 0;

    if (remainder_bytes == 0)
        return;

    (*num_arrays)++;

    *remainder_rows = remainder_bytes / device_geom.flash_code_bytes_per_row;

    if (remainder_bytes % device_geom.flash_code_bytes_per_row > 0)
        (*remainder_rows)++;
}

#endif
