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

# -----


# Overview
#
# This utility is used to create configuration data (address and data) values
# to initialise microcontroller configuration registers.
# It creates a list of various data structures. Each structure is designed
# to optimise one common type of data initialisation requirement (eg constant
# fill, data copy etc). The data may be stored anywayre in FLASH but on PSOC
# is typically stored in the so called FLASH "configuration" area. A C function
# is supplied to walk the list, decode and action each entry.


# Memory Data Structure
#
# The top level entity is a list of data structures. The first byte of a
# structure contains the structure type, the list ends with an element
# type of CFG_TT_UNDEF. There are 4 types of initialisation structure:
# Type: 1: constant fill, 2: offset,value, 3: data fill, 4: mem to mem copy
#
# list structure:
#   [ HEADER ELEMENT ]* 0000
#   ->
#   [ ttcc aaaa aaaa ELEMENT ]* 0000
#       tt: type
#       cc: count_low
#       a...a: 32 bit address
#       0000 is the list terminating value of ttcc
#
#  ELEMENT tt == 1:  constant fill
#    ccvv
#       cc: count_hi
#       vv: value
#
#  ELEMENT tt == 2:  offset value
#    [ oovv ]+
#       oo: offset
#       vv: value
#       "oovv" structure repeats cc (count_low) times
#
#  ELEMENT tt == 3:  data array
#    [ vv ]+
#       vv: value
#       "vv" structure repeats cc (count_low) times
#
#  ELEMENT tt == 4:  address copy
#    ssss ssss
#       s...s: 32 bit (source) address
#
# Note:
#  * For compactness addresses (eg 32 bit addresses) are NOT aligned on usual
#       memory boundaries.


# Source File Format
#
# Grammer:
#   #include "filename.h"
#   #define NAME VALUE
#   #label NAME [@ADDRESS]
#
#   addr : value [* count]              # type 1
#   addr : value                        # type 2
#   addr : hex bytes.... [ @ width ]    # type 3  NOTE: width not currently supported
#   addr : [src_addr] [* count]         # type 4
#
# Notes:
#  * addr, value, count may be expressed in standard C (well Python)
#       format, decimal, hex (0x...) etc.
#
#  * normal C include files should work to pull in #define lines
#
#  * only one label (before "addr:" lines) is currently permitted.
#
#  * there are two output modes - strictly in source order and optimised
#       (within a label group). The optimisation groups multiple
#       single value type 2 items into a CFG_TT_OV structure where possible.
#
#  * A single value could be stored via types 1, 2 or 3. The canoncial format is
#       type 2XXX to allow grouping of other single values.


import collections
import re
import sys


Debug = 0


CFG_TT_UNDEF = 0
CFG_TT_CF = 1
CFG_TT_OV = 2
CFG_TT_DA = 3
CFG_TT_COPY = 4
CFG_TT_LABEL = -1  # special type (put in configItem to maintain sequencing only)


ConfigItem = collections.namedtuple('ConfigItem', 'type addr value count')
re_define = re.compile(r"#define\s+(?P<name>\w+)\s+(?P<value>\w+)")


def read_symtab(symtab, filename):
    linenum = 0
    with open(filename) as f:
        for line in f.readlines():
            linenum += 1

            m = re_define.match(line)
            if m is not None:
                # print('DEF:', m.group('name'), m.group('value'))
                symtab[m.group('name')] = m.group('value')

    if Debug:
        for k,v in symtab.items():
            print('d:', k, v)


def get_uint(symtab, valuestr):
    ''' get integer value of valuestr performing any symtab substitutions '''

    # repeatedly convert #define to their values
    while valuestr in symtab:
        print("Converting %s to %s" % (valuestr, symtab[valuestr]))
        valuestr = symtab[valuestr]

    # FIXME: HACkish. The #defines being used have numbers ending in 'u' like 0x12345u
    if valuestr[-1:] == 'u':
        valuestr = valuestr[:-1]

    # should be an integer by now (in some base)
    value=int(valuestr, 0)
    return value


def parse(configfile):
    config = []
    symtab = {}

#    re_blank = re.compile("^$")
    re_expr = re.compile(r"(?P<addr>\w+):(?P<val>\w+|\[\w+\])(?P<hascount>\*)?(?P<count>.*)")
    re_brackets = re.compile(r"\[(?P<src_addr>\w+)\]")
