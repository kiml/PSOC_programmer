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
#include <string.h>
#include <assert.h>

#include "utils.h"


#if 0
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
#endif


void dump_data(FILE *fp, const uint8_t *data, int len, const char *msg)
{
    if (msg) fprintf(fp, "%s (%d): ", msg, len);
    if (!data || !len) return;

    int i;
    for (i=0; i<len; i++)
    {
        fprintf(fp, "%02x ", data[i]);
        if (i % 16 == 15) fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}


bool match(const uint8_t *data, int data_len, const char *hex_data)
{
   uint8_t hex_data_bin[2048]; // FIXME: hack    
   int bin_len = hex_to_bin(hex_data, hex_data_bin, 2048);
   int min = data_len <=  bin_len ? data_len : bin_len;
   //return memcmp(data, hex_data_bin, min) == 0;
   int match = memcmp(data, hex_data_bin, min) == 0;
   // fprintf(stderr, "%s [%s]\n", (match ? "Match" : "MISMATCH"), hex_data);
   return match;
}

void uint16_to_b2_BE(uint32_t data, uint8_t *b)
{
    b[0] = (data >> 8) & 0xFF;
    b[1] = data & 0xFF;
}


void uint16_to_b2_LE(uint32_t data, uint8_t *b)
{
    b[1] = (data >> 8) & 0xFF;
    b[0] = data & 0xFF;
}


void uint32_to_b4_BE(uint32_t data, uint8_t *b)
{
    b[0] = (data >> 24) & 0xFF;
    b[1] = (data >> 16) & 0xFF;
    b[2] = (data >> 8) & 0xFF;
    b[3] = data & 0xFF;
}


void uint32_to_b4_LE(uint32_t data, uint8_t *b)
{
    b[3] = (data >> 24) & 0xFF;
    b[2] = (data >> 16) & 0xFF;
    b[1] = (data >> 8) & 0xFF;
    b[0] = data & 0xFF;
}


uint32_t b4_BE_to_uint32(const uint8_t *b)
{
    uint32_t value = (b[0] << 24) | (b[1] << 16) | (b[2]<< 8) | b[3];
    return value;
}


uint32_t b4_LE_to_uint32(const uint8_t *b)
{
    uint32_t value = (b[3] << 24) | (b[2] << 16) | (b[1]<< 8) | b[0];
    return value;
}


int hex_to_bin(const char *hex, uint8_t *bin, int max_bin_len)
{
    // doesn't zero bin. If len hex is 0, bin untouched

    if (!hex)
        return -1; // or return 0 !?

    int length = strlen(hex);

    if (length > 2*max_bin_len || length % 2 != 0)
        return -1;

    uint8_t *dp = bin;
    int v = 0;

    int i;
    for (i=0; i<length/sizeof(hex[0]); i++)
    {
        uint8_t c = hex[i]; //  & 0x1f;  // to upper

        if (c >= '0' && c <= '9')
          v |= c - '0';
        else if (c >= 'a' && c <= 'f')
          v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
          v |= c - 'A' + 10;
        else
        {
          //fprintf(stderr, "Err: [%c,%2x]\n",  c, c);
          assert(0 && "invalid hex");
        }

      //fprintf(stderr, "i:%d %c\n",  i, c);
        if ((i & 0x01) == 0)
            v <<= 4;
        else
        {
            *(dp++) = v;
            v = 0;
        }
    }

    return length >> 1;
}


#if 0
int XXhex_to_bin(const char *hex, uint8_t *bin, int max_bin_len)
{
    if (!hex)
        return -1;

    int length = strlen(hex);

    if (length > max_bin_len || length % 2 != 0)
        return -1;

    int o=0;

    int i;
    for (i=0; i<length/sizeof(hex[0]); i+=2)
    {
        int v = 0;
        int j;

        for (j=0; j<2; j++)
        {
            uint8_t c = hex[i+j] & 0x3f;  // to upper

            if (c >= '0' && c <= '9')
              v |= c - '0';
            else if (c >= 'A' && c <= 'F')
              v |= c - '0' + 10;
            else
            {
              assert(0 && "invalid hex");
            }

            v <<= 4;
        }
        data[o++] = v;
    }
}
#endif
