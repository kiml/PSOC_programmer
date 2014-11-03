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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "HexData.h"
#include "utils.h"


static int debug = 0;

void dump_vector(FILE *fp, const char *msg, const v_uint8_t v);

// =======================

Block::Block(unsigned _max_len)
    : max_len(_max_len), type(RT_DATA), base_address(0)
{
    if (max_len != 0)
        data.reserve(max_len);
}


Block::Block(uint32_t address, v_uint8_t vdata)
    : max_len(vdata.size()), type(RT_DATA), base_address(address)
{
    data = vdata;
}


Block::Block(uint32_t address, const uint8_t *src, uint32_t src_len, uint8_t type)
    : max_len(src_len), type(type), base_address(address)
{
//    assert(src_len > 0);
    
    data.resize(src_len);
    if (src_len > 0)
        memcpy(ptr(), src, src_len);
}


void Block::clear(void)
{
   data.clear();
   base_address = 0;
   type = RT_DATA; // !?
}


#if 0
bool Block::append(const uint8_t *src, int src_len)
{
    if (len + src_len > max_len)
        return false;
    memcpy(data+len, src, src_len);
    len += src_len;
}
#endif


uint8_t Block::calculate_checksum(void) const
{
    uint8_t sum = 0;
    int i;
    int len = data.size();

    uint32_t low_address = base_address & 0xFFFF;
    // We store full 24+ bit address in base_address but checksum is based on bottom 16 bits only.
    // because written records can only contain 16 bit addresses
    sum = low_address >> 8;
    sum += low_address & 0xFF;
    sum += type;
    sum += len;

    for (i=0; i<len; i++)
    {
        sum += data[i];
    }

    // 2's complement
    sum = (~sum) + 1;
    // Note: although ~ works since uint is unsigned and FF+1=00 (byte)
    // it might be better to use: sum = ((sum ^ 0xFF) + 1) & 0xFF
    return sum;
}


void Block::dump(FILE *fp, int max_bytes) const
{
    if (fp == NULL) fp = stderr;
    if (max_bytes == 0)  max_bytes = max_len;

    int len = data.size();
    int dump_len = MIN(len, max_bytes);

    fprintf(fp,"(%d/%d)", dump_len, len);
    int i;
    for (i=0; i < dump_len; i++)
    {
        fprintf(fp, " %02x", data[i]);
    }
    fprintf(fp, "%s\n", (dump_len < len ? "...":""));
}


// ============

HexData::HexData()
{
    blockset.reserve(100); // FIXME: any justification for this
}

#if 0
HexData::HexData(const char *filename, uint32_t default_base_address)
{
    blockset.reserve(100);

    if (filename && !read_hex(filename, default_base_address))
        throw std::runtime_error(std::string("Failed to read file: ") + std::string(filename));
}
#endif


HexData::HexData(uint32_t base_address, v_uint8_t vdata)
{
    blockset.reserve(vdata.size()/16 + 2); // FIXME: hack

    //bin2block(base_address, data, len);
    add(base_address, vdata);
}


uint32_t HexData::parse_hex_int(const char **hex_buffer, int num_hex_digits)
{
    // parse data and consume input. hex_buffer is const
 
    assert(num_hex_digits <= 4);
    char buff[5]; // assumes buffer is at least num_hex_digits + 1 long
    memset(buff, 0, 5);
    memcpy(buff, *hex_buffer, num_hex_digits);

    uint32_t value = strtol(buff, NULL, 16);

#if 0
    int i;
    uint32_t value = 0;
    for (i=0; i<num_hex_digits; i++)
    {
        uint8_t digit = hex2int((*hexbuffer)[i]);
        value = (value < 4 + digit);
    }
#endif
#if 0
    uint8_t tmp = (*hex_buffer)[num_hex_digits];
    (*hex_buffer)[num_hex_digits] = '\0';

    uint32_t value = strtol(*hex_buffer, NULL, 16);
    (*hex_buffer)[num_hex_digits] = tmp;
#endif

    *hex_buffer += num_hex_digits;

    return value;
}


