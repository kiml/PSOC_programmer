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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <vector>

#include "AppData.h"
#include "HexFileFormat.h"

const char *Infile_optstring = "cdemnp";


void usage(void)
{
    fprintf(stderr, "mergehex (-[%s] infile.hex)+ [-o outfile.hex]\n", Infile_optstring);
    fprintf(stderr, "mergehex (-[%s] infile.hex)+ > outfile.hex\n", Infile_optstring);
    fprintf(stderr, "Eg:  mergehex -c infile.hex -nm config.hex > outfile.hex\n");
    // NOTE: cdemnp cannot be combined with other options like -o in a single argument
    exit(1);
}


struct infile_config_s
{
/*
    bool code;
    bool data; // config
    bool protection;
    bool registers;
    bool metadata;
    bool eeprom;
*/
    uint32_t flags;

    const char *filename;
};

#define MAX_INFILES 5

// In-File Flags
#define IFF_CODE        1
#define IFF_DATA        2
#define IFF_PROTECTION  4
#define IFF_NVR         8
#define IFF_METADATA   16
// EEPROM not supported in formal definition:
#define IFF_EEPROM     32

int main(int argc, char **argv)
{
    int debug = 0;

    struct infile_config_s config[MAX_INFILES];
    memset(config, 0, sizeof(config));
    char *outfile = NULL;

    int filenum = 0;
    int ch;
//    opterr = 0; // disable error messages (as we're doing tricky things)

    // we could probably do this in one getopt by checking that Infile_optstring arguments were not mixed with others in the same argument.
    // cdemnp do take an argument but each cluster shares the same argument. So process specially.

    while (optind < argc)
    {
        bool infile_cluster = false;

        while ((ch = getopt(argc, argv, "cdemnpo:")) != -1)
        // returns -1 when next option does not start with - (or no more). So ends when infile filename encountered
        {
            switch (ch)
            {
                case 'c':
                    config[filenum].flags |= IFF_CODE;
                    infile_cluster = true;
                    break;

                case 'd':
                    config[filenum].flags |= IFF_DATA;
                    infile_cluster = true;
                    break;

                case 'e':
                    config[filenum].flags |= IFF_EEPROM;
                    infile_cluster = true;
                    break;

                case 'p':
                    config[filenum].flags |= IFF_PROTECTION;
                    infile_cluster = true;
                    break;

                case 'n':
                    config[filenum].flags |= IFF_NVR;
                    infile_cluster = true;
                    break;

                case 'm':
                    config[filenum].flags |= IFF_METADATA;
                    infile_cluster = true;
                    break;

                case 'o':
                    if (infile_cluster)
                    {
                        fprintf(stderr, "Command Line Error. Cannot combine %s arguments with other arguments.\n", Infile_optstring);
                        usage();
                    }

                    outfile = optarg;
                    break;

                case '?':
                default:
                    usage();
             }
        } // while

        if (infile_cluster)
        {
            config[filenum].filename = argv[optind]; // next argument
            filenum++;
            optind++;
        }
    } // optind < argc

    argc -= optind;
    argv += optind;

    AppData outdata;
    uint32_t iff_done = 0;
    uint32_t min_address, max_address;

    int i;
    for(i=0; i<filenum; i++)
    {
        struct infile_config_s *cfg = &config[i];
        //fprintf(stderr, "FLAGS: 0x%x  FILE:%s\n", cfg->flags, cfg->filename);

        AppData tmp;
        uint32_t default_base_address = 0;


        // stanard tools create snippet files without address headers.
        if (cfg->flags & IFF_DATA)
            default_base_address = HexFileFormat::CONFIG_ADDRESS; // snippet file may omit correct address
        else if (cfg->flags & IFF_PROTECTION)
            default_base_address = HexFileFormat::PROTECTION_ADDRESS; // snippet file may omit correct address


        if (!tmp.read_hex_file(cfg->filename, default_base_address))
        {
            fprintf(stderr, "Failed to read file: %s\n", cfg->filename);
            exit(1);
        }

        if (cfg->flags & IFF_CODE)
        {
            if (iff_done & IFF_CODE) { fprintf(stderr, "Bad Options: code included multiple times.\n"); exit(1); }
            iff_done |= IFF_CODE;
            outdata.code = tmp.code;
        }

        if (cfg->flags & IFF_DATA)
        {
            if (iff_done & IFF_DATA) { fprintf(stderr, "Bad Options: data included multiple times.\n"); exit(1); }
            iff_done |= IFF_DATA;
            outdata.config = tmp.config;
#if 0
            if (tmp.config->length() == 0 && tmp.code->length() != 0)
            {
                // This happens when a "standard" config.hex file is used without the pre
                fprintf(stderr,"Error: Config data is NOT located at expected address of 0x%08x. Address record may be missing.\n", HexFileFormat::CONFIG_ADDRESS);
                exit(1);
            }
#endif
        }

        if (cfg->flags & IFF_EEPROM)
        {
            if (iff_done & IFF_EEPROM) { fprintf(stderr, "Bad Options: EEPROM included multiple times.\n"); exit(1); }
            iff_done |= IFF_EEPROM;
            outdata.eeprom = tmp.eeprom;
        }

        if (cfg->flags & IFF_PROTECTION)
        {
            if (iff_done & IFF_PROTECTION) { fprintf(stderr, "Bad Options: protection included multiple times.\n"); exit(1); }
            iff_done |= IFF_PROTECTION;
            outdata.protection = tmp.protection;
        }
        if (cfg->flags & IFF_NVR)
        {
            if (iff_done & IFF_NVR) { fprintf(stderr, "Bad Options: NVR included multiple times.\n"); exit(1); }
            iff_done |= IFF_NVR;

            outdata.device_config = tmp.device_config;
            outdata.security_WOL = tmp.security_WOL;
        }
        if (cfg->flags & IFF_METADATA)
        {
            if (iff_done & IFF_METADATA) { fprintf(stderr, "Bad Options: Metadata included multiple times.\n"); exit(1); }
            iff_done |= IFF_METADATA;

            // FIXME: ideally I could treat this as opaque data
            outdata.hex_file_version = tmp.hex_file_version;
            outdata.device_id = tmp.device_id;
            outdata.silicon_revision = tmp.silicon_revision;
            outdata.debug_enable = tmp.debug_enable;
            outdata.reserved = tmp.reserved;
        }

    }

    outdata.checksum = outdata.calc_checksum();

    outdata.write_hex_file(outfile);

    return 0;
}
