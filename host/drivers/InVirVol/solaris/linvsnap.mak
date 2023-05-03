WORKING_DIR := $(shell pwd)

.PHONY: all clean

ifeq (yes, $(debug))
        DEBUG_FLAGS = -D_VSNAP_DEBUG_
else
        ifeq (dev, $(debug))
                DEBUG_FLAGS = "-D_VSNAP_DEBUG_ -D_VSNAP_DEV_DEBUG_"
endif
endif

all:
	@mkdir -p bld_linvsnap
	@cp *.[ch] ${WORKING_DIR}/../common/*.[ch] Makefile linvsnap.conf bld_linvsnap/
	@cd bld_linvsnap; PATH=/usr/ccs/bin:${PATH}; export PATH; make clean; make DEBUG_FLAGS=$(DEBUG_FLAGS)

clean:
	@rm -rf bld_linvsnap
