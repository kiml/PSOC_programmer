#ifndef _USB_H
#define _USB_H

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

#include <libusb-1.0/libusb.h>

#ifdef __cplusplus
extern "C" {
#endif

bool usb_init(void);
void usb_finalise(void);

libusb_device_handle *open_device(int vid, int pid, int config, int interface, int alternate);
void close_device(libusb_device_handle *dev_handle, int interface);

int control_transfer(libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t *reply_length, uint16_t max_length);
int control_transfer_out_hex(libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, const char *hex_str);
int control_transfer_out(libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, const uint8_t *data, int len);

int bulk_data_transfer(libusb_device_handle *dev_handle, uint8_t epaddr, uint8_t *data, int *length);

//void send_data(libusb_device_handle *dev_handle, uint8_t *data, int length);
int send_bulk_data(libusb_device_handle *dev_handle, uint8_t epaddr, uint8_t *data, int length);
int send_bulk_data_hex(libusb_device_handle *dev_handle, uint8_t epaddr, const char *hex_data_str);
int recv_bulk_data(libusb_device_handle *dev_handle, uint8_t epaddr, uint8_t *data, int max_length);

int clear_endpoint_stall(libusb_device_handle *dev_handle, uint8_t epaddr);

void list_matching_devices(int vid, int pid);
//void print_device_info(libusb_device *dev, struct libusb_device_descriptor *pDesc);
void print_device_info(libusb_device *dev);
void print_device_info2(libusb_device_handle *dev_handle);
//void find_device(int vid, int pid, struct libusb_device_descriptor *pDesc);
//void print_string_descriptors(libusb_device_handle *dev_handle);
void print_string_descriptor(libusb_device_handle *dev_handle, int index, const char *label);

#ifdef __cplusplus
}
#endif

#endif
