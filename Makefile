all: prog testmyhex hex2bin hexinfo mergehex

INC=-I inih-read-only/cpp/ -I inih-read-only
CC=g++
LDFLAGS=-L /usr/local/lib -lusb-1.0
CFLAGS= $(INC) -g

#HEXDIR=libintelhex/src
#OBJS=main.o usb.o prog_cy8ckit.o utils.o fx2.o device_psoc5.o intelhex.o
OBJS=prog.o AppData.o DeviceData.o Programmer.o HexData.o utils.o fx2.o usb.o INIReader.o HierINIReader.o ini.o
#libintelhex/src/intelhex.o

prog: $(OBJS)

testmyhex: testmyhex.o HexData.o

hex2bin: hex2bin.o HexData.o

hexinfo: hexinfo.o AppData.o HexData.o utils.o

mergehex: mergehex.o AppData.o HexData.o utils.o

buildhex: buildhex.o AppData.o HexData.o utils.o

clean:
	 rm -f *.o prog testmyhex hex2bin hexinfo mergehex buildhex

%.o : %.cpp
	$(CC) $(CFLAGS) -c $<

%.o : %.cc
	$(CC) $(CFLAGS) -c $<

%.o : $(HEXDIR)/%.cc
	$(CC) $(CFLAGS) -c $<
