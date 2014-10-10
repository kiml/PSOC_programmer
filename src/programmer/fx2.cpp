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

#include "fx2.h"
#include "usb.h"


#define FX2_RW_RAM      0xA0
#define FX2_RW_EEPROM   0xA2


// EZ-USB TRM 001-13670 Appendix C
#define FX2_REG_CPUCS           0xE600
#define FX2_REG_IC_REVISION     0xE60A


// command 160 (0xA0) Firmware Load writes to the programmer (or usb interface). First argument is an address
// see for example EZ-USB TRM 001-13670 pg 40
// or https://github.com/makestuff/libfx2loader/blob/master/ram.c
 

int fx2_cmd_rw_ram(libusb_device_handle *dev_handle, uint16_t addr, const char *hex_data_str)
{
    uint8_t bmRequestType = 0x40; // VENDOR 0x40 | EP_OUT 0x00
    uint8_t bRequest = FX2_RW_RAM;
    uint16_t wIndex = 0;

    // wValue = addr

    return control_transfer_out_hex(dev_handle, bmRequestType, bRequest, addr, wIndex, hex_data_str);
}


int fx2_cmd_rw_ram(libusb_device_handle *dev_handle, const Block *block)
{
    uint8_t bmRequestType = 0x40; // VENDOR 0x40 | EP_OUT 0x00
    uint8_t bRequest = FX2_RW_RAM;
    uint16_t wIndex = 0;

    return control_transfer_out(dev_handle, bmRequestType, bRequest, block->base_address, wIndex, block->data.data(), block->length());
}


int fx2_cmd_8051_enable(libusb_device_handle *dev_handle, bool enable)
{
    //return fx2_cmd_rw_ram(dev_handle, FX2_REG_CPUCS, "00")
    uint8_t bmRequestType = 0x40; // VENDOR 0x40 | EP_OUT 0x00
    uint8_t bRequest = FX2_RW_RAM;
    uint16_t wIndex = 0;
    // wValue = addr
    uint8_t data = 0x01;

    if (enable) data = 0x00; // CPUCS register bit 0 - 8051RESet.  1=hold in reset, 0=run
    return control_transfer_out(dev_handle, bmRequestType, bRequest, FX2_REG_CPUCS, wIndex, &data, 1);
}

