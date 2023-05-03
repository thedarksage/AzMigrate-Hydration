#
# diffdatasort makefile
#
X_CONFIGURATION := release

X_SPECIFIC := $(shell ../get-specific-version-info)

ACE_ROOT := ../../thirdparty/ace-6.4.6/ACE_wrappers
ACE_INCLUDE := -I$(ACE_ROOT)/config_$(X_CONFIGURATION) -I$(ACE_ROOT)
ACE_LIBS := $(ACE_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libACE.a

BOOST_ROOT := ../../thirdparty/boost/boost_$(BOOST_VERSION)
BOOST_INCLUDE := -I$(BOOST_ROOT)


LIBDIR:=$(shell uname -ms| sed "s/ /_/g")

all:		diffdatasort

diffdatasort:	diffdatasort.o diffsortcriterion.o
	g++ -lz -o diffdatasort diffdatasort.o diffsortcriterion.o $(ACE_LIBS) -g3

diffdatasort.o:	diffdatasort.cpp 
	g++ -c -Wall -DSV_UNIX=1 -I../common -I../common/unix -I../inmsafecapis -I../inmsafecapis/unix $(BOOST_INCLUDE) $(ACE_INCLUDE) -I../dataprotection -I../cdpapplylib diffdatasort.cpp -g3

diffsortcriterion.o:	../cdpapplylib/diffsortcriterion.cpp
	g++ -c -Wall -DSV_UNIX=1 -I../dataprotection -I../common -I../common/unix -I../inmsafecapis -I../inmsafecapis/unix $(ACE_INCLUDE) $(BOOST_INCLUDE) ../cdpapplylib/diffsortcriterion.cpp -g3


clean:
	rm -f diffdatasort diffdatasort.dbg diffdatasort.o diffsortcriterion.o