bool HexData::read_hex(const char *filename, uint32_t default_base_address)
{
    // could have flags including:
    //      raw mode - read records as is no clever stuff,
    //      normal mode - keep data records as is but coalesce other record types
    //      canonical mode - one record per contig block.

    // default base address is to handle obscure cases where no address is set and default of 0 is wrong.
    // eg only occurs in partial hex files. Will be overridden by next high_address setting within file.
    // Note address is 32bits not 16 bits to be shifted later.

    // define a canonical format and store in that.

    FILE *fp = fopen(filename, "r");
    if (!fp) return false;

    char line[255];
    int line_num = 1;
    uint32_t high_address = default_base_address; // preshifted. Use + not | to add low address (because of type 2 records)

    while (fgets(line, 255, fp) != NULL)
    {
        const char *bp = line;
        if (*bp != ':')
        {
            fprintf(stderr, "%s: Line %d. Unexpected char. Expected ':' found '%c'\n", filename, line_num, *bp); 
            return false;
        }
        bp++;

        uint32_t count = parse_hex_int(&bp, 2);

        Block *block = new Block(count);
        assert(block);

        uint16_t low_address = parse_hex_int(&bp, 4);
        // Note low_address + count could potentially wrap 16 bits. But well-formed hex should not be a problem.

        block->base_address = high_address + low_address; // use + not | to add lsb
        block->type = (int)parse_hex_int(&bp, 2);

        block->data.resize(count);
        hexstr2bin(bp, count*2, block->ptr());
        bp += count*2;

        if (debug)
        {
            fprintf(stderr, "Line %d: %2d (0x%08x %04x) ", line_num, block->type, high_address, low_address);
            block->dump(stderr);
        }

        uint32_t checksum = parse_hex_int(&bp, 2);

        uint8_t calc_checksum = block->calculate_checksum();
        if (checksum != calc_checksum)
        {
            fprintf(stderr, "%s: Line %d. Bad checksum. Read 0x%02x, calculated 0x%02x\n",
                filename, line_num, checksum, calc_checksum);
            block->dump(stderr);
            delete block;
            fclose(fp);
            assert(0 && "bad checksum"); // TEMP
            return false;
        }

        if (block->type == RT_DATA) // 0 - most common first
        {
            // do nothing special
            blockset.push_back(block);
        }

        else if (block->type == RT_EXT_SEG_ADDR) // 2
        {
            // address is bits 4 - 19 (it's an archaic X86 thing)
            assert(block->length() == 2);
            high_address = ((block->data[0] << 8) | block->data[1]) << 4;
        }
        else if (block->type == RT_EXT_LIN_ADDR) // 4
        {
            assert(block->length() == 2);
            high_address = ((block->data[0] << 8) | block->data[1]) << 16;
            delete block;
        }

        else if (block->type == RT_START_SEG_ADDR) // 3
        {
            // 16bit application start address. (x86 ==  CS:IP register)
            // Not stored in output so can be ignored/discarded.
            assert(block->length() == 4);
            uint32_t start_address = B4BE_to_U32(block->data);
            fprintf(stderr, "Ignoring embedded start address (seg): 0x%08x\n", start_address);
            // use or discard
            delete block;
        }
        else if (block->type == RT_START_LIN_ADDR) // 5
        {
            // 32bit application start address (pre_main) typically but not startup code
            // 32bit Address space mode only (x86). ARM: odd address implies thumb code (ARM)
            // Not stored in output so can be ignored/discarded.

            assert(block->length() == 4);
            uint32_t start_address = B4BE_to_U32(block->data);
            fprintf(stderr, "Ignoring embedded start address (lin): 0x%08x\n", start_address);
            // use or discard
            delete block;
        }

        else if (block->type == RT_END) // 1
        {
            // Note: exclude the RT_END marker from stored data add add it on write
            delete block;
        }
        else
        {
            fprintf(stderr, "Unhandled block type: %d\n", block->type);
            assert(0 && "Unhandled block type");
        }

        line_num++;
    }

    fclose(fp);
    return true;
}


