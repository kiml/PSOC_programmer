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
import argparse
import binascii

# Input format "free format hex":
# - ASCII file
# - Ignores whitespace between pairs of hex digits
# - Ignores rest of line after a # (comment character)

# TODO as needed:
# intelhex could have a base address arg, or read it from an input #define

Output_formats = ("hex", "intelhex", "binary")


def parse_command_line():
    parser = argparse.ArgumentParser(description='Convert ASCII hex bytes to other formats')
    parser.add_argument('-f', required=True, nargs=1, dest='output_format', help='output format: %s' % (",".join(Output_formats)))
    parser.add_argument('-i', nargs=1, dest="infile", help="input file. Default stdin")
    parser.add_argument('-o', nargs=1, dest="outfile", help="output file. Default stdout")

    return (parser, parser.parse_args())


def read_file(infile):
    # infile is an array or null need infile[0]
    ba = bytearray()
    fin = None

    if infile is None:
        fin = sys.stdin
    else:
        fin = open(infile[0], 'r')

    linenum = 1
    for line in fin.readlines():
        line = line.split('#', 1)[0] # throw away any comment
        line = line.strip()
        # Note: fromhex() ignores whitespace between pairs of hex chars
        try:
            ba.extend(bytearray.fromhex(line))
        except (ValueError):
            print("Failed to parse input line %d :" % (linenum) ,line, file=sys.stderr)
            sys.exit(1)
        linenum += 1

    if fin != sys.stdin:
        fin.close()
    return ba


def output(outfile, ba):
    # FIXME: check if file already exists
    # Python 3 cares about 'b' even under Unix
    # Note: cannot output binary to stdout

    outfunc = None
    outmode = ''

    if format == "hex":
        outfunc = b2h
    elif format == "intelhex":
        outfunc = b2ih
    elif format == "binary":
        outfunc = b2b
        outmode = 'b'

    fout = None

    if outfile is None:
        fout = sys.stdout
        if outmode == 'b':
            print("Cannot output binary to stdout", file=sys.stderr)
            # FIXME: there are ways to do this...
            sys.exit(1)
    else:
        fout = open(outfile[0], 'w'+outmode)

    if fout is None:
        print("Failed to create file %s" % (outfile), file=sys.stderr)
        sys.exit(1)

    outfunc(ba, fout)

    if fout != sys.stdout:
        fout.close()


def b2h(ba, fout):
    s = binascii.b2a_hex(ba) # == hexlify()
    fout.write(s.decode().upper())


def b2b(ba, fout):
    fout.write(ba)


def b2ih(ba, fout):
    ''' bytes to intel hex '''

    max_linelen = 32
    base_addr = 0 # FIXME: set this !?
    # FIXME: does not handle address+offset > 2^16

    offset = 0
    while 1:
        b = ba[offset:offset+max_linelen]
        actual_linelen = len(b)
        if actual_linelen == 0:
            break

        ihexleader = "%02X%04X00" % (actual_linelen, base_addr+offset)
        bindata = binascii.a2b_hex(ihexleader) + b
        cksum = ((sum(bindata) ^ 0xFF) + 1) & 0xFF # two's complement of LSB
        asciihex = binascii.b2a_hex(bindata).decode('ascii').upper()
        print(":%s%02X" % (asciihex,cksum), file=fout)
        offset += actual_linelen

    print(":00000001FF", file=fout);

# --- main ---

(parser, args) = parse_command_line()

format = args.output_format[0]

if format not in Output_formats:
    print("Output format must be one of: %s" % (",".join(Output_formats)), file=sys.stderr)
    sys.exit(1)

ba = read_file(args.infile)
output(args.outfile, ba)
