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

# Source File Format
# ------------------
#
# Grammer:
#   #include "filename.h"
#   #define NAME VALUE
#   #label NAME ADDRESS
#
#   addr : value * count                # Source type 1
#   addr : value                        # Source type 2
#   addr : hex bytes.... [ @ width ]    # Source type 3  NOTE: dest width not currently supported
#   addr : [src_addr] [* count]         # Source type 4  Default count 1

#   Future: ??
#     addr : [off:value]+               # Source type 5
#
# Labels:
#  * Labels are an additional construct. They are output unchanged in the
#    freehex output format. Their purpose is to add metadata to the output file (eg data load address)
#
#  * FIXME: True or not: only one label is currently permitted and it must occur before any "addr:" lines.
#  * FIXME: Should any coalescence be prevented from crossing label locations so that labels can be used to
#    create distinct config data sets (eg with diff load addresses) in a single file.
#
#  
# Notes:
#  * addr, value, count may be expressed in standard C (well Python)
#       format, decimal, hex (0x...) etc.
#
#  * normal C include files can be used to pull in #define lines.
#    FIXME: This tool does not currently support #define macros or defines with expressions on RHS.
#
#  * A count == 0 in the source file is not valid.
#
#  * Source Types and Output types more or less match trivially.
#     Source Type   ->  Output Table Type (TT)
#      1                 1   CF     Constant Fill
#      2               ** 3 or 2. It depends. See below **
#      3                 3   DA     Data Array (single value is most compact)
#      4                 4   AC   Address Copy (memcpy)
#
#  * A single byte value could be INPUT using Source Types 1, 2 or 3
#  * For a single byte Source Type 3 syntactically reduces to Source Type 2.
#  * For a single byte all three source types (1,2,3) reduce to the same
#    internal format. Therefore single bytes will always be output in a
#    consistent format regardless of source construct.
#
#  * A single byte may be OUTPUT using Output Types 1 (CF), 2 (OV), 3(DA)
#    The most compact representation for a single byte happens to be DA.
#    Thus DA is the canonical format for outputting a single byte.
#
#    However a group of single byte values in a config file will often be
#    within a small address range. Therefore a run of single byte values
#    within an address range of 256 bytes can be efficiently coded using
#    output type 2 offset,value (OV).
#    Thus Output Type 2 (OV) is automatically generated as needed and
#    cannot be explicitly specified in the Source File unless Source Type 5
#    is implemented (to support entering small address offsets).
#
# Limits:
#  Constant Fill: count <= 65536 
#  Data Array:   <= 256 bytes
#  Address Copy: <= 256 bytes
#  Offset Value: address offset range <= 255  (count <=256 but offset <= 255)


# Output Modes
# ------------
#  * there are two output modes - strictly in source order and optimised
#       (within a label group?). The optimisation mode groups multiple
#       single value type 2 items into a CFG_TT_OV structure where possible.
#
#  * It is possible to write an address twice. A realistic use case is to zero
#    a large block (CF) and then sparse write values into it later.
#    With output mode preserving source order it is up to the data file creator
#    to put the block fill before any sparse fill of individual values.
#  * Optimised mode gathers Source Type 2 entries together to compress storage space.
#    To permit bulk fills and sparse writes to the same space the general
#    rule is bulk fills first then sparse.
#    In practice the key rule is probably only:
#       In output file put CF (bulk fill) before others.
#       An additional refinement might be CF before AC before others too.
#
#    To ensure results are determinant between version the following order is
#    used when output is optimised (LHS output/executed first):
#
#       CF -> AC -> DA(multi) -> OV -> DA(single)
#
#   Note that DA multi is like AC so it comes before OV and DA single


# Output/Memory Data Structure
# ----------------------------
#
# The top level entity is a list of data structures. The first byte of a
# structure contains the structure type, the list ends with an element
# type of CFG_TT_END. There are 4 types of initialisation structure: 
# Type: 1: constant fill, 2: data fill, 3:offset,value, 4: mem to mem copy
#
# Note: to permit count to handle common values of 256/65536 the stored count value has 1 added to it.
# Therefore a count of 0 is not possible.

