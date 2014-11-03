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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <memory>
//#include <stdexcept>

#include "Programmer.h"

#include "HexFileFormat.h"
#include "HierINIReader.h"
#include "usb.h"
#include "fx2.h"
#include "utils.h"


#define VID_CYPRESS     0x04B4
#define PID_PROG_DEVKIT5_UNCONFIGURED 0xF131
#define PID_PROG_DEVKIT5_CONFIGURED   0xF132


#define DEFAULT_FX2_CONFIG_FILE         "fx2_config.hex"
#define DEFAULT_FX2_VID_UNCONFIGURED    VID_CYPRESS
#define DEFAULT_FX2_PID_UNCONFIGURED    PID_PROG_DEVKIT5_UNCONFIGURED
#define DEFAULT_FX2_VID_CONFIGURED      VID_CYPRESS
#define DEFAULT_FX2_PID_CONFIGURED      PID_PROG_DEVKIT5_CONFIGURED


//#define VID_ANY         -1
//#define PID_ANY         -1

//#define CTRL_IN   (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
//#define CTRL_OUT  (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)


// SWD Packet Header definitions
#define APACC_ADDR_WRITE         0x8B
#define APACC_DATA_READ          0x9F
#define APACC_CTRLSTAT_WRITE     0xA3
#define APACC_DATA_WRITE         0xBB

#define DPACC_READBUFF_WRITE     0x99
#define DPACC_IDOCDE_READ        0xA5
#define DPACC_CTRLSTAT_WRITE     0xA9
#define DPACC_SELECT_WRITE       0xB1

#if 0
#define PORT_ACQUIRE_KEY_HEADER  0x99
#define TESTMODE_ADDRESS_HEADER  0x8B
#define TESTMODE_KEY_HEADER      0xBB
#endif


// FIXME: where did TIMEOUT value come from !?
#define SPC_POLL_TIMEOUT      8404
#define SPC_STATUS_DATA_READY 0x01
#define SPC_STATUS_IDLE       0x02


// each addr/data cmd is 5 bytes each (1 cmd, 4 data)
// Longest request relates to writing 288 bytes = 288 * 5 = 1440 so allow 2048
#define REQUEST_MAX_LEN 2048
#define REPLY_MAX_LEN 2048


#define EPADDR_BULK_OUT 0x02 
#define EPADDR_BULK_IN  0x84 

// System Performance Controller (SPC)

#define REG_SPC_CPU_DATA        0x40004720
#define REG_SPC_DMA_DATA        0x40004721
#define REG_SPC_STATUS          0x40004722

#define SPC_CMD_LOAD_BYTE           0x00
#define SPC_CMD_LOAD_MULTI_BYTE     0x01
#define SPC_CMD_LOAD_ROW            0x02
#define SPC_CMD_READ_BYTE           0x03
#define SPC_CMD_READ_MULTI_BYTE     0x04
#define SPC_CMD_WRITE_ROW           0x05
#define SPC_CMD_WRITE_NVL           0x06
// PSoC TRM 50235 Ch 44.3 suggest 0x05 erases first then writes but 0x07 only writes
#define SPC_CMD_PROG_ROW            0x07
#define SPC_CMD_ERASE_SECTOR        0x08
#define SPC_CMD_ERASE_ALL           0x09
#define SPC_CMD_READ_HIDDEN_ROW     0x0A
#define SPC_CMD_PROTECT             0x0B
#define SPC_CMD_GET_CHECKSUM        0x0C
#define SPC_CMD_GET_TEMPERATURE     0x0E
#define SPC_CMD_READ_NVL_VOL_BYTE   0x10

#define SPC_KEY1                0xb6
#define SPC_KEY2                0xd3

#define SPC_NV_AID_FLASH_START  0x00
#define SPC_NV_AID_FLASH_END    0x3E
#define SPC_NV_AID_FLASH_ALL    0x3F
#define SPC_NV_AID_EEPROM       0x40
#define SPC_NV_AID_CONFIG       0x80
#define SPC_NV_AID_WOL          0xF8


#define TEST_MODE_KEY_REGISTER  0x40050210
#define TEST_MODE_KEY           0xEA7E30A9


//#define CONFIG_ADDRESS_OFFSET   0x00800000

// ACK response is stored as a byte. Possible ACK values are:
//  0x01 - SWD_OK_ACK (SUCCESS)
//  0x02 - SWD_WAIT_ACK
//  0x04 - SWD_FAULT_ACK
//  0x1x - Timeout (T_testmode exceeded) - treat same as FAULT_ACK
// reply returned by USB interface. ACK may be LSbit. What are bits 5,6
#define REPLY_OK                    0x21
#define REPLY_XX1                   0x24

#define REPLY_JTAGID_MATCHED        0x31
#define REPLY_JTAGID_NOMATCH        0x27
// also get 0x31, 0x37

//--------

// REFERENCE: Use PSoC5 Device Progamming Specifications 001-64359


#if 0
struct program_layout_for_device_s
{
    int num_flash_arrays;
    int num_flash_rows;
    int bytes_per_row;
};
#endif

#if 0
class Program
{
  public:
    uint8_t *m_data;
    int m_length;
    struct program_layout_for_device_s layout;
};
#endif



static int debug = 1;

// ========================

class Request
{
    int m_len;
    uint8_t m_ep_addr;
    uint8_t m_data[REQUEST_MAX_LEN];

  public:
    uint32_t m_debug;
//    int n_commands;

  public:
    Request(uint8_t ep_addr);

    void reset(void);
    inline int length(void) { return m_len;}; // TEMP
    bool send(libusb_device_handle *dev_handle);
    bool clear_stall(libusb_device_handle *dev_handle);


    inline void apacc_addr_write(uint32_t addr) { c1d4(APACC_ADDR_WRITE, addr); }
    inline void apacc_data_write(uint32_t data) { c1d4(APACC_DATA_WRITE, data); }
//    inline void apacc_data_read(void) { c1(APACC_DATA_READ); }
    inline void apacc_ctrl_write(uint32_t reg) { c1d4(APACC_CTRLSTAT_WRITE, reg); }
    inline void dpacc_ctrl_write(uint32_t reg) { c1d4(DPACC_CTRLSTAT_WRITE, reg); }
    inline void dpacc_select_write(uint32_t reg) { c1d4(DPACC_SELECT_WRITE, reg); }
//    inline void dpacc_readbuff_write(uint32_t reg) { c1d4(DPACC_READBUFF_WRITE, reg); }
//    inline void dpacc_idcode_read(void) { c1(DPACC_IDOCDE_READ); }
    void apacc_data_read(int n_reads);

    void c1(uint8_t cmd);
    void c1d4(uint8_t cmd, uint32_t data);
    void raw_hex(const char *hexstr);
};


// ========================

class Reply
{
    uint8_t m_ep_addr;
    int m_len;
    uint8_t *m_data;
    int m_head;

  public:
    uint32_t m_debug;

    Reply(uint8_t ep_addr);
    ~Reply();

//    void set_data(uint8_t *data, int len);
    bool receive(libusb_device_handle *dev_handle);
    bool clear_stall(libusb_device_handle *dev_handle);

    bool pop_ok(int n=1);
    bool pop_b4_ok(uint8_t *data);
    bool pop_nb0_ok(uint8_t *data, int length);
};


// ========================


// contains programmer state: (FIXME excluding other things .. maybe adjust this. It was used to hide impl)
struct programmer_priv_s
{
    Request request;
    Reply   reply;
    libusb_device_handle * dev_handle;

    programmer_priv_s() : request(EPADDR_BULK_OUT), reply(EPADDR_BULK_IN), dev_handle(NULL) {}
};


// Public Interface
// ================


Programmer::Programmer()
    : m_programmer_configured(false) // FIXME: into priv_s ?
    , m_priv(NULL)
    , m_debug(0)
    , m_devdata(0)
    , m_fx2_config_file()
    , m_vid_unconfigured(0)
    , m_pid_unconfigured(0)
    , m_vid_configured(0)
    , m_pid_configured(0)

{
    //m_priv = (struct programmer_priv_s *) calloc(1, sizeof(struct programmer_priv_s));
    m_priv = new struct programmer_priv_s;
    assert(m_priv);
}

Programmer::~Programmer()
{
    fprintf(stderr, "Programmer Destructor\n");
    close();

    delete m_priv;
    m_priv = NULL;
}


bool Programmer::read_config(std::string config_path, std::string config_filename)
{
    // pass config_filepath and filename separately so that we have a base for finding fx2_config_file etc
    // FIXME: alternatively could use basename on a fq pathname !?

    std::string config_filepath = config_path + std::string("/") + config_filename; // FIXME: Need pathcat
    HierINIReader reader(config_filepath);

    if (reader.ParseError() < 0) {
        //std::cout << "Can't read '%s'\n" << filename;
        return false;
    }

    std::string Programmer_Config_Section = "Programmer";

    m_fx2_config_file = config_path + std::string("/") + reader.Get(Programmer_Config_Section, "fx2_config_file", DEFAULT_FX2_CONFIG_FILE); // FIXME: pathcat
    m_vid_unconfigured = reader.GetInteger(Programmer_Config_Section, "VID_unconfigured", DEFAULT_FX2_VID_UNCONFIGURED);
    m_pid_unconfigured = reader.GetInteger(Programmer_Config_Section, "PID_unconfigured", DEFAULT_FX2_PID_UNCONFIGURED);

    m_vid_configured = reader.GetInteger(Programmer_Config_Section, "VID", DEFAULT_FX2_VID_CONFIGURED); // FIXME is it valid to have a default since it depends on fx2_config_file - well we have default config_file so...!?
    m_pid_configured = reader.GetInteger(Programmer_Config_Section, "PID", DEFAULT_FX2_PID_CONFIGURED); // FIXME is it valid to have a default since it depends on fx2_config_file - well we have default config_file so...!?

    return true;
}


