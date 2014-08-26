#ifndef _FX2_H
#define _FX2_H

#include <stdint.h>
#include <libusb-1.0/libusb.h>

#include "HexData.h"


int fx2_cmd_rw_ram(libusb_device_handle *dev_handle, uint16_t addr, const char *hex_data_str);
int fx2_cmd_rw_ram(libusb_device_handle *dev_handle, const Block *block);

int fx2_cmd_8051_enable(libusb_device_handle *dev_handle, bool enable);

#endif
