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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "usb.h"
#include "utils.h"


//#define CTRL_IN   (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
//#define CTRL_OUT  (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)

#define DEBUG_CONTROL   0x01
#define DEBUG_BULK      0x02

static uint32_t usb_debug = 0;


bool usb_init(void)
{
    int rc = libusb_init(NULL);
    if (rc < 0)
    {
        fprintf(stderr, "Failed to initialise usb interface");

        // finalise
        libusb_exit(NULL);
        return false;
    }
    return true;
}

void usb_finalise(void)
{
    libusb_exit(NULL);
}


libusb_device_handle *open_device(int vid, int pid, int config, int interface, int alternate)
{
    fprintf(stderr,"Config:%d, Interface:%d, Alternate:%d\n", config, interface, alternate);

    //struct libusb_device_descriptor desc;
    int rc;

    libusb_device_handle *dev_handle;

    // not supposed to use this conv. func:
    dev_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
    if (dev_handle == NULL)
    {
        fprintf(stderr,"libusb: open device failed\n"); // , rc, libusb_strerror(rc));
        return NULL;
    }

#if 0
    int was_attached = 0;
    if (libusb_kernel_driver_active(dev_handle, interface))
    {
        rc = libusb_detach_kernel_driver(dev_handle, interface);
        if (rc < 0)
        {
            fprintf(stderr,"libusb: detach kernel driver failed: %d %s\n", rc, libusb_strerror(rc));
            libusb_close(dev_handle);
            return rc;
        }
        was_attached = 1;
    }
#else
    libusb_set_auto_detach_kernel_driver(dev_handle, 1);
#endif

    //get_string_descriptors(dev_handle);

#if 1
    rc = libusb_set_configuration(dev_handle, config); // config==bConfiguraitonValue
    if (rc < 0)
    {
        fprintf(stderr,"libusb: set configuraton %d failed: %d %s\n", config, rc, libusb_strerror((enum libusb_error)rc));
        libusb_close(dev_handle);
        return NULL;
    }
#endif

    rc = libusb_claim_interface(dev_handle, interface);
    if (rc < 0)
    {
        fprintf(stderr,"libusb: claim interface %d failed: %d %s\n", interface, rc, libusb_strerror((enum libusb_error)rc));
#if 0
        libusb_attach_kernel_driver(dev_handle, interface);
#endif
        libusb_close(dev_handle);
        return NULL;
    }

#if 1
    rc = libusb_set_interface_alt_setting(dev_handle, interface, alternate);
    if (rc < 0)
    {
        fprintf(stderr,"libusb: set alternate interface %d.%d failed: %d %s\n", interface, alternate, rc, libusb_strerror((enum libusb_error)rc));
        libusb_release_interface(dev_handle, interface);
        libusb_close(dev_handle);
        return NULL;
    }
#endif

    //unsigned char reply_data[2048];
    //memset(reply_data,0,2048);
    //rc = libusb_get_string_descriptor(dev_handle, 0x02, 0x0409, reply_data, 38);
    //fprintf(stderr, "1A string desc: %d, %s\n", rc, reply_data);

    return dev_handle;
}


void close_device(libusb_device_handle *dev_handle, int interface)
{
    if (dev_handle == NULL)
        return;

    int rc;

    rc = libusb_release_interface(dev_handle, interface);
    if (rc < 0)
    {
        fprintf(stderr,"libusb: release interface %d failed: %d %s\n", interface, rc, libusb_strerror((enum libusb_error)rc));
    }

#if 0
    if (was_attached)
        rc = libusb_attach_kernel_driver(dev_handle, interface);
        if (rc < 0)
            fprintf(stderr,"libusb: attach kernel driver failed: %d %s\n", rc, libusb_strerror((enum libusb_error)rc));
#endif

    libusb_close(dev_handle);
}


// Data transfer routines
// ======================

int control_transfer_out_hex(libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest,
        uint16_t wValue, uint16_t wIndex, const char *hex_str)
{
    // returns ????

    uint8_t data[2048]; // FIXME Hack

    if (!hex_str)
    {
        fprintf(stderr, "control_transfer_160_hex: null string\n");
        return -2;
    }

    int len = hex_to_bin(hex_str, data, 2048);
    if (len < 0)
    {
        //fprintf(stderr, "Failed convert hex data: %d %s\n", len, NULLSTR(hex_str));
        fprintf(stderr, "Failed convert hex data: %d %s\n", len, hex_str);
        return len;
    }
    //fprintf(stderr, "bin -> %d bytes", len);
#if 0
    fprintf(stderr, "H:%s\nB:", NULLSTR(hex_str));
    int i;
    for (i=0; i<len; i++)
    {
        fprintf(stderr, "%02x ", data[i]);
    }
    fprintf(stderr, "\n");
#endif

    return control_transfer_out(dev_handle, bmRequestType, bRequest, wValue, wIndex, data, len);
}

