LIBNAME=libhex.a
OBJS = HexData.o
TESTPROGNAMES=testmyhex

testmyhex: testmyhex.o libhex.a
	$(CXX) -o $@ $< -L. -lhex

include ../Makefile.inc