#    re_include = re.compile(r"#include\s+\"(?P<filename>\w+)\"|<(?<filename>\w+)>")
    re_include = re.compile(r"#include\s+\"(?P<filename>[^\"]+)\"")
    re_label = re.compile(r"#label\s+(?P<label>\w+)\s*@?\s*(?P<addr>.*)")

    linenum = 0
    with open(configfile) as f:
        for line in f.readlines():
            linenum += 1
            origline = line

            # process #include files (containing #define lines)
            m = re_include.match(line)
            if m is not None:
                filename = m.group('filename')
                read_symtab(symtab, filename)
                continue

            # process #define lines
            m = re_define.match(line)
            if m is not None:
                symtab[m.group('name')] = m.group('value')
                continue

            # process #label lines
            m = re_label.match(line)
            if m is not None:
                iaddr = get_uint(symtab, m.group('addr'))
                config.append(ConfigItem(CFG_TT_LABEL, iaddr, m.group('label'), 0)) # BIT OF A HACK
                continue

            # clean up line
            line = re.sub(r"#.*", '', line)  # remove comments
            line = re.sub(r"\s+", '', line)  # in this case we can remove ALL blanks

            # remove (resulting) blank lines
            #if re_blank.match(line): continue
            if line == '': continue

            m = re_expr.match(line)
            if m is None:
                print("Syntax error on line %d: [%s]" % (linenum, origline))
                return None
            
            if Debug:
                print("L:",line)
                for k,v in m.groupdict().items():
                    print(k,": ",v)
                print()

            # Line contains a data

            # flags for line
            value_is_addr = False
            hascount = False
            type = CFG_TT_UNDEF
            icount = 1

            iaddr = get_uint(symtab, m.group('addr'))

            valuestr = m.group('val')
            b = re_brackets.match(valuestr)
            if b is not None:
                value_is_addr = True
#                print(b.groupdict.keys())
                valuestr = b.group('src_addr')
            # How does python handle really long hex runs !?
            ivalue = get_uint(symtab, valuestr)

            if ivalue < 0:
                print("Negative values not allowed: line %d: [%s]" % (linenum, origline))
                return None


            if m.group('hascount') is not None:
                hascount = True
                icount=get_uint(symtab, m.group('count'))
                if icount < 0:
                    print("Negative count not allowed: line %d: [%s]" % (linenum, origline))
                    return None

            ci = None
            if value_is_addr:
                type = CFG_TT_COPY  # memcpy
                ci = ConfigItem(type, iaddr, ivalue, icount)
            elif ivalue < 256:
                if icount == 1:
                    type = CFG_TT_DA  # single value (data array len 1 is most compact, so is the canonical form)
                else: # count > 1
                    type = CFG_TT_CF  # constant fill (of single byte value)
                    assert(icount < 65536)
                assert(ivalue >= 0 and ivalue < 256)
                ci = ConfigItem(type, iaddr, ivalue, icount)
            else:
                type = CFG_TT_DA  # data array
                if valuestr[0:2] != "0x":
                    print("data array case must be prefixed by 0x as a reminder data must be hex format: line %d: [%s]" % (linenum, origline))
                    sys.exit(1)
                valuestr = valuestr[2:] # remove 0x, it has served its purpose

                icount = len(valuestr)
                assert(icount % 2 == 0)
                icount >>= 1
                ci = ConfigItem(type, iaddr, valuestr, icount)

            # Note: offset value form (CFG_TT_OV) never occurs 'naturally'

            if Debug:
                print("T:%d, C:%d, A:%x, V:%d" % (type, icount, iaddr, ivalue))

            assert(icount > 0)
#            if type in [CFG_TT_CF, CFG_TT_OV]:
#                assert(ivalue >= 0 and ivalue < 256)
            if type != CFG_TT_CF:
                assert(icount < 256)

            config.append(ci)

    return config
            

def output_header(item, fout):
    addr_bytes = item.addr.to_bytes(4, byteorder=addr_byteorder)
    iaddr = int.from_bytes(addr_bytes, byteorder='big')
    print('%02x %02x %08x ' % (item.type, item.count & 0xFF, iaddr), file=fout, end='')


def output_trailer(fout):
    # end marker
    print('%02x %02x' % (0, 0), file=fout)


