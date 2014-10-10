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
#include <string>

#include "usb.h"
#include "utils.h"
#include "Programmer.h"
#include "AppData.h"
#include "DeviceData.h"
#include "version.h"


// FIXME: put into a siteconfig.h file (and other defaults ?)
#define DEFAULT_CONFIG_DIR      "config"
#define DEFAULT_CONFIG_FILE     "config.ini"
#define DEFAULT_DEVICE_FILE     "devices.dat"


struct config_s
{
    std::string config_dir;
    std::string config_filename;
    std::string device_filename;

    std::string device_name;

    DeviceData *devdata;
    Programmer *programmer;

    // from config file:
    // ...
    config_s() : devdata(0), programmer(0) {};
};

typedef int (*cmd_func)(struct config_s *config, int argc, char **argv);

int cmd_program(struct config_s *config, int argc, char **argv);
int cmd_verify(struct config_s *config, int argc, char **argv);
int cmd_enter_programming(struct config_s *config, int argc, char **argv);
int cmd_reset(struct config_s *config, int argc, char **argv);
int cmd_jtag_id(struct config_s *config, int argc, char **argv);
int cmd_usb_clear(struct config_s *config, int argc, char **argv);
int cmd_upload(struct config_s *config, int argc, char **argv);
int cmd_help(struct config_s *config, int argc, char **argv);
int cmd_erase(struct config_s *config, int argc, char **argv);

//int parse_commands(struct config_s *config, int argc, char **argv);


struct cmds_s {
    const char *cmd;
    const char *args_desc;
    const char *cmd_desc;
    int n_mand_args;
    cmd_func  func;
    bool do_connect; // convenience - do programmer open/close for command
    bool enter_prog_mode;
    bool device_specific;
} Cmds[] = {
    {"program", "filename", "program device", 1, cmd_program, false, true, true},
    {"upload", "filename", "read device and save in file", 1, cmd_upload, true, true, true},
    {"verify", "filename", "verify device", 1, cmd_verify, false, true, true},
    {"reset", "", "reset device", 0, cmd_reset, true, true, false},
    {"erase", "", "erase device", 0, cmd_erase, true, true, false},
    {"id", "", "get jtag id", 0, cmd_jtag_id, true, true, false /* hmmm */ },
    {"usb_clear", "", "clear USB error on device", 0, cmd_usb_clear, true, false, false},
    {"help", "", "display help", 0, cmd_help, false, false, false}
};

int NCmds = sizeof(Cmds)/sizeof(struct cmds_s);
char *Progname = NULL;



//void usage(const char *progname)
void usage(void)
{
    fprintf(stderr, "PSoC Programmer, v%s\n"
        "Copyright (C) 2014 Kim Lester, http://www.dfusion.com.au/\n\n"
        "Usage: %s CMD\n  where CMD is:\n", VERSION_STR, Progname);
    int i;
    for (i=0; i<NCmds; i++)
    {
        struct cmds_s *cmd = Cmds + i;
        fprintf(stderr, "    %-10s %-10s - %s\n", cmd->cmd, cmd->args_desc, cmd->cmd_desc);
    }
    
    exit(1);
}


bool read_device_config(struct config_s *config)
{
    if (config->device_name.length() == 0)
    {
        fprintf(stderr,"Device Name not specified\n");
        return false;
    }

    config->devdata = new DeviceData;

    std::string device_filepath = config->config_dir + std::string("/") + config->device_filename;

    fprintf(stderr, "devdata: filepath:%s, device_name:%s\n",
        device_filepath.c_str(), config->device_name.c_str());

    config->devdata->read_file(device_filepath, config->device_name);
    //config->devdata->dump();

    if (!config->devdata->validate())
    {
        fprintf(stderr, "Device %s (File: %s) failed basic validity checks\n",
            config->device_name.c_str(), device_filepath.c_str());
        config->devdata->dump();
        return false;
    }

    return true;
}


Programmer *programmer_open(const struct config_s *config)
{
    Programmer *programmer = new Programmer;

//    std::string config_filepath = config->config_dir + std::string("/") + config->config_filename; // FIXME: pathcat

//    fprintf(stderr,"programmer open: config_path:%s\n", config_filepath.c_str());
    if (programmer->open(config->config_dir, config->config_filename, config->devdata) != SUCCESS)
    {
        fprintf(stderr, "Failed to open device\n");
        return NULL;
    }

    programmer->usb_print_info();

    return programmer;
}