int control_transfer_out(libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest,
        uint16_t wValue, uint16_t wIndex, const uint8_t *data, int len)
{

    uint32_t timeout = 0;

    int len_xfer = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValue, wIndex, (unsigned char *)data, len, timeout);

    if (len_xfer < 0)
    {
        fprintf(stderr, "Failed transfer control data: %d/%d %s\n", len_xfer, len, libusb_strerror((enum libusb_error)len));
        return len_xfer;
    }

    return 0;
}


int control_transfer(libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *reply_data, uint16_t *reply_length, uint16_t max_length)
{
    uint32_t timeout = 0;
//    unsigned char send_data[1];
    int len;

//    send_data[0] = 0;

    // DEBUG:
    memset(reply_data, 0, max_length);

    len = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValue, wIndex, reply_data, max_length, timeout);
    if (len < 0)
    {
        fprintf(stderr, "Failed transfer control data: %d %s\n", len, libusb_strerror((enum libusb_error)len));
        return len;
    }

    *reply_length = len;

    int i;
    if (usb_debug & DEBUG_CONTROL) fprintf(stderr,"CT: wValue=0x%02x Data (%d): ", wValue, len);

    for(i=0;i<len;i++)
    {
        fprintf(stderr, "%02x ", reply_data[i]);
    }
    fprintf(stderr,"\n");
    return 0;
}


int bulk_data_transfer(libusb_device_handle *dev_handle, uint8_t epaddr, uint8_t *data, int *length)
{
    // OUT: send *length data
    // IN:  *length > 0: expect *length bytes.  *length==0: take what we're given
    int timeout_ms = 100;
    int actual = 0;
    int rc;
//    uint8_t dir;

//    dir = LIBUSB_ENDPOINT_OUT;

//    if (dir == LIBUSB_ENDPOINT_IN)
//        epaddr |= 0x80;

    rc = libusb_bulk_transfer(dev_handle, epaddr, data, *length, &actual, timeout_ms);

    if (usb_debug & DEBUG_BULK)
    {
//    if (dir == LIBUSB_ENDPOINT_OUT)
        if (epaddr & 0x80) // in if set
            fprintf(stderr, "EP 0x%02x: read %d (of a max %d) bytes. %s\n", epaddr, actual, *length, rc==0?"OK":"FAILED");
        else
            fprintf(stderr, "EP 0x%02x: wrote %d/%d bytes. %s\n", epaddr, actual, *length, rc==0?"OK":"FAILED");
    }

    if (rc != 0)
        fprintf(stderr, "Error %d %s\n", rc, libusb_strerror((enum libusb_error)rc));

    *length = actual;

    return rc;
}


int send_bulk_data(libusb_device_handle *dev_handle, uint8_t epaddr, uint8_t *data, int length)
{
    int rc = bulk_data_transfer(dev_handle, epaddr, data, &length);
    return rc;
}


int send_bulk_data_hex(libusb_device_handle *dev_handle, uint8_t epaddr, const char *hex_str)
{
    uint8_t data[2048]; // FIXME Hack

    if (!hex_str)
    {
        fprintf(stderr, "send_bulk_hex: null string\n");
        return -2;
    }

    int len = hex_to_bin(hex_str, data, 2048);
    if (len < 0)
    {
        fprintf(stderr, "Failed convert hex data: %d %s\n", len, hex_str);
        return len;
    }

    int rc = bulk_data_transfer(dev_handle, epaddr, data, &len);
    return rc;
}


int recv_bulk_data(libusb_device_handle *dev_handle, uint8_t epaddr, uint8_t *data, int max_length)
{
    int rc = bulk_data_transfer(dev_handle, epaddr, data, &max_length);
    if (rc < 0)
        return rc;

    return max_length; // length actually received
}


int clear_endpoint_stall(libusb_device_handle *dev_handle, uint8_t epaddr)
{
    uint32_t timeout = 0;

    int len = libusb_control_transfer(dev_handle, LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_ENDPOINT,
                LIBUSB_REQUEST_CLEAR_FEATURE, 0 /* ENDPOINT_HALT wValue */, epaddr /* wIndex */, NULL /* reply_data */, 0, timeout);
    if (len < 0)
    {
        fprintf(stderr, "Failed transfer control data: %d %s\n", len, libusb_strerror((enum libusb_error)len));
        return len;
    }

    return 0;
}

