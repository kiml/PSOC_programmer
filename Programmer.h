#ifndef _PROGRAMMER_H
#define _PROGRAMMER_H

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <string>

#include "AppData.h"
#include "DeviceData.h"
#include "types.h"

//#define SUCCESS   true
//#define FAILURE   false
#define SUCCESS   0
#define FAILURE   -1

#define DEBUG_REQUEST   0x01
#define DEBUG_REPLY     0x02
#define DEBUG_SPC       0x04


#define RD_TRIM_FLASH   0x01
#define RD_TRIM_EEPROM  0x02
#define RD_TRIM_ALL     (RD_TRIM_FLASH | RD_TRIM_EEPROM)


// verify device return code
#define VERIFY_MATCH                    0x00
#define VERIFY_MISMATCH_CODE            0x01
#define VERIFY_MISMATCH_CONFIG          0x02
#define VERIFY_MISMATCH_PROTECTION      0x04
#define VERIFY_MISMATCH_EEPROM          0x08
#define VERIFY_MISMATCH_WOL             0x10
#define VERIFY_MISMATCH_DEVCONFIG       0x20
#define VERIFY_MISMATCH_JTAGID          0x40

#define VERIFY_MISSING_FILE_DATA         0x1000
#define VERIFY_DEVICE_READ_FAILED        0x2000



struct programmer_priv_s;


class Programmer
{
    // state
    struct programmer_priv_s  *m_priv;
    bool m_programmer_configured;
    DeviceData *m_devdata;

    // config
    std::string m_fx2_config_file;
    uint32_t m_debug;
    uint16_t m_vid_unconfigured;
    uint16_t m_pid_unconfigured;
    uint16_t m_vid_configured;
    uint16_t m_pid_configured;

   // internal
    bool SPC_is_idle(bool wait=true);
    bool SPC_is_data_ready(bool wait=true);
    uint8_t SPC_status(void);
    bool SPC_wait_for_status(uint8_t status, bool wait);
    bool SPC_cmd(uint8_t cmd, int arg1 = -1, int arg2 = -1, int arg3 = -1);
    bool SPC_cmd(uint8_t cmd, uint8_t *args, int nargs);
    bool SPC_cmd_idle(uint8_t cmd, int arg1 = -1, int arg2 = -1, int arg3 = -1);
    bool SPC_cmd_read(uint8_t *data, int len, uint8_t cmd, int arg1 = -1, int arg2 = -1, int arg3 = -1);
    bool SPC_cmd_addr24(uint8_t cmd, uint8_t aid, uint32_t addr, int len);
    bool SPC_read_data_b0(uint8_t *data, int len);
    bool SPC_cmd_write(const uint8_t *data, int len, uint8_t cmd, int arg1 = -1, int arg2 = -1, int arg3 = -1);
    bool SPC_read_data_b4(uint32_t address, uint32_t *data, int len);
    bool SPC_cmd_load_row(uint8_t array_id, const uint8_t *data, int len);

    bool NV_WOL_read(AppData *appdata);
    bool NV_WOL_write(const AppData *appdata);
    bool _NV_WOL_write(const uint8_t *data, int len);

    bool NV_device_config_read(AppData *appdata);
    bool NV_device_config_write(const AppData *appdata);
    bool _NV_device_config_write(const uint8_t *data, int len);

    bool NV_flash_read(AppData *appdata, bool trim);
    bool NV_flash_read_row(v_uint8_t &vdata, uint8_t array_num, uint32_t address);
    bool NV_flash_write(const AppData *appdata);
    bool NV_flash_write_row(const uint8_t *data, int len, uint8_t array_num, uint32_t address, int even);
    int NV_flash_row_length(const AppData *appdata) const;

    bool NV_protection_read(AppData *appdata);
    bool NV_protection_write(const AppData *appdata);

    bool NV_eeprom_read(AppData *appdata, bool trim);
    bool NV_eeprom_write(const AppData *appdata);

    bool NV_erase_flash(void);
    bool NV_erase_sector(uint8_t array_id, uint8_t sector);

    bool NV_flash_checksum(AppData *appdata);
    bool NV_checksum_all(uint32_t *checksum);
    bool NV_checksum_rows(uint8_t array_id, uint16_t start_row, uint16_t nrows, uint32_t *checksum);

    bool NV_read_multi_bytes(uint8_t array_id, uint32_t address, uint8_t *data, int len);
    bool NV_write_row(uint8_t array_id, uint16_t row_num, int die_temp, const uint8_t *data, int len, bool erase_first=true);
    bool NV_protect(uint8_t array_id, const uint8_t *data, int len);
    bool NV_read_b4(uint8_t array_id, uint32_t *value);
    bool NV_write_b4(uint8_t array_id, uint32_t value);


    bool configure_usb_programmer(void);
    bool switch_to_swd(void);
    bool send_receive(void);
    bool send_c1d4_recv_ok(uint8_t cmd, uint32_t data);
    bool jtag_to_swd(void);
    bool acquire_target_device(void);
    bool release_target_device(void);

    bool ap_register_read(uint32_t address, uint32_t *value, bool dummy_preread=true);
    bool ap_register_read(uint32_t address, uint8_t *data, bool dummy_preread=true);
    bool ap_register_write(uint32_t address, uint32_t value);

    void set_debug(uint32_t flags) { m_debug = flags; }
//    bool verify_checksum(uint16_t reference_checksum);

    void program_geom(int code_len, int *num_arrays, int *row_remainder);
    bool read_config(std::string config_dir, std::string config_filename);

  public:
    Programmer();
    ~Programmer();

    int open(std::string config_dir, std::string config_filename, DeviceData *devdata);
    void close();

    bool enter_programming_mode(void);
    void exit_programming_mode(void);

    bool configure_target_device(void); // TEMP public !/

    bool NV_read_checksum(int code_len, uint32_t *checksum);

    bool read_device(AppData *appdata, uint32_t flags);
    bool write_device(const AppData *appdata);
    bool write_hexfile(const char *filename, const AppData *appdata);
    uint32_t verify_device(const AppData *appdata, uint32_t flags);
    void dump_flash_data(const AppData *appdata, bool shortform, const char *filename=NULL);
    bool erase_flash(void);
    void reset_cpu(void);
    void usb_clear_stall(void);

    std::string verify_status_string(uint32_t verify_status);

  public:
    // misc
    int get_die_temperature(void);
    uint32_t get_jtag_id(void);

    void usb_print_info(void);
    void dostuff(void);
};

#endif
