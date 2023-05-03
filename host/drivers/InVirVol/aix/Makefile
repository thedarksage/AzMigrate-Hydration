GMAKE=gmake

all:
	@ln -fs ../common/*.[ch] .;$(GMAKE) -k -f Makefile.gmake debug=$(debug)

expand:
	@ln -fs ../common/*.[ch] .;$(GMAKE) -k -f Makefile.gmake debug=$(debug) expand

clean:
	@find . -type l -print | xargs rm -f; $(GMAKE) -f Makefile.gmake clean
