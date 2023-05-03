WORKING_DIR := $(shell pwd)

.PHONY: all clean

ifneq (,$(KDIR))
KERDIR := "KDIR=$(KDIR)"
else
KERDIR :=
endif

all:
	@mkdir -p bld_linvsnap
	@cp *.[ch] ${WORKING_DIR}/../common/*.[ch] Makefile bld_linvsnap/
	@cd bld_linvsnap; make clean; make debug=$(debug) fabric=$(fabric) $(KERDIR)

clean:
	@rm -rf bld_linvsnap
