#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include "config_data.h"


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr,"Usage: %s FILENAME\n", argv[0]);
        exit(1);
    }

    const char *filename = argv[1];

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fprintf(stderr,"Failed to open file: %s - %s\n", filename, strerror(errno));
        exit(1);
    }

    struct stat st;

    fstat(fileno(fp), &st);
    
    uint8_t *buf = (uint8_t *)calloc(st.st_size, 1);

    fread(buf, st.st_size, 1, fp);

    cfg_set_regions(buf);
}
