# ======================================================================
# for debugging rules.mak so that changes to these files don't
# force everything to rebuild
#
# just comment out any entry for the file you are updating and it will
# not cause everything to rebuild. you always have to comment out
# RULES_MAK 
# 
# DO NOT CHECK THIS IN WITH ANY OF THESE COMMENTED OUT, REMOVED OR 
# SET TO BLANK
# ======================================================================
RULES_MAK=rules.mak
MAKEFILE=Makefile
TOP_MAK=top.mak
BOTTOM_MAK=bottom.mak
ONCE_MAK=once.mak
THIRDPARTY_BUILD=thirdparty_build
GET_SPECIFIC_VERSION_INFO=get-specific-version-info
X_ARCH_RULES_MAK=$(X_ARCH)-rules.mak

# ======================================================================
# common rules include by $(X_ARCH)-rules.mak
#
# in general everything here should be generic so you should not have to 
# modify anything when porting to a new platform as there should be no 
# specific os apis used directly instead create a variable with the same 
# (similar) name and use that instead. e.g.
#
# MKDIR := mkdir -p
#
# However if you need to add/modify/delete the actual rules and/or actions 
# and those changes apply to all platforms then do it in here. If it is 
# platform specific then you need to create a new rules.mak file for that
# platform and include that inside the $(X_ARCH)-rules.mak
# do that instead of just putting the rules directly in the 
# $(X_ARCH)-rules.mak in case other platforms need those same changes too
# ======================================================================

# If module is vacpunix and X_VXFSVACP set to yes, add -DVXFSVACP to CFLAGS
ifeq (vacpunix, $(X_MODULE))
ifeq (yes, $(X_VXFSVACP))
        CFLAGS += -DVXFSVACP
endif
endif

# ----------------------------------------------------------------------
# combine module specific and global values
# ----------------------------------------------------------------------
$(X_MODULE)_CFLAGS := \
	$(CFLAGS_MODE) \
	$(WARN_LEVEL) \
	$(CFLAGS) \
	$(X_CFLAGS)


# LDFLAGS
$(X_MODULE)_LDFLAGS := $(LDFLAGS) $(X_LDFLAGS)

# make sure all the needed library modules have been loaded
ifneq (, $(X_LIBS))
X_MODULES_SET := $(shell ./validate-modules-set $(X_LIBS) -m $(patsubst %/Makefile,%,$(X_MAKEFILES)))
ifneq (,$(X_MODULES_SET))
$(error "$(X_MODULES_SET)" module(s) must appear in the top level Makfile MODULES list before module "$(X_MODULE)")
endif
# set up all X_LIBS with their full path and library name 
X_DIR_LIBS := $(foreach LIB,$(X_LIBS),$($(LIB)_BINARY))
else
X_DIR_LIBS := 
endif

# lib dependencies needed for binary rule so that it will rebuild if any fo the libs were modified
$(X_MODULE)_LIB_DEPS := $(X_DIR_LIBS) $(X_THIRDPARTY_LIBS) $(DIR_LIBS)

# combine all the libs adding the correct compiler library options needed for cyclic libraris
# TODO: we should really removee cyclic dependencies between our libraries
$(X_MODULE)_DIR_LIBS := $(LIB_CYCLIC_START_OPT) $(X_DIR_LIBS) $(X_THIRDPARTY_LIBS) $(LIB_CYCLIC_END_OPT) $(DIR_LIBS) $(X_SYSTEM_LIBS) $(SYSTEM_LIBS)

# ----------------------------------------------------------------------
# include source dependencies but only do that if not doing a clean as 
# no need to generate .d files just to delete them
# ----------------------------------------------------------------------
ifneq (clean, $(findstring clean,$(MAKECMDGOALS)))
-include $($(X_MODULE)_OBJS:.o=.d)
else
 # keep make happy when cleaning and output dirs don't exist
$($(X_MODULE)_OUTPUT):
endif

# ----------------------------------------------------------------------
# compute dependencies and compile CPP code
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/%.d:: CPP_DEP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/%.d:: $(X_MODULE_DIR)/%.cpp $(X_MODULE_DIR)/Makefile $(THIRDPARTY_BUILD) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(ONCE_MAK) $(DEPEND_SH) $(X_GENERATED_HEADERS)
	$(VERBOSE)./$(DEPEND_SH) "$@" $(CPP_DEP_CFLAGS) '$<'
	$(RULE_SEPARATOR)

