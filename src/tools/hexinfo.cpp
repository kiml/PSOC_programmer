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