int Programmer::open(std::string config_path, std::string config_filename, DeviceData *devdata) // const char *device_name) // , const char *config_file)
{
    // Bit dangerouse but Device data is not needed in every case. Caller reponsible. Should add some protection in though.

    if (m_devdata)
        delete(m_devdata); // FIXME should I do this in constructor instead !?

    if (devdata)
        m_devdata = new DeviceData(*devdata); // Hmmm should I copy it Issue with ownership or use shared_ptr/auto_ptr !?

#if 0
    if (m_devdata == NULL)
    {
        fprintf(stderr, "Device Data is not defined\n");
        return FAILURE;
    }
#endif

    if (m_priv->dev_handle != NULL)
    {
        fprintf(stderr, "Device already open\n");
        return SUCCESS;
    }

    if (!read_config(config_path, config_filename))
    {
        fprintf(stderr, "Failed to read/parse config file: '%s %s'\n", config_path.c_str(), config_filename.c_str());
        return FAILURE;
    }


    int i;
    for (i=0; i<2; i++) // try twice
    {
        fprintf(stderr, "Opening configured device 0x[%04x:%04x]...\n", m_vid_configured, m_pid_configured);
        m_priv->dev_handle = open_device(m_vid_configured, m_pid_configured, 1, 0, 0); // vid, pic, config, interface, alternate
        if (m_priv->dev_handle)
            break;

        if (!m_programmer_configured)
            configure_usb_programmer(); // choosing to ignore rc for the moment. I know 

        if (i > 0)
        {
            fprintf(stderr, "Failed to configure programmer\n");
            return FAILURE;
        }
    }

#if 0
    // instantiate programmable device info
    // Note: This is clearly a hack but I've only got one device for now...
    if (strcmp(device_name, "PSOC5") == 0)
    {
        m_device_family = DEVICE_PSOC5; //new PSoC5();
        //m_device_geom = PSoC5::get_device_geometry();
    }
    else
    {
        fprintf(stderr, "Unrecognised device [%s]\n", device_name);
        // close anything !?
        return FAILURE;
    }
#endif

    return SUCCESS;
}


void Programmer::close()
{
    fprintf(stderr,"CLOSE\n");
    if (m_priv->dev_handle)
        close_device(m_priv->dev_handle, 0);

    m_priv->dev_handle = NULL;

    if (m_devdata) delete(m_devdata); // FIXME: who owns/frees this
    m_devdata = NULL;
}


bool Programmer::read_device(AppData *appdata, uint32_t flags)
{
    // No "code" state in Programmer - all "state" in appdata
    bool config_trim_flash = flags & RD_TRIM_FLASH;    // affects code and config
    bool config_trim_eeprom = flags & RD_TRIM_EEPROM;

    bool rc;

    appdata->device_id = get_jtag_id();

    rc = NV_WOL_read(appdata);
    if (!rc) return false;

    rc = NV_device_config_read(appdata);
    if (!rc) return false;

    rc = NV_flash_read(appdata, config_trim_flash); // code and config
    if (!rc) return false;

    rc = NV_flash_checksum(appdata); // stores checksum in appdata

    uint32_t cksum = appdata->calc_checksum(false);
    if (!rc) return false;
    fprintf(stderr,"Code checksums %s\n", (cksum == appdata->checksum)? "MATCH" : "MISMATCH!");
    if (cksum != appdata->checksum)
        fprintf(stderr, "Flash checksum: 0x%02x, calculated checksum: 0x%02x\n", appdata->checksum, cksum);

    rc = NV_protection_read(appdata);
    if (!rc) return false;

    rc = NV_eeprom_read(appdata, config_trim_eeprom);
    if (!rc) return false;

    return true;
}


bool Programmer::erase_flash(void)
{
    return NV_erase_flash();
}


bool Programmer::write_device(const AppData *appdata)
{
    fprintf(stderr,"Note: WOL/device_config writes are disabled\n");
    // Assume checksum ok !?
    // TODO: Check device_id matches (file vs actual device)

    bool rc;
#if 0
    NV_erase_flash();  // FIXME: could optionally just erase written blocks
                        // using CMD 0x05 in flash_write which self erases
    return true;
#endif
    rc = NV_flash_write(appdata); // code and config. Note erases as it goes (but just written locations)
    if (!rc) return false;


    if (appdata->protection->length() == 0)
        fprintf(stderr,"Note: protection memory remains unchanged (ie not cleared)\n");
#if 0
    rc = NV_protection_write(appdata);
    if (!rc) return false;
#else
    if (appdata->protection->length() > 0)
        fprintf(stderr,"Note: protection memory write is disabled (code not tested)\n");
#endif

    rc = NV_WOL_write(appdata);
    if (!rc) return false;

    rc = NV_device_config_write(appdata);
    if (!rc) return false;


    if (appdata->eeprom->length() == 0)
        fprintf(stderr,"Note: EEPROM memory remains unchanged (ie not cleared)\n");
#if 0
    rc = NV_eeprom_write(appdata);
    if (!rc) return false;
#else
    if (appdata->eeprom->length() > 0)
        fprintf(stderr,"Note: EEPROM write is disabled (code not tested)\n");
#endif

    // FIXME: validate checksum !?
    // rc = NV_readchecksum(appdata);
    // if (!rc) return false;
    return true;
}



std::string Programmer::verify_status_string(uint32_t match_status)
{
    if (match_status == 0) return "OK";

    std::string status_str("Mismatch: ");

    if (match_status & VERIFY_MISMATCH_CODE) status_str += "Code,";
    if (match_status & VERIFY_MISMATCH_CONFIG) status_str += "Config,";
    if (match_status & VERIFY_MISMATCH_PROTECTION) status_str += "Protectioin,";
    if (match_status & VERIFY_MISMATCH_EEPROM) status_str += "EEPROM,";
    if (match_status & VERIFY_MISMATCH_WOL) status_str += "WOL,";
    if (match_status & VERIFY_MISMATCH_DEVCONFIG) status_str += "Devconfig,";
    if (match_status & VERIFY_MISMATCH_JTAGID) status_str += "JTAGId,";
    if (match_status & VERIFY_MISSING_FILE_DATA) status_str += "Missing File,";
    if (match_status & VERIFY_DEVICE_READ_FAILED) status_str += "Device Read Failed,";

    status_str.resize(status_str.length() - 1); // status_str.pop_back(); // remove trailing comma

    return status_str;
}


uint32_t Programmer::verify_device(const AppData *file_appdata, uint32_t flags)
{
    // flags:  how to handle extra data sets (eg eeprom/protection etc) from device that are not in file data and non-zero
    // return bitmask of mismatches
    
    assert(0 && "TO DO");
    uint32_t match_status;

    if (!file_appdata) return VERIFY_MISSING_FILE_DATA;

    AppData device_appdata;
    uint32_t read_dev_flags = 0; // FIXME correct !?

    bool rc = read_device(&device_appdata, read_dev_flags);
    if (!rc) return VERIFY_DEVICE_READ_FAILED;

    // compare code and config via checksum. if checksum mismatches check code and config by length/byte-by-byte if both exist
    if (file_appdata->checksum != device_appdata.checksum)
    {
        // For now:
        match_status |= (VERIFY_MISMATCH_CODE | VERIFY_MISMATCH_CONFIG);
        // FIXME: more checks to distinguish CODE and CONFIG mismatch
    }

    // compare protection, eeprom byte for byte. missing data assume == 0
    // FIXME

    // compare security_WOL, devconfig
    if (file_appdata->security_WOL != device_appdata.security_WOL)
        match_status |= VERIFY_MISMATCH_WOL;

    if (file_appdata->device_config != device_appdata.device_config)
        match_status |= VERIFY_MISMATCH_DEVCONFIG;

    // compare jtag_id to relevant field in metadata. Ignore rest of metadata
    if (file_appdata->device_id != device_appdata.device_id)
        match_status |= VERIFY_MISMATCH_JTAGID;
    
    return match_status;
}



#if 0
bool Programmer::verify_flash(Program &prog)
{
    int prog_len = PSoC5::flash_size(); // FIXME

    uint8_t *data = (uint8_t *)calloc(prog_len, 1); // HACK
    assert(data != NULL);

    bool rc = read_flash(data, prog_len);
    assert(rc == true);

    bool same = false;
    if (memcmp(data, prog.m_data, prog.m_length) == 0)
    {
        same = true;
    }

    free(data);

    return same;
}
#endif



// Low level support routines
// ==========================

bool Programmer::configure_usb_programmer(/*const char *config_file*/)
{
    if (m_programmer_configured)
    {
        // FIXME: maybe close and retry
        fprintf(stderr, "Can't (re)configure - device already configured\n");
        return m_programmer_configured; // true
    }

    if (m_priv->dev_handle)
    {
        // FIXME: maybe close and retry
        fprintf(stderr, "Shouldn't configure - device already open\n");
        return m_programmer_configured;
    }

    // Unconfigured device handle (not same as m_dev_handle)
    fprintf(stderr, "Opening unconfigured device 0x[%04x:%04x]...\n", m_vid_unconfigured, m_pid_unconfigured);
    libusb_device_handle *dev_handle = open_device(m_vid_unconfigured, m_pid_unconfigured, 1, 0, 0); // vid, pic, config, interface, alternate
    if (!dev_handle)
    {
        fprintf(stderr, "Failed to find unconfigured programmer 0x%04x.%04x\n", m_vid_unconfigured, m_pid_unconfigured);
        return m_programmer_configured;
    }

    fprintf(stderr, "Configuring USB programmer\n");

    int rc;

//    rc = fx2_cmd_rw_ram(dev_handle, FX2_REG_CTRLSTATUS, "01"); // FX2  force 8051 into reset
    rc = fx2_cmd_8051_enable(dev_handle, false);

//    try {
    HexData hexdata;

    if (!hexdata.read_hex(m_fx2_config_file.c_str()))
    {
        fprintf(stderr, "Error reading FX2 config file: %s\n", m_fx2_config_file.c_str());
        close_device(dev_handle, 0);
    }

    //fprintf(stderr, "read hex config file\n");

    HexData *newhexdata = hexdata.reshape(2048);

    int nblocks = newhexdata->nblocks();
    int i;
    fprintf(stderr,"Configuring (%d) : ", nblocks);
    for(i=0; i<nblocks; i++)
    {
        const Block *block = (*newhexdata)[i];
        assert(block != NULL);
        fprintf(stderr,"%d ", i);
        if (debug)
        {
            fprintf(stderr,"Sending...:\n");
            block->dump(stderr, 48);
        }
        rc = fx2_cmd_rw_ram(dev_handle, block);
    }
    fprintf(stderr,"\n");
    delete newhexdata;
#if 0
    }
    catch (std::runtime_error& e)
    {
        //fprintf(stderr, "Failed to read FX2 config file: %s\n", m_fx2_config_file.c_str());
        fprintf(stderr, "Error reading FX2 config file: %s\n", e.what()); // m_fx2_config_file.c_str());
        // FIXME: should rethrow here  and not attempt to configure again in this case
        close_device(dev_handle, 0);
        return false;
    }
