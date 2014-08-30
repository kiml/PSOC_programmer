#include <stdio.h>

#include "AppData.h"


void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s filename.hex\n", progname);
    
    exit(1);
}


int main(int argc, char **argv)
{
    int rc = 0;
    const char *progname = argv[0];

    if (argc <= 1)  usage(progname);

    const char *filename = argv[1];

    AppData appdata;
    appdata.read_hex_file(filename);

    appdata.dump(1,NULL);
}

