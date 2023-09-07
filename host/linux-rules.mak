# ----------------------------------------------------------------------
# set the minors rules variable so that changes in here will cause
# the make system to rebuild things as needed
# ----------------------------------------------------------------------
X_MINOR_RULES_MAK=linux-rules.mak

OPENSSL_OS_LIST := $(shell cat openssl_dynamic_distros_list.txt)
$(info    linux-rules.mak: OPENSSL_OS_LIST = $(OPENSSL_OS_LIST))

X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)
X_KERVER:=$(shell uname -r)
ifeq ($(X_OS), RHEL6-64)
ifeq ($(X_KERVER), 2.6.32-358.23.2.el6.x86_64)
	X_OS:=RHEL6U4-64
else
	ifeq ($(X_KERVER), 2.6.32-358.el6.x86_64)
		X_OS:=RHEL6U4-64
	endif
endif
endif

$(info    linux-rules.mak: X_OS = $(X_OS))

# ----------------------------------------------------------------------
# compilers, linkers, lib creation, etc. commands
# ----------------------------------------------------------------------
COMPILE.cpp = g++ -c
LINK.cpp = g++
COMPILE.c = gcc -c
LINK.c = gcc
AR = ar r
RANLIB = ranlib
MKDIR = mkdir -p

# needed by config/Makefile must be the native compilier
# compiler for building fromheader.cpp
FROM_HEADER.cpp = g++ -pthread
# compiler and options to determine what a file depends on
DEPENDENCIES.cpp = g++ -MM -x c++

# command to generate source dependencies rule
DEPEND_SH = gcc-depend

# ----------------------------------------------------------------------
# options for linking libraries that have cyclic dependencies  
# ----------------------------------------------------------------------
LIB_CYCLIC_START_OPT = -Wl,--start-group
LIB_CYCLIC_END_OPT = -Wl,--end-group

# ----------------------------------------------------------------------
# We are shipping libgcc and libstdc++ as part of VX package in case of
# Solaris. Binaries should resolve the above libraries from ../lib
# This value will be empty in linux makefiles
#
# As per Greg's suggestion upgraded gcc compiler on RHEL3-32 build machine
# from old gcc 3.2.3 to gcc 3.4.6 as builds are failing in compilation 
# error in boost_1_67_0. We are shipping libgcc and libstdc++ for RHEL3-32 
# OS as part of FX agent builds.
# ----------------------------------------------------------------------
ifeq (RHEL3-32,$(X_OS))
RPATH = -Wl,-R./lib
else
RPATH =
endif

# ----------------------------------------------------------------------
# this section is for adding platform specific options that are needed 
# by 1 or more modules but not by all modules. just define them here
# and use them in individual Makefiles as needed. for any definition
# listed not needed by a particular platform, either delete the 
# definition or set it to blank. 
#
# this will prevent the need for platform specific make files when
# special/different options are needed by individual modules but not all
# ----------------------------------------------------------------------
LIBCURSES = -lncurses

ifeq (SLES11-64,$(X_OS))
NO_SSE2 =
else
NO_SSE2 = -mno-sse2
endif

ifeq ($(X_ARCH),Linux_x86_64)
LIB64 = 64
else 
LIB64 =
endif

# ----------------------------------------------------------------------
# warnings 
# warnlevel=error|warn|none (default: none)
# ----------------------------------------------------------------------
WARN_FLAGS := -Wall -W -Wconversion -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings

ifeq (error,$(warnlevel))
WARN_LEVEL := -Werror $(WARN_FLAGS)
else 
ifeq (warn,$(warnlevel))
WARN_LEVEL := $(WARN_FLAGS)
else
WARN_LEVEL := 
endif 
endif

# ----------------------------------------------------------------------
# configuration mode settings
# ----------------------------------------------------------------------
ifeq (release,$(X_CONFIGURATION))
CFLAGS_MODE := -O3 -DNDEBUG
define BINARY_SYMBOLS_CMD
cp $@ $@.dbg
objcopy --strip-debug $@
objcopy --add-gnu-debuglink=$@.dbg $@
endef
else
CFLAGS_MODE := -DDEBUG -O0 -fno-optimize-sibling-calls -fno-omit-frame-pointer
BINARY_SYMBOLS_CMD =
endif