# list structure:
#   [ HEADER ELEMENT ]* 0000
#   ->
#   [ ttcc aaaa aaaa ELEMENT ]* 0000
#       tt: type
#       cc: count_low - 1   # add 1 to get real count (ie stored 0 -> count=1, 1->2 etc)
#       a...a: 32 bit address
#       0000 is the list terminating value of ttcc
#
#
#  ELEMENT tt == 1:  constant fill (CF)
#    ccvv
#       cc: count_hi
#       vv: value
#
#  ELEMENT tt == 2:  offset value (OV)
#    [ oovv ]+
#       oo: offset
#       vv: value
#       "oovv" structure repeats cc (count_low) times
#
#  ELEMENT tt == 3:  data array (DA)
#    [ vv ]+
#       vv: value
#       "vv" structure repeats cc (count_low) times
#
#  ELEMENT tt == 4:  address copy (AC)
#    ssss ssss
#       s...s: 32 bit (source) address
#
#
# Note:
#  * For compactness addresses (eg 32 bit addresses) are NOT aligned on usual
#       memory boundaries. Note that ARMv7-M can handle unaligned 16/32 bit reads (not 64) IIRC,
#       Form ARMv6 and earlier can do byte reads and reconstruct any 16/32 bit values.

# TODO
# ----
#   add option for preserve/optimise order
#   optional: source type 5
#   optional: add other output formats. However can just pipe output to freehex2other.py
#   optional: add command line argument for endianness
#   optional: add command line argument for debug
#   optional: implement dest width in source type 3


import collections
import re
import sys
import argparse


Debug = 0


CFG_TT_UNDEF = -1
CFG_TT_END = 0
CFG_TT_CF = 1
CFG_TT_OV = 2
CFG_TT_DA = 3
CFG_TT_AC = 4
CFG_TT_LABEL = -1  # special type (put in configItem to maintain sequencing only)


ConfigItem = collections.namedtuple('ConfigItem', 'type addr value count')
# For output type OV: ConfigItem value is a dict[off] = value, count = len(dict)
# For output type AC: ConfigItem value is the source address

re_define = re.compile(r"#define\s+(?P<name>\w+)\s+(?P<value>.*)")

#Output_formats = ("hex", "intelhex", "binary")


def parse_command_line():
    parser = argparse.ArgumentParser(description='Compile config data into freehex file format')
#    parser.add_argument('-f', required=True, nargs=1, dest='output_format', help='output format: %s' % (",".join(Output_formats)))
    # Note I don't currently see any point in supporting read from stdin
    # Leave as -i though rather than positional for consistency and so natural
    # cmdline order is infile then outfile
    parser.add_argument('-i', required=True, nargs=1, dest="infile", help="input file")
    parser.add_argument('-o', nargs=1, dest="outfile", help="output file. Default stdout")

    return (parser, parser.parse_args())


def read_symtab(symtab, filename):
    linenum = 0
    with open(filename) as f:
        for line in f.readlines():
            linenum += 1

            m = re_define.match(line)
            if m is not None:
                # print('DEF:', m.group('name'), m.group('value'), file=sys.stderr)
                symtab[m.group('name')] = m.group('value')

    if Debug:
        for k,v in symtab.items():
            print('d:', k, v, file=sys.stderr)


def get_uint(symtab, valuestr, context=None):
    ''' get integer value of valuestr performing any symtab substitutions '''
    # context is optional list: (linenum origline)

    orig_valuestr = valuestr # for error handling only

    # repeatedly convert #define to their values
    while valuestr in symtab:
        # print("Converting %s to %s" % (valuestr, symtab[valuestr]), file=sys.stderr)
        valuestr = symtab[valuestr]

    # FIXME: HACkish. The #defines being used have numbers ending in 'u' like 0x12345u
    if valuestr[-1:] == 'u':
        valuestr = valuestr[:-1]

    # should be an integer by now (in some base)
    try:
        value=int(valuestr, 0)
    except (ValueError):
        context_msg = ""
        if context is not None:
            context_msg = " at line %d.\nOriginal line: %sFailed expression after substitution was: %s" % (context[0], context[1], valuestr)
        print("Failed to convert variable '%s' to an integer%s" % (orig_valuestr, context_msg), file=sys.stderr)
        # not great style to exit here but it's a simple program
        sys.exit(1);

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
                iaddr = get_uint(symtab, m.group('addr'), (linenum, origline))
                config.append(ConfigItem(CFG_TT_LABEL, iaddr, m.group('label'), 0)) # FIXME: BIT OF A HACK
                continue

            # clean up line
            line = re.sub(r"#.*", '', line)  # remove comments
            line = re.sub(r"\s+", '', line)  # in this case we can remove ALL blanks

            # remove (resulting) blank lines
            #if re_blank.match(line): continue
            if line == '': continue

            m = re_expr.match(line)
            if m is None:
                print("Syntax error on line %d: [%s]" % (linenum, origline), file=sys.stderr)
                return None
            
            if Debug:
                print("L:",line, file=sys.stderr)
                for k,v in m.groupdict().items():
                    print(k,": ",v, file=sys.stderr)
                print(file=sys.stderr)

            # Line contains a data

            # flags for line
            value_is_addr = False
            hascount = False
            type = CFG_TT_UNDEF
            icount = 1

            iaddr = get_uint(symtab, m.group('addr'), (linenum, origline))

            valuestr = m.group('val')
            b = re_brackets.match(valuestr)
            if b is not None:
                value_is_addr = True
