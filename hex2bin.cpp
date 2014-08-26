#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <vector>

#include "HexData.h"
#include "HexFileFormat.h"
#include "utils.h"


void usage(void)
{
    fprintf(stderr, "hex2bin infile.hex outfile.bin\n");
    exit(1);
}


int main(int argc, char **argv)
{
    int debug = 0;

    if (argc < 3) usage();

    const char *infile = argv[1];
    const char *outfile = argv[2];

    HexData hexdata(infile);
    if (debug) hexdata.dump(stdout);

    //code = canon->extract(HEX_FILE_FLASH_CODE_ADDRESS, HEX_FILE_FLASH_CODE_SIZE);
    //config = canon->extract(HEX_FILE_CONFIG_ADDRESS, HEX_FILE_CONFIG_SIZE);
    //int code_len = hexdata.length(HEX_FILE_FLASH_CODE_ADDRESS, HEX_FILE_FLASH_CODE_SIZE, NULL);
    uint32_t min_address, max_address;
    bool rc = hexdata.minmax_address(HexFileFormat::FLASH_CODE_ADDRESS, HexFileFormat::FLASH_CODE_SIZE, &min_address, &max_address);
    fprintf(stderr, "Code:  ok:%d, min:0x%0x, max:0x%0x, len=%d\n", rc, min_address, max_address, max_address-min_address);
    uint8_t *code = NULL;
    if (rc)
    {
        code = hexdata.extract2bin(min_address, max_address, NULL);
        FILE * fp = fopen(outfile,"w");
        fwrite(code, max_address-min_address, 1, fp);
        fclose(fp);
    }

    if (code) free(code);

//    int config_len = hexdata.length(HEX_FILE_FLASH_CONFIG_ADDRESS, HEX_FILE_FLASH_CONFIG_SIZE, NULL);
    rc = hexdata.minmax_address(HexFileFormat::CONFIG_ADDRESS, HexFileFormat::CONFIG_SIZE, &min_address, &max_address);
    fprintf(stderr, "Config:  ok:%d, min:0x%0x, max:0x%0x, len=%d\n", rc, min_address,max_address, max_address-min_address);
#if 0
    uint8_t *config = NULL;
    if (rc)
    {
        config = hexdata.extract2bin(min_address, max_address, NULL);
        FILE * fp = fopen(outfile,"w");
        fwrite(code, max_address-min_address, 1, fp);
        fclose(fp);
    }
    if (config) free(config);
#endif
    
}