$($(X_MODULE)_OUTPUT)/%.o:: CPP_COMP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/%.o:: $(X_MODULE_DIR)/%.cpp $(X_MODULE_DIR)/Makefile $(MAKEFILE) $(TOP_MAK) $(BOTTOM_MAK) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(X_ARCH).mak $(GET_SPECIFIC_VERSION_INFO)
	$(VERBOSE)$(COMPILE.cpp)  $(CPP_COMP_CFLAGS) -o '$@' '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# compute dependencies and compile CPP code that is under another module
# NOTE: in general we should not need this instead the code under 
# another module should be in a library and this module should use it
# but it is here as that was not always done 
# we could remove this rule and that would let us know when we
# need to create a library instead of allowing this
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/$(DOTDOT)/%.d:: CPP_SUBDIR_DEP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/$(DOTDOT)/%.d:: $(X_MODULE_DIR)/../%.cpp $(X_MODULE_DIR)/Makefile $(THIRDPARTY_BUILD) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(ONCE_MAK) $(DEPEND_SH) $(X_GENERATED_HEADERS)
	$(VERBOSE)./$(DEPEND_SH) "$@" $(CPP_SUBDIR_DEP_CFLAGS) '$<'
	$(RULE_SEPARATOR)

$($(X_MODULE)_OUTPUT)/$(DOTDOT)/%.o:: CPP_SUBDIR_COMP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/$(DOTDOT)/%.o:: $(X_MODULE_DIR)/../%.cpp $(X_MODULE_DIR)/Makefile $(MAKEFILE) $(TOP_MAK) $(BOTTOM_MAK) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(X_ARCH).mak $(GET_SPECIFIC_VERSION_INFO)
	$(VERBOSE)$(COMPILE.cpp) $(CPP_SUBDIR_COMP_CFLAGS) -o '$@' '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# compute dependencies and compile CPP X_MAJOR_ARCH specific code 
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.d:: CPP_MAJ_DEP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.d:: $(X_MODULE_DIR)/$(X_MAJOR_ARCH)/%.cpp $(X_MODULE_DIR)/Makefile $(THIRDPARTY_BUILD) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(ONCE_MAK) $(DEPEND_SH) $(X_GENERATED_HEADERS)
	$(VERBOSE)./$(DEPEND_SH) "$@" $(CPP_MAJ_DEP_CFLAGS) '$<'
	$(RULE_SEPARATOR)

$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.o:: CPP_MAJ_COMP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.o:: $(X_MODULE_DIR)/$(X_MAJOR_ARCH)/%.cpp $(X_MODULE_DIR)/Makefile $(MAKEFILE) $(TOP_MAK) $(BOTTOM_MAK)  $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(X_ARCH).mak $(GET_SPECIFIC_VERSION_INFO)
	$(VERBOSE)$(COMPILE.cpp) $(CPP_MAJ_COMP_CFLAGS) -o '$@' '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# compute dependencies and compile C code
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/%.d:: C_DEP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/%.d:: $(X_MODULE_DIR)/%.c $(X_MODULE_DIR)/Makefile $(THIRDPARTY_BUILD) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(ONCE_MAK) $(DEPEND_SH) $(X_GENERATED_HEADERS)
	$(VERBOSE)./$(DEPEND_SH) "$@" $(C_DEP_CFLAGS) '$<'
	$(RULE_SEPARATOR)

$($(X_MODULE)_OUTPUT)/%.o:: C_COMP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/%.o:: $(X_MODULE_DIR)/%.c $(X_MODULE_DIR)/Makefile $(MAKEFILE) $(TOP_MAK) $(BOTTOM_MAK) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(X_ARCH).mak $(GET_SPECIFIC_VERSION_INFO)
	$(VERBOSE)$(COMPILE.c) $(C_COMP_CFLAGS) -o '$@' '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# compute dependencies and compile C X_MAJOR_ARCH specific code
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.d:: C_MAJ_DEP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.d:: $(X_MODULE_DIR)/$(X_MAJOR_ARCH)/%.c $(X_MODULE_DIR)/Makefile $(THIRDPARTY_BUILD) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(ONCE_MAK) $(DEPEND_SH) $(X_GENERATED_HEADERS)
	$(VERBOSE)./$(DEPEND_SH) "$@" $(C_MAJ_DEP_CFLAGS) '$<'
	$(RULE_SEPARATOR)

