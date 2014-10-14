PROGNAMES=test_config_data

INC = -I ../../support
CFLAGS += -DSRC_TEST_MODE

all: $(TARGETS) tests

include ../../Makefile.inc

config_data.c:
	$(COPY) ../../support/$@ .

test_config_data: config_data.o test_config_data.o

tests: test_config_data
	./test_config_data test.bin > test1.out
	diff test1.out test1.ref
#	[ $? -eq 0 ] || echo "ERROR"
#	rm test1.out


clean::
	$(RM) test1.out
