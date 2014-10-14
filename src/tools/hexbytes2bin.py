#!/usr/bin/env python3

#  Copyright (C) 2014 Kim Lester
#  http://www.dfusion.com.au/
#
#  This Program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This Program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this Program.  If not, see <http://www.gnu.org/licenses/>.


import sys


def h2b(infile, outfile):

    fout = open(outfile, 'wb')
    # FIXME: check if file already exists
    if fout is None:
        print("Failed to create file %s" % (outfile))
        sys.exit(1)

    with open(infile) as fin:
        for line in fin.readlines():
            line = line.strip()
            if line[0] == '#': continue  # FIXME: quick hack
            #print(line)
            ba = bytearray.fromhex(line)  # handles spaces between pairs of hex chars
            fout.write(ba)

    fout.close()


# --- main ---

if len(sys.argv) < 2:
    print("Usage: %s INFILE OUTFILE" % sys.argv[0])
    sys.exit(1)

infile = sys.argv[1]
outfile = sys.argv[2]

h2b(infile, outfile)
