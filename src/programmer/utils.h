#ifndef _UTILS_H
#define _UTILS_H

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

#define NULLSTR(s)       ((s) ? (s) : "<NULL>")

// FIXME: I don't actually have dest types so to_Uxx is academic
// FIXME: I have both macros and funcs below
#define B4LE_to_U32(c)  (((c)[3]<<24) | ((c)[2] << 16) | ((c)[1] << 8) | (c)[0] )
#define B4BE_to_U32(c)  (((c)[0]<<24) | ((c)[1] << 16) | ((c)[2] << 8) | (c)[3] )
#define B2LE_to_U32(c)  (((c)[1] << 8) | (c)[0] )
#define B2BE_to_U16(c)  (((c)[0] << 8) | (c)[1] )

#ifndef MIN
#define MIN(a,b)        ((a) <= (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)        ((a) >= (b) ? (a) : (b))
#endif


int hex_to_bin(const char *hex, uint8_t *bin, int max_bin_len);
void dump_data(FILE *fp, const uint8_t *data, int len, const char *msg=NULL);
bool match(const uint8_t *data, int data_len, const char *hex_data);

void uint16_to_b2_BE(uint32_t data, uint8_t *b);
void uint16_to_b2_LE(uint32_t data, uint8_t *b);
void uint32_to_b4_BE(uint32_t data, uint8_t *b);
void uint32_to_b4_LE(uint32_t data, uint8_t *b);

uint32_t b4_BE_to_uint32(const uint8_t *b);
uint32_t b4_LE_to_uint32(const uint8_t *b);

#endif