#                print(b.groupdict.keys(), file=sys.stderr)
                valuestr = b.group('src_addr')
            # How does python handle really long hex runs !?
            ivalue = get_uint(symtab, valuestr, (linenum, origline))

            if ivalue < 0:
                print("Negative values not allowed: line %d: [%s]" % (linenum, origline), file=sys.stderr)
                return None


            if m.group('hascount') is not None:
                hascount = True
                icount=get_uint(symtab, m.group('count'), (linenum, origline))
                if icount < 1:
                    print("Count less than 1 not allowed: line %d: [%s]" % (linenum, origline), file=sys.stderr)
                    return None

            ci = None
            if value_is_addr:
                type = CFG_TT_AC  # memory copy
                ci = ConfigItem(type, iaddr, ivalue, icount)

            elif ivalue < 256: # single data byte
                if icount == 1:
                    type = CFG_TT_DA  # single value (data array len 1 is most compact, so is the canonical form)
                else: # count > 1
                    type = CFG_TT_CF  # constant fill (of single byte value)
                    assert(icount < 65536)
                assert(ivalue >= 0 and ivalue < 256)
                ci = ConfigItem(type, iaddr, ivalue, icount)

            else: # multiple data bytes
                type = CFG_TT_DA  # data array
                if valuestr[0:2] != "0x":
                    print("Data array case must be prefixed by 0x as a reminder data must be hex format: line %d: [%s]" % (linenum, origline), file=sys.stderr)
                    sys.exit(1)
                valuestr = valuestr[2:] # remove 0x, it has served its purpose

                icount = len(valuestr)
                assert(icount % 2 == 0)
                icount >>= 1
                ci = ConfigItem(type, iaddr, valuestr, icount)

            # Note: offset value form (CFG_TT_OV) never occurs 'naturally'

            if Debug:
                print("T:%d, C:%d, A:%x, V:%d" % (type, icount, iaddr, ivalue), file=sys.stderr)

            assert(icount > 0)
#            if type in [CFG_TT_CF, CFG_TT_OV]:
#                assert(ivalue >= 0 and ivalue < 256)
            if type != CFG_TT_CF:
                assert(icount < 256)

            config.append(ci)

    return config
            

# ---------------

def output_item_header(fout, item):
    addr_bytes = item.addr.to_bytes(4, byteorder=addr_byteorder)
    iaddr = int.from_bytes(addr_bytes, byteorder='big')
    print('%02x %02x %08x ' % (item.type, (item.count - 1) & 0xFF, iaddr), file=fout, end='')


def output_end_of_table(fout):
    # end marker
    print('%02x %02x' % (CFG_TT_END, 0), file=fout)


def output_label(fout, item):
    # FIXME: is this the best syntax !?
    # FIXME: #define or #label !?
    print('#label %s 0x%08x' % (item.value, item.addr), file=fout)


def output_item_element(fout, item, addr_byteorder):
    if item.type == CFG_TT_CF:
        print('%02x %02x' % ((((item.count - 1) >> 8) & 0xFF), item.value), file=fout)
    elif item.type == CFG_TT_OV:
        print(file=fout)
        for offset,value in item.value.items():
            print('  %02x %02x' % (offset, value), file=fout)
    elif item.type == CFG_TT_DA:
        # FIXME: HACK
        print('%s' % (item.value), file=fout)
    elif item.type == CFG_TT_AC:
        isaddr = int.from_bytes(item.value.to_bytes(4, byteorder=addr_byteorder), byteorder='big')
        print('%08x' % (isaddr), file=fout)
    else:
       print("Unexpected item type %d" % (item.type), file=sys.stderr)
       sys.exit(1)


def output_item(fout, item, addr_byteorder):
    if item.type == CFG_TT_LABEL:
        output_label(fout, item)
    else:
        output_item_header(fout, item)
        output_item_element(fout, item, addr_byteorder)


def output_items_of_type(fout, items, type, addr_byteorder):
    # Outputs matching items preserving relative order
    # Does not coalesce (OVs) even if possible
    itemlist = [item for item in items if item.type == type]
    if len(itemlist) == 0: return

    for item in itemlist:
        output_item(fout, item, addr_byteorder)