#endif

    // Re-enable CPU
//    rc = fx2_cmd_rw_ram(dev_handle, FX2_REG_CTRLSTATUS, "00"); // FX2  run 8051
    rc = fx2_cmd_8051_enable(dev_handle, true);


//    libusb_reset_device(dev_handle);
    close_device(dev_handle, 0);
    sleep(3); // one second isn't long enough for device to reset

    m_programmer_configured = true;

    fprintf(stderr, "Configured USB programmer\n");

    return m_programmer_configured;
}


// ==============

bool Programmer::switch_to_swd(void)
{
    // Switches the PSoC programming interface to SWD interface

    // The switching sequence is implemented in JtagToSwdSequence()
    // Device ID read (DPACC IDCODE Read) is needed to complete the switching.
    // The Device ID code returned may be discarded.

    // Return:
    //  SUCCESS - Successfully switched to SWD interface
    //  FAILURE - SWD interface switching failed 

    bool ok = true;

    if (!jtag_to_swd())  /* Initial part of the switching sequence */
    {
        fprintf(stderr,"Jtag to SWD FAILED\n");
    }
    
    get_jtag_id();

    // FIXME: set return value appropriately

    return ok;
}


bool Programmer::jtag_to_swd(void)
{
    // line reset
    //  swdio high
    //   loop 51:
    //     clk  Low, High
    // send 0x9e, 0xe7
    // line reset
    //    ....
    // line idle:
    //   line reset
    //   swdio low
    //   loop 3:  (Dummy clocks)
    //     clk Low, High

    fprintf(stderr, "JTAG TO SWD\n");
    uint8_t bmRequestType = 0xc0;
    uint8_t reply_data[100];
    uint16_t reply_length = 0;
    int rc;

#if 1
// FIXME: Try to remove this:
    rc = control_transfer(m_priv->dev_handle, bmRequestType, 95, 0x0000 /*wValue*/, 0, reply_data, &reply_length, 10); // test4.766
        // without this we normally get SPC timeout errors. Maybe some var isn't set or maybe there some timing issue
//    usleep(20);

    // ap_register_write(TEST_MODE_KEY_REGISTER, TEST_MODE_KEY);
    m_priv->request.reset();
    m_priv->request.apacc_addr_write(TEST_MODE_KEY_REGISTER);
    m_priv->request.apacc_data_write(TEST_MODE_KEY);
    if (!send_receive()) return false;
    // pop_ok (FIXME) ->  373131317714a02b31
#endif

    m_priv->request.reset();
    //m_priv->request.raw_hex("f0f0f0f0f0f0f0f0f0f0f0f0f0e09070e0f0f0f0f0f0f0f0f0f0f0f0f0f0");
    m_priv->request.raw_hex("F0F000000000000000000000000000000000000000000000000000000000");
    if (!send_receive()) return false;
//    m_priv->reply.pop_ok(15);
    m_priv->reply.pop_ok(-1);

//    usleep(20); // Not sure if this helps
/*
#REPLY:
    21h: cmd accepted OK !?
    27h: JTAG PORT ID didn't match fixed value in firmware: x2ba01477
    31h: JTAG PORT ID DID match
*/

// Note: Only needed first time after programming and it doesn't really matter what command it is.
// Without it we get errors like:
// f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 e0 90 70 
// e0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 
// pop_ok Failed Expected 0x21 found 0x27. Data: a9 30 7e ea 27 
// pop_ok Failed Expected 0x21 found 0x27. Data: 27 27 27 27 27 27 27 27 27 27 27 
// enter_programming_mode - END
// pop_ok Failed Expected 0x21 found 0x27. Data: 02 00 00 00 27 
// Silicon ID: 0x0000
// READ PROTECTION
// FIXME: assuming ecc used for data
// pop_ok Failed Expected 0x21 found 0x27. Data: 27 22 47 00 40 27 
// pop_ok Failed Expected 0x21 found 0x27. Data: 27 22 47 00 40 27 
// pop_ok Failed Expecte

// OR

// e0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 f0 
// pop_ok Failed Expected 0x21 found 0x24. Data: 24 24 24 24 24 24 24 24 24 24 24 
// enter_programming_mode - END
// Silicon ID: 0x2ba01477
// READ PROTECTION
// FIXME: assuming ecc used for data
// pop_ok Failed Expected 0x21 found 0x24. Data: 24 00 00 00 00 24 
// pop_ok Failed Expected 0x21 found 0x24. Data: 24 00 00 00 00 24 
// pop_ok Failed Expected 0x21 found 0x24. Data: 24 00 00 00 00 24 


// THIS SHOULD NOT BE NEEDED HERE or do I needto get it twice !?
    get_jtag_id(); // without this sometimes get operation timed out - may be timing issue

#if 0
// FIXME: Try to remove this:
    rc = control_transfer(m_priv->dev_handle, bmRequestType, 95, 0x0000 /*wValue*/, 0, reply_data, &reply_length, 10); // test4.766

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(TEST_MODE_KEY_REGISTER);
    m_priv->request.apacc_data_write(TEST_MODE_KEY);
    if (!send_receive()) return false;
    // pop_ok (FIXME) ->  373131317714a02b31
#endif

    return true;
}


bool Programmer::enter_programming_mode(void)
{
//    fprintf(stderr,"configure_target_device - START\n");
    fprintf(stderr,"enter_programming_mode - START\n");
    // Configures the PSoC device to prepare for programming

    if (switch_to_swd() == FAILURE)
        return false;
    
    m_priv->request.reset();
    m_priv->request.dpacc_ctrl_write(0x50000000); // test4.823
    m_priv->request.dpacc_select_write(0x00000000); // Clear DP Select Register  // test4.825
    m_priv->request.apacc_ctrl_write(0x22000002); // Set 32-bit DAP transfer mode  // test4.827

    m_priv->request.apacc_addr_write(0xE000EDF0); // ..
    m_priv->request.apacc_data_write(0xA05F0003); // Halt CPU and Activate Debug  // ..

    m_priv->request.apacc_addr_write(0x4008000C); // ..
    m_priv->request.apacc_data_write(0x00000002); // release Cortex-M3 CPU reset  // test4.835

    m_priv->request.apacc_addr_write(0x400043A0);
    m_priv->request.apacc_data_write(0x000000BF); // enable individual chip subsystem

    m_priv->request.apacc_addr_write(0x40004200);
    m_priv->request.apacc_data_write(0x00000002); // IMO set to 24MHz
 
    if (!send_receive()) return false;

    m_priv->reply.pop_ok(-1); // FIXME

    //fprintf(stderr,"configure_target_device - END\n");
    fprintf(stderr,"enter_programming_mode - END\n");
    return true;   
}


void Programmer::exit_programming_mode(void)
{
    fprintf(stderr,"exit_programming_mode - TODO\n");
    // reset ? CMD 100
}


void Programmer::reset_cpu(void)
{
    uint8_t reply_data[2];
    uint16_t reply_length = 0;
    bool rc = control_transfer(m_priv->dev_handle, 0xc0, 100, 0x0001 /*wValue*/, 0, reply_data, &reply_length, 2);
    usleep(100); // enough !?
    rc = control_transfer(m_priv->dev_handle, 0xc0, 100, 0x0000 /*wValue*/, 0, reply_data, &reply_length, 2);
}

#if 0
int Programmer::num_rows_in_array(const AppData *appdata, uint8_t array_id)
{
    return (array_id == (prog_geom.num_arrays - 1))
      ? (prog_geom.num_rows - (array_id * m_devdata->flash_rows_per_array))
      : m_devdata->flash_rows_per_array;
}
#endif


// High level NV methods
// ----------------------


bool Programmer::NV_flash_read(AppData *appdata, bool trim)
{
    // Data stored in HexData uses a base address (based on Hex File) not address of internal PSoC mem addresses
    // Note: Could use straght address interface !?

    fprintf(stderr,"FLASH READ\n");
    // Note: If extra flash is being used for ECC then we'll ens up storign ECC data in config structure. Issued ?? FIXME ??.
    // Prob just want to avoid writing it back later on.

    assert(appdata != NULL);

    if (appdata->code) delete appdata->code;
    if (appdata->config) delete appdata->config;

    appdata->code = new HexData();
    appdata->config = new HexData();

    v_uint8_t pcode(m_devdata->flash_code_bytes_per_row); // local buffer
    v_uint8_t pconfig(m_devdata->flash_config_bytes_per_row); // local buffer

    uint8_t ai; // array index
    for(ai = 0; ai < m_devdata->flash_num_arrays; ai++)
    {
        int ri; // row index
        for(ri = 0; ri < m_devdata->flash_rows_per_array; ri++)
        {
            // Code region address
            uint32_t address_offset = ri * m_devdata->flash_code_bytes_per_row;
            uint32_t dev_address = address_offset + m_devdata->flash_code_base_address;

            //std::fill(pcode.begin(), pcode.end(), 0); // I prefer memset !!
            pcode.assign(m_devdata->flash_code_bytes_per_row, 0);

            bool rc = NV_read_multi_bytes(ai, dev_address, pcode.data(), pcode.size());
            if (!rc) return false;
            appdata->code->add(HexFileFormat::FLASH_CODE_ADDRESS + address_offset, pcode);


            // Config region address
            address_offset = ri * m_devdata->flash_config_bytes_per_row;
            dev_address = address_offset + m_devdata->flash_config_base_address;   // top bit set means ECC/config address space

            //std::fill(pconfig.begin(), pconfig.end(), 0); // I prefer memset !!
            pconfig.assign(m_devdata->flash_config_bytes_per_row, 0);

            rc = NV_read_multi_bytes(ai, dev_address, pconfig.data(), pconfig.size());
            if (!rc) return false;

            // FIXME: this ain't right. is config == program or is config == data ?
            //if (devdata->geom->config_used_for_code)
            if (appdata->extra_flash_used_for_config())
                appdata->config->add(HexFileFormat::CONFIG_ADDRESS + address_offset, pconfig);
        } // ri
    } // ai
    
    if (trim)
    {
        if (debug) fprintf(stderr,"Untrimmed code len:%d\n",appdata->code->length());
        appdata->code->trim();
        appdata->config->trim();
        if (debug) fprintf(stderr,"Trimmed code len:%d\n",appdata->code->length());
    }

    return true;
}


