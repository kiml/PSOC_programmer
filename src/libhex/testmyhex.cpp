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


#if 0
void f(const char *msg, int block_start_address, int block_len, int start_address, int end_address)
{
    fprintf(stderr,"%s:  BSA:%d, BL:%d, SA:%d, EA:%d\n", msg, block_start_address, block_len, start_address, end_address);
    int block_end_address = block_start_address + block_len;
    int start_offset = block_start_address - start_address;
    int end_offset = end_address - block_end_address;
    int max_0_so = MAX(0, start_offset);
    int num_bytes = block_len + MIN(0, start_offset) + MIN(0, end_offset);

    fprintf(stderr,"%s COPY(data + %d,  bsid + %d,  len=%d)\n\n", num_bytes <= 0 ? "Don't" : "",
       max_0_so, MAX(0,-start_offset), num_bytes);

}
#endif

bool testcheck(int testnum, const uint8_t *testdata, const uint8_t *refdata, int n)
{
    if (memcmp(testdata, refdata, n) == 0)
    {
        fprintf(stderr, "Test %d: Passed\n", testnum);
        return true;
    }

    int i;
    fprintf(stderr, "Test %d: Failed\n", testnum);
    // print differencecs
    fprintf(stderr, "TestData: ");
    for (i=0;i<n;i++) fprintf(stderr, "%x ", testdata[i]);
    fprintf(stderr, "\n");
    fprintf(stderr, "RefData: ");
    for (i=0;i<n;i++) fprintf(stderr, "%x ", refdata[i]);
    fprintf(stderr, "\n");

    return false;
}


void setref(uint8_t *refdata, int start_i, int len, int start_value, bool clear_first)
{
    if (clear_first) memset(refdata, 0, 256);
    int i;
    int end_i = start_i + len;
    assert(end_i <= 256);

    for(i=start_i; i<end_i; i++)
    {
        refdata[i] = start_value++;
    }
}


int main()
{
    HexData test1;
    int i;
    uint32_t  addr = 256; // start_addr
    v_uint8_t vuint(32);
    for(i=0; i<256; i++)
    {
        int idx = i%32;

        vuint[idx] = i;

        if (idx == 31)
        {
            test1.add(addr, vuint);
            addr += 32;
            vuint.assign(32, 0);
        }
    }

    //   ......  256: 0,  257:1, ....  511:255, .....
    test1.dump(stderr, 256);
    uint8_t *data;
    uint8_t refdata[512];
    int addr_start, len, testnum;
    memset(refdata,0,512);


    testnum = 1; // below
    addr_start = 0;
    len = 256;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "below", addr_start, addr_start + len);
    data = test1.extract2bin(addr_start, len);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 2; // above
    addr_start = 512;
    len = 64;
    fprintf(stderr,"Test %d: %s, %d - %d\n", testnum, "above", addr_start, addr_start + len);
    data = test1.extract2bin(addr_start, len);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 3; // across lower boundary
    addr_start = 128;
    len = 200;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "across lower boundary", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    setref(refdata, 256 - addr_start, 256 - (256 - addr_start), 0, true);
    testcheck(testnum, data, refdata, 256 - (256 - addr_start));
    free(data);

    testnum = 4;
    addr_start = 511;
    len = 64; // across upper boundary
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "across upper boundary", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    memset(refdata, 0, 256);
    refdata[0] = 255;
    testcheck(testnum, data, refdata, len);
    free(data);
    
    testnum = 5; // exact length
    addr_start = 256;
    len = 256;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "exact length", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    setref(refdata, 0, len, 0, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 6; // inside
    addr_start = 300;
    len = 64;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "inside", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    setref(refdata, 0, len, 300-256, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 7; // outside
    addr_start = 128;
    len = 512;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "outside", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    memset(refdata, 0, 512);
    setref(refdata+128, 0, 256, 0, false);
    testcheck(testnum, data, refdata, 512);
    free(data);

    testnum = 8; // exact length (one block)
    addr_start = 288;
    len = 32;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "exact length (one block)", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    setref(refdata, 0, len, 288-256, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 9; // inside (one block)
    addr_start = 290;
    len = 12;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "inside (one block)", addr_start, addr_start+len);
    data = test1.extract2bin(addr_start, len);
    setref(refdata, 0, len, 290-256, true);
    testcheck(testnum, data, refdata, len);
    free(data);


    memset(refdata,0,512);

    testnum = 10; // below
    addr_start = 0;
    len = 256;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "below", addr_start, addr_start + len);
    HexData *hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 11; // above
    addr_start = 512;
    len = 64;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "above", addr_start, addr_start + len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 12; // across lower boundary
    addr_start = 128;
    len = 200;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "across lower boundary", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    setref(refdata, 256 - addr_start, 256 - (256 - addr_start), 0, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 13;
    addr_start = 511;
    len = 64; // across upper boundary
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "across upper boundary", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    memset(refdata, 0, 256);
    refdata[0] = 255;
    testcheck(testnum, data, refdata, len);
    free(data);
    
    testnum = 14; // exact length
    addr_start = 256;
    len = 256;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "exact length", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    setref(refdata, 0, len, 0, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 15; // inside
    addr_start = 300;
    len = 64;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "inside", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    setref(refdata, 0, len, 300-256, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 16; // outside
    addr_start = 128;
    len = 512;
    fprintf(stderr, "Test %d: %s, %d - %d\n", testnum, "outside", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    memset(refdata, 0, 512);
    setref(refdata+128, 0, 256, 0, false);
    testcheck(testnum, data, refdata, 512);
    free(data);

    testnum = 17; // exact length (one block)
    addr_start = 288;
    len = 32;
    fprintf(stderr,"Test %d: %s, %d - %d\n", testnum, "exact length (one block)", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    setref(refdata, 0, len, 288-256, true);
    testcheck(testnum, data, refdata, len);
    free(data);

    testnum = 18; // inside (one block)
    addr_start = 290;
    len = 12;
    fprintf(stderr,"Test %d: %s, %d - %d\n", testnum, "inside (one block)", addr_start, addr_start+len);
    hdata = test1.extract(addr_start, len);
    data = hdata->extract2bin(addr_start, len);
    setref(refdata, 0, len, 290-256, true);
    testcheck(testnum, data, refdata, len);
    free(data);


#if 0
//    HexData hexdata("test.hex");
//    HexData hexdata("fx2lp_fw.hex");
    HexData hexdata("CapSense_CSD_Design01.hex");
    hexdata.dump(stdout);
//    hexdata.write_hex("tst.hex");

    //HexData *newhexdata = hexdata.reshape(32);
    int newsize = 0; // 32
    HexData *newhexdata = hexdata.reshape(newsize);
    assert(newhexdata);
#endif

#if 0
    int size = newhexdata->blockset.size();
    int i;
    for(i=0; i<size; i++)
    {
        Block *block = newhexdata->blockset[i];
        assert(block != NULL);
        block->dump();
    }
#else
//    newhexdata->dump(stdout);
//    if (newsize > 0 && newsize < 256)
//        newhexdata->write_hex("testout.hex");
#endif

#if 0
    uint8_t * data = hexdata.extract2bin(32, 0x1000);
    dump_data(stdout, data, 512);

    f("b above", 20,10, 10, 18);
    f("b encomp", 20,10, 22, 28);
    f("b below", 20,10, 31, 38);
    f("b border top", 20,10, 15, 22);
    f("b border bottom", 20,10, 25, 32);
    f("b inside", 20,10, 18, 33);
#endif
}