def output_element(item, addr_byteorder, fout):
    if item.type == CFG_TT_CF:
        print('%02x %02x' % (((item.count >> 8) & 0xFF), item.value), file=fout)
    elif item.type == CFG_TT_OV:
        offset = 0
        print('%02x %02x' % (offset, item.value), file=fout)
    elif item.type == CFG_TT_DA:
        # HACK
        print('%s' % (item.value), file=fout)
    elif item.type == CFG_TT_COPY:
        isaddr = int.from_bytes(item.value.to_bytes(4, byteorder=addr_byteorder), byteorder='big')
        print('%08x' % (isaddr), file=fout)
    else:
       print("Unexpected item type %d" % (item.type))
       sys.exit(1)


def output_optimised_da_ov(asublist, addrdict, fout, addr_byteorder):
    ''' if len(asublist) == 1 outputs canoncial DA format for single item. Else outputs OV structure '''
    assert(len(asublist) > 0)
    if len(asublist) == 1:
        # space optimiaston use DA format
        a = asublist[0]
        iaddr = int.from_bytes(a.to_bytes(4, byteorder=addr_byteorder), byteorder='big')
        print('%02x %02x %08x %02x' % (CFG_TT_DA, 1, iaddr, addrdict[a]), file=fout, end='\n')
    else:
        print('%02x %02x %08x ' % (CFG_TT_OV, len(asublist), asublist[0]), file=fout, end='\n')
        for a in asublist:
            print('  %02x %02x' % (a-asublist[0], addrdict[a]), file=fout)


def output_preserve_order(config, outfile, addr_byteorder):
    # Note: for now at least can only have label at beginning
    # FIXME: should we check for just one label !?

    current_type = CFG_TT_UNDEF
    for item in config:
        if item.type == CFG_TT_LABEL:
            print('# @%s %08x' % (item.value, item.addr), file=fout)
            continue

        if item.type != current_type:
            output_header(item, fout)

        output_element(item, addr_byteorder, fout)


def output_optimised_order(config, outfile, addr_byteorder):
    addrdict = {}

    # First loop: output any label and combine any DAs into a subset for possible conversion to OVs
    for item in config:
        if item.type == CFG_TT_LABEL:
            print('@%s 0x%08x' % (item.value, item.addr), file=fout)
            continue

        if item.type != CFG_TT_DA or item.count != 1: continue
        # OV is made up of multiple single value DA's
        # FIXME:  remove item from config !?
        if item.addr in addrdict:
            print("Cannot set same address twice in this optimisation case. Addr:0x%x values: %d and %d" % (item.addr, addrdict[item.addr], item.value))
            sys.exit(1)
        addrdict[item.addr] = item.value

    # Output any OVs or unconverted DAs
    if len(addrdict) > 0:
        addrlist = sorted(addrdict.keys())
        asublist = None
        for addr in addrlist: 
            if asublist is None:
                asublist = [ addr ]
            elif addr - asublist[0] < 256 and len(asublist) < 256:
                asublist.append(addr)
            else:
                assert(len(asublist) < 256)
                output_optimised_da_ov(asublist, addrdict, fout, addr_byteorder)
                asublist = [ addr ]

        # process any leftovers
        if asublist is not None:
            output_optimised_da_ov(asublist, addrdict, fout, addr_byteorder)

    # Output other data types (not worth trying to optimise these)
    current_type = CFG_TT_UNDEF
    for item in config:
        if item.type == CFG_TT_OV: continue # should never occur as OV is not a 'natural' type
        if item.type == CFG_TT_DA and item.count == 1: continue # FIXME: should we just remove these ?
        if item.type == CFG_TT_LABEL: continue  # already handled

        if item.type != current_type:
            output_header(item, fout)

        output_element(item, addr_byteorder, fout)



def output_hex(config, outfile, preserve_order, addr_byteorder):
    # Note: outputs "human readable" hex version
    # FIXME: check if file already exists
    fout = open(outfile, 'w')
    if fout is None:
        print("Failed to create file %s" % (outfile))
        sys.exit(1)

    if preserve_order:
        output_preserve_order(config, outfile, addr_byteorder)

    else: # optimise for space
        output_optimised_order(config, outfile, addr_byteorder)

    output_trailer(fout)
    fout.close()


# --- main ---

if len(sys.argv) < 2:
    print("Usage: %s INFILE OUTFILE" % sys.argv[0])
    sys.exit(1)

configfile = sys.argv[1]
outfile = sys.argv[2]

config = parse(configfile)

if Debug:
    for item in config:
        print(item)
    
preserve_order = False
addr_byteorder = 'little'  # 32 bit addresses are output LSB first for ARM
output_hex(config, outfile, preserve_order, addr_byteorder)