int Programmer::NV_flash_row_length(const AppData *appdata) const
{
    // if reading device data then it should be based on contents of ECC field in device Device Config (USER NVL) !?
    // if writing a program then it should be based on program Device Config field

    assert(appdata);

    return m_devdata->flash_code_bytes_per_row
           + (appdata->extra_flash_used_for_config() ? m_devdata->flash_config_bytes_per_row : 0);
}


bool Programmer::NV_flash_write(const AppData *appdata)
{
    // FIXME: don't write checksum that is stored at address 0x90300000 (MSB) and 0001 (LSB)
    fprintf(stderr,"FLASH WRITE\n");

    assert(appdata);

    int code_len = 0;
    if (appdata->code)
        code_len = appdata->code->length();

    int config_len = 0;
    if (appdata->config)
        config_len = appdata->config->length();

    if (code_len == 0)
    {
        if (config_len == 0)
        {
            fprintf(stderr, "flash_write: code and config both empty. nothing to do\n");
            return true;
        }
    }

    // FIXME: Also need to ensure device is in correct state to receive any extra config data. ie ECCEN must be correct first)
   // read devconfig and  check ECCEN bit

    if (code_len > m_devdata->flash_code_max_size)
    {
        fprintf(stderr, "flash_write: too much data. Have %d expected no more than %d bytes\n", appdata->code->length(), m_devdata->flash_code_max_size);
        return false;
    }

    if (config_len > 0 && appdata->extra_flash_used_for_config() == false)
    {
        fprintf(stderr,"Config data len:%d, extra_flash_used_for_config:%d\n", config_len, appdata->extra_flash_used_for_config());
        assert("Mismatch between config usage and config data");
    }

    // assume that if we have config data it's able to be written (ie not ECC flash mode)

    int num_code_rows = code_len / m_devdata->flash_code_bytes_per_row;
    if (code_len % m_devdata->flash_code_bytes_per_row != 0) num_code_rows++;
    
    int num_config_rows = config_len / m_devdata->flash_config_bytes_per_row;
    if (code_len % m_devdata->flash_config_bytes_per_row != 0) num_config_rows++;

    // we have to program:  num_rows = MAX(num_code_rows, num_config_rows)
    int num_rows = MAX(num_code_rows, num_config_rows);

#if 0
    if (!send_c1d4_recv_ok(APACC_CTRLSTAT_WRITE, 0x22000002)) return false; // 4byte access to SRAM (only for SRAM Version for write_row)
#endif

#if 0
#if 1
    int narrays, remainder_rows;
    PSoC5::program_geom(devdata->geom, code_len, &narrays, &remainder_rows);
#else
    int array_size = m_devdata->flash_rows_per_array * m_devdata->flash_code_bytes_per_row;
    int narrays = code_len / array_size;

    int remainder = code_len % array_size;
    if (remainder) narrays++;
    int remainder_rows = remainder / m_devdata->flash_code_bytes_per_row;
    if (remainder % m_devdata->flash_code_bytes_per_row) remainder_rows++;
#endif
#endif

    int narrays = num_rows / m_devdata->flash_rows_per_array;  // CHECK flash_rows_per_array same for main and ECC (would have to be!!)
    int remainder_rows = num_rows % m_devdata->flash_rows_per_array;
    if (remainder_rows) narrays++;

// FIXME const int row_len = NV_flash_row_length(appdata);
// it was defaulting to 256 but device is configured for no ECC so SPC subsystem timed out.
    int row_len = m_devdata->flash_code_bytes_per_row;

   if (appdata->extra_flash_used_for_config())
        row_len += m_devdata->flash_config_bytes_per_row;


    fprintf(stderr,"code_len:%d, n_code_rows:%d, config_len:%d, n_config_rows:%d, narrays:%d, remainder_rows:%d, row_len:%d\n",
                code_len, num_code_rows, config_len, num_config_rows, narrays, remainder_rows, row_len);

    assert(narrays <= m_devdata->flash_num_arrays);

    uint8_t *row_data = (uint8_t *)malloc(row_len);
    assert(row_data);

    int die_temp = get_die_temperature(); // first value post reset is wrong - discard
    die_temp = get_die_temperature();

    int ai; // array index
    for(ai = 0; ai < narrays; ai++)
    {
        //int rows_in_array = num_rows_in_array(prog_geom-> ai);
        int rows_in_array = (remainder_rows && ai==narrays-1) ? remainder_rows : m_devdata->flash_rows_per_array;
        uint32_t hexdata_address;
        
        int ri; // row index
        for(ri = 0; ri < rows_in_array; ri++)
        {
            memset(row_data, 0, row_len);

            if (code_len) // optimisation only
            {
                // fill up 256 bytes in buffer. If no code for this area we get back a zeroed buffer
                hexdata_address = HexFileFormat::FLASH_CODE_ADDRESS + ri * m_devdata->flash_code_bytes_per_row;
                appdata->code->extract2bin(hexdata_address, m_devdata->flash_code_bytes_per_row, row_data);
            }

            if (config_len) // optimisation only.
            {
                // fill up extra 32 bytes in buffer. If no code for this area we get back a zeroed buffer
                hexdata_address = HexFileFormat::CONFIG_ADDRESS + ri * m_devdata->flash_config_bytes_per_row;
                appdata->config->extract2bin(hexdata_address, 
                        m_devdata->flash_config_bytes_per_row, row_data + m_devdata->flash_code_bytes_per_row);
                dump_data(stderr, row_data + m_devdata->flash_code_bytes_per_row, m_devdata->flash_config_bytes_per_row, "CONFIG");
            }

            if (!NV_write_row(ai, ri, die_temp, row_data, row_len))
            {
                assert(0 && "failed writing flash data - write row failed");
                free(row_data);
                return false;
            }
        }
    } // ai
    
    free(row_data);

    if (!SPC_is_idle())
    {
        assert(0 && "failed writing flash data - not idle");
        return false;    
    }

    return true;     
}


bool Programmer::NV_protection_read(AppData *appdata)
{
    // Data stored in HexData uses a base address (based on Hex File) not address of internal PSoC mem addresses
    // Note: I don't think Config flash is covered by protection bits

    // PSoC5 Arch TRM 001-69820, Ch 12, Ch22
    // PSoC5 Dev Prog Specs 001-64359 pg 56

    fprintf(stderr, "PROTECTION READ NOT VERIFIED\n");

    assert(appdata != NULL);

    if (appdata->protection) delete appdata->protection;
    appdata->protection = new HexData();

    int row_len = m_devdata->flash_code_bytes_per_row; // FIXME is 256 but not sure if it's == code len (it is one code row)
    int num_protection_bytes_per_array = m_devdata->flash_rows_per_array / m_devdata->flash_rows_per_protection_byte;
    // There is one hidden "protection row" per array.
    // We read (row_len) 256 bytes however only first 64 (num_protection_bytes_per_array) are protection data.
    // Rest are padding We also don't cover config data IIUC.

    v_uint8_t data(row_len); // local buffer
    uint32_t address_offset = 0;

    int ai;
    for(ai = 0; ai < m_devdata->flash_num_arrays; ai++)
    {
        // Sec 43.3.1.1: The Row Select (row_id) parameter is used for Flash arrays that have a row size less than 256 bytes.
        // Because all Flash arrays have 256-byte rows, this parameter should always be 0x00."

        //data.resize(row_len); // is this inefficient or not !?
        //std::fill(data.begin(), data.end(), 0);
        data.assign(row_len, 0); // resets size too

        bool rc = SPC_cmd_read(data.data(), row_len, SPC_CMD_READ_HIDDEN_ROW, ai, 0 /* row_id */); // read 256 bytes always
        if (!rc)
        {
            fprintf(stderr,"Failed reading protection data\n");
            return false;
        }

        // resize down so we only add proection buts not padding (is there a better way?)
        data.resize(num_protection_bytes_per_array); // is this inefficient or not !?

        appdata->protection->add(HexFileFormat::PROTECTION_ADDRESS + address_offset, data);
        address_offset += num_protection_bytes_per_array;
    }

    if (debug) appdata->protection->dump(stderr);
    fprintf(stderr, "PROTECTION READ END\n");
    return true;
}


