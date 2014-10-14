PROGNAMES=hex2bin mergehex hexinfo

INC = -I ../libhex -I ../programmer
LDFLAGS = -L ../libhex -lhex

include ../Makefile.inc

mergehex: mergehex.cpp ../programmer/AppData.cpp ../programmer/utils.c

hexinfo: hexinfo.cpp ../programmer/AppData.cpp ../programmer/utils.c

hex2bin: hex2bin.cpp