$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.o:: C_MAJ_COMP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/$(X_ARCH)/%.o:: $(X_MODULE_DIR)/$(X_MAJOR_ARCH)/%.c $(X_MODULE_DIR)/Makefile $(MAKEFILE) $(TOP_MAK) $(BOTTOM_MAK) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(X_ARCH).mak $(GET_SPECIFIC_VERSION_INFO)
	$(VERBOSE)$(COMPILE.c) $(C_MAJ_COMP_CFLAGS) -o '$@' '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# compute dependencies and compile CPP code in the X_ARCH dir
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/%.d:: CPP_ARCH_DEP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/%.d:: $(X_ARCH)/$(X_MODULE_DIR)/%.cpp $(X_MODULE_DIR)/Makefile $(THIRDPARTY_BUILD) $(X_ARCH_RULES_MAK) $(X_MINOR_RULES_MAK) $(RULES_MAK) $(ONCE_MAK) $(DEPEND_SH) $(X_GENERATED_HEADERS)
	$(VERBOSE)./$(DEPEND_SH) "$@" $(CPP_ARCH_DEP_CFLAGS) '$<'
	$(RULE_SEPARATOR)

$($(X_MODULE)_OUTPUT)/%.o:: CPP_ARCH_COMP_CFLAGS:=$($(X_MODULE)_CFLAGS)

$($(X_MODULE)_OUTPUT)/%.o:: $(X_ARCH)/$(X_MODULE_DIR)/%.cpp  $(X_ARCH)/$(X_MODULE_DIR)/%.h
	$(VERBOSE)$(COMPILE.cpp) $(CPP_ARCH_COMP_CFLAGS) -o '$@' '$<' 
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# create libs 
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/$(BINARY)$(X_LIBEXT):: $($(X_MODULE)_OBJS)
	$(VERBOSE)$(AR) '$@' $^
	$(VERBOSE)$(RANLIB) '$@'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# link final binaries
# note: does not re-link if a system library has been updated
# since the last time make was run
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/$(BINARY)$(X_EXEEXT):: $($(X_MODULE)_OBJS) $($(X_MODULE)_LIB_DEPS) $($(X_MODULE)_OUTPUT)/dummy_target$(X_MODULE) 
	$(VERBOSE)$(LINK.cpp) $($(subst dummy_target,,$(notdir $(word $(words $^), $^)))_OBJS) $($(subst dummy_target,,$(notdir $(word $(words $^), $^)))_LDFLAGS) $($(subst dummy_target,,$(notdir $(word $(words $^), $^)))_DIR_LIBS) $(RPATH) -o $@
	$(VERBOSE)$(BINARY_SYMBOLS_CMD)
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# dummy file used to let rule execution know which module defined the rule
# ----------------------------------------------------------------------
$($(X_MODULE)_OUTPUT)/dummy_target$(X_MODULE):
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# clean individual module (all configurations)
# ----------------------------------------------------------------------
.PHONY: clean_$(X_MODULE)_all
clean_$(X_MODULE)_all:: $(X_ARCH)/$(X_MODULE_DIR)
	$(VERBOSE)rm -Rf '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# clean individual module configuration
# ----------------------------------------------------------------------
.PHONY: clean_$(X_MODULE)
clean_$(X_MODULE):: $($(X_MODULE)_OUTPUT)
	$(VERBOSE)rm -Rf '$<'
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# create tag files used for vim and emacs source navigation
# ----------------------------------------------------------------------
.PHONY: ctags
ctags::
	$(VERBOSE)ctags -R "--exclude=bin/*"
	$(RULE_SEPARATOR)

.PHONY: etags
etags::
	$(VERBOSE)ctags -e -R "--exclude=bin/*"
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# some branding helper commands
# ----------------------------------------------------------------------
ifndef X_ALREADY_INCLUDED
.PHONY: show-version 
show-version:
	$(VERBOSE)echo $(X_VERSION_DOTTED)
	$(RULE_SEPARATOR)

.PHONY: show-partner
show-partner:
	$(VERBOSE)echo $(X_PARTNER)
	$(RULE_SEPARATOR)
endif