bool Programmer::NV_protection_write(const AppData *appdata)
{
    fprintf(stderr, "PROTECTION WRITE UNTESTED\n");
    // PSoC TRM 50235 Ch 44
    // Note: rowid can be 0x3F to programme all arrays at once with same values

    assert(appdata);

    int prot_len = 0;
    if (appdata->protection)
        prot_len = appdata->protection->length();

    if (prot_len == 0)
        return true; // not an error - nothing to do.

    int row_len = m_devdata->flash_code_bytes_per_row; // FIXME is 256 but not sure if it's == code len
    int num_protection_bytes_per_array = m_devdata->flash_rows_per_array / m_devdata->flash_rows_per_protection_byte;

    int narrays = prot_len / num_protection_bytes_per_array;
    int remainder = prot_len % num_protection_bytes_per_array;
    if (remainder) narrays++;

    fprintf(stderr,"prot_len:%d, narrays:%d, remainder:%d, row_len:%d\n", prot_len, narrays, remainder, row_len);

    assert(narrays <= m_devdata->flash_num_arrays);

    uint8_t *row_data = (uint8_t *)malloc(row_len);
    assert(row_data);

    bool rc = true;

    int ai; // array index
    for(ai = 0; ai < narrays; ai++)
    {
        memset(row_data, 0, row_len); // note row length is 256 however there will only be first (n_p_b_p_a) 64 bytes filled.
        // Remainder are ignored anyway.

        uint32_t hexdata_address = HexFileFormat::PROTECTION_ADDRESS + ai * num_protection_bytes_per_array;
        appdata->protection->extract2bin(hexdata_address, num_protection_bytes_per_array, row_data);

//        int pi = 0;
//        assert((*appdata->protection)[pi]->length() == num_protection_bytes_per_array);
//        memcpy(tmp_data, (*appdata->protection)[pi]->data.data(), num_protection_bytes_per_array);

        rc = NV_protect(ai, row_data, row_len);
        if (!rc) break;

//        pd += num_protection_bytes_per_array;
    }

    free(row_data);
    return rc;
}


bool Programmer::NV_eeprom_read(AppData *appdata, bool trim)
{
    fprintf(stderr, "EEPROM READ NOT VERIFIED\n");
    // Note this seems to use the generalise address access functions as opposed to the SPC commands
    // Should work for flash too. Pros and Cons !?

    assert(appdata != NULL);

    if (appdata->eeprom)
    {
        delete appdata->eeprom;
    }
    appdata->eeprom = new HexData();

    int nrows = m_devdata->eeprom_size / m_devdata->eeprom_bytes_per_row;
    int nreads = m_devdata->eeprom_bytes_per_row / 4; // 4 bytes per read

    v_uint8_t vdata(m_devdata->eeprom_bytes_per_row);

    int address_offset = 0;
    int ri;
    for(ri = 0; ri < nrows; ri++)
    {
        uint32_t dev_address = m_devdata->eeprom_base_address + address_offset;
        uint32_t hexdata_address = HexFileFormat::EEPROM_ADDRESS + address_offset;

        //std::fill(vdata.begin(), vdata.end(), 0);
        vdata.assign(m_devdata->eeprom_bytes_per_row, 0);

        uint8_t *dp = vdata.data();
        int count;
        for (count=0; count < nreads; count++)
        {
            ap_register_read(dev_address, dp);

            address_offset += 4;
            dp += 4;
        }

        appdata->eeprom->add(hexdata_address, vdata);
    }

    if (trim)
    {
        appdata->eeprom->trim();
    }

    if (debug) appdata->eeprom->dump(stderr);

    return true;
}


bool Programmer::NV_eeprom_write(const AppData *appdata)
{
    fprintf(stderr, "EEPROM WRITE NOT TESTED\n");

    assert(appdata);

    int eeprom_len = 0;
    if (appdata->eeprom)
        eeprom_len = appdata->eeprom->length();

    if (eeprom_len == 0)
        return true; // not an error - nothing to do.

    if (eeprom_len > m_devdata->eeprom_size)
    {
        fprintf(stderr, "eeprom_write: too much data. Have %d expected no more than %d bytes\n", eeprom_len, m_devdata->eeprom_size);
        return false;
    }

    // In current versions and for forseeable future (I think) there will be only one EEPROM array.
    int ai = 0;

    int row_len = m_devdata->eeprom_bytes_per_row; // 16

    int nrows = eeprom_len / row_len;
    if (eeprom_len % row_len)  nrows++;

    uint8_t *row_data = (uint8_t *)malloc(row_len);
    assert(row_data);

    int die_temp = get_die_temperature(); // first value post reset is wrong
    die_temp = get_die_temperature();

    bool rc = true;
    int ri;
    for(ri = 0; ri < nrows; ri++)
    {
        memset(row_data, 0, row_len);

        uint32_t hexdata_address = HexFileFormat::EEPROM_ADDRESS + ri * row_len;
        appdata->eeprom->extract2bin(hexdata_address, row_len, row_data);

        rc = NV_write_row(ai, ri, die_temp, row_data, row_len);
        if (!rc) break;
    }

    free(row_data);

    return rc;
}


bool Programmer::NV_WOL_read(AppData *appdata)
{
    return NV_read_b4(SPC_NV_AID_WOL, &appdata->security_WOL);
}


bool Programmer::NV_WOL_write(const AppData *appdata)
{
    // we only write if different to current values (to save on flash wear and tear)
    // WOL is only good for about 100 writes!

    uint32_t read_WOL;
    bool rc = NV_read_b4(SPC_NV_AID_WOL, &read_WOL);
    if (!rc) return false;

    if (read_WOL == appdata->security_WOL)
    {
        fprintf(stderr,"Skipping WOL write - same as existing\n");
        return true; // same
    }

    fprintf(stderr, "WRITING WOL - Don't do this too often!!\n");
    fprintf(stderr,"Writing WOL: 0x%08x\n", appdata->security_WOL);

    return NV_write_b4(SPC_NV_AID_WOL, appdata->security_WOL);
}


bool Programmer::NV_device_config_read(AppData *appdata)
{
    return NV_read_b4(SPC_NV_AID_CONFIG, &appdata->device_config);
}


bool Programmer::NV_device_config_write(const AppData *appdata)
{
    // we only write if different to current values (to save on flash wear and tear)
    // NVL is only good for about 100 writes!

    uint32_t read_device_config;
    bool rc = NV_read_b4(SPC_NV_AID_CONFIG, &read_device_config);
    if (!rc) return false;

    if (read_device_config == appdata->device_config)
    {
        fprintf(stderr,"Skipping device config write - same as existing\n");
        return true; // same
    }

    bool ecc_bit_error = (((read_device_config ^ appdata->device_config) >> 24) & 0xFF  == 0x08);

    bool pullup_enable =    (((appdata->device_config >> 16) & 0xFF) == 0x80)
                         && ((read_device_config & 0x0C) != 0x08);

    fprintf (stderr, "dev_config:0x%08x, file_dev_config:0x%08x\n", appdata->device_config, read_device_config);
    fprintf (stderr, "ecc_bit_err:%d, pullup_enable:%d\n", ecc_bit_error, pullup_enable);

    if (pullup_enable)
    {
        uint32_t reg_addr = 0x4000500A;
        uint32_t pinstate;
        rc = ap_register_read(reg_addr, &pinstate);
        if (!rc) return false;
        pinstate = (pinstate & 0x00F00000) | 0x00050000; // set pull-up drive mode
        ap_register_write(reg_addr, pinstate);
    }

    fprintf(stderr,"WRITING CONFIG - Don't do this too often!!\n");
    fprintf(stderr,"Writing Device Config: 0x%08x\n", appdata->device_config);

    rc = NV_write_b4(SPC_NV_AID_CONFIG, appdata->device_config);
    if (!rc) return false;

    if (ecc_bit_error)
    {
        // Need to do this so new ECC settings take effect
        if (!enter_programming_mode()) return false;
//        if (!configure_target_device()) return false;
    }

    return true;
}


bool Programmer::NV_flash_checksum(AppData *appdata)
{
    // FIXME: Maybe we should just return checksum rather than store it.

    // Operates on Flash. File version includes both code and config. (zero values don't alter checksum)
    // Note: As it checks code and config at same time we can't short circuit it if config has more data rows than code.
    // Only safe thing to do is check entire flash always.
    uint32_t checksum = 0;

    if (!NV_checksum_all(&checksum)) return false;

#if 0
    if (appdata->extra_flash_used_for_config())
        ....
    int code_len = appdata->code->length() + (devdata->geom->config_used_for_code ? appdata->config->length() : 0);
    fprintf(stderr,"dd cl:%d, dd configl:%d,  cl=%d\n", appdata->code->length(), appdata->config->length(), code_len);

    appdata->checksum = 0;

    // computes a checksum on just code_len
    fprintf(stderr, "NV READ CHECKSUM: code_len=%d\n", code_len);
#if 0
    if (len > geom.flash_size)
        len = geom.flash_size;
#endif
    int num_arrays, row_remainder;
    PSoC5::program_geom(m_devdata, code_len, &num_arrays, &row_remainder);

    uint32_t checksum = 0;

    if (debug) fprintf(stderr,"Number of arrays used: %d\n", num_arrays);

    int ai;
    for(ai = 0; ai < num_arrays; ai++)
    {
        //int rows_in_array = num_rows_in_array(appdata, ai);
        int nrows = ai < num_arrays - 1 ? geom.flash_rows_per_array : row_remainder;
        if (debug) fprintf(stderr,"Rows in array %d: %d\n", ai, nrows);
        const int start_row = 0; // always 0

        uint32_t array_checksum = 0;
        bool rc = NV_checksum_rows(ai, start_row, nrows, &array_checksum);

        checksum += array_checksum;
    }
#endif
    
    if (debug) fprintf(stderr,"PSoC code checksum: 0x%04x\n", checksum);

    appdata->checksum = checksum;

    return true;
}


// ===========
// low level NV methods
// ---------------------

// Lower level NV funcs Below here don't use appdata, maybe they shouldn't use geom either
// FIXME separate class !?


bool Programmer::NV_erase_flash(void)
{
    // erase all flash data, config, protection, row latches

    return SPC_cmd_idle(SPC_CMD_ERASE_ALL);
}


bool Programmer::NV_erase_sector(uint8_t array_id, uint8_t sector_id)
{
    // Operates on Flash and EEPROM

    // A sector is a block of 64 rows starting on a 64 row boundry
    // For flash arrays config bytes are also erased
    return SPC_cmd_idle(SPC_CMD_ERASE_SECTOR, array_id, sector_id);
}


