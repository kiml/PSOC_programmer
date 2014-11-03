PROGNAMES=hex2bin mergehex hexinfo

INC = -I ../libhex -I ../programmer
LIBS = -L ../libhex -lhex

include ../Makefile.inc

mergehex: mergehex.cpp ../programmer/AppData.cpp ../programmer/utils.c
	$(CXX) $(INC) -o $@ $^ $(LIBS)

hexinfo: hexinfo.cpp ../programmer/AppData.cpp ../programmer/utils.c
	$(CXX) $(INC) -o $@ $^ $(LIBS)

hex2bin: hex2bin.cpp
	$(CXX) $(INC) -o $@ $^ $(LIBS)