bool HexData::write_hex(const char *filename) const
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        return false;

    write_hex_data(fp);
    write_end_record(fp);

    fclose(fp);
    return true;
}


bool HexData::write_hex_data(FILE *fp, unsigned int width) const
{
    if (width > 0)
    {
        if (width > 255)
        {
            fprintf(stderr, "FATAL: Cannot write hex records > 255 bytes. Use reshape() to change record length.\n");
            assert(0 && "bad hex record length");
            return false;
        }

        HexData *tmp = reshape(width);
        tmp->_write_hex_data(fp);
        delete tmp;

        return true;
    }

    // if width == 0 should I examine datastructure widths !? nah just use assert
        
    _write_hex_data(fp);
    return true;
}


void HexData::_write_hex_data(FILE *fp) const
{
    int nblocks = blockset.size();
    uint32_t high_address = 0;
        
    int i;
    for(i=0; i<nblocks; i++)
    {
        //uint32_t low_address = block->base_address & 0xFFFF;
        uint32_t block_high_address = blockset[i]->base_address >> 16;
        if (block_high_address != high_address)
        {
            high_address = block_high_address;
            // High Address record
            write_ext_address_record(fp, high_address);
        }

        write_block_record(fp, blockset[i]);
    }
}


void HexData::dump(FILE *fp, int max_bytes) const
{
    // FIXME: maybe comine with write_hex if they remain similar
    if (fp == NULL) fp = stderr;
    bool dump_all = (max_bytes == 0) ? true : false;

    int nblocks = blockset.size();
    fprintf(fp, "Dumping Hexdata (%d blocks):\n", nblocks);

    int i;
    for(i=0; i<nblocks; i++)
    {
        Block *block = blockset[i];
        assert(block != NULL);
        int len = block->length();

        fprintf(fp, "%8X %s (%2d): ", block->base_address, type_str(block->type), len);

        int j;
        for (j=0; j<len; j++)
        {
            fprintf(fp, "%02X ", block->data[j]);
        }

        fprintf(fp, "%02X\n", block->calculate_checksum());

        if (dump_all) continue;

        max_bytes -= len;
        if (max_bytes <= 0)
        {
            fprintf(fp, "...\n");
            break;
        }
    }
}


void HexData::trim(void)
{
    // removes records where entire record is 00's
    // NOTE: Might this be FF? in some cases !? FIXME
    // Might zeros actually be important (but this is prog space) what about data. Hmmm.
    // FIXME: Check what erase value is: FF ?
    int default_value = 0x00;

    int nblocks = blockset.size();
    int i;
    for(i=nblocks-1; i>=0; i--) // suspect more efficient going backwards (normally)
    {
        Block *block = blockset[i];
        assert(block != NULL);

        bool non_default = false;
        int len = block->length();
        int j;
        for (j=0; j<len; j++)
        {
            if (block->data[j] != default_value)
            {
                non_default = true;
                break;
            }
        }

        if (non_default == false)
        {
            blockset.erase(blockset.begin() + i);
            delete block;
        }
    }
}


int HexData::length(void) const
{
    int i;
    int nblocks = blockset.size();
    int length = 0;
    for(i=0; i<nblocks; i++)
    {
        length += blockset[i]->length();
    }
    return length;
}


#if 0
// Not sure of the point... I think max_address() is more useful
int HexData::length(uint32_t start_address, uint32_t address_length) const
{
    // number of actual bytes between start and end addresses. But we don't know if they are contiguous or not

    int i;
    int nblocks = blockset.size();
    int actual_length = 0;

    fprintf(stderr,"length Start: start_address 0x%08x  len: 0x%08x, nblocks:%d\n", start_address, address_length, nblocks);
    for(i=0; i<nblocks; i++) // FIXME: make more efficient if we know blocks ordered
    {
        Block *block = blockset[i];
        int block_len = block->length();

        int32_t start_offset = block->base_address - start_address;
        int32_t end_offset = (start_address + address_length) - (block->base_address + block_len);
        int num_bytes = block_len + MIN(0, start_offset) + MIN(0, end_offset);

//fprintf(stderr,"length %d bytes in block %d (bs:%x, len:%d) (so:%d, eo:%d)\n", num_bytes, i, block->base_address, block_len, start_offset, end_offset);
        if (end_offset < 0) continue; // FIXME need to redo the logic here
        if (num_bytes <= 0) continue;
            
        actual_length += num_bytes;
    } // for

    return actual_length;
}
#endif