bool Programmer::NV_checksum_all(uint32_t *checksum)
{
    // checksum entire flash
    *checksum = 0;

    uint8_t args[5];
    args[0] = 0x3F; // All Flash
    args[1] = 0;
    args[2] = 0;
    args[3] = 0;
    args[4] = 0;

    if (!SPC_cmd(SPC_CMD_GET_CHECKSUM, args, 5)) return false;

    uint8_t cksum[4];
    if (!SPC_read_data_b0(cksum, 4)) return false;
    bool rc = SPC_cmd(SPC_CMD_GET_CHECKSUM, args, 5);

    // Note: PSoC TRM Ch 44.3.1.1 Checksum Data is MSB first
    *checksum = B4BE_to_U32(cksum);

    return true;
}


bool Programmer::NV_checksum_rows(uint8_t array_id, uint16_t start_row, uint16_t nrows, uint32_t *checksum)
{
    // nrows: not sure if it is limited to one array or not, assume so !?
    *checksum = 0;

    uint8_t args[5];
    args[0] = array_id;
    args[1] = (start_row >> 8) & 0xFF;          // starting row number MSB (despite some docs)
    args[2] = (start_row) & 0xFF;               // starting row number LSB
    args[3] = ((nrows -1) >> 8) & 0xFF;         // nrows -1 (== 0)  MSB
    args[4] = (nrows -1) & 0xFF;                // nrows -1  LSB

    if (!SPC_cmd(SPC_CMD_GET_CHECKSUM, args, 5)) return false;

    uint8_t cksum[4];
    if (!SPC_read_data_b0(cksum, 4)) return false;

    // Note: PSoC TRM Ch 44.3.1.1 Checksum Data is MSB first
    *checksum = B4BE_to_U32(cksum);

    return true;
}


//bool Programmer::NV_read_multi_bytes(uint8_t array_id, uint32_t address, v_uint8_t &vdata) // nothing else here uses vectors. All or nothing.
bool Programmer::NV_read_multi_bytes(uint8_t array_id, uint32_t address, uint8_t *data, int len)
{
    // Flash (Main or config) or EEPROM, via SPC. Flash <= 256, EEPROM <= 16. Don't cross a row boundry
    // Starting address of the flash row first byte
    // reads vdata.size() bytes

    //  FIXME should I just use *data, len
    //int len = vdata.size();

    assert(len <= 256);

    fprintf(stderr, "NV_read_multi_bytes: aid:%d, addr:0x%0x, len:%d\n", array_id, address, len);

    // note: interface is only one byte wide so all data, args are serialised into bottom byte

#if 1
    if (!SPC_is_idle())
    {
        fprintf(stderr,"NV_read_multi_bytes: SPC not Idle!\n");
        return false; 
    }
#endif

    uint8_t cmd = SPC_CMD_READ_MULTI_BYTE;

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(REG_SPC_CPU_DATA);
    m_priv->request.apacc_data_write((uint32_t)SPC_KEY1);
    m_priv->request.apacc_data_write((uint32_t)(SPC_KEY2 + cmd));
    m_priv->request.apacc_data_write((uint32_t)cmd);
    m_priv->request.apacc_data_write((uint32_t)array_id);
    m_priv->request.apacc_data_write((uint32_t)((address >> 16) & 0xFF));
    m_priv->request.apacc_data_write((uint32_t)((address >> 8) & 0xFF));
    m_priv->request.apacc_data_write((uint32_t)(address & 0xFF));
    m_priv->request.apacc_data_write((uint32_t)(len - 1)); // to read n bytes pass n-1 (because max is 256)

    if (!send_receive())
    {
        fprintf(stderr, "NV_read_multi_bytes: send_receive failed\n");
        return false;
    }

    if (!m_priv->reply.pop_ok(-1)) return false; // FIXME: what value

    return SPC_read_data_b0(data, len);
}


bool Programmer::NV_write_row(uint8_t array_id, uint16_t row_num, int die_temp, const uint8_t *data, int len, bool erase_first)
{
    // works for Flash 255, 288, EEPROM 16
    //assert(vdata.size() <= 288);
    assert(len <= 288);
    fprintf(stderr, "NV_write_row: ai:%d, ri:%d, len:%d\n", array_id, row_num, len);

    int die_temp_mag = abs(die_temp);
    int die_temp_sign = die_temp < 0 ? 0 : 1; // assume 1 is positive !?  Ch 36

    bool rc = SPC_cmd_load_row(array_id, data, len);
    assert(rc); // TEMP
    if (!rc) return false;

#if 1
    if (!SPC_is_idle()) // FIXME one is needed in this func - not sure if this one is needed
    {
        assert(0 && "failed writing flash/eeprom data - not idle");
        return false;    
    }
#endif

    uint8_t args[5];
    args[0] = array_id;
    args[1] = (row_num >> 8) & 0xFF;
    args[2] = row_num & 0xFF;
    args[3] = die_temp_sign;
    args[4] = die_temp_mag;

    uint8_t cmd = erase_first ? SPC_CMD_WRITE_ROW : SPC_CMD_PROG_ROW;
    rc = SPC_cmd(cmd, args, 5);
    // FIXME: program row less documented. Not clear if prog row needs temp. Possible benefit is to save double erase...

    assert(rc); // TEMP
    if (!rc) return false;

    // possibly don't need to wait for programming to end before loading next row...

    if (!SPC_is_idle()) // FIXME one is needed in this func - not sure if this one is needed
    {
        assert(0 && "failed writing flash/eeprom data - not idle 2");
        return false;    
    }
    return true;
}


#if 0
bool Programmer::NV_flash_write_row(uint8_t array_num, uint8_t row_num, int die_temp, const uint8_t *data, int row_len)
{
    // THIS DOESN'T SEEM TO WORK. NOT SURE WHY. Maybe ARM isn't running !? Should work faster than SPC version
    // FIXME: Uses CMD 7 however that doesn't appear to take die temp. Even if not may be harmless !?
    assert(row_len <= 288);

    int die_temp_mag = abs(die_temp);
    int die_temp_sign = die_temp < 0 ? 0 : 1; // assume 1 is positive !? Ch 36
    bool even = row_num % 2 == 0;


    // Note: Len is typically <= CODE_BYTES_PER_ROW or ECC_BYBTES_PER_ROW
    // Starting address of the flash row first byte
    //fprintf(stderr, "write_flash_row: array:%d, row:%d, addr: 0x%0x, row_len:%d, die_temp:%d\n", array_num, row_num, address, row_len, die_temp);
    fprintf(stderr, "write_flash_row: array:%d, row:%d, row_len:%d, die_temp:%d (%d:%d), even:%d\n", array_num, row_num, row_len, die_temp, die_temp_sign, die_temp_mag, even);

// FIXME: TEMP
dump_data(stderr, data, row_len, NULL);
//return true;

    // Store data and "program row (0x07) command in memory
    // Store the following in memory:
    //  @SRAM base (for odd row):
    //  CMD 2: LOAD ROW                                   CMD 7: PROGRAM
    //  b6 d5 02 AID, DD[0] .... DD[rowlen-1] :: 00 00 00 b6, (d3 + 07) 07 AID RIDh=0, RIDl temp_sign temp_mag 00,
    // Note the 00 00 00 are noted as "3 NOPs for a short delay" (valid commands start with a b6)
    // Total length of fata = rowlen + 15

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(0x20000000 + even ? 0x0 : 0x200);
    m_priv->request.apacc_data_write(0x0002d5b6 | ((array_num & 0xFF)<<24));
    if (!send_receive()) { fprintf(stderr,"flash write row early failure\n"); exit(1); }
    m_priv->reply.pop_ok(2);

    int eo_offset = even ? 0x0 : 0x200;

    int i;
    //for (i=0; i< PSoC5::CODE_BYTES_PER_FLASH_ROW; i+=4) 
    for (i=0; i< row_len; i+=4) 
    {
        m_priv->request.reset();
        m_priv->request.apacc_addr_write(0x20000000 | i + 0x04 + eo_offset); // xx xx HI LO   // FIXME: ODD + 0x204  L 1106
        m_priv->request.apacc_data_write(B4LE_to_U32(data + i));
        if (!send_receive()) { fprintf(stderr,"flash write row early failure 2\n"); exit(1); }
        m_priv->reply.pop_ok(2);
    }
        fprintf(stderr,"\n");

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(0x20000000 | ((row_len - 1 + 0x05 + eo_offset) & 0xffff)); // BIGENDIAN_2  // ODD + 0x205  L1131
    m_priv->request.apacc_data_write(0xb6000000);                               // 00 00 00 b6

    m_priv->request.apacc_addr_write(0x20000000 | ((row_len - 1 + 0x09 + eo_offset) & 0xffff)); // BIGENDIAN_2  // ODD + 0x209
    m_priv->request.apacc_data_write(0x000007da | ((array_num & 0xff) << 16));  // da 7d 00 00

    m_priv->request.apacc_addr_write(0x20000000 | ((row_len - 1 + 0x0d + eo_offset) & 0xffff)); // BIGENDIAN_2  // ODD + 0x209
    m_priv->request.apacc_data_write(0x00000000 | (die_temp_mag << 16) | (die_temp_sign << 8) | (row_num & 0xFF)); // 0x00 is row_count high byte

    m_priv->request.apacc_addr_write(0x40007018 + even?0:0x10); // 7028 ODD     PHUB_CH0_BASIC_STATUS, PHUB_CH1_BASIC_STATUS
    m_priv->request.apacc_data_write(0x00000000 + even?0:0x100); // 0100 ODD    Address of the current CHn_ORIG_TD0/1 in the chain, Use TDMEM1_PROG+TD0

    m_priv->request.apacc_addr_write(0x40007010 + even?0:0x10); // 7020         ODD PHUB_CH0_BASIC_CFG, PHUB_CH1_BASIC_CFG
    m_priv->request.apacc_data_write(0x00000021); // Enable DMA Ch 0

    m_priv->request.apacc_addr_write(0x40007600 + even?0:0x08); // 7608 ODD     PHUB_CFGMEM0_CFG0, PHUB_CFGMEM1_CFG0
    m_priv->request.apacc_data_write(0x00000080); // DMA rquest needed for each burst

    m_priv->request.apacc_addr_write(0x40007604 + even?0:0x08); // 760C ODD     PHUB_CFGMEM0_CFG1, PHUB_CFGMEM1_CFG1
    m_priv->request.apacc_data_write(0x40002000); // upper 16 bits of source/dest

    m_priv->request.apacc_addr_write(0x40007800 + even?0:0x08); // 7808 ODD     PHUB_TDMEM0_ORIG_TD0, PHUB_TDMEM1_ORIG_TD0
    m_priv->request.apacc_data_write(0x01ff0000 + 15 + row_len); // request length

    m_priv->request.apacc_addr_write(0x40007804 + even?0:0x08); // 780C ODD     PHUB_TDMEM0_ORIG_TD1, PHUB_TDMEM1_ORIG_TD1
    m_priv->request.apacc_data_write(0x47200000 + eo_offset);  // 47200200 ODD  lower 16 bits of source/dest

    send_receive();
    m_priv->reply.m_debug = 1; // TEMP
    m_priv->reply.pop_ok(-1);
    m_priv->reply.m_debug = 0; // TEMP

    if (!SPC_is_idle())
    {
        assert(0 && "failed writing flash data - not idle");
        return false;    
    }

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(0x40007014 + even?0:0x10); // 7024 ODD     PHUB_CH0_ACTION
    m_priv->request.apacc_data_write(0x00000001); // Create DMA request for CH 0

    send_receive();
    m_priv->reply.pop_ok(-1);

    return true;    
}
#endif


