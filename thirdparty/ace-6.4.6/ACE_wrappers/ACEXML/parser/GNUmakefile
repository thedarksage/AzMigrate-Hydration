# -*- makefile -*-
#----------------------------------------------------------------------------
#       GNU ACE Workspace
#
# 
#
# This file was generated by MPC.  Any changes made directly to
# this file will be lost the next time it is generated.
#
# MPC Command:
# /home/johnny/DOC_ROOT/stage-4724/ACE_wrappers/bin/mwc.pl -type gnuace -exclude TAO/TAO_ACE.mwc -workers 8 -recurse -hierarchy -relative ACE_ROOT=/home/johnny/DOC_ROOT/stage-4724/ACE_wrappers -relative TAO_ROOT=/home/johnny/DOC_ROOT/stage-4724/ACE_wrappers/TAO
#
#----------------------------------------------------------------------------

MAKEFILE = GNUmakefile

ifeq ($(findstring k,$(MAKEFLAGS)),k)
  KEEP_GOING = -
endif

include $(ACE_ROOT)/include/makeinclude/macros.GNU

all: ACEXML_Parser

depend: ACEXML_Parser-depend

REMAINING_TARGETS := $(filter-out all depend,$(TARGETS_NESTED:.nested=)) $(CUSTOM_TARGETS)
.PHONY: $(REMAINING_TARGETS)

$(REMAINING_TARGETS):
	$(KEEP_GOING)@cd parser && $(MAKE) -f GNUmakefile.ACEXML_Parser $(@)

.PHONY: ACEXML_Parser
ACEXML_Parser:
	$(KEEP_GOING)@cd parser && $(MAKE) -f GNUmakefile.ACEXML_Parser all

.PHONY: ACEXML_Parser-depend
ACEXML_Parser-depend:
	$(KEEP_GOING)@cd parser && $(MAKE) -f GNUmakefile.ACEXML_Parser depend

project_name_list:
	@echo ACEXML_Parser