bool HexData::minmax_address(uint32_t range_start_address, uint32_t range_address_length, uint32_t *min_address, uint32_t *max_address) const
{
    // Find the highest used address in this range. Note may want to trim data first... Doesn't check for zerod rows.
    // min address will always be a block start address, max_address will always be a block end address

    *min_address = *max_address = 0;

    uint32_t range_end_address = range_start_address + range_address_length;

    uint32_t tmp_min_address = range_end_address;
    uint32_t tmp_max_address = range_start_address;
    bool block_in_range = false;

    int nblocks = blockset.size();
    int i;
    for(i=0; i<nblocks; i++) // FIXME: make more efficient if we know blocks ordered
    {
        Block *block = blockset[i];
        int block_len = block->length();
        uint32_t block_end_address = block->base_address + block_len;

        // don't assume blocks are ordered
        if (range_end_address < block->base_address) continue;
        if (range_start_address > block_end_address) continue;

        // FIXME: what if a block has no data !?
        block_in_range = true;

        if (tmp_min_address > block->base_address)  tmp_min_address = block->base_address;
        if (tmp_max_address < block_end_address)  tmp_max_address = block_end_address;
    } // for

    if (!block_in_range) return false;

    *min_address = tmp_min_address;
    *max_address = tmp_max_address;

    return true;
}


HexData *HexData::extract(uint32_t start_address, uint32_t address_length) const
{
    // FIXME: can't define extract in terms of e2v because the address range in actalised in e2v!
//    return new HexData(start_address, extract2vector(start_address, address_length));
    // this version does not produce canonical/consistent blocks. Could call reshape() at end.

    // rename to subset ??
    HexData *extract = new HexData();

    int nblocks = blockset.size();

    if (debug)
        fprintf(stderr,"extract Start: start_address 0x%08x  len: 0x%08x, nblocks:%d\n",
            start_address, address_length, nblocks);

    int i;
    for(i=0; i<nblocks; i++) // FIXME: make more efficient if we know blocks ordered
    {
        Block *block = blockset[i];
        int block_len = block->length();

        // Note: end address (start + len) is not included in block data
        // Careful of maths limits. address can be any uint32_t so signed maths dangerous unless using 64 bits

        uint32_t block_end_address = block->base_address + block_len;
        uint32_t end_address = start_address + address_length;

        uint32_t clipped_s = MAX(start_address, block->base_address);
        uint32_t clipped_e = MIN(end_address, block_end_address);

        if (clipped_s >= clipped_e) continue; // outside current block

        uint32_t num_bytes = clipped_e - clipped_s;
        uint32_t block_start_offset = clipped_s - block->base_address;

        if (debug)
            fprintf(stderr,"extract %d bytes in block %d (bs:0x%x, len:%d) (cs:%d, ce:%d, bso:%d)\n",
                num_bytes, i, block->base_address, block_len, clipped_s, clipped_e, block_start_offset);

        assert(num_bytes <= block_len); // paranoia

        if (num_bytes == block_len) // shortcut
        {
            extract->add(*block);
        }
        else // subset
        {
            // copy block[start:end]
            uint8_t *vp = block->ptr();
            v_uint8_t tmp(num_bytes);

            memcpy(tmp.data(), vp + block_start_offset, num_bytes);
            extract->add(clipped_s, tmp);
        }
    } // for

    // FIXME: optionally canonicalise or reshape() to clean up structure !?
    return extract;
}