bool Programmer::NV_protect(uint8_t array_id, const uint8_t *data, int len)
{
    if (!SPC_cmd_load_row(array_id, data, len)) return false;

    return SPC_cmd(SPC_CMD_PROTECT, array_id, 0 /* row_id */);
    // FIXME; SPC_cmd_idle() !?
}


bool Programmer::NV_read_b4(uint8_t array_id, uint32_t *value)
{
    // Note for NVL we have to ues read byte not read multi-bytes
    // This convenience function reads 4 bytes (used by all the NVLs).
    // Byte index 0 from the SPC_cmd_read is the LSB.
    // Valid array_ids : 0x80 (User NVL), 0xF8 (WOL)

    // Note: According to PSoC arch TRM. Ch 10.3 we could call READ_BYTE once then 0x10 3 more times

    int size = 4;
    uint8_t data[4];

    memset(data, 0, size*sizeof(data[0]));

    int i;
    for (i=0; i<size; i++)
    {
        if (!SPC_cmd_read(data + i, 1, SPC_CMD_READ_BYTE, array_id, i))
            return false;
    }

    // data is sent and received LSB first
    // in converting to uint32 this means that byte[0] -> LSB of equivalent int

    *value = b4_LE_to_uint32(data);
//    if (debug) fprintf(stderr,"NV READ: 0x%08x\n", *value);

    return true;
}


bool Programmer::NV_write_b4(uint8_t array_id, uint32_t value)
{
    // For NVL use
    // Valid array_ids : 0x80 (User NVL), 0xF8 (WOL)

    int size=4;
    uint8_t data[4];

    // data is sent and received LSB first
    uint32_to_b4_LE(value, data);

    int i;
    for (i=0; i<size ;i++)
    {
        if (!SPC_cmd_idle(SPC_CMD_LOAD_BYTE, array_id, i, data[i])) return false;
        fprintf (stderr," 0x%02x", data[i]);
    }
    fprintf (stderr,"\n");

#if 0
    if (!SPC_cmd_idle(SPC_CMD_WRITE_NVL, array_id)) return false;
#else
    fprintf(stderr, "NV_write_b4 is disabled. CODE NOT TESTED and NVL writing can be fatal.");
    return false;
#endif

    return true;
}


// =============
// Misc functions

int Programmer::get_die_temperature(void)
{
    // Measures the internal chip temperature. -1 for fail
    // For PSoC 5, this always returns 25 C, as the sensor is not accurate.
    // note first time called value is wrong. Need to call twice.

    //fprintf(stderr,"* get_die_temperature\n");

    uint8_t tmp_data[2];
    bool rc = SPC_cmd_read(tmp_data, 2, SPC_CMD_GET_TEMPERATURE, 3 /* num_samples 1-5 */ , 0);
    if (!rc) return FAILURE;

    int sign = tmp_data[0];
    int mag = tmp_data[1];

    int temp = sign ? mag : -mag; // FIXME: seems to return 78 for PSoC 5 rather than fixed 25 ?

    //fprintf(stderr, "Die Temp  Sign: %d, Mag:%d\n", sign, mag);

    return temp;
}


uint32_t Programmer::get_jtag_id(void)
{
    uint32_t idcode = 0;

    m_priv->request.reset();
//    m_priv->request.dpacc_idcode_read();
    m_priv->request.c1(DPACC_IDOCDE_READ);

    if (!send_receive()) return idcode;

    uint8_t idcode_bytes[4];
    if (m_priv->reply.pop_b4_ok(idcode_bytes))
    {
        idcode = B4LE_to_U32(idcode_bytes);
    }

    return idcode;
}


bool Programmer::ap_register_read(uint32_t address, uint32_t *value, bool dummy_preread)
{
    // I think dummy_preread is always true !?
    uint8_t tmp_data[4];
    bool rc = ap_register_read(address, tmp_data, dummy_preread);
    *value = B4LE_to_U32(tmp_data);
    return rc;
}


bool Programmer::ap_register_read(uint32_t address, uint8_t *data, bool dummy_preread)
{
    // I think dummy_preread is always true !?
    // Assumes uint8_t data[4]

    int nreads = 1;
    if (dummy_preread) nreads = 2;

    memset(data, 0, 4*sizeof(uint8_t));

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(address);
    m_priv->request.apacc_data_read(nreads);

    if (!send_receive()) return false;

    if (!m_priv->reply.pop_ok()) return false;
    if (dummy_preread)
        m_priv->reply.pop_b4_ok(NULL);

    return m_priv->reply.pop_b4_ok(data);
}


bool Programmer::ap_register_write(uint32_t address, uint32_t value)
{
    m_priv->request.reset();
    m_priv->request.apacc_addr_write(address);
    m_priv->request.apacc_data_write(value);

    if (!send_receive()) return false;

    m_priv->reply.pop_ok(2);
}


bool Programmer::send_c1d4_recv_ok(uint8_t cmd, uint32_t data)
{
    // note uses m_priv req and receive
    m_priv->request.reset();
    m_priv->request.c1d4(cmd, data);

    if (!send_receive()) return false;
    if (!m_priv->reply.pop_ok()) return false;

    return true;
}


// ---------
// Lower level functions

bool Programmer::send_receive(void)
{
    bool ok = true;

    ok = m_priv->request.send(m_priv->dev_handle);
    if (!ok) return false;
    ok = m_priv->reply.receive(m_priv->dev_handle);

    return ok;
}


#if 0
void * Programmer::get_device_handle(void)
{
    // TEMPORARY
    return m_priv ? m_priv->dev_handle : NULL;
}
#endif


void Programmer::usb_print_info(void)
{
    if (m_priv->dev_handle)
        print_device_info2(m_priv->dev_handle);
}


void Programmer::usb_clear_stall(void)
{
    bool rc;
    rc = m_priv->request.clear_stall(m_priv->dev_handle);
//    fprintf(stderr,"REQ clear stall -> %d (0 = fail)\n", rc);
    rc = m_priv->reply.clear_stall(m_priv->dev_handle);
//    fprintf(stderr,"REPLY clear stall -> %d (0 = fail)\n", rc);
}


#if 0
void Programmer::dostuff(void)
{
}
#endif



// SPC methods
// -----------

bool Programmer::SPC_is_idle(bool wait)
{
    // Polls SPC Status register till it is idle or a timeout occurs

    return SPC_wait_for_status(SPC_STATUS_IDLE, wait);
}


bool Programmer::SPC_is_data_ready(bool wait)
{
    // Polls SPC Status register till data is ready or a timeout occurs
    
    return SPC_wait_for_status(SPC_STATUS_DATA_READY, wait);
}


uint8_t Programmer::SPC_status(void)
{
    // This function is called during non volatile memory Read operations like
    // Flash read, NV read to check if SPC Data register has data to be read.
    // The PSoC SPC status register value is present in the third least significant

    // NOTE: First time this function is called data must be discarded but subsequent values are valid
    uint8_t data[4];

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(REG_SPC_STATUS /* 0x40004722 */);
    m_priv->request.apacc_data_read(1);
    m_priv->request.send(m_priv->dev_handle);
    
//    send_receive();

    m_priv->reply.receive(m_priv->dev_handle);
    m_priv->reply.pop_ok();        // reply to addr_write
//    m_priv->reply.pop_b4_ok(NULL); // first read - discard
    m_priv->reply.pop_b4_ok(data); // second read

    // FIXME: note pop_data must reverse data bytes, so LSB is in data[0]

    return data[2]; // PSoC 5 is 3rd LSB, PSoC 3 is LSB refer AN73051 Appendix C
}


bool Programmer::SPC_wait_for_status(uint8_t status, bool wait)
{
    // if not wait then just do twice, else until ready or timeout

    int i;
    uint8_t data = 0;
    int loop_max = wait ? SPC_POLL_TIMEOUT : 1;

    if (m_debug & DEBUG_SPC) fprintf(stderr, "SPC: waiting for status %d\n", status);
    SPC_status(); // discard first value

    for (i=0; i < loop_max; i++)
    {
        data = SPC_status();
        if (data == status)
        {
            if (m_debug & DEBUG_SPC) fprintf(stderr, "SPC: got status %d\n", status);
            return true;
        }
    }

    fprintf(stderr, "SPC: TIMEOUT waiting for status %d\n", status);
    return false;
}


bool Programmer::SPC_cmd_idle(uint8_t cmd, int arg1, int arg2, int arg3)
{
    // convenience method
    bool rc = SPC_cmd(cmd, arg1, arg2, arg3);
    if (!rc) return false;
    return SPC_is_idle();    
}


