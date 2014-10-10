#ifndef _FX2_H
#define _FX2_H

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
#include <libusb-1.0/libusb.h>

#include "HexData.h"


int fx2_cmd_rw_ram(libusb_device_handle *dev_handle, uint16_t addr, const char *hex_data_str);
int fx2_cmd_rw_ram(libusb_device_handle *dev_handle, const Block *block);

int fx2_cmd_8051_enable(libusb_device_handle *dev_handle, bool enable);

#endif