// Support routines
// ================


#if 0
void print_string_descriptors(libusb_device_handle *dev_handle, struct libusb_device_descriptor desc)
{
    print_string_descriptor(dev_handle, desc->iManufacturer);
    print_string_descriptor(dev_handle, desc->iProduct);
    print_string_descriptor(dev_handle, desc->iSerialNumber);
}
#endif

void print_string_descriptor(libusb_device_handle *dev_handle, int index, const char *label)
{
    const int max_len = 256;
    unsigned char data[max_len+1];

    data[0] = 0;

    //fprintf(stderr,"DEBUG: %p, max_len=%d\n", dev_handle, max_len);
    if (index == 0)
    {
        fprintf(stderr,"libusb: get_string_descriptor %s %d failed: Cannot get ascii on index=0\n", (label ? label : "Descriptor"), index);
        return;
    }
        
    int len = libusb_get_string_descriptor_ascii(dev_handle, (uint8_t) index, data, max_len);
    if (len < 0)
    {
        fprintf(stderr,"libusb: get_string_descriptor %s %d failed: %d %s\n", (label ? label : "Descriptor"), index, len, libusb_strerror((enum libusb_error)len));
        return;
    }
    // FIXME: NULL TERM ?
    assert(len <= 256);

    data[len] = 0;
    fprintf(stderr, "%s: %s\n", (label ? label : "Descriptor"), data);
}

//void release_device(libusb_device_handle *dev_handle)
//{
//    int rc = libusb_release_interface(dev_handle, 0);
//}

//void send_data(libusb_device_handle *dev_handle, uint8_t *data, int length)
//{
//    //libusb_bulk_transfer(dev_handle, epaddr, data, length, &actual, 100);
//}



void list_matching_devices(int vid, int pid)
{
    // if vid == -1, list all devices, if pid == -1 list all devices for matching vid
    
    fprintf(stderr, "Listing devices matching VID.PID == %04x.%04x\n", vid, pid);

    libusb_device **devs, *dev;

    int ndev = libusb_get_device_list(NULL, &devs);

    int i = 0;
    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        int rc = libusb_get_device_descriptor(dev, &desc);
        if (rc < 0) 
        {
            fprintf(stderr, "Failed to get device at index %d: %d %s\n", i, rc, libusb_strerror((enum libusb_error)rc));
            continue;
        }
        
        if (vid == -1 || vid == desc.idVendor)
        {
            if (pid == -1 || pid == desc.idProduct)
            {
                //get_device_info(pDesc)
                fprintf( stderr, "Found matching device: %04x.%04x\n", vid, pid);
                //print_device_info(dev, &desc);
                print_device_info(dev);
            }
        }
    }

    libusb_free_device_list(devs, 1);
    fprintf(stderr,"-------------------------\n");
}

//dev_handle1 = libusb_open_device_with_vid_pid(NULL, 0x04B4, desc.idProduct);  //opens the Cypress device. Gets the device handle for all future operations.

const char * Type_str[] = { "Control", "Isochronous", "Bulk", "Interrupt" }; // CHECK 
const char * Dir_str[] = { "Out", "In"};


//void print_device_info(libusb_device *dev, struct libusb_device_descriptor *pDesc)
void print_device_info(libusb_device *dev)
{
    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(dev, &desc);

    fprintf(stderr, "Bus Num: %2d, Address: %2d\n",
        libusb_get_bus_number(dev), libusb_get_device_address(dev));

    fprintf(stderr, "VID.PID: %04x.%04x\n", desc.idVendor, desc.idProduct);

    fprintf(stderr, "nConfigs: %d\n", desc.bNumConfigurations);

    int config_index = 0;
    for(config_index = 0; config_index< desc.bNumConfigurations; config_index++)
    {
        struct libusb_config_descriptor *config;
        libusb_get_config_descriptor(dev, config_index, &config);

        //fprintf( stderr, "Config: %d\n", config_index);
        fprintf( stderr, "Config: %d\n", config->bConfigurationValue);

        fprintf( stderr, "  nInterfaces: %d\n", config->bNumInterfaces);
        
        int i,j,k;

        for(i=0; i<config->bNumInterfaces; i++)
        {   
            const struct libusb_interface *inter;
            inter = &config->interface[i];
            fprintf( stderr, "  nAlternates: %d\n",inter->num_altsetting);

            for(j=0; j<inter->num_altsetting; j++)
            {
                const struct libusb_interface_descriptor *interdesc;
                interdesc = &inter->altsetting[j]; // interface descriptor
                fprintf( stderr, "  CIA (%d.%d.%d)\n", config->bConfigurationValue, interdesc->bInterfaceNumber, interdesc->bAlternateSetting);
//                fprintf( stderr, "    interface Number  %d\n", interdesc->bInterfaceNumber);
//                fprintf( stderr, "    Alternate Setting  %d\n", interdesc->bAlternateSetting);
                fprintf( stderr, "    nEndpoints:  %d\n", interdesc->bNumEndpoints);
                
                for(k=0; k<interdesc->bNumEndpoints; k++)
                {
                    const struct libusb_endpoint_descriptor *epdesc;
                    epdesc = &interdesc->endpoint[k]; // endpoint descriptor
                    fprintf( stderr, "      EP address: %2x  ", epdesc->bEndpointAddress);
                    
                    int type = epdesc->bmAttributes & 0x03;
                    int dir =  epdesc->bEndpointAddress & 0x80 ? 1 : 0;
                    fprintf( stderr, "      %s %s endpoint\n", Type_str[type], Dir_str[dir]);
                }
            }
        }
        
        libusb_free_config_descriptor(config);
    }
}