# ----------------------------------------------------------------------
# CFLAGS that apply to all modules
# ----------------------------------------------------------------------
CFLAGS := \
	-g3 \
	-ggdb \
	-DSV_UNIX \
	-DSV_LINUX \
   -DSTDC_HEADERS \
   -DHAVE_UNISTD_H \
   -DHAVE_SIGACTION \
   -DHAVE_SETSID \
   -DHAVE_STRCASECMP \
   -DHAVE_NETDB_H \
	-DHAVE_NETINET_H \
	-DACE_AS_STATIC_LIBS \
	-D_LARGEFILE_SOURCE=1 \
	-D_LARGEFILE64_SOURCE=1 \
	-D_FILE_OFFSET_BITS=64 \
	-DBOOST_SYSTEM_NO_DEPRECATED\
	-pthread \
	$(NO_SSE2) \

ifeq (1, $(SV_FABRIC))
	CFLAGS += -DSV_FABRIC
endif

ifeq (SLES11-32, $(X_OS))
	CFLAGS += -DINMAGE_ALLOW_UNSUPPORTED
endif

ifeq (SLES11-64, $(X_OS))
	CFLAGS += -DINMAGE_ALLOW_UNSUPPORTED
endif

ifeq ($(X_OS),$(filter $(X_OS), RHEL9-64  UBUNTU-22.04-64 OL9-64))
	CFLAGS += -DBOOST_ASIO_DISABLE_STD_FUTURE
endif

ifeq (XenServer,$(X_XENOS))
# get gcc to do garbage collection before it runs out of memory
# can increase compile times, but these seem to work and improve 
# compile times for the cases where memory was running low or
# running out
	CFLAGS += --param ggc-min-expand=1 --param ggc-min-heapsize=102400
endif

ifeq (RHEL6-32, $(X_OS))
        CFLAGS += -DRHEL6
endif

ifeq (RHEL6-64, $(X_OS))
        CFLAGS += -DRHEL6
endif

ifeq (RHEL6U4-64, $(X_OS))
        CFLAGS += -DRHEL6
endif

ifeq (UBUNTU-14.04-64, $(X_OS))
ifneq (,$(filter $(X_MODULE),cxpslib cxpscli))
        CFLAGS += -fpermissive
endif
endif

# ----------------------------------------------------------------------
# dirs and libs that apply to all modules
# you must specify the complet path and library name
# E.g. 
# /usr/local/foo/libfoo.a
# ----------------------------------------------------------------------
DIR_LIBS := 

# ----------------------------------------------------------------------
# system libs that apply to all modules 
# you must specify the -l switch and library name
# E.g. 
# -lfoo
# ----------------------------------------------------------------------
SYSTEM_LIBS := -lpthread \
	-ldl \
	-lrt \
	-luuid \

ifeq (RHEL6-32, $(X_OS))
    SYSTEM_LIBS += -lacl
endif

ifeq (RHEL6-64, $(X_OS))
    SYSTEM_LIBS += -lacl
endif

ifeq (RHEL6U4-64, $(X_OS))
    SYSTEM_LIBS += -lacl
endif

ifeq ($(findstring $(X_OS),$(OPENSSL_OS_LIST)),$(X_OS))
    SYSTEM_LIBS += -lssl -lcrypto
endif

$(info    linux-rules.mak: SYSTEM_LIBS = $(SYSTEM_LIBS))

# ----------------------------------------------------------------------
# LDFLAGS that apply to all modues
# ----------------------------------------------------------------------
LDFLAGS :=

ifeq (yes,$(intel))
include vx_intel_setup.mak
else
include fx_setup.mak
include vx_setup.mak
include ua_setup.mak
endif

# ----------------------------------------------------------------------
# include the rules that this platform should use
# this must be the very last thing in this file
# ----------------------------------------------------------------------
include rules.mak