def output_preserve_order(fout, items, addr_byteorder):
    # Output data in original order but do coalesce adjacent OVs into a single
    # table entry where possible (ie where address within offset range).

    for item in items:
        output_item(fout, item, addr_byteorder)
    output_end_of_table(fout)


# --- START optimise code ---

def optimise_da_ov(items):
    # Returns list of all DAs some will be converted to OVs's others remain DAs

    da_list = []
    addr_dict = {} # addr_dict[addr] = value

    # Extract all multi DAs and copy into da_list unchanged.
    # Extract all single DAs int addr_dict for possible conversion to OVs
    for item in items:
        if item.type != CFG_TT_DA: continue
        if item.count > 1:
            da_list.append(item)   # leave these untouched
            continue

        # assert: item.count == 1
        # OV is made up of multiple single value DA's

        if item.addr in addr_dict:
            print("Cannot set same address twice in this optimisation case."
                " Addr:0x%x values: %d and %d" % \
                (item.addr, addr_dict[item.addr], item.value), file=sys.stderr)
            sys.exit(1)

        addr_dict[item.addr] = item.value

    if len(addr_dict) == 0:
        return da_list

    addr_list = sorted(addr_dict.keys())

    # group addresses with a range of 255
    ov_dict = {}  # ov_dict[addr_offset] = value
    base_addr = 0

    for addr in addr_list: 
        offset = 0

        if len(ov_dict) == 0:
            base_addr = addr
            ov_dict[0] = addr_dict[addr]
        else:
            offset = addr - base_addr
            if offset < 256:
                ov_dict[addr - base_addr] = addr_dict[addr]

        if offset >= 256 or addr == addr_list[-1]: # offset too large or last item in list
            # output current set
            count = len(ov_dict)
            assert(count > 0 and count < 256)
            if count == 1: # output in original form as a DA
                ci = ConfigItem(CFG_TT_DA, addr, addr_dict[addr], count)
            else:
                ci = ConfigItem(CFG_TT_OV, base_addr, ov_dict, count)
            da_list.append(ci)
            ov_dict = {}

        if offset >= 256:
            base_addr = addr
            ov_dict[0] = addr_dict[addr]

    # remainder case not handled above: if offset >= 256 and it was last item
    assert(len(ov_dict) <= 1)
    if len(ov_dict) == 1:
        ci = ConfigItem(CFG_TT_DA, addr, addr_dict[addr], count)
        da_list.append(ci)

    return da_list


def output_optimise(fout, items, addr_byteorder):
    # Output order: (labels) -> CF -> AC -> DA(multi) -> OV -> DA(single)

    optimised_da_ov_list = optimise_da_ov(items)

    for type in [CFG_TT_LABEL, CFG_TT_CF, CFG_TT_AC]:
        output_items_of_type(fout, items, type, addr_byteorder)

    # output_items_of_type(DA multi-byte)
    da_multi = [item for item in optimised_da_ov_list if item.type == type and item.count > 1]
    if len(da_multi) > 0:
        for item in da_multi:
            output_item(fout, item, addr_byteorder)

    # OV
    output_items_of_type(fout, optimised_da_ov_list, CFG_TT_OV, addr_byteorder)

    # output_items_of_type(DA single)
    da_single = [item for item in optimised_da_ov_list if item.type == type and item.count == 1]
    if len(da_single) > 0:
        for item in da_single:
            output_item(fout, item, addr_byteorder)

    output_end_of_table(fout)


# --- END optimise code ---


def output_hex(outfile, items, preserve_order, addr_byteorder):
    # Outfile is None or filename in outfile[0]
    # Note: outputs "human readable" hex version
    # FIXME: check if file already exists

    fout = None
    outmode = ''

    if outfile is None:
        fout = sys.stdout
#        if outmode == 'b':
#            print("Cannot output binary to stdout", file=sys.stderr)
#            # FIXME: there are ways to do this...
#            sys.exit(1)
    else:
        fout = open(outfile[0], 'w'+outmode)

    if fout is None:
        print("Failed to create file %s" % (outfile), file=sys.stderr)
        sys.exit(1)

    if preserve_order:
        output_preserve_order(fout, items, addr_byteorder)

    else: # optimise for space
        output_optimise(fout, items, addr_byteorder)

    fout.close()


# --- main ---

(parser, args) = parse_command_line()

#if format not in Output_formats:
#    print("Output format must be one of: %s" % (",".join(Output_formats)), file=sys.stderr)
#    sys.exit(1)

config = parse(args.infile[0])

preserve_order = False
addr_byteorder = 'little'  # 32 bit addresses are output LSB first for ARM

if Debug:
    for item in config:
        print(item, file=sys.stderr)
    

output_hex(args.outfile, config, preserve_order, addr_byteorder)
