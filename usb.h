#ifndef _USB_H
#define _USB_H

#include <stdint.h>

#include <libusb-1.0/libusb.h>


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

#endif