void print_device_info2(libusb_device_handle *dev_handle)
{
    libusb_device *dev = libusb_get_device(dev_handle);
    assert(dev);

    fprintf(stderr, "Bus Num: %2d, Address: %2d\n",
        libusb_get_bus_number(dev), libusb_get_device_address(dev));

    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(dev, &desc);

    fprintf(stderr, "VID.PID: %04x.%04x\n", desc.idVendor, desc.idProduct);
//    fprintf(stderr, "DEBUG: string index, M:%d, P:%d, S/N:%d\n", desc.iManufacturer, desc.iProduct, desc.iSerialNumber);
#if 0

    // FIXME: HACK
    desc.iManufacturer = 1;
    desc.iProduct = 2;
    desc.iSerialNumber = 3;
#endif

//    print_string_descriptor(dev_handle, 0, "Language");
    print_string_descriptor(dev_handle, desc.iManufacturer, "Manufacturer");
    print_string_descriptor(dev_handle, desc.iProduct, "Product");
    print_string_descriptor(dev_handle, desc.iSerialNumber, "Serial Number");

    fprintf(stderr, "nConfigs: %d\n", desc.bNumConfigurations);

    int config_index = 0;
    for(config_index = 0; config_index< desc.bNumConfigurations; config_index++)
    {
        struct libusb_config_descriptor *config;
        libusb_get_config_descriptor(dev, config_index, &config);

        //fprintf( stderr, "Config: %d\n", config_index);
        fprintf( stderr, "Config: %d\n", config->bConfigurationValue);
//        print_string_descriptor(dev_handle, config->iConfiguration, "Configuration");

        fprintf( stderr, "  nInterfaces: %d\n", config->bNumInterfaces);
        
        int i,j,k;

        for(i=0; i<config->bNumInterfaces; i++)
        {   
            const struct libusb_interface *inter;
            inter = &config->interface[i];
            fprintf( stderr, "  nAlternates: %d\n",inter->num_altsetting);

            for(j=0; j<inter->num_altsetting; j++)
            {
                const struct libusb_interface_descriptor *interdesc;
                interdesc = &inter->altsetting[j]; // interface descriptor
                fprintf( stderr, "  CIA (%d.%d.%d)\n", config->bConfigurationValue, interdesc->bInterfaceNumber, interdesc->bAlternateSetting);
//                print_string_descriptor(dev_handle, interdesc->iInterface, "Interface");
//                fprintf( stderr, "    interface Number  %d\n", interdesc->bInterfaceNumber);
//                fprintf( stderr, "    Alternate Setting  %d\n", interdesc->bAlternateSetting);
                fprintf( stderr, "    nEndpoints:  %d\n", interdesc->bNumEndpoints);
                
                for(k=0; k<interdesc->bNumEndpoints; k++)
                {
                    const struct libusb_endpoint_descriptor *epdesc;
                    epdesc = &interdesc->endpoint[k]; // endpoint descriptor
                    fprintf( stderr, "      EP address: %2x  ", epdesc->bEndpointAddress);
                    
                    int type = epdesc->bmAttributes & 0x03;
                    int dir =  epdesc->bEndpointAddress & 0x80 ? 1 : 0;
                    fprintf( stderr, "      %s %s endpoint\n", Type_str[type], Dir_str[dir]);
                }
            }
        }
        
        libusb_free_config_descriptor(config);
    }
}
