LIBNAME ?=
PROGNAMES ?=
TESTPROGNAMES ?=
TARGETS = $(LIBNAME) $(PROGNAMES) $(TESTPROGNAMES)

CC=gcc
CXX=g++
CFLAGS += $(INC)
CPPFLAGS += $(INC)
INSTALL=install
COPY=cp


# General Targets

.PHONY: all install clean

all: $(TARGETS)

install:
ifdef PROGNAMES
	@if [ "$(INSTALL_DIR)" == "" ] ; then \
	echo INSTALL_DIR is not set; \
	exit 1; \
	else \
	echo Install directory: $(INSTALL_DIR); \
	echo Installing: $(PROGNAMES); \
	$(INSTALL) $(PROGNAMES) $(INSTALL_DIR); \
	fi
endif


clean::
	 rm -f *.o $(TARGETS)


# General Rules

$(LIBNAME): $(OBJS)
	$(RM) -f $(LIBNAME)
	$(AR) cr $(LIBNAME) $(OBJS)


%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c $<

%.o: %.cc
	$(CXX) $(CPPFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

#%.a: %.o
#	$(RM) -f $@
#	$(AR) cr $@ $.*o

.o.a:
	$(RM) -f $@
	$(AR) cr $@ $.*o

%.d: %.c
	$(SHELL) -ec '$(CC) -M $(CFLAGS) $< \
		| sed '\"s/$*\\.o[ :]*/& $@/g'\" > $@'