void parse_args(int *argc, char ***argv, struct config_s *config)
{
    config->config_dir = DEFAULT_CONFIG_DIR;
    config->config_filename = DEFAULT_CONFIG_FILE;
    config->device_filename = DEFAULT_DEVICE_FILE;

    int ch;
    while ((ch = getopt(*argc, *argv, "hC:d:")) != -1)
    {
        switch (ch)
        {
            case 'C':
                config->config_dir = optarg;
                break;

            case 'd':
                config->device_name = optarg;
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    *argc -= optind;
    *argv += optind;
}


int parse_commands(struct config_s *config, int argc, char **argv)
{
    const char *cmd_str = argv[0];
    argc--;

    int i;
    for (i=0; i<NCmds; i++)
    {
        struct cmds_s *cmd = Cmds + i;
        if (strcasecmp(cmd_str, cmd->cmd) != 0)
            continue;

        if (argc < cmd->n_mand_args)
            break;

        // found command

        if (cmd->device_specific && !read_device_config(config))
        {
            fprintf(stderr, "Failed to read device config\n");
            return -1;
        }

        if (cmd->do_connect)
        {
            // FIMXE: messy. maybe create a class.
            config->programmer = programmer_open(config);

            if (config->programmer == NULL) return -1;

            if (cmd->enter_prog_mode)
                config->programmer->enter_programming_mode(); // every time !?

            int rc = cmd->func(config, argc, ++argv);

            config->programmer->close();
            return rc;
        }
        else
        {
            return cmd->func(config, argc, ++argv);
        }
    }

    usage();
    return -1; // never reaches here
}


// CMDS

int cmd_help(struct config_s *config, int nargs, char **argv)
{
    usage();
    return -1; // never reached
}


int cmd_program(struct config_s *config, int nargs, char **argv)
{
    // read file (check it exists  and is ok before connecting to programmer)

    const char *filename = argv[0];
    fprintf(stderr, "program %s\n", filename);

    AppData appdata;
    bool rc = appdata.read_hex_file(filename);

    // FIXME: really should verify file signature here - make it a function to read and verify

    if (!rc)
    {
        fprintf(stderr,"failed to read file [%s]\n", filename);
        return -1;
    }

    // open programmer

    if (!config->programmer)
        config->programmer = programmer_open(config);

    if (config->programmer == NULL) return -1;

    config->programmer->enter_programming_mode(); // Seem to need to be in programming mode to get idcode

    uint32_t idcode = config->programmer->get_jtag_id();
    fprintf(stderr,"Silicon ID: 0x%04x\n", idcode);
    // TODO: check it matches file version

    if (appdata.device_id != idcode)
    {
        fprintf(stderr,"Error. device ids mismatch (file: 0x%08x, device: 0x%08x).\n", appdata.device_id, idcode);

        if (idcode == 0)
            fprintf(stderr,"Failed to obtain device id\n");
        else
            fprintf(stderr, "Program was compiled for a different type of device.");

        config->programmer->close();
        return -1;
    }

    if (!config->programmer->write_device(&appdata))
    {
        fprintf(stderr, "* WRITE FAILED!\n");
        config->programmer->close();
        return -1;
    }

    config->programmer->close();
    return 0;
}



int cmd_upload(struct config_s *config, int nargs, char **argv)
{
    const char *filename = argv[0];

    AppData appdata;

    config->programmer->enter_programming_mode();
//    uint32_t idcode = programmer->get_jtag_id();
//    fprintf(stderr,"Silicon ID: 0x%04x\n", idcode);
    int flags = RD_TRIM_ALL; // FIXME from  config

    bool rc = config->programmer->read_device(&appdata, flags);

    appdata.write_hex_file(filename);

    return rc;
}


int cmd_verify(struct config_s *config, int nargs, char **argv)
{
    assert(config && config->programmer);

    assert(0 && "TO DO");

    return 0;
}


int cmd_enter_programming(struct config_s *config, int nargs, char **argv)
{
    assert(config && config->programmer);

    config->programmer->enter_programming_mode();
    return 0;
}


int cmd_reset(struct config_s *config, int nargs, char **argv)
{
    fprintf(stderr,"config=%p  cp=%p\n", config, config->programmer);
    assert(config && config->programmer);

    config->programmer->reset_cpu();
    return 0;
}


int cmd_erase(struct config_s *config, int nargs, char **argv)
{
    assert(config && config->programmer);

    config->programmer->erase_flash();
    return 0;
}


int cmd_jtag_id(struct config_s *config, int nargs, char **argv)
{
    assert(config && config->programmer);

    uint32_t idcode = config->programmer->get_jtag_id();
    fprintf(stderr,"Silicon ID: 04%04x\n", idcode);
    return 0;
}


int cmd_usb_clear(struct config_s *config, int nargs, char **argv)
{
    assert(config && config->programmer);

    config->programmer->usb_clear_stall();
    return 0;
}


// =====

int main(int argc, char **argv)
{
    int rc = 0;
//    const char *progname = argv[0];
    Progname = argv[0];

    struct config_s config;

    parse_args(&argc, &argv, &config);

    if (argc < 1)  usage();

    if (!usb_init())
    {
        fprintf(stderr, "Failed to initialise usb interface");
        return -1;
    }

    //libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
    //list_matching_devices(VID_CYPRESS, PID_ANY);

    //memset(&config, 0, sizeof(config));

    // FIXME default and cmdline override (also should it be strdup?):

//    std::string config_filepath = config.config_dir + std::string("/") + config.config_filename; // FIXME: pathcat
//    INIReader reader(config_filepath); // FIXME: I read file multiple times. Should read it once (but later when structure sorted out)

    // parse main commandline arguments
    // string default_device_name = reader.Get(...); // FIXME

    // FIXME: if config_file or device_file start with / don't prepend config_dir, Also need pathcat...
    //if (!read_device_config(&config)) { fprintf(stderr, "Failed to read device config\n"); exit(1); }

    // process commands
    rc =  parse_commands(&config, argc, argv);

    usb_finalise();

    return rc;
}