bool Programmer::SPC_cmd_read(uint8_t *data, int len, uint8_t cmd, int arg1, int arg2, int arg3)
{
    // convenience method
    bool rc = SPC_cmd(cmd, arg1, arg2, arg3);
    if (!rc) return false;

    rc = SPC_read_data_b0(data, len);
    return rc;
}


bool Programmer::SPC_cmd_load_row(uint8_t array_id, const uint8_t *data, int len)
{
    uint8_t cmd = SPC_CMD_LOAD_ROW;

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(REG_SPC_CPU_DATA);
    m_priv->request.apacc_data_write((uint32_t)SPC_KEY1);
    m_priv->request.apacc_data_write((uint32_t)(SPC_KEY2 + cmd));
    m_priv->request.apacc_data_write((uint32_t)cmd);
    m_priv->request.apacc_data_write((uint32_t)array_id);

    int i;
    for (i=0; i<len; i++)
    {
        m_priv->request.apacc_data_write((uint32_t)data[i]);
    }

    if (!send_receive())
    {
        fprintf(stderr, "SPC_cmd_load_row: send_receive failed\n");
        return false;
    }

    //fprintf(stderr, "SPC_cmd_load_row\n");
    return m_priv->reply.pop_ok(-1); // FIXME: what value
}


bool Programmer::SPC_cmd(uint8_t cmd, int arg1, int arg2, int arg3)
{
    // convenience function

    uint8_t args[3];
    int nargs = 0;

    args[0] = arg1;
    args[1] = arg2;
    args[2] = arg3;

    if (arg3 >= 0) nargs = 3;
    else if (arg2 >= 0) nargs = 2;
    else if (arg1 >= 0) nargs = 1;

    return SPC_cmd(cmd, args, nargs);
}


bool Programmer::SPC_cmd(uint8_t cmd, uint8_t *args, int nargs)
{
    // FIXME: could use varargs

    if (m_debug & DEBUG_SPC)
    {
        fprintf(stderr,"SPC_cmd(cmd: 0x%02x,", cmd);
        dump_data(stderr, args, nargs, NULL);
    }
#if 1
    if (!SPC_is_idle()) return false;    
#endif

    // note: interface is only one byte wide so all data, args are serialised into bottom byte
    m_priv->request.reset();
    m_priv->request.apacc_addr_write(REG_SPC_CPU_DATA);
    m_priv->request.apacc_data_write((uint32_t)SPC_KEY1);
    m_priv->request.apacc_data_write((uint32_t)(SPC_KEY2 + cmd));
    m_priv->request.apacc_data_write((uint32_t)cmd);

    int i;
    for(i=0; i<nargs; i++)
    {
        m_priv->request.apacc_data_write((uint32_t)args[i]);
    }

    if (!send_receive())
    {
        fprintf(stderr, "SPC_cmd: send_receive failed\n");
        return false;
    }

    return m_priv->reply.pop_ok(-1); // FIXME: what value
}


bool Programmer::SPC_read_data_b0(uint8_t *data, int len)
{
    // read n bytes of data from previously submitted read command
    if (m_debug & DEBUG_SPC) fprintf(stderr,"SPC_read_data_b0(len=%d)\n", len);

    if (!SPC_is_data_ready())
    {
        fprintf(stderr, "SPC_read_data_b0: Data not ready\n");
        return false;    
    }

    m_priv->request.reset();
    m_priv->request.apacc_addr_write(REG_SPC_CPU_DATA);
    m_priv->request.apacc_data_read(len + 1); // one dummy read, plus real reads

    if (!send_receive())
    {
        fprintf(stderr, "SPC_read_data_b0: send_receive failed\n");
        return false;
    }

    // Data reply is  21 (for addr_write), ddxxxxxx 21  ddxxxxxx 21 ...
    m_priv->reply.pop_ok(); // cmd
    m_priv->reply.pop_nb0_ok(NULL, 1); // discard dummy
    m_priv->reply.pop_nb0_ok(data, len);

    if (m_debug & DEBUG_SPC) fprintf(stderr,"SPC_read_data_b0() - END\n", len);

    return SPC_is_idle();
}


// ========================

Request::Request(uint8_t ep_addr)
    : m_len(0)
    , m_ep_addr(ep_addr)
    , m_debug(0)
{
    memset(m_data, 0, REQUEST_MAX_LEN); // Much be big enough
}


void Request::reset(void)
{
    // memset(m_data, 0, REPLY_MAX_LEN); // Much be big enough
    m_len = 0;
}


void Request::raw_hex(const char *hexstr)
{
    // FIXME: should this append or replace !? Fr now it appends
    int len = strlen(hexstr);

    assert(REQUEST_MAX_LEN - m_len >= len);

    int binlen = hex_to_bin(hexstr, m_data + m_len, len);
    m_len += binlen;
    //dump_data(stderr, m_data, m_len, NULL);
}


void Request::c1(uint8_t cmd)
{
    m_data[m_len++] = cmd;
}


void Request::c1d4(uint8_t cmd, uint32_t data)
{
    m_data[m_len++] = cmd;
    // LSB first
    m_data[m_len++] = (data & 0xff);
    m_data[m_len++] = ((data>>8) & 0xff);
    m_data[m_len++] = ((data>>16) & 0xff);
    m_data[m_len++] = ((data>>24) & 0xff);
}


void Request::apacc_data_read(int n_reads=1)
{
// NO make dummy explicity for now    ---- // Note: we do n+1 reads. First read is a dummy (hardware reasons)
    int i;
    //for (i=0; i<=n_reads; i++)
    for (i=0; i<n_reads; i++)
        m_data[m_len++] = APACC_DATA_READ;
}


bool Request::send(libusb_device_handle *dev_handle)
{
    int rc = send_bulk_data(dev_handle, m_ep_addr, m_data, m_len);
    if (m_debug)
    {
        dump_data(stderr, m_data, m_len, "send bulk");
        //fprintf(stderr,"send_bulk_data (%d bytes) -> %d\n", m_len, rc);
        fprintf(stderr,"send bulk rc -> %d\n", rc);
    }
    return rc == 0;
}


bool Request::clear_stall(libusb_device_handle *dev_handle)
{
    int rc = clear_endpoint_stall(dev_handle, m_ep_addr);
    return (rc == 0);
}

// ========================


//#define DEBUG_REPLY  0
//#if DEBUG_REPLY
//#define DEBUG_REPLY_MSG(a)  (a)
//#else
//#define DEBUG_REPLY_MSG(a) 
//#endif

Reply::Reply(uint8_t ep_addr)
    : m_data(NULL)
    , m_len(0)
    , m_ep_addr(ep_addr)
    , m_head(0)
    , m_debug(0)
{
    m_data = (uint8_t *) calloc(REPLY_MAX_LEN, 1);
}


Reply::~Reply()
{
    if (m_data) free(m_data);
    m_data = NULL;
    m_len = 0;
    m_ep_addr = 0;
    m_head = 0;
}


bool Reply::receive(libusb_device_handle *dev_handle)
{
    memset(m_data, 0, REPLY_MAX_LEN); // paranoia
    m_head = 0;

    int max_length = REPLY_MAX_LEN;
    int actual_len = recv_bulk_data(dev_handle, m_ep_addr, m_data, max_length);
    if (actual_len < 0)
    {
        fprintf(stderr,"Reply::receive failed: %d\n", actual_len);
        return false;
    }
    m_len = actual_len;

    if (m_debug) { dump_data(stderr, m_data, actual_len, "Reply:receive"); }
    return true;
}


bool Reply::clear_stall(libusb_device_handle *dev_handle)
{
    int rc = clear_endpoint_stall(dev_handle, m_ep_addr);
    return (rc == 0);
}


bool Reply::pop_ok(int n)
{
    // pop n items off stack. If all of them were REPLY_OK, return true

    int data = -1;
    bool ok = true;

    if (n<0)
        n = m_len - m_head;  // pop all remaining data

    assert( n >= 0 );

    //DEBUG_REPLY_MSG(fprintf(stderr, "pop_ok(%d) ", n));
    if (m_debug) fprintf(stderr, "pop_ok(%d) ", n);

    while (n--)
    {
        if (m_head > m_len )
           assert(0 && "unexpected pop_ok");

        data = m_data[m_head++];
        //DEBUG_REPLY_MSG(fprintf(stderr, "P[%x] ", data));
        if (m_debug) fprintf(stderr, "P[%x] ", data);

        if (data != REPLY_OK && data != REPLY_JTAGID_MATCHED)
            ok = false;
    }

    //DEBUG_REPLY_MSG(fprintf(stderr,"%s\n", ok ? "OK" : "FAILED"));
    if (!ok || m_debug)
    {
        fprintf(stderr, "\npop_ok %s ", ok ? "OK" : "Failed");
        fprintf(stderr, "Expected 0x%x found 0x%x.\nData (%d): ", REPLY_OK , data, m_len);
        dump_data(stderr, m_data, m_len, NULL);
    }

    return ok;
}


bool Reply::pop_b4_ok(uint8_t *data)
{
    // expect  STATUS(1) DATA(4)
    // have data even if status not ok !?

    bool ok = true;

    if (m_head > m_len - 4 )
       assert(0 && "unexpected pop_b4_ok insufficient data left");

    if (data)
    {
        // reverse direction of bytes !? (LSB first in input data)
        data[0] = m_data[m_head + 0];
        data[1] = m_data[m_head + 1];
        data[2] = m_data[m_head + 2];
        data[3] = m_data[m_head + 3];
    }

    m_head += 4;

    if (!pop_ok())
        ok = false;

    return ok;
}


bool Reply::pop_nb0_ok(uint8_t *data, int length)
{
// FIXME: consistency of naming with pop_b4_ok
    // pops length 32 bit values (and status) off reply.
    // Saves byte 0 of each 32 bit value in data array

    // FIXME: pop data regardless of status !?

    uint8_t *dp = data;
    uint8_t data_item[4];
    bool ok = true;

    assert (length >= 0);

    while (length--)
    {
        if (!pop_b4_ok(data_item))
            ok = false;

        if (data)
            *dp++ = data_item[0];
    }

    return ok;
}