uint8_t *HexData::extract2bin(uint32_t start_address, uint32_t address_length, uint8_t *data) const
{
    // Does not assume ordered blocks.
    // if data is NULL we malloc and caller to free.
    // if data supplied, asssume it is at least address_length long
    // End address points to one past last data (because start address is 0-based)

    // I probably need a generator that yields and returns say 1KB at a time.
    // pads out data - really asking for an address range

    // NOTE: I may not actually need the entire address_length but I do malloc it all. Careful with large address ranges.

    if (data == NULL)
        data = (uint8_t *)calloc(address_length, sizeof(uint8_t));
    else
        memset(data, 0, address_length * sizeof(uint8_t));
        // assumes data is big enough!


    assert(data);

    int nblocks = blockset.size();
    int total_bytes_copied = 0;

    int i;
    for(i=0; i<nblocks; i++) // FIXME: make more efficient if we know blocks ordered
    {
        // FIXME: can remove block_start_address variable
        Block *block = blockset[i];
        int block_len = block->length();

        // Note: end address (start + len) is not included in block data
        // Careful of maths limits. address can be any uint32_t so signed maths dangerous unless using 64 bits

        uint32_t block_end_address = block->base_address + block_len;
        uint32_t end_address = start_address + address_length;

        uint32_t clipped_s = MAX(start_address, block->base_address);
        uint32_t clipped_e = MIN(end_address, block_end_address);

        if (clipped_s >= clipped_e) continue; // outside current block

        uint32_t num_bytes = clipped_e - clipped_s;
        uint32_t block_start_offset = clipped_s - block->base_address;

        if (debug)
            fprintf(stderr,"extract %d bytes in block %d (bs:0x%x, len:%d) (cs:%d, ce:%d, bso:%d)\n",
                num_bytes, i, block->base_address, block_len, clipped_s, clipped_e, block_start_offset);

        assert(num_bytes <= block_len); // paranoia

        memcpy(data + clipped_s - start_address, block->ptr() + block_start_offset, num_bytes);
        total_bytes_copied += num_bytes;

        if (total_bytes_copied >= address_length)
            break; // efficiency early return
    }

    // FIXME: need to return total bytes copied - could be 0 !! Or maybe go via a vector !?

    return data;
}


v_uint8_t HexData::extract2vector(uint32_t start_address, uint32_t address_length /*, v_uint8_t data*/) const
{
    // FIXME: could define in terms of extract.
    // Note: vector has no concept of sparse contents (like blocks) so we have to malloc the entire address length. Careful...

    v_uint8_t extract(address_length);

    extract2bin(start_address, address_length, extract.data());

    return extract;
}


