SUBDIRS = src

export TOPDIR := $(shell echo $$PWD)
export INSTALL_DIR=$(TOPDIR)/bin

.PHONY: all install clean

all install clean::
	for dir in $(SUBDIRS); do \
	    $(MAKE) -C $$dir $@; \
	done

clean::
	$(RM) ./bin/*
