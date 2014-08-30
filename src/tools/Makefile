PROGNAMES=hex2bin mergehex hexinfo

INC = -I ../libhex -I ../programmer
LDFLAGS = -L ../libhex -lhex

include ../Makefile.inc

mergehex: mergehex.o ../programmer/AppData.o ../programmer/utils.o

hexinfo: hexinfo.o ../programmer/AppData.o ../programmer/utils.o

hex2bin: hex2bin.o