HexData *HexData::reshape(int new_max_len) const
{
    // Creates a new HexData object, Original is unchanged.

    // purpose of this function is to convert consecutive blocks into blocks with new length
    // of max size max_len.
    // if new_max_len == 0 then canonicalise (ie no limit). Be careful of vector allocs.

    // When one needs to operate in a different sized block it's easier just to recalc the whole
    // thing once rather than to keep pointers to offsets and do fiddly copies/mallocs etc

    // non-consecutive blocks remain unconsolidated.
    // Caller to delete returned block

    // index is the internal state. (happens to be internal block number). Assumes no change
    // between calls. Each time we reenter we start where we left off.
    // index should be primed to 0 on first call.

    // NOTE: does not break up blocks - ie either a block fits entirely or not at all
    // NOTE: Assmes max_size >= block->len

// FIXME: CHECK ALL CASES: a) out of order blocks, and various start addresses and lengths < block length etc etc 
// Input is not always in sorted order, may not be wise to sort either. Does assume generally sorted. Works ok in practice.

    // FIXME: should check if input format already complies with output.
#define DEFAULT_BLOCK_SIZE      1024
    bool canonical_format = (new_max_len == 0);

    int block_size = canonical_format ? DEFAULT_BLOCK_SIZE: new_max_len;

    HexData *hexdata = new HexData();

    int index = 0;
    int nblocks = blockset.size();
    int block_copy_offset = 0;

    while(index < nblocks) // outer loop: new output blocks
    {
        Block *output = new Block(block_size);  // will be exact size in all cases bar canonical
        output->base_address = blockset[index]->base_address + block_copy_offset;

        if (debug) fprintf(stderr, "\nstart_addr:0x%0x, index:%d/%d\n", output->base_address, index, nblocks);

        while(index < nblocks) // inner loop: iterate over existing blocks
        {
            Block *block = blockset[index];
            int block_len = block->length();
            int32_t block_end_address = block->base_address + block_len - 1;

            if (debug) fprintf(stderr, " index:%d output(len:%d) [base:0x%x, end:0x%x len:%d]\n",
                index, output->length(), block->base_address, block_end_address, block_len);

            // in canonical format we have "infinite" space but current block_len is always enough.
            int remaining_space = canonical_format ? block_len : (output->max_len - output->length());

            if (remaining_space <= 0)
            {
                if (debug) fprintf(stderr, " Output block full. len:%d\n", output->length());
                break; // go back to outer loop and create new output block
            }

            int num_bytes_to_copy = MIN(remaining_space, block_len - block_copy_offset);
            if (num_bytes_to_copy == 0) // ie block->len - block_copy_offset == 0
            {
                index++;
                block_copy_offset = 0;

                if (index >= nblocks)
                    continue; // and thence exit inner loop

                if (debug) fprintf(stderr, " next base: 0x%02x\n", blockset[index]->base_address);

                if (block_end_address + 1 != blockset[index]->base_address)
                {
                    if (debug) fprintf(stderr, " break point\n");
                    break; // go back to outer loop and crate new output block
                }

                continue; // restart inner loop with new src block
            }

            int current_output_len = output->length();
            output->data.resize(current_output_len + num_bytes_to_copy); // may involve realloc for canonical case
            uint8_t *dp = output->ptr() + current_output_len; 
            memcpy(dp, block->ptr() + block_copy_offset, num_bytes_to_copy);
            block_copy_offset += num_bytes_to_copy;

            if (debug) fprintf(stderr, " copy data. New output len:%d\n", output->length());

        } // inner loop


        if (output->length() > 0)
            hexdata->blockset.push_back(output);
        else
            delete output;
    }

    return hexdata;
}
       

#if 0
bool HexData::bin2block(uint32_t base_address, uint8_t *data, int len)
{
    // do we assume a reset structure or not !?
    int blocksize = 16; // FIXME !?

    int num_blocks = len / blocksize;
    int remainder = len % blocksize;
    if (remainder) num_blocks++;
    int address = base_address & 0xFFFF;
    uint8_t *dp = data;

    int i;
    for(i=0; i<num_blocks; i++)
    {
        int count = (remainder && i==num_blocks-1) ? remainder : blocksize;
        // FIXME: what if address wraps !?

        if (count == 0)
            break;

        Block *block = new Block(address, dp, count);
//        memcpy(block->data, dp, count);
//        block->base_address = address;
//        block->type = RT_DATA;

        blockset.push_back(block);
        dp += count;

    }
}
#endif


void HexData::add_hex(uint32_t base_address, const char *hex_str)
{
    int nhex_digits = strlen(hex_str);
    assert(nhex_digits%2 == 0);
    uint8_t *data = hexstr2bin(hex_str, nhex_digits, NULL);
    Block *block = new Block(base_address, data, nhex_digits/2);
    assert(block);
    blockset.push_back(block);
    free(data);
}

void HexData::add(const Block &block)
{
    // FIXME: how many copies does this whole thing go through. Just want one
    blockset.push_back(new Block(block));
}

void HexData::add(uint32_t base_address, v_uint8_t vdata)
{
    // FIXME: tends to assume monotonically increasing
//    int address = base_address & 0xFFFF;
    blockset.push_back(new Block(base_address, vdata));
}


// static

void HexData::write_hex_record(FILE *fp, uint32_t address, uint8_t type, const v_uint8_t data)
{
    // static
    write_hex_record(fp, address, type, data.data(), data.size());
}


void HexData::write_hex_record(FILE *fp, uint32_t address, uint8_t type, const uint8_t *data, int len)
{
    // static
    write_ext_address_record(fp, address>>16);
    write_raw_record(fp, address, RT_DATA, data, len);
}


