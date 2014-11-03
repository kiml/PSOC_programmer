#ifndef _MYHEX_H
#define _MYHEX_H

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
#include <stdint.h>
#include <stdbool.h>
#include <vector>

typedef std::vector<uint8_t> v_uint8_t;

//#include "types.h"
//#include "utils.h"


// Intel Hex File Format.
//    :CCAAAATTDDDDD...DDDKK
// where
//    ':'  literal character (start code)
//    CC   line data byte count (hex) 
//    AAAA address (hex)
//    TT   record type (hex) (see below)
//    DD.. data
//    KK   checksum : 2's compl. of LSB of sum of line contents (binary byte values) except ':' and KK

// Record (Block) Type
#define RT_DATA             0
#define RT_END              1
#define RT_EXT_SEG_ADDR     2
#define RT_START_SEG_ADDR   3
#define RT_EXT_LIN_ADDR     4
#define RT_START_LIN_ADDR   5

struct HexData;


struct Block
{
    v_uint8_t data;
    unsigned max_len;
    uint32_t base_address;
    uint8_t type;

    Block(unsigned _max_len);
    Block(uint32_t address, const uint8_t *src, uint32_t src_len, uint8_t type=RT_DATA);
    Block(uint32_t address, v_uint8_t vdata);

    uint8_t calculate_checksum(void) const;
    void clear(void);
    void dump(FILE *fp=NULL, int max_bytes=0) const;
    int length(void) const { return data.size(); }
    uint8_t *ptr(void) { return data.data(); }
};


// ============
typedef std::vector<struct Block *>  Blockset;

struct HexData
{
    Blockset blockset;

    public:

    enum {BIGENDIAN, LITTLEENDIAN};

    HexData();
    //HexData(const char *filename=NULL, uint32_t default_base_address=0);
    // HexData(uint32_t base_address, uint8_t *data, int len);
    HexData(uint32_t base_address, v_uint8_t vdata);

//    HexData& operator=(const HexData& other);
//    HexData (const HexData& other);

    // high level
    bool read_hex(const char *filename, uint32_t default_base_address=0);
    bool write_hex(const char *filename) const;
    bool write_hex_data(FILE *fp, unsigned int width=0) const;
    void _write_hex_data(FILE *fp) const;

    void add(const Block &block);
    void add(uint32_t base_address, v_uint8_t vdata);
    void add_hex(uint32_t base_address, const char *hex_str);
    HexData *reshape(int new_max_len) const;
    HexData *canonicalise(void) const { return reshape(0); }

    HexData *extract(uint32_t start_address, uint32_t address_length) const;
    v_uint8_t extract2vector(uint32_t start_address, uint32_t address_length) const;
    uint8_t *extract2bin(uint32_t start_address, uint32_t address_length, uint8_t *data=NULL) const;

    //Block &operator[](std::size_t i) { return (*blockset)[i]; }
    //const Block &operator[](std::size_t i) const { return const_cast<Block&>(*blockset)[i]; }
    Block *operator[](std::size_t i) { return blockset[i]; }
    const Block *operator[](std::size_t i) const { return const_cast<Block*>(blockset[i]); }

    // support
    void dump(FILE *fp=NULL, int max_bytes=0) const;
    int nblocks(void) const { return blockset.size(); }
    int length(void) const;
    //int length(uint32_t start_address, uint32_t address_length) const;
    bool minmax_address(uint32_t range_start_address, uint32_t range_address_length, uint32_t *min_address, uint32_t *max_address) const;

    void trim(void);
    void clear(void) { blockset.clear(); }
    uint32_t uint_at(uint32_t address, unsigned int len, uint8_t endian) const;

    static uint32_t parse_hex_int(const char **hex_buffer, int num_hex_digits);
    static uint8_t *hexstr2bin(const char *hex_buffer, int num_hex_digits, uint8_t *bin_buffer);
    static const char *type_str(int type);

    static void write_block_record(FILE *fp, const Block *block);
    static void write_ext_address_record(FILE *fp, uint32_t high_address);
    static void write_end_record(FILE *fp);
    static void write_raw_record(FILE *fp, uint32_t address, uint8_t type, const uint8_t *data, int len);
    static void write_raw_record(FILE *fp, uint32_t address, uint8_t type, const v_uint8_t data);
    static void write_hex_record(FILE *fp, uint32_t address, uint8_t type, const uint8_t *data, int len);
    static void write_hex_record(FILE *fp, uint32_t address, uint8_t type, const v_uint8_t data);
};

#endif