void HexData::write_ext_address_record(FILE *fp, uint32_t high_address)
{
    // static
    // ":02000004aaaaCC"  aaaa = high 16 bits of address, CC = checksum
    uint8_t data[2];
    data[0] = (high_address >> 8) & 0xFF;
    data[1] = high_address & 0xFF;
    write_raw_record(fp, 0, RT_EXT_LIN_ADDR, data, 2);
}


void HexData::write_end_record(FILE *fp)
{
    // static
    // ":00000001FF"
    write_raw_record(fp, 0, RT_END, NULL, 0);
}


void HexData::write_raw_record(FILE *fp, uint32_t address, uint8_t type, const v_uint8_t data)
{
    // static
    write_raw_record(fp, address, type, data.data(), data.size());
}


void HexData::write_raw_record(FILE *fp, uint32_t address, uint8_t type, const uint8_t *data, int len)
{
    // static
    Block *block = new Block(address, data, len, type);
    write_block_record(fp, block);
    delete block;
}


void HexData::write_block_record(FILE *fp, const Block *block)
{
    // static
    if (!block)
        return;

    int len = block->length();
    assert(len < 256);

    fprintf(fp, ":%02X%04X%02X", len, (block->base_address & 0xFFFF), block->type);

    int i;
    for (i=0; i<len; i++)
    {
        fprintf(fp, "%02X", block->data[i]);
    }

    fprintf(fp, "%02X\n", block->calculate_checksum());
}


uint8_t *HexData::hexstr2bin(const char *hex_buffer, int num_hex_digits, uint8_t *bin_buffer)
{
    // static
    // parse and consume input. Note num_hex_digits is not always strlen()
    assert(num_hex_digits %2 == 0);
  
    const char *hp = hex_buffer;
    int num_bytes = num_hex_digits/2;

    if (bin_buffer == NULL)
        bin_buffer = (uint8_t *)malloc(num_bytes); // FIXME: should probably replace with with a vector

    uint8_t *bp = bin_buffer;
    int i;
    for (i=0; i<num_bytes; i++)
    {
        uint32_t byte = parse_hex_int(&hp, 2);
        *(bp++) = (uint8_t) byte;
    }

//    *hex_buffer += num_hex_digits;

    return bin_buffer;
}


const char *HexData::type_str(int type)
{
    // static
    if (type == 0) return "Data";
    if (type == 1) return "END";
    if (type == 2) return "Extended Segment Address";
    if (type == 3) return "Start Segment Address";
    if (type == 4) return "Extended Linear Address";
    if (type == 5) return "Start Linear Address";
    return "<Unknown>";
}


// support

void dump_vector(FILE *fp, const char *msg, const v_uint8_t v)
{
    if (msg) fprintf(fp, "%s: ", msg);

    const uint8_t *vp = v.data();
    int i;
    for(i=0;i<v.size(); i++)
    {
        fprintf(fp,"%x ", vp[i]);
    }
    fprintf(fp,"\n");
}

#if 0

//#define LE_MODE = 0
//#define BE_MODE = 1

uint32_t uint_at(const HexData &hexdata, uint32_t address, unsigned int len, bool be)
{
    // be: BE or LE - is it just change_endianness !?
    assert(len <= 4);
    uint32_t value = 0;

    uint8_t *data = hexdata.extract2bin(address, len);
    assert(data);

    int i;
    for(i=0; i<len; i++)
    {
        value <= 8;
        value |= (be ? data[i] : data[len-i-1]);
    }

    free(data);

    return value;
}
#endif

uint32_t HexData::uint_at(uint32_t address, unsigned int len, uint8_t endian) const
{
    // be: BE or LE - is it just change_endianness !?
    assert(len <= 4);
    uint32_t value = 0;

    uint8_t *data = extract2bin(address, len);
    assert(data);

    if (debug) fprintf(stderr, "uint_at(addr:0x%0x) ", address);
    int i;
    for(i=0; i<len; i++)
    {
        if (debug) fprintf(stderr, "[%02x] ", data[i]);
        value <<= 8;
        value |= (endian == BIGENDIAN ? data[i] : data[len-i-1]);
    }
    if (debug) fprintf(stderr, "-> 0x%0x\n", value);

    free(data);

    return value;
}

